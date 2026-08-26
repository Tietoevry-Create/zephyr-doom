//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Networking module which uses Zephyr BSD sockets.
//
//     This is a protocol-compatible drop-in replacement for the upstream
//     Chocolate Doom net_sdl module. It implements the same net_module_t
//     interface, but uses Zephyr's portable socket API (zsock_*) instead of
//     SDL_net, so the same code works on native_sim (host-offloaded sockets)
//     and on any board with an IP-capable network interface.
//
//     ORIGIN: new file, written for zephyr-doom as part of multiplayer
//     support. It is adapted from Chocolate Doom 3.0.0's src/net_sdl.c (hence
//     the retained Simon Howard copyright above): the net_module_t interface,
//     the address table and the packet framing follow that file, with the
//     SDL_net calls replaced by zsock_* equivalents. Two deliberate
//     divergences from net_sdl.c are documented at their call sites: socket
//     errors are non-fatal (a transient link blip must not kill the game), and
//     addresses are kept in network byte order like SDL_net's IPaddress so the
//     ported logic stays unchanged. See docs/multiplayer-design.md for the
//     rationale and the bring-up history; git log for authorship.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "net_defs.h"
#include "net_io.h"
#include "net_packet.h"
#include "net_zephyr.h"
#include "z_zone.h"

#define DEFAULT_PORT 2342

// Stored address. host and port are kept in network byte order, exactly
// like SDL_net's IPaddress, so the address handling logic ported from
// net_sdl.c stays unchanged.

typedef struct
{
    uint32_t host;
    uint16_t port;
} zaddr_t;

static boolean initted = false;
static int port = DEFAULT_PORT;
static int udpsocket = -1;

typedef struct
{
    net_addr_t net_addr;
    zaddr_t zephyr_addr;
} addrpair_t;

static addrpair_t **addr_table;
static int addr_table_size = -1;

// Initializes the address table

static void NET_Zephyr_InitAddrTable(void)
{
    addr_table_size = 16;

    addr_table = Z_Malloc(sizeof(addrpair_t *) * addr_table_size,
                          PU_STATIC, 0);
    memset(addr_table, 0, sizeof(addrpair_t *) * addr_table_size);
}

static boolean AddressesEqual(zaddr_t *a, zaddr_t *b)
{
    return a->host == b->host
        && a->port == b->port;
}

// Finds an address by searching the table.  If the address is not found,
// it is added to the table.

static net_addr_t *NET_Zephyr_FindAddress(zaddr_t *addr)
{
    addrpair_t *new_entry;
    int empty_entry = -1;
    int i;

    if (addr_table_size < 0)
    {
        NET_Zephyr_InitAddrTable();
    }

    for (i=0; i<addr_table_size; ++i)
    {
        if (addr_table[i] != NULL
         && AddressesEqual(addr, &addr_table[i]->zephyr_addr))
        {
            return &addr_table[i]->net_addr;
        }

        if (empty_entry < 0 && addr_table[i] == NULL)
            empty_entry = i;
    }

    // Was not found in list.  We need to add it.

    // Is there any space in the table? If not, increase the table size

    if (empty_entry < 0)
    {
        addrpair_t **new_addr_table;
        int new_addr_table_size;

        // after reallocing, we will add this in as the first entry
        // in the new block of memory

        empty_entry = addr_table_size;

        // allocate a new array twice the size, init to 0 and copy
        // the existing table in.  replace the old table.

        new_addr_table_size = addr_table_size * 2;
        new_addr_table = Z_Malloc(sizeof(addrpair_t *) * new_addr_table_size,
                                  PU_STATIC, 0);
        memset(new_addr_table, 0, sizeof(addrpair_t *) * new_addr_table_size);
        memcpy(new_addr_table, addr_table,
               sizeof(addrpair_t *) * addr_table_size);
        Z_Free(addr_table);
        addr_table = new_addr_table;
        addr_table_size = new_addr_table_size;
    }

    // Add a new entry

    new_entry = Z_Malloc(sizeof(addrpair_t), PU_STATIC, 0);

    new_entry->zephyr_addr = *addr;
    new_entry->net_addr.handle = &new_entry->zephyr_addr;
    new_entry->net_addr.module = &net_zephyr_module;

    addr_table[empty_entry] = new_entry;

    return &new_entry->net_addr;
}

static void NET_Zephyr_FreeAddress(net_addr_t *addr)
{
    int i;

    for (i=0; i<addr_table_size; ++i)
    {
        if (addr == &addr_table[i]->net_addr)
        {
            Z_Free(addr_table[i]);
            addr_table[i] = NULL;
            return;
        }
    }

    I_Error("NET_Zephyr_FreeAddress: Attempted to remove an unused address!");
}

// Opens a non-blocking UDP socket bound to the given local port (0 selects
// an ephemeral port, as a client does).

static boolean NET_Zephyr_OpenSocket(int bind_port)
{
    struct sockaddr_in bind_addr;
    int broadcast = 1;

    udpsocket = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (udpsocket < 0)
    {
        I_Error("NET_Zephyr_OpenSocket: Unable to open a socket! (errno %d)",
                errno);
        return false;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(bind_port);

    if (zsock_bind(udpsocket, (struct sockaddr *) &bind_addr,
                   sizeof(bind_addr)) < 0)
    {
        I_Error("NET_Zephyr_OpenSocket: Unable to bind to port %d (errno %d)",
                bind_port, errno);
        return false;
    }

    // Allow sending to the broadcast address (used for LAN server queries).
    zsock_setsockopt(udpsocket, SOL_SOCKET, SO_BROADCAST,
                     &broadcast, sizeof(broadcast));

    return true;
}

static boolean NET_Zephyr_InitClient(void)
{
    int p;

    if (initted)
        return true;

    //!
    // @category net
    // @arg <n>
    //
    // Use the specified UDP port for communications, instead of
    // the default (2342).
    //

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
        port = atoi(myargv[p+1]);

    if (!NET_Zephyr_OpenSocket(0))
        return false;

    initted = true;

    return true;
}

static boolean NET_Zephyr_InitServer(void)
{
    int p;

    if (initted)
        return true;

    p = M_CheckParmWithArgs("-port", 1);
    if (p > 0)
        port = atoi(myargv[p+1]);

    if (!NET_Zephyr_OpenSocket(port))
        return false;

    initted = true;

    return true;
}

static void NET_Zephyr_SendPacket(net_addr_t *addr, net_packet_t *packet)
{
    struct sockaddr_in dest;
    zaddr_t *zaddr;
    ssize_t sent;

    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;

    if (addr == &net_broadcast_addr)
    {
        dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        dest.sin_port = htons(port);
    }
    else
    {
        zaddr = (zaddr_t *) addr->handle;
        dest.sin_addr.s_addr = zaddr->host;
        dest.sin_port = zaddr->port;
    }

    sent = zsock_sendto(udpsocket, packet->data, packet->len, 0,
                        (struct sockaddr *) &dest, sizeof(dest));

    if (sent < 0)
    {
        // Drop the packet; don't crash. UDP is lossy and Doom's netcode
        // retransmits, so a transient send failure must not be fatal.
        static int64_t last_log;
        int64_t now = k_uptime_get();

        if (now - last_log > 1000)
        {
            printf("NET_Zephyr_SendPacket: dropped packet, sendto errno %d\n",
                   errno);
            last_log = now;
        }
    }
}

static boolean NET_Zephyr_RecvPacket(net_addr_t **addr, net_packet_t **packet)
{
    static byte buf[1500];
    struct sockaddr_in src;
    socklen_t src_len = sizeof(src);
    zaddr_t zaddr;
    ssize_t result;

    // Non-blocking receive: MSG_DONTWAIT keeps the lockstep loop from
    // ever stalling waiting on the network, matching the zero-timeout
    // poll that the upstream SDL_net module relies on.

    result = zsock_recvfrom(udpsocket, buf, sizeof(buf), ZSOCK_MSG_DONTWAIT,
                            (struct sockaddr *) &src, &src_len);

    if (result < 0)
    {
        // No packet this poll. Any error (not just EAGAIN) is non-fatal, so a
        // transient link hiccup can't kill the game.
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            static int64_t last_log;
            int64_t now = k_uptime_get();

            if (now - last_log > 1000)
            {
                printf("NET_Zephyr_RecvPacket: recvfrom errno %d\n", errno);
                last_log = now;
            }
        }

        return false;
    }

    // Put the data into a new packet structure

    *packet = NET_NewPacket(result);
    memcpy((*packet)->data, buf, result);
    (*packet)->len = result;

    // Address

    zaddr.host = src.sin_addr.s_addr;
    zaddr.port = src.sin_port;
    *addr = NET_Zephyr_FindAddress(&zaddr);

    return true;
}

void NET_Zephyr_AddrToString(net_addr_t *addr, char *buffer, int buffer_len)
{
    zaddr_t *zaddr;
    uint32_t host;
    uint16_t addr_port;

    zaddr = (zaddr_t *) addr->handle;
    host = ntohl(zaddr->host);
    addr_port = ntohs(zaddr->port);

    M_snprintf(buffer, buffer_len, "%i.%i.%i.%i",
               (host >> 24) & 0xff, (host >> 16) & 0xff,
               (host >> 8) & 0xff, host & 0xff);

    // If we are using the default port we just need to show the IP address,
    // but otherwise we need to include the port.
    if (addr_port != DEFAULT_PORT)
    {
        char portbuf[10];
        M_snprintf(portbuf, sizeof(portbuf), ":%i", addr_port);
        M_StringConcat(buffer, portbuf, buffer_len);
    }
}

net_addr_t *NET_Zephyr_ResolveAddress(char *address)
{
    zaddr_t zaddr;
    struct in_addr ip;
    char *addr_hostname;
    int addr_port;
    char *colon;

    colon = strchr(address, ':');

    if (colon != NULL)
    {
        addr_hostname = M_StringDuplicate(address);
        addr_hostname[colon - address] = '\0';
        addr_port = atoi(colon + 1);
    }
    else
    {
        addr_hostname = address;
        addr_port = port;
    }

    if (zsock_inet_pton(AF_INET, addr_hostname, &ip) == 1)
    {
        // Numeric dotted-quad address.

        zaddr.host = ip.s_addr;
    }
    else
    {
        // Resolve a hostname via the host resolver.

        struct zsock_addrinfo hints;
        struct zsock_addrinfo *result;
        int err;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        err = zsock_getaddrinfo(addr_hostname, NULL, &hints, &result);

        if (err != 0 || result == NULL)
        {
            if (addr_hostname != address)
                free(addr_hostname);
            return NULL;
        }

        zaddr.host = ((struct sockaddr_in *) result->ai_addr)->sin_addr.s_addr;
        zsock_freeaddrinfo(result);
    }

    zaddr.port = htons(addr_port);

    if (addr_hostname != address)
        free(addr_hostname);

    return NET_Zephyr_FindAddress(&zaddr);
}

// Complete module

net_module_t net_zephyr_module =
{
    NET_Zephyr_InitClient,
    NET_Zephyr_InitServer,
    NET_Zephyr_SendPacket,
    NET_Zephyr_RecvPacket,
    NET_Zephyr_AddrToString,
    NET_Zephyr_FreeAddress,
    NET_Zephyr_ResolveAddress,
};
