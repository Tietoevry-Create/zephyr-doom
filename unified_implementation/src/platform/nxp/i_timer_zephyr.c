//
// Timer backend for Zephyr uptime (used on FRDM-MCXN947).
//

#include "i_timer.h"

#undef PACKED_STRUCT
#include <zephyr/kernel.h>

int I_GetTime(void) {
    /* Return tics (35 Hz) since boot */
    uint32_t ms = k_uptime_get_32();
    return (int)((ms * TICRATE) / 1000u);
}

int I_GetTimeMS(void) { return (int)k_uptime_get_32(); }

uint32_t I_RawTimeToFps(uint32_t time_delta) {
    if (time_delta == 0u) {
        return 0u;
    }
    return 1000u / time_delta;
}

uint32_t I_GetTimeRaw(void) { return k_uptime_get_32(); }

void I_Sleep(int ms) { k_msleep(ms); }

void I_SleepUS(int us) { k_usleep(us); }

void I_WaitVBL(int count) { I_Sleep((count * 1000) / 70); }

void I_InitTimer(void) {}
