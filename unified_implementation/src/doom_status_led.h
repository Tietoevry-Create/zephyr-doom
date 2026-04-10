#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DOOM_STATUS_LED_LOADING = 0,
    DOOM_STATUS_LED_RUNNING = 1,
    DOOM_STATUS_LED_CRASHED = 2,
} doom_status_led_state_t;

void doom_status_led_init(void);
void doom_status_led_set(doom_status_led_state_t state);

#ifdef __cplusplus
}
#endif
