/* Native (native_sim) input backend.
 * Host keyboard -> gpio-emul-sdl -> emulated GPIO pins -> Doom events. */

#include <stdbool.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "d_event.h"
#include "doomkeys.h"

struct native_key {
    struct gpio_dt_spec gpio;
    int doom_code;
    bool was_down;
};

static struct native_key keys[] = {
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_up),     gpios, {0}), KEY_UPARROW,    false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_down),   gpios, {0}), KEY_DOWNARROW,  false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_left),   gpios, {0}), KEY_LEFTARROW,  false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_right),  gpios, {0}), KEY_RIGHTARROW, false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_fire),   gpios, {0}), 'h',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_use),    gpios, {0}), 'j',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_speed),  gpios, {0}), 'k',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_strafe), gpios, {0}), KEY_RALT,       false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_menu),   gpios, {0}), KEY_ESCAPE,     false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_enter),  gpios, {0}), KEY_ENTER,      false },
    /* Host Tab -> automap toggle (game binds key_map_toggle = 'm'). */
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_automap), gpios, {0}), 'm',           false },
    /* Host 1-7 -> weapon slots (game binds key_weapon1..7 = '1'..'7'). */
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_weap1),  gpios, {0}), '1',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_weap2),  gpios, {0}), '2',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_weap3),  gpios, {0}), '3',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_weap4),  gpios, {0}), '4',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_weap5),  gpios, {0}), '5',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_weap6),  gpios, {0}), '6',            false },
    { GPIO_DT_SPEC_GET_OR(DT_ALIAS(doom_weap7),  gpios, {0}), '7',            false },
};

#define NUM_KEYS (sizeof(keys) / sizeof(keys[0]))

static void post_event(evtype_t type, int code) {
    event_t ev;
    ev.type  = type;
    ev.data1 = (short)code;
    ev.data2 = 0;
    ev.data3 = 0;
    D_PostEvent(&ev);
}

void N_ButtonsInit(void) {
    unsigned i;

    for (i = 0; i < NUM_KEYS; i++) {
        if (keys[i].gpio.port != NULL && gpio_is_ready_dt(&keys[i].gpio)) {
            gpio_pin_configure_dt(&keys[i].gpio, GPIO_INPUT);
        }
        keys[i].was_down = false;
    }
}

void N_ReadButtons(void) {
    unsigned i;

    for (i = 0; i < NUM_KEYS; i++) {
        if (keys[i].gpio.port == NULL) {
            continue;
        }
        {
            bool down = gpio_pin_get_dt(&keys[i].gpio) > 0;

            if (down && !keys[i].was_down) {
                post_event(ev_keydown, keys[i].doom_code);
                keys[i].was_down = true;
            } else if (!down && keys[i].was_down) {
                post_event(ev_keyup, keys[i].doom_code);
                keys[i].was_down = false;
            }
        }
    }
}

int N_ButtonState(int idx) {
    if (idx < 0 || (unsigned)idx >= NUM_KEYS) {
        return 0;
    }
    return keys[idx].was_down ? 1 : 0;
}
