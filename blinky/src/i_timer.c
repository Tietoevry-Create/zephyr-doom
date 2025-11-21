//
// Copyright(C) 1993-1996 Id Software, Inc.
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
//      Timer functions.
//

#include "i_timer.h"

#undef PACKED_STRUCT
#include <zephyr/kernel.h>

//
// I_GetTime
// returns time in 1/35th second tics
//

// static Uint32 basetime = 0;

// NRFD-TODO: Handle overflow of timer

int I_GetTime(void) {
    /* Return tics (35 Hz) since boot */
    uint32_t ms = k_uptime_get_32();
    return (int)((ms * TICRATE) / 1000u);
}

//
// Same as I_GetTime, but returns time in milliseconds
//

int I_GetTimeMS(void) { return (int)k_uptime_get_32(); }

uint32_t I_RawTimeToFps(uint32_t time_delta) {
    /* Convert a delta in milliseconds to FPS */
    if (time_delta == 0u) {
        return 0u;
    }
    return 1000u / time_delta;
}

uint32_t I_GetTimeRaw(void) {
    /* Raw time in milliseconds */
    return k_uptime_get_32();
}

// Sleep for a specified number of ms

void I_Sleep(int ms) { k_msleep(ms); }

void I_SleepUS(int us) { k_usleep(us); }

void I_WaitVBL(int count) { I_Sleep((count * 1000) / 70); }

void I_InitTimer(void) { /* No hardware init needed; Zephyr provides uptime */ }
