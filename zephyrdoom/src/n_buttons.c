/*
 * Copyright (c) 2019 - 2020, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Unified button backend:
 * - nRF5340DK: 4 buttons, simple edge->keydown/keyup mapping.
 * - FRDM-MCXN947: 2 buttons, debounced with long-press gestures.
 *
 * GPIOs are selected via Zephyr devicetree aliases sw0..sw3.
 */

#if !defined(CONFIG_FEATURE_DOOM_BUTTONS)

void N_ButtonsInit(void) {}
void N_ReadButtons(void) {}
int N_ButtonState(int idx) {
    (void)idx;
    return 0;
}

#else

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "d_event.h"
#include "doomkeys.h"

/* Common helpers */

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

/* Button aliases (available on both boards; missing ones become !ready) */

#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)

static const struct gpio_dt_spec sw0_gpio =
    GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios, {0});
static const struct gpio_dt_spec sw1_gpio =
    GPIO_DT_SPEC_GET_OR(SW1_NODE, gpios, {0});
static const struct gpio_dt_spec sw2_gpio =
    GPIO_DT_SPEC_GET_OR(SW2_NODE, gpios, {0});
static const struct gpio_dt_spec sw3_gpio =
    GPIO_DT_SPEC_GET_OR(SW3_NODE, gpios, {0});

static bool gpio_pressed(const struct gpio_dt_spec* gpio) {
    if (!gpio_is_ready_dt(gpio)) {
        return false;
    }

    int v = gpio_pin_get_dt(gpio);
    return v > 0;
}

#if defined(CONFIG_BOARD_FRDM_MCXN947) || \
    defined(CONFIG_BOARD_FRDM_MCXN947_MCXN947_CPU0)

/*
 * FRDM: 2-button UI with debounce and long-press actions.
 *  - sw1 alias (physical "User SW3"): up/forward, long-press => ESC
 *  - sw0 alias (physical "User SW2"): short => ENTER + FIRE, long => USE
 *
 * Note: sw0/sw1 are Zephyr devicetree aliases (sw0=user_button_2 "User SW2",
 * sw1=user_button_3 "User SW3"), not the board silkscreen. Physical SW1 on the
 * FRDM-MCXN947 is the RESET button and is not used here.
 */

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
} button_state_t;

static button_state_t b0;
static button_state_t b1;

static void init_button(button_state_t* b, const struct gpio_dt_spec* gpio) {
    b->gpio = gpio;
    b->ready = gpio_is_ready_dt(gpio);
    b->raw = false;
    b->debounced = false;
    b->raw_changed_ms = k_uptime_get_32();
    b->pressed_ms = 0;
    b->long_sent = false;
    b->up_down_sent = false;

    if (!b->ready) {
        return;
    }

    (void)gpio_pin_configure_dt(gpio, GPIO_INPUT);
    b->raw = gpio_pressed(gpio);
    b->debounced = b->raw;
}

static void update_debounced(button_state_t* b, uint32_t now_ms) {
    if (!b->ready) {
        return;
    }

    bool new_raw = gpio_pressed(b->gpio);
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
    init_button(&b0, &sw0_gpio);
    init_button(&b1, &sw1_gpio);
}

void N_ReadButtons(void) {
    uint32_t now_ms = k_uptime_get_32();

    bool b0_prev = b0.debounced;
    bool b1_prev = b1.debounced;

    update_debounced(&b0, now_ms);
    update_debounced(&b1, now_ms);

    /* sw1 (User SW3): up/forward with long-press ESC */
    if (b1.debounced && !b1_prev) {
        b1.pressed_ms = now_ms;
        b1.long_sent = false;
        b1.up_down_sent = true;
        post_key_event(ev_keydown, KEY_UPARROW);
    }

    if (b1.debounced && b1.up_down_sent && !b1.long_sent) {
        if ((uint32_t)(now_ms - b1.pressed_ms) >= BUTTON_LONGPRESS_MS) {
            post_key_event(ev_keyup, KEY_UPARROW);
            b1.up_down_sent = false;
            pulse_key(KEY_ESCAPE);
            b1.long_sent = true;
        }
    }

    if (!b1.debounced && b1_prev) {
        if (b1.up_down_sent) {
            post_key_event(ev_keyup, KEY_UPARROW);
        }
        b1.up_down_sent = false;
    }

    /* sw0 (User SW2): short = ENTER + FIRE, long = USE */
    if (b0.debounced && !b0_prev) {
        b0.pressed_ms = now_ms;
        b0.long_sent = false;
    }

    if (b0.debounced && !b0.long_sent) {
        if ((uint32_t)(now_ms - b0.pressed_ms) >= BUTTON_LONGPRESS_MS) {
            pulse_key(' ');
            b0.long_sent = true;
        }
    }

    if (!b0.debounced && b0_prev) {
        if (!b0.long_sent) {
            pulse_key(KEY_ENTER);
            pulse_key(KEY_RCTRL);
        }
    }
}

int N_ButtonState(int idx) {
    switch (idx) {
        case 0:
            return b0.debounced;
        case 1:
            return b1.debounced;
        default:
            return 0;
    }
}

#else

/*
 * nRF5340DK: 4-button mapping, edge->keydown/keyup.
 * Uses the board's default aliases sw0..sw3.
 */

static bool button_prev_state[4];

static const int button_map[4] = {KEY_UPARROW, KEY_DOWNARROW, KEY_ENTER,
                                  KEY_ESCAPE};

static const struct gpio_dt_spec* const button_gpios[4] = {
    &sw0_gpio,
    &sw1_gpio,
    &sw2_gpio,
    &sw3_gpio,
};

void N_ButtonsInit(void) {
    for (int i = 0; i < 4; ++i) {
        if (gpio_is_ready_dt(button_gpios[i])) {
            (void)gpio_pin_configure_dt(button_gpios[i], GPIO_INPUT);
        }
        button_prev_state[i] = gpio_pressed(button_gpios[i]);
    }
}

void N_ReadButtons(void) {
    for (int i = 0; i < 4; ++i) {
        bool pressed = gpio_pressed(button_gpios[i]);

        if (pressed && !button_prev_state[i]) {
            post_key_event(ev_keydown, button_map[i]);
        } else if (!pressed && button_prev_state[i]) {
            post_key_event(ev_keyup, button_map[i]);
        }

        button_prev_state[i] = pressed;
    }
}

int N_ButtonState(int num) {
    if (num < 0 || num >= 4) {
        return 0;
    }
    return button_prev_state[num];
}

#endif

#endif /* CONFIG_FEATURE_DOOM_BUTTONS */
