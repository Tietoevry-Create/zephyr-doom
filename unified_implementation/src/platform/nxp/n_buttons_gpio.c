/*
 * Minimal FRDM button-to-key mapping using Zephyr GPIO aliases sw0/sw1.
 */

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "d_event.h"
#include "doomkeys.h"

#define SW2_NODE DT_ALIAS(sw0)
#define SW3_NODE DT_ALIAS(sw1)

static const struct gpio_dt_spec sw2_gpio =
    GPIO_DT_SPEC_GET_OR(SW2_NODE, gpios, {0});
static const struct gpio_dt_spec sw3_gpio =
    GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0});

enum { BUTTON_DEBOUNCE_MS = 30, BUTTON_LONGPRESS_MS = 600 };

typedef struct {
    const struct gpio_dt_spec* gpio;
    bool ready;

    bool raw;
    bool debounced;
    uint32_t raw_changed_ms;

    uint32_t pressed_ms;
    bool long_sent;

    bool up_down_sent;
    bool up_suppressed;
} button_state_t;

static button_state_t sw2;
static button_state_t sw3;

static void post_key_event(evtype_t type, int key) {
    event_t ev;
    ev.type = type;
    ev.data1 = key;
    ev.data2 = 0;
    ev.data3 = 0;
    D_PostEvent(&ev);
}

static void pulse_key(int key) {
    post_key_event(ev_keydown, key);
    post_key_event(ev_keyup, key);
}

static void init_button(button_state_t* b, const struct gpio_dt_spec* gpio) {
    b->gpio = gpio;
    b->ready = gpio_is_ready_dt(gpio);
    b->raw = false;
    b->debounced = false;
    b->raw_changed_ms = k_uptime_get_32();
    b->pressed_ms = 0;
    b->long_sent = false;
    b->up_down_sent = false;
    b->up_suppressed = false;

    if (!b->ready) {
        return;
    }

    (void)gpio_pin_configure_dt(gpio, GPIO_INPUT);
    int v = gpio_pin_get_dt(gpio);
    b->raw = (v > 0);
    b->debounced = b->raw;
}

static void update_debounced(button_state_t* b, uint32_t now_ms) {
    if (!b->ready) {
        return;
    }

    int v = gpio_pin_get_dt(b->gpio);
    bool new_raw = (v > 0);

    if (new_raw != b->raw) {
        b->raw = new_raw;
        b->raw_changed_ms = now_ms;
    }

    if (b->debounced != b->raw &&
        (uint32_t)(now_ms - b->raw_changed_ms) >= BUTTON_DEBOUNCE_MS) {
        b->debounced = b->raw;
    }
}

void N_ButtonsInit(void) {
    init_button(&sw2, &sw2_gpio);
    init_button(&sw3, &sw3_gpio);
}

void N_ReadButtons(void) {
    uint32_t now_ms = k_uptime_get_32();

    bool sw2_prev = sw2.debounced;
    bool sw3_prev = sw3.debounced;

    update_debounced(&sw2, now_ms);
    update_debounced(&sw3, now_ms);

    /* SW3: forward/up with long-press ESC */
    if (sw3.debounced && !sw3_prev) {
        sw3.pressed_ms = now_ms;
        sw3.long_sent = false;
        sw3.up_suppressed = false;
        sw3.up_down_sent = true;
        post_key_event(ev_keydown, KEY_UPARROW);
    }

    if (sw3.debounced && sw3.up_down_sent && !sw3.long_sent) {
        if ((uint32_t)(now_ms - sw3.pressed_ms) >= BUTTON_LONGPRESS_MS) {
            post_key_event(ev_keyup, KEY_UPARROW);
            sw3.up_suppressed = true;
            sw3.up_down_sent = false;
            pulse_key(KEY_ESCAPE);
            sw3.long_sent = true;
        }
    }

    if (!sw3.debounced && sw3_prev) {
        if (sw3.up_down_sent) {
            post_key_event(ev_keyup, KEY_UPARROW);
        }
        sw3.up_down_sent = false;
        sw3.up_suppressed = false;
    }

    /* SW2: short = ENTER+FIRE, long = USE */
    if (sw2.debounced && !sw2_prev) {
        sw2.pressed_ms = now_ms;
        sw2.long_sent = false;
    }

    if (sw2.debounced && !sw2.long_sent) {
        if ((uint32_t)(now_ms - sw2.pressed_ms) >= BUTTON_LONGPRESS_MS) {
            pulse_key(' ');
            sw2.long_sent = true;
        }
    }

    if (!sw2.debounced && sw2_prev) {
        if (!sw2.long_sent) {
            pulse_key(KEY_ENTER);
            pulse_key(KEY_RCTRL);
        }
    }
}

int N_ButtonState(int idx) {
    switch (idx) {
        case 0:
            return sw2.debounced;
        case 1:
            return sw3.debounced;
        default:
            return 0;
    }
}
