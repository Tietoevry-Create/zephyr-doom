#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include <bluetooth/services/hogp.h>
#include <dk_buttons_and_leds.h>
#include <errno.h>
#include <stddef.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include "d_event.h"
#include "doomkeys.h"
#include "m_controls.h"

#define BLINK_INTERVAL_MS 500
#define LED1_NODE DT_ALIAS(led0)
#define LED2_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);
static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);

static void blink_timer_handler(struct k_timer *timer_id) {
    if (gpio_is_ready_dt(&led1)) {
        gpio_pin_toggle_dt(&led1);
    }
    if (gpio_is_ready_dt(&led2)) {
        gpio_pin_toggle_dt(&led2);
    }
}

K_TIMER_DEFINE(blink_timer, blink_timer_handler, NULL);

typedef enum {
    DEVICE_TYPE_NONE,
    DEVICE_TYPE_XBOX,
    DEVICE_TYPE_KEYBOARD,
} connected_device_t;

static struct bt_conn *default_conn;
static struct bt_conn *auth_conn;
static struct bt_hogp hogp;
static connected_device_t current_device_type = DEVICE_TYPE_NONE;

static void hids_on_ready(struct k_work *work);
static K_WORK_DEFINE(hids_ready_work, hids_on_ready);

static void parse_xbox_report(const uint8_t *data, size_t len) {
    static bool prev_dpad_up, prev_dpad_down, prev_button_back,
        prev_button_start;
    static event_t prev_joystick_event;

    event_t joystick_event;
    event_t keyboard_event;
    joystick_event.type = ev_joystick;
    keyboard_event.type = ev_keydown;

    bool dpad_up = data[12] == 0x01;
    bool dpad_down = data[12] == 0x05;
    bool button_A = data[13] == 0x01;
    bool button_B = data[13] == 0x02;
    bool button_X = data[13] == 0x08;
    bool button_Y = data[13] == 0x10;
    bool button_back = data[14] == 0x04;
    bool button_start = data[14] == 0x08;
    joystick_event.data1 =
        (button_A << 4) | (button_B << 5) | (button_X << 2) | (button_Y << 3);

    int32_t leftJoyX = (data[1] << 8 | data[0]);
    int32_t leftJoyY = (data[3] << 8 | data[2]);

    leftJoyX = ((leftJoyX - 32767) / (double)32767) * 65;
    leftJoyY = ((leftJoyY - 32767) / (double)32767) * 65;
    if (leftJoyX > -8 && leftJoyX < 8) {
        leftJoyX = 0;
    }
    if (leftJoyY > -8 && leftJoyY < 8) {
        leftJoyY = 0;
    }

    joystick_event.data2 = leftJoyX;
    joystick_event.data3 = leftJoyY;

    int16_t LT = data[8];
    int16_t RT = data[10];
    if (LT > 5) {
        joystick_event.data4 = -LT;
    } else if (RT > 5) {
        joystick_event.data4 = RT;
    } else {
        joystick_event.data4 = 0;
    }

    if (joystick_event.data1 != prev_joystick_event.data1 ||
        joystick_event.data2 != prev_joystick_event.data2 ||
        joystick_event.data3 != prev_joystick_event.data3 ||
        joystick_event.data4 != prev_joystick_event.data4) {
        D_PostEvent(&joystick_event);
        prev_joystick_event = joystick_event;
    }

    if (dpad_up && !prev_dpad_up) {
        keyboard_event.data1 = key_up;
        D_PostEvent(&keyboard_event);
    }
    if (dpad_down && !prev_dpad_down) {
        keyboard_event.data1 = key_down;
        D_PostEvent(&keyboard_event);
    }
    if (button_back && !prev_button_back) {
        keyboard_event.data1 = key_map_toggle;
        D_PostEvent(&keyboard_event);
    }
    if (button_start && !prev_button_start) {
        keyboard_event.data1 = key_menu_activate;
        D_PostEvent(&keyboard_event);
    }

    prev_dpad_up = dpad_up;
    prev_dpad_down = dpad_down;
    prev_button_back = button_back;
    prev_button_start = button_start;
}

static void parse_keyboard_report(const uint8_t *data, size_t len) {
    typedef struct {
        uint8_t hid;
        int doom;
        int ascii;
    } keymap_t;
    static const keymap_t key_map[] = {
        {0x04, 'a', 'a'},         {0x05, 'b', 'b'},
        {0x06, 'c', 'c'},         {0x07, 'd', 'd'},
        {0x08, 'e', 'e'},         {0x09, 'f', 'f'},
        {0x0A, 'g', 'g'},         {0x0B, 'h', 'h'},
        {0x0C, 'i', 'i'},         {0x0D, 'j', 'j'},
        {0x0E, 'k', 'k'},         {0x0F, 'l', 'l'},
        {0x10, 'm', 'm'},         {0x11, 'n', 'n'},
        {0x12, 'o', 'o'},         {0x13, 'p', 'p'},
        {0x14, 'q', 'q'},         {0x15, 'r', 'r'},
        {0x16, 's', 's'},         {0x17, 't', 't'},
        {0x18, 'u', 'u'},         {0x19, 'v', 'v'},
        {0x1A, 'w', 'w'},         {0x1B, 'x', 'x'},
        {0x1C, 'y', 'y'},         {0x1D, 'z', 'z'},
        {0x1E, '1', '1'},         {0x1F, '2', '2'},
        {0x20, '3', '3'},         {0x21, '4', '4'},
        {0x22, '5', '5'},         {0x23, '6', '6'},
        {0x24, '7', '7'},         {0x25, '8', '8'},
        {0x26, '9', '9'},         {0x27, '0', '0'},
        {0x28, KEY_ENTER, 0},     {0x29, KEY_ESCAPE, 0},
        {0x2A, KEY_BACKSPACE, 0}, {0x2B, KEY_TAB, 0},
        {0x2C, ' ', ' '},         {0x36, ',', ','},
        {0x37, '.', '.'},         {0x4F, KEY_RIGHTARROW, 0},
        {0x50, KEY_LEFTARROW, 0}, {0x51, KEY_DOWNARROW, 0},
        {0x52, KEY_UPARROW, 0},   {0x3A, KEY_F1, 0},
        {0x3B, KEY_F2, 0},        {0x3C, KEY_F3, 0},
        {0x3D, KEY_F4, 0},        {0x3E, KEY_F5, 0},
        {0x3F, KEY_F6, 0},        {0x40, KEY_F7, 0},
        {0x41, KEY_F8, 0},        {0x42, KEY_F9, 0},
        {0x43, KEY_F10, 0},       {0x44, KEY_F11, 0},
        {0x45, KEY_F12, 0},
    };
    typedef struct {
        uint8_t bitmask;
        int doom;
    } modmap_t;
    static const modmap_t mod_map[] = {
        {0x01, KEY_RCTRL}, {0x02, KEY_RSHIFT}, {0x04, KEY_RALT},
        {0x10, KEY_RCTRL}, {0x20, KEY_RSHIFT}, {0x40, KEY_RALT},
    };

    static uint8_t last_modifier_byte = 0;
    static uint8_t previously_pressed_hids[6] = {0};

    if (len < 8) return;

    uint8_t current_modifier_byte = data[0];
    int mod_map_size = sizeof(mod_map) / sizeof(mod_map[0]);

    for (int i = 0; i < mod_map_size; i++) {
        uint8_t bit = mod_map[i].bitmask;
        if ((current_modifier_byte & bit) && !(last_modifier_byte & bit)) {
            event_t e = {.type = ev_keydown, .data1 = mod_map[i].doom};
            D_PostEvent(&e);
        } else if (!(current_modifier_byte & bit) &&
                   (last_modifier_byte & bit)) {
            event_t e = {.type = ev_keyup, .data1 = mod_map[i].doom};
            D_PostEvent(&e);
        }
    }
    last_modifier_byte = current_modifier_byte;

    uint8_t current_pressed_hids[6];
    memcpy(current_pressed_hids, &data[2], sizeof(current_pressed_hids));
    int key_map_size = sizeof(key_map) / sizeof(key_map[0]);

    for (int i = 0; i < 6; i++) {
        uint8_t current_hid = current_pressed_hids[i];
        if (current_hid == 0x00 || current_hid == 0x01) continue;
        bool was_pressed_before = false;
        for (int j = 0; j < 6; j++) {
            if (previously_pressed_hids[j] == current_hid) {
                was_pressed_before = true;
                break;
            }
        }
        if (!was_pressed_before) {
            for (int k = 0; k < key_map_size; k++) {
                if (key_map[k].hid == current_hid) {
                    event_t e = {.type = ev_keydown,
                                 .data1 = key_map[k].doom,
                                 .data2 = key_map[k].ascii};
                    D_PostEvent(&e);
                    break;
                }
            }
        }
    }
    for (int i = 0; i < 6; i++) {
        uint8_t previous_hid = previously_pressed_hids[i];
        if (previous_hid == 0x00 || previous_hid == 0x01) continue;
        bool still_pressed = false;
        for (int j = 0; j < 6; j++) {
            if (current_pressed_hids[j] == previous_hid) {
                still_pressed = true;
                break;
            }
        }
        if (!still_pressed) {
            for (int k = 0; k < key_map_size; k++) {
                if (key_map[k].hid == previous_hid) {
                    event_t e = {.type = ev_keyup, .data1 = key_map[k].doom};
                    D_PostEvent(&e);
                    break;
                }
            }
        }
    }
    memcpy(previously_pressed_hids, current_pressed_hids,
           sizeof(previously_pressed_hids));
}

struct bt_gatt_dm {
    struct bt_conn *conn;
    void *context;
    struct bt_gatt_discover_params discover_params;
    struct bt_gatt_dm_attr attrs[CONFIG_BT_GATT_DM_MAX_ATTRS];
    size_t cur_attr_id;
    union {
        struct bt_uuid uuid;
        struct bt_uuid_16 u16;
        struct bt_uuid_32 u32;
        struct bt_uuid_128 u128;
    } svc_uuid;
    sys_slist_t chunk_list;
    size_t cur_chunk_len;
    const struct bt_gatt_dm_cb *cb;
    bool search_svc_by_uuid;
};

static bool is_device_xbox(struct bt_gatt_dm *dm) {
    const struct bt_uuid *xbox_uuid = BT_UUID_DECLARE_128(
        BT_UUID_128_ENCODE(BT_UUID_HIDS_REPORT_VAL, 0, 0, 0, 0));

    for (int i = 0; dm->attrs[i].handle != 0; i++) {
        if (dm->attrs[i].uuid->type == BT_UUID_TYPE_128) {
            if (!bt_uuid_cmp(dm->attrs[i].uuid, xbox_uuid)) {
                return true;
            }
        }
    }

    return false;
}

static void convert_xbox_uuids(struct bt_gatt_dm *dm) {
    for (int i = 0; dm->attrs[i].handle != 0; i++) {
        if (dm->attrs[i].uuid->type != BT_UUID_TYPE_128) {
            continue;
        }

        if (!bt_uuid_cmp(dm->attrs[i].uuid,
                         BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(
                             BT_UUID_HIDS_REPORT_MAP_VAL, 0, 0, 0, 0)))) {
            dm->attrs[i].uuid->type = BT_UUID_TYPE_16;
            BT_UUID_16(dm->attrs[i].uuid)->val = BT_UUID_HIDS_REPORT_MAP_VAL;

        } else if (!bt_uuid_cmp(
                       dm->attrs[i].uuid,
                       BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(
                           BT_UUID_HIDS_CTRL_POINT_VAL, 0, 0, 0, 0)))) {
            dm->attrs[i].uuid->type = BT_UUID_TYPE_16;
            BT_UUID_16(dm->attrs[i].uuid)->val = BT_UUID_HIDS_CTRL_POINT_VAL;

        } else if (!bt_uuid_cmp(dm->attrs[i].uuid,
                                BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(
                                    BT_UUID_HIDS_REPORT_VAL, 0, 0, 0, 0)))) {
            dm->attrs[i].uuid->type = BT_UUID_TYPE_16;
            BT_UUID_16(dm->attrs[i].uuid)->val = BT_UUID_HIDS_REPORT_VAL;

        } else if (!bt_uuid_cmp(dm->attrs[i].uuid,
                                BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(
                                    BT_UUID_HIDS_INFO_VAL, 0, 0, 0, 0)))) {
            dm->attrs[i].uuid->type = BT_UUID_TYPE_16;
            BT_UUID_16(dm->attrs[i].uuid)->val = BT_UUID_HIDS_INFO_VAL;
        }
    }
}

static uint8_t hogp_notify_cb(struct bt_hogp *hogp_ctx,
                              struct bt_hogp_rep_info *rep, uint8_t err,
                              const uint8_t *data) {
    if (err || !data) {
        return BT_GATT_ITER_STOP;
    }

    switch (current_device_type) {
        case DEVICE_TYPE_XBOX:
            parse_xbox_report(data, bt_hogp_rep_size(rep));
            break;
        case DEVICE_TYPE_KEYBOARD:
            parse_keyboard_report(data, bt_hogp_rep_size(rep));
            break;
        default:
            break;
    }

    return BT_GATT_ITER_CONTINUE;
}

static void discovery_completed_cb(struct bt_gatt_dm *dm, void *context) {
    printk("Discovery completed.\n");

    k_timer_stop(&blink_timer);
    if (is_device_xbox(dm)) {
        printk("Device identified as: XBOX Controller\n");
        current_device_type = DEVICE_TYPE_XBOX;
        convert_xbox_uuids(dm);

        gpio_pin_set_dt(&led1, 1);
        gpio_pin_set_dt(&led2, 0);
    } else {
        printk("Device identified as: Standard Keyboard\n");
        current_device_type = DEVICE_TYPE_KEYBOARD;

        gpio_pin_set_dt(&led1, 0);
        gpio_pin_set_dt(&led2, 1);
    }

    int err = bt_hogp_handles_assign(dm, &hogp);
    if (err) {
        printk("Could not init HIDS client object, error: %d\n", err);
    }

    bt_gatt_dm_data_release(dm);
}

static void discovery_service_not_found_cb(struct bt_conn *conn,
                                           void *context) {
    printk("HIDS service not found.\n");
    k_timer_start(&blink_timer, K_MSEC(BLINK_INTERVAL_MS), K_MSEC(BLINK_INTERVAL_MS));
}

static void discovery_error_cb(struct bt_conn *conn, int err, void *context) {
    printk("Discovery failed, error: %d\n", err);
    k_timer_start(&blink_timer, K_MSEC(BLINK_INTERVAL_MS), K_MSEC(BLINK_INTERVAL_MS));
}

static const struct bt_gatt_dm_cb discovery_callbacks = {
    .completed = discovery_completed_cb,
    .service_not_found = discovery_service_not_found_cb,
    .error_found = discovery_error_cb,
};

static void gatt_discover(struct bt_conn *conn) {
    int err = bt_gatt_dm_start(conn, BT_UUID_HIDS, &discovery_callbacks, NULL);
    if (err) {
        printk("Could not start discovery, error: %d\n", err);
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    if (err) {
        printk("Security failed: level %u, err %d\n", level, err);
        return;
    }
    printk("Security changed: level %u\n", level);
    gatt_discover(conn);
}

static void connected(struct bt_conn *conn, uint8_t conn_err) {
    if (conn_err) {
        printk("Connection failed (err %u)\n", conn_err);
        if (default_conn) {
            bt_conn_unref(default_conn);
            default_conn = NULL;
        }
        bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
        return;
    }
    printk("Connected.\n");

    default_conn = bt_conn_ref(conn);
    int err = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (err) {
        printk("Failed to set security: %d\n", err);
        gatt_discover(conn);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    printk("Disconnected (reason 0x%02x)\n", reason);
    if (bt_hogp_assign_check(&hogp)) {
        bt_hogp_release(&hogp);
    }
    if (default_conn == conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
        current_device_type = DEVICE_TYPE_NONE;

        gpio_pin_set_dt(&led1, 0);
        gpio_pin_set_dt(&led2, 0);
        k_timer_start(&blink_timer, K_MSEC(BLINK_INTERVAL_MS),
                      K_MSEC(BLINK_INTERVAL_MS));

        bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

static void hids_on_ready(struct k_work *work) {
    printk("HIDS is ready. Subscribing to reports...\n");
    struct bt_hogp_rep_info *rep = NULL;
    while (NULL != (rep = bt_hogp_rep_next(&hogp, rep))) {
        if (bt_hogp_rep_type(rep) == BT_HIDS_REPORT_TYPE_INPUT) {
            int err = bt_hogp_rep_subscribe(&hogp, rep, hogp_notify_cb);
            if (err) {
                printk("Subscribe error (%d) for report id %u\n", err,
                       bt_hogp_rep_id(rep));
            } else {
                printk("Subscribed to report id %u\n", bt_hogp_rep_id(rep));
            }
        }
    }
}

static void hogp_ready_cb(struct bt_hogp *hogp_ctx) {
    k_work_submit(&hids_ready_work);
}

static void hogp_prep_fail_cb(struct bt_hogp *hogp_ctx, int err) {
    printk("ERROR: HIDS client preparation failed (%d)!\n", err);
}

static void scan_init(void) {
    struct bt_scan_init_param scan_init_param = {.connect_if_match = 1};
    bt_scan_init(&scan_init_param);
    BT_SCAN_CB_INIT(scan_callbacks, NULL, NULL, NULL, NULL);
    bt_scan_cb_register(&scan_callbacks);
    bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_HIDS);
    bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
}

static void num_comp_reply(bool accept) {
    if (accept) {
        bt_conn_auth_passkey_confirm(auth_conn);
    } else {
        bt_conn_auth_cancel(auth_conn);
    }
    bt_conn_unref(auth_conn);
    auth_conn = NULL;
}

static void button_handler(uint32_t button_state, uint32_t has_changed) {
    if (auth_conn) {
        if (has_changed & button_state & DK_BTN1_MSK) num_comp_reply(true);
        if (has_changed & button_state & DK_BTN2_MSK) num_comp_reply(false);
    }
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey) {
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Passkey for %s: %06u\n", addr, passkey);
    printk("Press Button 1 to confirm, Button 2 to reject.\n");
    auth_conn = bt_conn_ref(conn);
}

static void auth_cancel(struct bt_conn *conn) {
    printk("Pairing cancelled.\n");
}

static struct bt_conn_auth_cb auth_callbacks = {
    .passkey_confirm = auth_passkey_confirm,
    .cancel = auth_cancel,
};

int bluetooth_control_init(void) {
    int err;
    printk("Initializing Unified Bluetooth Controller\n");

    if (!gpio_is_ready_dt(&led1) || !gpio_is_ready_dt(&led2)) {
        printk("Error: LED device(s) are not ready\n");
        return -ENODEV;
    }
    err = gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
    if (err) {
        printk("Error %d: failed to configure LED1 pin\n", err);
        return err;
    }
    err = gpio_pin_configure_dt(&led2, GPIO_OUTPUT_INACTIVE);
    if (err) {
        printk("Error %d: failed to configure LED2 pin\n", err);
        return err;
    }
    k_timer_start(&blink_timer, K_MSEC(BLINK_INTERVAL_MS),
                  K_MSEC(BLINK_INTERVAL_MS));

    const struct bt_hogp_init_params hogp_init_params = {
        .ready_cb = hogp_ready_cb,
        .prep_error_cb = hogp_prep_fail_cb,
    };
    bt_hogp_init(&hogp, &hogp_init_params);

    err = bt_conn_auth_cb_register(&auth_callbacks);
    if (err) {
        printk("Failed to register authorization callbacks.\n");
        return err;
    }

    err = dk_buttons_init(button_handler);
    if (err) {
        printk("Failed to initialize buttons (err %d)\n", err);
        return err;
    }

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }
    printk("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
        printk("Settings loaded.\n");
    }

    scan_init();

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return err;
    }
    printk("Scanning successfully started\n");
    return 0;
}
