
#include "doom_status_led.h"

#include <stdbool.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

/*
 * Game status LED policy:
 * - Uses DT alias doom-status-led.
 * - LOADING: blink
 * - RUNNING: solid on
 * - CRASHED: off
 */

#if defined(CONFIG_FEATURE_DOOM_LEDS) && DT_HAS_ALIAS(doom_status_led)
#define DOOM_STATUS_LED_ENABLED 1
#else
#define DOOM_STATUS_LED_ENABLED 0
#endif

#if DOOM_STATUS_LED_ENABLED

static const struct gpio_dt_spec status_led =
    GPIO_DT_SPEC_GET(DT_ALIAS(doom_status_led), gpios);

static struct k_timer status_led_timer;
static bool status_led_inited;
static doom_status_led_state_t status_led_state;

static void status_led_timer_expiry(struct k_timer* timer) {
    ARG_UNUSED(timer);
    if (!status_led_inited) {
        return;
    }
    (void)gpio_pin_toggle_dt(&status_led);
}

static void status_led_apply_state(doom_status_led_state_t state) {
    status_led_state = state;

    switch (state) {
        case DOOM_STATUS_LED_LOADING:
            /* Fast blink: visible but not too distracting. */
            k_timer_start(&status_led_timer, K_NO_WAIT, K_MSEC(200));
            break;
        case DOOM_STATUS_LED_RUNNING:
            k_timer_stop(&status_led_timer);
            (void)gpio_pin_set_dt(&status_led, 1);
            break;
        case DOOM_STATUS_LED_CRASHED:
        default:
            k_timer_stop(&status_led_timer);
            (void)gpio_pin_set_dt(&status_led, 0);
            break;
    }
}

void doom_status_led_init(void) {
    if (status_led_inited) {
        return;
    }

    if (!device_is_ready(status_led.port)) {
        return;
    }

    if (gpio_pin_configure_dt(&status_led, GPIO_OUTPUT_INACTIVE) != 0) {
        return;
    }

    k_timer_init(&status_led_timer, status_led_timer_expiry, NULL);
    status_led_inited = true;
    status_led_apply_state(DOOM_STATUS_LED_LOADING);
}

void doom_status_led_set(doom_status_led_state_t state) {
    if (!status_led_inited) {
        doom_status_led_init();
    }
    if (!status_led_inited) {
        return;
    }

    if (state == status_led_state) {
        return;
    }

    status_led_apply_state(state);
}

#else

void doom_status_led_init(void) {}
void doom_status_led_set(doom_status_led_state_t state) { (void)state; }

#endif
