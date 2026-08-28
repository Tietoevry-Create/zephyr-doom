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
//     Headless replacement for the relevant part of net_gui.c.
//
//     Upstream Chocolate Doom shows a textscreen (libtextscreen) lobby in
//     NET_WaitForLaunch while waiting for the server to start the game. This
//     fork does not build libtextscreen, so we provide a minimal headless
//     equivalent: poll the network until the server launches the game.
//

#include <stdio.h>
#include <stdlib.h>

#include "doomtype.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_argv.h"
#include "net_client.h"
#include "net_server.h"
#include "net_defs.h"

void NET_WaitForLaunch(void)
{
    boolean launch_requested = false;
    int expected_nodes;
    int p;

    //!
    // @arg <n>
    // @category net
    //
    // As the controlling player, automatically start the game once n nodes
    // (players + drones) have joined. This mirrors the upstream lobby's
    // CheckAutoLaunch; without an interactive lobby it is how a headless
    // controller decides to start. A non-controller client ignores this and
    // simply waits for the controller (e.g. a desktop chocolate-doom player).
    //
    expected_nodes = 0;
    p = M_CheckParmWithArgs("-nodes", 1);
    if (p > 0)
    {
        expected_nodes = atoi(myargv[p + 1]);
    }

    printf("NET_WaitForLaunch: connected, waiting for the game to start...\n");

    while (net_waiting_for_launch)
    {
        NET_CL_Run();
        NET_SV_Run();

        if (!net_client_connected)
        {
            I_Error("Lost connection to server");
        }

        if (!launch_requested
         && net_client_received_wait_data
         && net_client_wait_data.is_controller
         && expected_nodes > 0
         && (net_client_wait_data.num_players
             + net_client_wait_data.num_drones) >= expected_nodes)
        {
            printf("NET_WaitForLaunch: %d node(s) present, launching game.\n",
                   expected_nodes);
            NET_CL_LaunchGame();
            launch_requested = true;
        }

        I_Sleep(100);
    }

    printf("NET_WaitForLaunch: game starting.\n");
}
