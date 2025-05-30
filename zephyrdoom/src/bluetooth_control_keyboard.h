/* main.c - Application main entry point */

/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

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
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include "d_event.h"
#include "doomkeys.h"

/**
 * Switch between boot protocol and report protocol mode.
 */
#define KEY_BOOTMODE_MASK DK_BTN2_MSK
/**
 * Switch CAPSLOCK state.
 *
 * @note
 * For simplicity of the code it works only in boot mode.
 */
#define KEY_CAPSLOCK_MASK DK_BTN1_MSK
/**
 * Switch CAPSLOCK state with response
 *
 * Write CAPSLOCK with response.
 * Just for testing purposes.
 * The result should be the same like usine @ref KEY_CAPSLOCK_MASK
 */
#define KEY_CAPSLOCK_RSP_MASK DK_BTN3_MSK

/* Key used to accept or reject passkey value */
#define KEY_PAIRING_ACCEPT DK_BTN1_MSK
#define KEY_PAIRING_REJECT DK_BTN2_MSK

static struct bt_conn *default_conn;
static struct bt_hogp hogp;
static struct bt_conn *auth_conn;
static uint8_t capslock_state;

static void hids_on_ready(struct k_work *work);
static K_WORK_DEFINE(hids_ready_work, hids_on_ready);

static void scan_filter_match(struct bt_scan_device_info *device_info,
                              struct bt_scan_filter_match *filter_match,
                              bool connectable) {
    char addr[BT_ADDR_LE_STR_LEN];

    if (!filter_match->uuid.match || (filter_match->uuid.count != 1)) {
        printk("Invalid device connected\n");

        return;
    }

    const struct bt_uuid *uuid = filter_match->uuid.uuid[0];

    bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

    printk("Filters matched on UUID 0x%04x.\nAddress: %s connectable: %s\n",
           BT_UUID_16(uuid)->val, addr, connectable ? "yes" : "no");
}

static void scan_connecting_error(struct bt_scan_device_info *device_info) {
    printk("Connecting failed\n");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
                            struct bt_conn *conn) {
    default_conn = bt_conn_ref(conn);
}
/** .. include_startingpoint_scan_rst */
static void scan_filter_no_match(struct bt_scan_device_info *device_info,
                                 bool connectable) {
    int err;
    struct bt_conn *conn;
    char addr[BT_ADDR_LE_STR_LEN];

    if (device_info->recv_info->adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
        printk("Direct advertising received from %s\n", addr);
        bt_scan_stop();

        err = bt_conn_le_create(device_info->recv_info->addr,
                                BT_CONN_LE_CREATE_CONN, device_info->conn_param,
                                &conn);

        if (!err) {
            default_conn = bt_conn_ref(conn);
            bt_conn_unref(conn);
        }
    }
}
/** .. include_endpoint_scan_rst */
BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match,
                scan_connecting_error, scan_connecting);

static void discovery_completed_cb(struct bt_gatt_dm *dm, void *context) {
    int err;

    printk("The discovery procedure succeeded\n");

    bt_gatt_dm_data_print(dm);

    err = bt_hogp_handles_assign(dm, &hogp);
    if (err) {
        printk("Could not init HIDS client object, error: %d\n", err);
    }

    err = bt_gatt_dm_data_release(dm);
    if (err) {
        printk(
            "Could not release the discovery data, error "
            "code: %d\n",
            err);
    }
}

static void discovery_service_not_found_cb(struct bt_conn *conn,
                                           void *context) {
    printk("The service could not be found during the discovery\n");
}

static void discovery_error_found_cb(struct bt_conn *conn, int err,
                                     void *context) {
    printk("The discovery procedure failed with %d\n", err);
}

static const struct bt_gatt_dm_cb discovery_cb = {
    .completed = discovery_completed_cb,
    .service_not_found = discovery_service_not_found_cb,
    .error_found = discovery_error_found_cb,
};

static void gatt_discover(struct bt_conn *conn) {
    int err;

    if (conn != default_conn) {
        return;
    }

    err = bt_gatt_dm_start(conn, BT_UUID_HIDS, &discovery_cb, NULL);
    if (err) {
        printk(
            "could not start the discovery procedure, error "
            "code: %d\n",
            err);
    }
}

static void connected(struct bt_conn *conn, uint8_t conn_err) {
    int err;
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (conn_err) {
        printk("Failed to connect to %s (%u)\n", addr, conn_err);
        if (conn == default_conn) {
            bt_conn_unref(default_conn);
            default_conn = NULL;

            /* This demo doesn't require active scan */
            err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
            if (err) {
                printk("Scanning failed to start (err %d)\n", err);
            }
        }

        return;
    }

    printk("Connected: %s\n", addr);

    err = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (err) {
        printk("Failed to set security: %d\n", err);

        gatt_discover(conn);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (auth_conn) {
        bt_conn_unref(auth_conn);
        auth_conn = NULL;
    }

    printk("Disconnected: %s (reason %u)\n", addr, reason);

    if (bt_hogp_assign_check(&hogp)) {
        printk("HIDS client active - releasing");
        bt_hogp_release(&hogp);
    }

    if (default_conn != conn) {
        return;
    }

    bt_conn_unref(default_conn);
    default_conn = NULL;

    /* This demo doesn't require active scan */
    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
    }
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err) {
        printk("Security changed: %s level %u\n", addr, level);
    } else {
        printk("Security failed: %s level %u err %d\n", addr, level, err);
    }

    gatt_discover(conn);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {.connected = connected,
                                     .disconnected = disconnected,
                                     .security_changed = security_changed};

static void scan_init(void) {
    int err;

    struct bt_scan_init_param scan_init = {
        .connect_if_match = 1,
        .scan_param = NULL,
        .conn_param = BT_LE_CONN_PARAM_DEFAULT};

    bt_scan_init(&scan_init);
    bt_scan_cb_register(&scan_cb);

    err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_HIDS);
    if (err) {
        printk("Scanning filters cannot be set (err %d)\n", err);

        return;
    }

    err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
    if (err) {
        printk("Filters cannot be turned on (err %d)\n", err);
    }
}

#include "d_event.h"
#include "doomkeys.h"
#include <string.h>

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
    {0x01, KEY_RCTRL},
    {0x02, KEY_RSHIFT},
    {0x04, KEY_RALT},
    {0x10, KEY_RCTRL},
    {0x20, KEY_RSHIFT},
    {0x40, KEY_RALT},
};

#define HID_MAX_KEYS_IN_REPORT 6

static uint8_t hogp_notify_cb(struct bt_hogp *hogp,
                              struct bt_hogp_rep_info *rep,
                              uint8_t err,
                              const uint8_t *data)
{
    uint8_t size = bt_hogp_rep_size(rep);

    // The HID report for keyboard is 8 bytes:
    // data[0]: modifier_byte
    // data[1]: reserved (should be 0)
    // data[2] - data[7]: keycodes (up to 6 keys)
    if (err || !data || size < 8) {
        return BT_GATT_ITER_CONTINUE;
    }

    // printk("Notification, id: %u, size: %u, data:", bt_hogp_rep_id(rep), size);
    // for (uint8_t i = 0; i < size; i++) {
    //     printk(" 0x%02x", data[i]);
    // }
    // printk("\n");

    static uint8_t last_modifier_byte = 0;
    uint8_t current_modifier_byte = data[0];
    int mod_map_size = sizeof(mod_map) / sizeof(mod_map[0]);

    for (int i = 0; i < mod_map_size; i++) {
        uint8_t bit = mod_map[i].bitmask;
        if ((current_modifier_byte & bit) && !(last_modifier_byte & bit)) { // Pressed
            event_t e = { .type = ev_keydown, .data1 = mod_map[i].doom };
            D_PostEvent(&e);
            // printk("MOD KeyDown: 0x%02x -> Doom %d\n", bit, mod_map[i].doom);
        } else if (!(current_modifier_byte & bit) && (last_modifier_byte & bit)) { // Released
            event_t e = { .type = ev_keyup, .data1 = mod_map[i].doom };
            D_PostEvent(&e);
            // printk("MOD KeyUp: 0x%02x -> Doom %d\n", bit, mod_map[i].doom);
        }
    }
    last_modifier_byte = current_modifier_byte;

    static uint8_t previously_pressed_hids[HID_MAX_KEYS_IN_REPORT] = {0};
    uint8_t current_pressed_hids[HID_MAX_KEYS_IN_REPORT];

    for (int i = 0; i < HID_MAX_KEYS_IN_REPORT; i++) {
        current_pressed_hids[i] = data[i + 2];
    }

    int key_map_size = sizeof(key_map) / sizeof(key_map[0]);

    // 1. Process Key Downs: Keys in current_pressed_hids but not in previously_pressed_hids
    for (int i = 0; i < HID_MAX_KEYS_IN_REPORT; i++) {
        uint8_t current_hid = current_pressed_hids[i];
        if (current_hid == 0x00 || current_hid == 0x01) continue; // Skip "NoKey" and "ErrorRollOver"

        bool was_pressed_before = false;
        for (int j = 0; j < HID_MAX_KEYS_IN_REPORT; j++) {
            if (previously_pressed_hids[j] == current_hid) {
                was_pressed_before = true;
                break;
            }
        }

        if (!was_pressed_before) { // New key press
            for (int k = 0; k < key_map_size; k++) {
                if (key_map[k].hid == current_hid) {
                    event_t e = { .type = ev_keydown, .data1 = key_map[k].doom, .data2 = key_map[k].ascii };
                    D_PostEvent(&e);
                    // printk("KeyDOWN: HID 0x%02x -> Doom %d, ASCII '%c'\n", current_hid, key_map[k].doom, key_map[k].ascii ? key_map[k].ascii : ' ');
                    break;
                }
            }
        }
    }

    // 2. Process Key Ups: Keys in previously_pressed_hids but not in current_pressed_hids
    for (int i = 0; i < HID_MAX_KEYS_IN_REPORT; i++) {
        uint8_t previous_hid = previously_pressed_hids[i];
        if (previous_hid == 0x00 || previous_hid == 0x01) continue; // Skip "NoKey" and "ErrorRollOver"

        bool still_pressed = false;
        for (int j = 0; j < HID_MAX_KEYS_IN_REPORT; j++) {
            if (current_pressed_hids[j] == previous_hid) {
                still_pressed = true;
                break;
            }
        }

        if (!still_pressed) { // Key was released
            for (int k = 0; k < key_map_size; k++) {
                if (key_map[k].hid == previous_hid) {
                    event_t e = { .type = ev_keyup, .data1 = key_map[k].doom };
                    D_PostEvent(&e);
                    // printk("KeyUP: HID 0x%02x -> Doom %d\n", previous_hid, key_map[k].doom);
                    break;
                }
            }
        }
    }

    // 3. Update state for the next call
    memcpy(previously_pressed_hids, current_pressed_hids, sizeof(previously_pressed_hids));

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t hogp_boot_mouse_report(struct bt_hogp *hogp,
                                      struct bt_hogp_rep_info *rep, uint8_t err,
                                      const uint8_t *data) {
    uint8_t size = bt_hogp_rep_size(rep);
    uint8_t i;

    if (!data) {
        return BT_GATT_ITER_STOP;
    }
    printk("Notification, mouse boot, size: %u, data:", size);
    for (i = 0; i < size; ++i) {
        printk(" 0x%x", data[i]);
    }
    printk("\n");
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t hogp_boot_kbd_report(struct bt_hogp *hogp,
                                    struct bt_hogp_rep_info *rep, uint8_t err,
                                    const uint8_t *data) {
    uint8_t size = bt_hogp_rep_size(rep);
    uint8_t i;

    if (!data) {
        return BT_GATT_ITER_STOP;
    }
    printk("Notification, keyboard boot, size: %u, data:", size);
    for (i = 0; i < size; ++i) {
        printk(" 0x%x", data[i]);
    }
    printk("\n");
    return BT_GATT_ITER_CONTINUE;
}

static void hogp_ready_cb(struct bt_hogp *hogp) {
    k_work_submit(&hids_ready_work);
}

static void hids_on_ready(struct k_work *work) {
    int err;
    struct bt_hogp_rep_info *rep = NULL;

    printk("HIDS is ready to work\n");

    while (NULL != (rep = bt_hogp_rep_next(&hogp, rep))) {
        if (bt_hogp_rep_type(rep) == BT_HIDS_REPORT_TYPE_INPUT) {
            printk("Subscribe to report id: %u\n", bt_hogp_rep_id(rep));
            err = bt_hogp_rep_subscribe(&hogp, rep, hogp_notify_cb);
            if (err) {
                printk("Subscribe error (%d)\n", err);
            }
        }
    }
    if (hogp.rep_boot.kbd_inp) {
        printk("Subscribe to boot keyboard report\n");
        err = bt_hogp_rep_subscribe(&hogp, hogp.rep_boot.kbd_inp,
                                    hogp_boot_kbd_report);
        if (err) {
            printk("Subscribe error (%d)\n", err);
        }
    }
    if (hogp.rep_boot.mouse_inp) {
        printk("Subscribe to boot mouse report\n");
        err = bt_hogp_rep_subscribe(&hogp, hogp.rep_boot.mouse_inp,
                                    hogp_boot_mouse_report);
        if (err) {
            printk("Subscribe error (%d)\n", err);
        }
    }
}

static void hogp_prep_fail_cb(struct bt_hogp *hogp, int err) {
    printk("ERROR: HIDS client preparation failed!\n");
}

static void hogp_pm_update_cb(struct bt_hogp *hogp) {
    printk("Protocol mode updated: %s\n",
           bt_hogp_pm_get(hogp) == BT_HIDS_PM_BOOT ? "BOOT" : "REPORT");
}

/* HIDS client initialization parameters */
static const struct bt_hogp_init_params hogp_init_params = {
    .ready_cb = hogp_ready_cb,
    .prep_error_cb = hogp_prep_fail_cb,
    .pm_update_cb = hogp_pm_update_cb};

static void button_bootmode(void) {
    if (!bt_hogp_ready_check(&hogp)) {
        printk("HID device not ready\n");
        return;
    }
    int err;
    enum bt_hids_pm pm = bt_hogp_pm_get(&hogp);
    enum bt_hids_pm new_pm =
        ((pm == BT_HIDS_PM_BOOT) ? BT_HIDS_PM_REPORT : BT_HIDS_PM_BOOT);

    printk("Setting protocol mode: %s\n",
           (new_pm == BT_HIDS_PM_BOOT) ? "BOOT" : "REPORT");
    err = bt_hogp_pm_write(&hogp, new_pm);
    if (err) {
        printk("Cannot change protocol mode (err %d)\n", err);
    }
}

static void hidc_write_cb(struct bt_hogp *hidc, struct bt_hogp_rep_info *rep,
                          uint8_t err) {
    printk("Caps lock sent\n");
}

static void button_capslock(void) {
    int err;
    uint8_t data;

    if (!bt_hogp_ready_check(&hogp)) {
        printk("HID device not ready\n");
        return;
    }
    if (!hogp.rep_boot.kbd_out) {
        printk("HID device does not have Keyboard OUT report\n");
        return;
    }
    if (bt_hogp_pm_get(&hogp) != BT_HIDS_PM_BOOT) {
        printk("This function works only in BOOT Report mode\n");
        return;
    }
    capslock_state = capslock_state ? 0 : 1;
    data = capslock_state ? 0x02 : 0;
    err = bt_hogp_rep_write_wo_rsp(&hogp, hogp.rep_boot.kbd_out, &data,
                                   sizeof(data), hidc_write_cb);

    if (err) {
        printk("Keyboard data write error (err: %d)\n", err);
        return;
    }
    printk("Caps lock send (val: 0x%x)\n", data);
}

static uint8_t capslock_read_cb(struct bt_hogp *hogp,
                                struct bt_hogp_rep_info *rep, uint8_t err,
                                const uint8_t *data) {
    if (err) {
        printk("Capslock read error (err: %u)\n", err);
        return BT_GATT_ITER_STOP;
    }
    if (!data) {
        printk("Capslock read - no data\n");
        return BT_GATT_ITER_STOP;
    }
    printk("Received data (size: %u, data[0]: 0x%x)\n", bt_hogp_rep_size(rep),
           data[0]);

    return BT_GATT_ITER_STOP;
}

static void capslock_write_cb(struct bt_hogp *hogp,
                              struct bt_hogp_rep_info *rep, uint8_t err) {
    int ret;

    printk("Capslock write result: %u\n", err);

    ret = bt_hogp_rep_read(hogp, rep, capslock_read_cb);
    if (ret) {
        printk("Cannot read capslock value (err: %d)\n", ret);
    }
}

static void button_capslock_rsp(void) {
    if (!bt_hogp_ready_check(&hogp)) {
        printk("HID device not ready\n");
        return;
    }
    if (!hogp.rep_boot.kbd_out) {
        printk("HID device does not have Keyboard OUT report\n");
        return;
    }
    int err;
    uint8_t data;

    capslock_state = capslock_state ? 0 : 1;
    data = capslock_state ? 0x02 : 0;
    err = bt_hogp_rep_write(&hogp, hogp.rep_boot.kbd_out, capslock_write_cb,
                            &data, sizeof(data));
    if (err) {
        printk("Keyboard data write error (err: %d)\n", err);
        return;
    }
    printk("Caps lock send using write with response (val: 0x%x)\n", data);
}

static void num_comp_reply(bool accept) {
    if (accept) {
        bt_conn_auth_passkey_confirm(auth_conn);
        printk("Numeric Match, conn %p\n", auth_conn);
    } else {
        bt_conn_auth_cancel(auth_conn);
        printk("Numeric Reject, conn %p\n", auth_conn);
    }

    bt_conn_unref(auth_conn);
    auth_conn = NULL;
}

static void button_handler(uint32_t button_state, uint32_t has_changed) {
    uint32_t button = button_state & has_changed;

    if (auth_conn) {
        if (button & KEY_PAIRING_ACCEPT) {
            num_comp_reply(true);
        }

        if (button & KEY_PAIRING_REJECT) {
            num_comp_reply(false);
        }

        return;
    }

    if (button & KEY_BOOTMODE_MASK) {
        button_bootmode();
    }
    if (button & KEY_CAPSLOCK_MASK) {
        button_capslock();
    }
    if (button & KEY_CAPSLOCK_RSP_MASK) {
        button_capslock_rsp();
    }
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey) {
    char addr[BT_ADDR_LE_STR_LEN];

    auth_conn = bt_conn_ref(conn);

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Passkey for %s: %06u\n", addr, passkey);
    printk("Press Button 1 to confirm, Button 2 to reject.\n");
}

static void auth_cancel(struct bt_conn *conn) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing cancelled: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
    .passkey_display = auth_passkey_display,
    .passkey_confirm = auth_passkey_confirm,
    .cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    .pairing_complete = pairing_complete, .pairing_failed = pairing_failed};

int bluetooth_main_keyboard(void) {
    int err;

    printk("Starting Bluetooth Central HIDS example\n");

    bt_hogp_init(&hogp, &hogp_init_params);

    err = bt_conn_auth_cb_register(&conn_auth_callbacks);
    if (err) {
        printk("failed to register authorization callbacks.\n");
        return 0;
    }

    err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
    if (err) {
        printk("Failed to register authorization info callbacks.\n");
        return 0;
    }

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    scan_init();

    err = dk_buttons_init(button_handler);
    if (err) {
        printk("Failed to initialize buttons (err %d)\n", err);
        return 0;
    }

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return 0;
    }

    printk("Scanning successfully started\n");
    return 0;
}
