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
//     Network client stubs for builds without networking.
//
//     Origin: this is the nrf-doom fork's cut-down net_client.c, which had the
//     whole Chocolate Doom client commented out to save flash on the MCU. When
//     CONFIG_FEATURE_DOOM_NET was added, the real client came back as
//     net_client.c and this copy was kept under the name net_client_stub.c so
//     that a CONFIG_FEATURE_DOOM_NET=n build still links: d_loop.c and d_net.c
//     reference these symbols unconditionally.
//
//     CMakeLists.txt compiles exactly one of the two. Every function here is a
//     no-op or returns a "not connected" answer, so the engine runs
//     single-player.
//

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doom_config.h"
#include "doomtype.h"
#include "deh_main.h"
#include "deh_str.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "m_config.h"
#include "m_misc.h"
#include "net_client.h"
#include "net_common.h"
#include "net_defs.h"
#include "net_gui.h"
#include "net_io.h"
#include "net_packet.h"
#include "net_server.h"
#include "net_structrw.h"
#include "w_checksum.h"
#include "w_wad.h"

extern void D_ReceiveTic(ticcmd_t *ticcmds, boolean *playeringame);

typedef enum
{
    // waiting for the game to launch

    CLIENT_STATE_WAITING_LAUNCH,

    // waiting for the game to start

    CLIENT_STATE_WAITING_START,

    // in game

    CLIENT_STATE_IN_GAME,
} net_clientstate_t;

// Type of structure used in the receive window

typedef struct
{
    // Whether this tic has been received yet

    boolean active;

    // Last time we sent a resend request for this tic

    unsigned int resend_time;

    // Tic data from server

    net_full_ticcmd_t cmd;
} net_server_recv_t;

// Type of structure used in the send window

typedef struct
{
    // Whether this slot is active yet

    boolean active;

    // The tic number

    unsigned int seq;

    // Time the command was generated

    unsigned int time;

    // Ticcmd diff

    net_ticdiff_t cmd;
} net_server_send_t;

extern fixed_t offsetms;

static net_connection_t client_connection;
static net_clientstate_t client_state;
static net_addr_t *server_addr;
static net_context_t *client_context;

// game settings, as received from the server when the game started

static net_gamesettings_t settings;

// true if the client code is in use

boolean net_client_connected;

// true if we have received waiting data from the server,
// and the wait data that was received.

boolean net_client_received_wait_data;
// Defined even in the stub so d_loop.c (which references it) links in
// single-player / non-networked builds.
net_waitdata_t net_client_wait_data;

// Waiting at the initial wait screen for the game to be launched?

boolean net_waiting_for_launch = false;

// Name that we send to the server

char *net_player_name = NULL;

// Connected but not participating in the game (observer)

boolean drone = false;

// The last ticcmd constructed

static ticcmd_t last_ticcmd;

// Buffer of ticcmd diffs being sent to the server

// Are we playing with the freedoom IWAD?

unsigned int net_local_is_freedoom;

// Average time between sending our ticcmd and receiving from the server

static fixed_t average_latency;

#define NET_CL_ExpandTicNum(b) NET_ExpandTicNum(recvwindow_start, (b))

// Called when we become disconnected from the server

static void NET_CL_Disconnected(void)
{
    D_ReceiveTic(NULL, NULL);
}

// Expand a net_full_ticcmd_t, applying the diffs in cmd->cmds as
// patches against recvwindow_cmd_base.  Place the results into
// the d_net.c structures (netcmds/nettics) and save the new ticcmd
// back into recvwindow_cmd_base.

static void NET_CL_ExpandFullTiccmd(net_full_ticcmd_t *cmd, unsigned int seq,
                                    ticcmd_t *ticcmds)
{
}

// Advance the receive window

static void NET_CL_AdvanceWindow(void)
{
}

// Shut down the client code, etc.  Invoked after a disconnect.

static void NET_CL_Shutdown(void)
{
}

void NET_CL_LaunchGame(void)
{
}

void NET_CL_StartGame(net_gamesettings_t *settings)
{
}

static void NET_CL_SendGameDataACK(void)
{
}

static void NET_CL_SendTics(int start, int end)
{
}

// Add a new ticcmd to the send queue

void NET_CL_SendTiccmd(ticcmd_t *ticcmd, int maketic)
{
}

// Parse a SYN packet received back from the server indicating a successful
// connection attempt.
static void NET_CL_ParseSYN(net_packet_t *packet)
{
}

// data received while we are waiting for the game to start

static void NET_CL_ParseWaitingData(net_packet_t *packet)
{
}

static void NET_CL_ParseLaunch(net_packet_t *packet)
{
}

static void NET_CL_ParseGameStart(net_packet_t *packet)
{
}

static void NET_CL_SendResendRequest(int start, int end)
{
}

// Check for expired resend requests

static void NET_CL_CheckResends(void)
{
}

// Parsing of NET_PACKET_TYPE_GAMEDATA packets
// (packets containing the actual ticcmd data)

static void NET_CL_ParseGameData(net_packet_t *packet)
{
}

// Parse a resend request from the server due to a dropped packet

static void NET_CL_ParseResendRequest(net_packet_t *packet)
{
}

// Console message that the server wants the client to print

static void NET_CL_ParseConsoleMessage(net_packet_t *packet)
{
}

// parse a received packet

static void NET_CL_ParsePacket(net_packet_t *packet)
{
}

// "Run" the client code: check for new packets, send packets as
// needed

void NET_CL_Run(void)
{
}

static void NET_CL_SendSYN(net_connect_data_t *data)
{
}

// Connect to a server
boolean NET_CL_Connect(net_addr_t *addr, net_connect_data_t *data)
{
    return false;
}

// read game settings received from server

boolean NET_CL_GetSettings(net_gamesettings_t *_settings)
{
    if (client_state != CLIENT_STATE_IN_GAME)
    {
        return false;
    }

    memcpy(_settings, &settings, sizeof(net_gamesettings_t));

    return true;
}

// disconnect from the server

void NET_CL_Disconnect(void)
{
}

void NET_CL_Init(void)
{
    // Try to set from the USER and USERNAME environment variables
    // Otherwise, fallback to "Player"


    if (net_player_name == NULL)
        net_player_name = "Player";
}

void NET_Init(void)
{
    NET_CL_Init();
}

void NET_BindVariables(void)
{
    // M_BindStringVariable("player_name", &net_player_name);
}
