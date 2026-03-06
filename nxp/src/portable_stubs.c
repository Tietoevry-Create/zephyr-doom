/*
 * Portable stubs for Nordic-specific functions removed in the minimal NXP
 * build. Provide no-op or simplified implementations so the Doom core links.
 */
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "d_event.h"
#include "doomkeys.h"
#include "doomtype.h"

LOG_MODULE_REGISTER(portable_stubs, LOG_LEVEL_INF);

/* Button / input replacements */
// Two on-board buttons (FRDM-MCXN947: SW2/SW3) exposed via aliases sw0/sw1.
// We use them as minimal Doom controls:
// - SW3 (sw1): hold = KEY_UPARROW (move forward / menu up); long-press =
// KEY_ESCAPE (open menu / back)
// - SW2 (sw0): short-press = KEY_ENTER + KEY_RCTRL (menu select + fire);
// long-press = ' ' (use/open)

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

    // SW3: forward/up with long-press ESC
    if (sw3.debounced && !sw3_prev) {
        sw3.pressed_ms = now_ms;
        sw3.long_sent = false;
        sw3.up_suppressed = false;
        sw3.up_down_sent = true;
        post_key_event(ev_keydown, KEY_UPARROW);
    }

    if (sw3.debounced && sw3.up_down_sent && !sw3.long_sent) {
        if ((uint32_t)(now_ms - sw3.pressed_ms) >= BUTTON_LONGPRESS_MS) {
            // Long-press: open menu/back. Stop forwarding to avoid constant
            // menu scrolling.
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

    // SW2: short = ENTER+FIRE, long = USE
    if (sw2.debounced && !sw2_prev) {
        sw2.pressed_ms = now_ms;
        sw2.long_sent = false;
    }

    if (sw2.debounced && !sw2.long_sent) {
        if ((uint32_t)(now_ms - sw2.pressed_ms) >= BUTTON_LONGPRESS_MS) {
            pulse_key(' ');  // Use/open
            sw2.long_sent = true;
        }
    }

    if (!sw2.debounced && sw2_prev) {
        if (!sw2.long_sent) {
            pulse_key(KEY_ENTER);  // Menu select
            pulse_key(KEY_RCTRL);  // Fire
        }
    }
}

/* Radio / joystick replacements */
int N_rjoy_init(void) { return 0; }
void N_rjoy_read(void) {}

/* Filesystem / QSPI / memory stubs */
void N_qspi_wait(void) {}
void N_fs_init(void) {}

/* Hardfault hooks */
void N_hardfault_init(void) {}

/* Memory related stubs */
void N_mem_init(void) {}

/* SD / File listing fallback used in main */
/* Defined in main.c, so we don't need it here */
/*
int sd_card_list_files(char const* const path, char* buf, size_t* buf_size) {
    return -ENOTSUP;
}
*/

/* Bluetooth stub */
void bluetooth_main_xbox(void) {}

/* Input stubs */
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

/* QSPI / External flash via Zephyr flash API */

/*
 * The MCXN947 FlexSPI maps the external W25Q64 flash at 0x80000000
 * (as used by the WAD hex file flashed with --change-addresses 0x80000000).
 * The Zephyr flash API uses offsets from the start of the flash device
 * (offset 0 = byte 0 of flash), while N_qspi_data_pointer returns an
 * absolute XIP address for direct memory-mapped reads.
 */
#define EXT_FLASH_BASE 0x80000000
#define N_QSPI_BLOCK_SIZE (64 * 1024)
#define FLASH_SECTOR_SIZE 4096  /* W25Q64 uses 4 KiB sectors */

static const struct device *flash_dev;
static size_t qspi_next_loc;

static void ensure_flash_dev(void) {
    if (flash_dev == NULL) {
        flash_dev = DEVICE_DT_GET(DT_NODELABEL(w25q64jvssiq));
        if (!device_is_ready(flash_dev)) {
            LOG_ERR("Flash device not ready");
            flash_dev = NULL;
        } else {
            LOG_INF("Flash device ready: %s", flash_dev->name);
        }
    }
}

void* N_malloc(size_t size) { return malloc(size); }

void N_qspi_reserve_blocks(size_t block_count) {
    qspi_next_loc += block_count * N_QSPI_BLOCK_SIZE;
}

size_t N_qspi_alloc_block(void) {
    size_t loc = qspi_next_loc;
    qspi_next_loc += N_QSPI_BLOCK_SIZE;
    return loc;
}

void* N_qspi_data_pointer(size_t loc) { return (void*)(EXT_FLASH_BASE + loc); }

void N_qspi_erase_block(size_t loc) {
    ensure_flash_dev();
    if (flash_dev == NULL) {
        return;
    }
    /* Erase a 64 KiB logical block using multiple 4 KiB sector erases */
    int rc = flash_erase(flash_dev, loc, N_QSPI_BLOCK_SIZE);
    if (rc != 0) {
        LOG_ERR("Flash erase failed at offset 0x%x: %d", (unsigned)loc, rc);
    } else {
        LOG_INF("Flash erased 64KB at offset 0x%x", (unsigned)loc);
    }
}

void N_qspi_write(size_t loc, void* buffer, size_t size) {
    ensure_flash_dev();
    if (flash_dev == NULL) {
        return;
    }
    int rc = flash_write(flash_dev, loc, buffer, size);
    if (rc != 0) {
        LOG_ERR("Flash write failed at offset 0x%x, size %u: %d",
                (unsigned)loc, (unsigned)size, rc);
    } else {
        LOG_INF("Flash wrote %u bytes at offset 0x%x",
                (unsigned)size, (unsigned)loc);
    }
}

void N_qspi_write_block(size_t loc, void* buffer, size_t size) {
    /* Erase then write — mirrors the NRF5340 implementation */
    N_qspi_erase_block(loc);
    N_qspi_write(loc, buffer, size);
}

void N_qspi_read(size_t loc, void* buffer, size_t size) {
    ensure_flash_dev();
    if (flash_dev == NULL) {
        return;
    }
    int rc = flash_read(flash_dev, loc, buffer, size);
    if (rc != 0) {
        LOG_ERR("Flash read failed at offset 0x%x, size %u: %d",
                (unsigned)loc, (unsigned)size, rc);
    }
}
