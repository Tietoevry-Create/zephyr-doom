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

#include "board_config.h"

//
// I_GetTime
// returns time in 1/35th second tics
//

// static Uint32 basetime = 0;

// NRFD-TODO: Handle overflow of timer

int I_GetTime(void) {
#ifdef NRF_DOOM_TIMER
    NRF_DOOM_TIMER->TASKS_CAPTURE[0] = 1;
    uint64_t cc = NRF_DOOM_TIMER->CC[0];
    uint64_t tickTime = (cc * TICRATE) * 10 / 312 / 1000;
    return tickTime;
#else
    /* Return tics (35 Hz) since boot */
    uint32_t ms = k_uptime_get_32();
    return (int)((ms * TICRATE) / 1000u);
#endif
}

//
// Same as I_GetTime, but returns time in milliseconds
//

int I_GetTimeMS(void) {
#ifdef NRF_DOOM_TIMER
    NRF_DOOM_TIMER->TASKS_CAPTURE[0] = 1;
    uint64_t cc = NRF_DOOM_TIMER->CC[0];
    cc = cc * 10 / 312;
    return cc;
#else
    return (int)k_uptime_get_32();
#endif
}

uint32_t I_RawTimeToFps(uint32_t time_delta) {
#ifdef NRF_DOOM_TIMER
    if (time_delta == 0u) {
        return 0u;
    }
    return 31200u / time_delta;
#else
    if (time_delta == 0u) {
        return 0u;
    }
    return 1000u / time_delta;
#endif
}

uint32_t I_GetTimeRaw(void) {
#ifdef NRF_DOOM_TIMER
    NRF_DOOM_TIMER->TASKS_CAPTURE[0] = 1;
    uint32_t cc = NRF_DOOM_TIMER->CC[0];
    return cc;
#else
    return k_uptime_get_32();
#endif
}

// Sleep for a specified number of ms

void I_Sleep(int ms) { k_msleep(ms); }

void I_SleepUS(int us) { k_usleep(us); }

void I_WaitVBL(int count) { I_Sleep((count * 1000) / 70); }

void I_InitTimer(void) {
#ifdef NRF_DOOM_TIMER
    // initialize timer
    NRF_DOOM_TIMER->MODE = TIMER_MODE_MODE_Timer;
    NRF_DOOM_TIMER->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    NRF_DOOM_TIMER->PRESCALER = 9;
    // fTIMER = 16 MHz / (2*PRESCALER)
    // 2**9 = 512
    // fTIMER = 31.25Khz;
    // NOTE: If timer is changed, update HU_Ticker (or make global variable)
    NRF_DOOM_TIMER->TASKS_START = 1;
#else
    /* No explicit init needed for Zephyr uptime */
#endif
}
