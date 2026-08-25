/*
 * Wired joystick shield backend for FRDM-MCXN947.
 */

#if defined(CONFIG_FEATURE_DOOM_SHIELD) && (CONFIG_FEATURE_DOOM_SHIELD > 0) && \
    defined(CONFIG_ADC) && (CONFIG_ADC > 0) && defined(CONFIG_GPIO) &&         \
    (CONFIG_GPIO > 0)

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

#include "d_event.h"
#include "doomkeys.h"
#include "doom/doomstat.h"
#include "m_controls.h"

/*
 * Devicetree:
 *  - ADC axes: alias 'doom-joy' must point to a node with 'io-channels'
 *    containing X (index 0) and Y (index 1).
 *  - Buttons: aliases 'doom-shield-*' are optional; missing ones are ignored.
 */

enum {
    SHIELD_AXIS_DEADZONE = 3, /* in scaled units */
    SHIELD_AXIS_SCALE = 65,   /* matches bluetooth_control.c */
    SHIELD_AXIS_X_CENTER = 1800,
    SHIELD_AXIS_Y_CENTER = 1680,
    SHIELD_AXIS_RAW_RANGE = 1800,
};

#define DOOM_JOY_NODE DT_ALIAS(doom_joy)

#define DOOM_JOY_SWAP_AXES DT_PROP_OR(DOOM_JOY_NODE, swap_axes, 0)
#define DOOM_JOY_INVERT_X DT_PROP_OR(DOOM_JOY_NODE, invert_x, 0)
#define DOOM_JOY_INVERT_Y DT_PROP_OR(DOOM_JOY_NODE, invert_y, 0)

static bool joy_adc_ready;
static struct adc_dt_spec joy_x;
static struct adc_dt_spec joy_y;

#define BTN_FIRE_NODE DT_ALIAS(doom_shield_fire)
#define BTN_USE_NODE DT_ALIAS(doom_shield_use)
#define BTN_SPEED_NODE DT_ALIAS(doom_shield_speed)
#define BTN_WEAPON_NODE DT_ALIAS(doom_shield_weapon)
#define BTN_MAP_NODE DT_ALIAS(doom_shield_mapbtn)
#define BTN_MENU_NODE DT_ALIAS(doom_shield_menu)

static const struct gpio_dt_spec btn_fire =
    GPIO_DT_SPEC_GET_OR(BTN_FIRE_NODE, gpios, {0});
static const struct gpio_dt_spec btn_use =
    GPIO_DT_SPEC_GET_OR(BTN_USE_NODE, gpios, {0});
static const struct gpio_dt_spec btn_speed =
    GPIO_DT_SPEC_GET_OR(BTN_SPEED_NODE, gpios, {0});
static const struct gpio_dt_spec btn_weapon =
    GPIO_DT_SPEC_GET_OR(BTN_WEAPON_NODE, gpios, {0});
static const struct gpio_dt_spec btn_map =
    GPIO_DT_SPEC_GET_OR(BTN_MAP_NODE, gpios, {0});
static const struct gpio_dt_spec btn_menu =
    GPIO_DT_SPEC_GET_OR(BTN_MENU_NODE, gpios, {0});

static bool prev_weapon;
static bool prev_map;
static bool prev_menu;

static int8_t prev_menu_nav_x; /* -1 left, 0 neutral, +1 right */
static int8_t prev_menu_nav_y; /* -1 up, 0 neutral, +1 down */

static event_t prev_joystick_event;

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

static bool gpio_pressed(const struct gpio_dt_spec* gpio) {
    if (!gpio_is_ready_dt(gpio)) {
        return false;
    }

    int v = gpio_pin_get_dt(gpio);
    return v > 0;
}

static int adc_read_channel_raw(const struct adc_dt_spec* spec, int16_t* out) {
    int16_t sample = 0;
    struct adc_sequence seq;

    if (spec == NULL || out == NULL) {
        return -EINVAL;
    }

    if (!device_is_ready(spec->dev)) {
        return -ENODEV;
    }

    memset(&seq, 0, sizeof(seq));
    if (adc_sequence_init_dt(spec, &seq) < 0) {
        /* Driver requires DT channel config (eg. configurable inputs). */
        return -ENOTSUP;
    }
    seq.buffer = &sample;
    seq.buffer_size = sizeof(sample);

    int err = adc_read(spec->dev, &seq);
    if (err < 0) {
        return err;
    }

    *out = sample;
    return 0;
}

static int16_t scale_axis(int16_t raw, int32_t center) {
    /* Convert raw ADC values around the observed shield neutral point. */
    int32_t delta = (int32_t)raw - center;
    int32_t scaled = (delta * SHIELD_AXIS_SCALE) / SHIELD_AXIS_RAW_RANGE;

    if (scaled > SHIELD_AXIS_SCALE) {
        scaled = SHIELD_AXIS_SCALE;
    } else if (scaled < -SHIELD_AXIS_SCALE) {
        scaled = -SHIELD_AXIS_SCALE;
    }

    if (scaled > -SHIELD_AXIS_DEADZONE && scaled < SHIELD_AXIS_DEADZONE) {
        scaled = 0;
    }

    return (int16_t)scaled;
}

static void maybe_post_joystick_event(event_t* ev) {
    if (ev->data1 != prev_joystick_event.data1 ||
        ev->data2 != prev_joystick_event.data2 ||
        ev->data3 != prev_joystick_event.data3 ||
        ev->data4 != prev_joystick_event.data4 ||
        ev->data5 != prev_joystick_event.data5) {
        D_PostEvent(ev);
        prev_joystick_event = *ev;
    }
}

int n_rjoy_backend_init(void) {
    joy_adc_ready = false;
    prev_weapon = prev_map = prev_menu = false;
    prev_menu_nav_x = 0;
    prev_menu_nav_y = 0;
    memset(&prev_joystick_event, 0, sizeof(prev_joystick_event));

    /* Configure optional buttons. */
    if (gpio_is_ready_dt(&btn_fire)) {
        (void)gpio_pin_configure_dt(&btn_fire, GPIO_INPUT);
    }
    if (gpio_is_ready_dt(&btn_use)) {
        (void)gpio_pin_configure_dt(&btn_use, GPIO_INPUT);
    }
    if (gpio_is_ready_dt(&btn_speed)) {
        (void)gpio_pin_configure_dt(&btn_speed, GPIO_INPUT);
    }
    if (gpio_is_ready_dt(&btn_weapon)) {
        (void)gpio_pin_configure_dt(&btn_weapon, GPIO_INPUT);
    }
    if (gpio_is_ready_dt(&btn_map)) {
        (void)gpio_pin_configure_dt(&btn_map, GPIO_INPUT);
    }
    if (gpio_is_ready_dt(&btn_menu)) {
        (void)gpio_pin_configure_dt(&btn_menu, GPIO_INPUT);
    }

    /* ADC axes via alias doom-joy. */
    if (!DT_NODE_EXISTS(DOOM_JOY_NODE) ||
        !DT_NODE_HAS_PROP(DOOM_JOY_NODE, io_channels)) {
        return 0;
    }

    joy_x = (struct adc_dt_spec)ADC_DT_SPEC_GET_BY_IDX(DOOM_JOY_NODE, 0);
    joy_y = (struct adc_dt_spec)ADC_DT_SPEC_GET_BY_IDX(DOOM_JOY_NODE, 1);

    if (!device_is_ready(joy_x.dev) || !device_is_ready(joy_y.dev)) {
        return 0;
    }

    /* Use DT-provided channel configuration (MCUX LPADC requires inputs). */
    if (joy_x.channel_cfg_dt_node_exists) {
        (void)adc_channel_setup(joy_x.dev, &joy_x.channel_cfg);
    }
    if (joy_y.channel_cfg_dt_node_exists) {
        (void)adc_channel_setup(joy_y.dev, &joy_y.channel_cfg);
    }

    joy_adc_ready = true;
    return 1;
}

void n_rjoy_backend_read(void) {
    /* Edge-triggered key events for weapon-switch/map/menu buttons. */
    bool weapon = gpio_pressed(&btn_weapon);
    bool map = gpio_pressed(&btn_map);
    bool menu = gpio_pressed(&btn_menu);

    /* Momentary press cycles to the next weapon. */
    if (weapon && !prev_weapon) {
        pulse_key(key_nextweapon);
    }
    prev_weapon = weapon;

    if (map && !prev_map) {
        pulse_key(key_map_toggle);
    }
    prev_map = map;

    if (menu && !prev_menu) {
        pulse_key(key_menu_activate);
    }
    prev_menu = menu;

    if (!joy_adc_ready) {
        return;
    }

    /* Joystick event: axes + selected buttons packed into data1 bitmask. */
    event_t joystick_event;
    memset(&joystick_event, 0, sizeof(joystick_event));
    joystick_event.type = ev_joystick;

    /*
     * Keep button bit indices aligned with the Xbox BLE path and the engine's
     * fixed defaults in m_controls.c:
     *  - bit 5: fire (joybfire)
     *  - bit 3: use (joybuse)
     *  - bit 2: run/speed (joybspeed)
     */
    joystick_event.data1 = ((gpio_pressed(&btn_speed) ? 1 : 0) << 2) |
                           ((gpio_pressed(&btn_use) ? 1 : 0) << 3) |
                           ((gpio_pressed(&btn_fire) ? 1 : 0) << 5);

    int16_t scaled_x = 0;
    int16_t scaled_y = 0;
    {
        int16_t raw_x = 0;
        int16_t raw_y = 0;

        bool ok_x = (adc_read_channel_raw(&joy_x, &raw_x) == 0);
        bool ok_y = (adc_read_channel_raw(&joy_y, &raw_y) == 0);

        if (!ok_x || !ok_y) {
            return;
        }

        if (ok_x) {
            scaled_x = scale_axis(raw_x, SHIELD_AXIS_X_CENTER);
        }
        if (ok_y) {
            scaled_y = scale_axis(raw_y, SHIELD_AXIS_Y_CENTER);
        }

        int16_t joy_out_x = scaled_x;
        int16_t joy_out_y = scaled_y;

        if (DOOM_JOY_SWAP_AXES) {
            int16_t tmp = joy_out_x;
            joy_out_x = joy_out_y;
            joy_out_y = tmp;
        }

        if (DOOM_JOY_INVERT_X) {
            joy_out_x = -joy_out_x;
        }
        if (DOOM_JOY_INVERT_Y) {
            joy_out_y = -joy_out_y;
        }

        joystick_event.data2 = joy_out_x;
        joystick_event.data3 = joy_out_y;
    }

    /*
     * Menu navigation: emulate the Xbox D-pad edge behavior and suppress the
     * analog axes so m_menu.c does not also repeat on the same joystick event.
     */
    if (menuactive) {
        enum { MENU_NAV_THRESHOLD = 20, MENU_NAV_RELEASE = 10 };

        const int16_t menu_axis_x = (int16_t)joystick_event.data2;
        const int16_t menu_axis_y = (int16_t)joystick_event.data3;
        int8_t nav_x = 0;
        int8_t nav_y = 0;

        if (menu_axis_x <= -MENU_NAV_THRESHOLD) {
            nav_x = -1;
        } else if (menu_axis_x >= MENU_NAV_THRESHOLD) {
            nav_x = 1;
        }
        if (menu_axis_y <= -MENU_NAV_THRESHOLD) {
            nav_y = -1;
        } else if (menu_axis_y >= MENU_NAV_THRESHOLD) {
            nav_y = 1;
        }

        if (nav_x != 0 && prev_menu_nav_x == 0) {
            pulse_key((nav_x < 0) ? key_menu_left : key_menu_right);
            prev_menu_nav_x = nav_x;
        } else if (prev_menu_nav_x != 0 &&
                   (menu_axis_x > -MENU_NAV_RELEASE &&
                    menu_axis_x < MENU_NAV_RELEASE)) {
            prev_menu_nav_x = 0;
        }

        if (nav_y != 0 && prev_menu_nav_y == 0) {
            pulse_key((nav_y < 0) ? key_menu_up : key_menu_down);
            prev_menu_nav_y = nav_y;
        } else if (prev_menu_nav_y != 0 &&
                   (menu_axis_y > -MENU_NAV_RELEASE &&
                    menu_axis_y < MENU_NAV_RELEASE)) {
            prev_menu_nav_y = 0;
        }

        joystick_event.data2 = 0;
        joystick_event.data3 = 0;
    } else {
        prev_menu_nav_x = 0;
        prev_menu_nav_y = 0;
    }

    joystick_event.data4 = 0;
    joystick_event.data5 = 0;

    maybe_post_joystick_event(&joystick_event);
}

#else

int n_rjoy_backend_init(void) { return 0; }
void n_rjoy_backend_read(void) {}

#endif
