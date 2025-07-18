/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic UART Service Client sample
 */

#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include <bluetooth/services/nus.h>
#include <bluetooth/services/nus_client.h>
#include <errno.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>

/* UART payload buffer element size. */
#define UART_BUF_SIZE 20

#define KEY_PASSKEY_ACCEPT DK_BTN1_MSK
#define KEY_PASSKEY_REJECT DK_BTN2_MSK

#define NUS_WRITE_TIMEOUT K_MSEC(150)
#define UART_WAIT_FOR_BUF_DELAY K_MSEC(50)
#define UART_RX_TIMEOUT \
    50000 /* Wait for RX complete event time in microseconds. */

#define DEADZONE_X 50
#define DEADZONE_Y 40

const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
struct k_work_delayable uart_work;

K_SEM_DEFINE(nus_write_sem, 0, 1);

struct uart_data_t {
    void *fifo_reserved;
    uint8_t data[UART_BUF_SIZE];
    uint16_t len;
};

K_FIFO_DEFINE(fifo_uart_tx_data);
K_FIFO_DEFINE(fifo_uart_rx_data);

struct bt_conn *default_conn;
struct bt_nus_client nus_client;

void ble_data_sent(struct bt_nus_client *nus, uint8_t err,
                   const uint8_t *const data, uint16_t len) {
    ARG_UNUSED(nus);

    k_sem_give(&nus_write_sem);

    if (err) {
        LOG_WRN("ATT error code: 0x%02X", err);
    }
}

#include "d_event.h"

uint8_t ble_data_received(struct bt_nus_client *nus, const uint8_t *data,
                          uint16_t len) {
    ARG_UNUSED(nus);

    int err;
    int16_t adc_buf0, adc_buf1;
    bool button_A, button_B, button_C, button_D, button_E, button_F;

    char buf[sizeof(adc_buf0) + sizeof(adc_buf1) + 6 * sizeof(bool)];

    memcpy(buf, data, len);

    size_t offset = 0;

    memcpy(&adc_buf0, buf, sizeof(adc_buf0));
    offset += sizeof(adc_buf0);
    memcpy(&adc_buf1, buf + offset, sizeof(adc_buf1));
    offset += sizeof(adc_buf1);

    memcpy(&button_A, buf + offset, sizeof(bool));
    offset += sizeof(bool);
    memcpy(&button_B, buf + offset, sizeof(bool));
    offset += sizeof(bool);
    memcpy(&button_C, buf + offset, sizeof(bool));
    offset += sizeof(bool);
    memcpy(&button_D, buf + offset, sizeof(bool));
    offset += sizeof(bool);
    memcpy(&button_E, buf + offset, sizeof(bool));
    offset += sizeof(bool);
    memcpy(&button_F, buf + offset, sizeof(bool));
    offset += sizeof(bool);

    printk("ADC0: %d, ADC1: %d, A: %d, B: %d, C: %d, D: %d, E: %d, F: %d\n",
           adc_buf0, adc_buf1, button_A, button_B, button_C, button_D, button_E,
           button_F);
    int centered_adc = adc_buf0 - 470;
    if (centered_adc > -DEADZONE_X && centered_adc < DEADZONE_X) {
        adc_buf0 = 0;
    } else {
        adc_buf0 = centered_adc;
    }
    centered_adc = adc_buf1 - 481;
    if (centered_adc > -DEADZONE_Y && centered_adc < DEADZONE_Y) {
        adc_buf1 = 0;
    } else {
        adc_buf1 = centered_adc;
    }
    adc_buf0 /= 10;
    adc_buf1 /= 8;
    adc_buf0 = -adc_buf0;
    adc_buf1 = -adc_buf1;

    // Joystick state.
    //    data1: Bitfield of buttons currently pressed.
    //    data2: X axis mouse movement (turn).
    //    data3: Y axis mouse movement (forward/backward).
    //    data4: Third axis mouse movement (strafe).
    //    data5: Fourth axis mouse movement (look)
    event_t joystick_event;
    joystick_event.type = ev_joystick;
    joystick_event.data1 = !button_A | (!button_B << 1) | (!button_C << 2) |
                           (!button_D << 3) | (!button_E << 4) |
                           (!button_F << 5);
    joystick_event.data2 = adc_buf0;
    joystick_event.data3 = adc_buf1;

    D_PostEvent(&joystick_event);

    return BT_GATT_ITER_CONTINUE;
}

void uart_cb(const struct device *dev, struct uart_event *evt,
             void *user_data) {
    ARG_UNUSED(dev);

    static size_t aborted_len;
    struct uart_data_t *buf;
    static uint8_t *aborted_buf;
    static bool disable_req;

    switch (evt->type) {
        case UART_TX_DONE:
            LOG_DBG("UART_TX_DONE");
            if ((evt->data.tx.len == 0) || (!evt->data.tx.buf)) {
                return;
            }

            if (aborted_buf) {
                buf = CONTAINER_OF(aborted_buf, struct uart_data_t, data[0]);
                aborted_buf = NULL;
                aborted_len = 0;
            } else {
                buf =
                    CONTAINER_OF(evt->data.tx.buf, struct uart_data_t, data[0]);
            }

            k_free(buf);

            buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
            if (!buf) {
                return;
            }

            if (uart_tx(uart, buf->data, buf->len, SYS_FOREVER_MS)) {
                LOG_WRN("Failed to send data over UART");
            }

            break;

        case UART_RX_RDY:
            LOG_DBG("UART_RX_RDY");
            buf = CONTAINER_OF(evt->data.rx.buf, struct uart_data_t, data[0]);
            buf->len += evt->data.rx.len;

            if (disable_req) {
                return;
            }

            if ((evt->data.rx.buf[buf->len - 1] == '\n') ||
                (evt->data.rx.buf[buf->len - 1] == '\r')) {
                disable_req = true;
                uart_rx_disable(uart);
            }

            break;

        case UART_RX_DISABLED:
            LOG_DBG("UART_RX_DISABLED");
            disable_req = false;

            buf = k_malloc(sizeof(*buf));
            if (buf) {
                buf->len = 0;
            } else {
                LOG_WRN("Not able to allocate UART receive buffer");
                k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
                return;
            }

            uart_rx_enable(uart, buf->data, sizeof(buf->data), UART_RX_TIMEOUT);

            break;

        case UART_RX_BUF_REQUEST:
            LOG_DBG("UART_RX_BUF_REQUEST");
            buf = k_malloc(sizeof(*buf));
            if (buf) {
                buf->len = 0;
                uart_rx_buf_rsp(uart, buf->data, sizeof(buf->data));
            } else {
                LOG_WRN("Not able to allocate UART receive buffer");
            }

            break;

        case UART_RX_BUF_RELEASED:
            LOG_DBG("UART_RX_BUF_RELEASED");
            buf =
                CONTAINER_OF(evt->data.rx_buf.buf, struct uart_data_t, data[0]);

            if (buf->len > 0) {
                k_fifo_put(&fifo_uart_rx_data, buf);
            } else {
                k_free(buf);
            }

            break;

        case UART_TX_ABORTED:
            LOG_DBG("UART_TX_ABORTED");
            if (!aborted_buf) {
                aborted_buf = (uint8_t *)evt->data.tx.buf;
            }

            aborted_len += evt->data.tx.len;
            buf = CONTAINER_OF(aborted_buf, struct uart_data_t, data[0]);

            uart_tx(uart, &buf->data[aborted_len], buf->len - aborted_len,
                    SYS_FOREVER_MS);

            break;

        default:
            break;
    }
}

void uart_work_handler(struct k_work *item) {
    struct uart_data_t *buf;

    buf = k_malloc(sizeof(*buf));
    if (buf) {
        buf->len = 0;
    } else {
        LOG_WRN("Not able to allocate UART receive buffer");
        k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
        return;
    }

    uart_rx_enable(uart, buf->data, sizeof(buf->data), UART_RX_TIMEOUT);
}

int uart_init(void) {
    int err;
    struct uart_data_t *rx;

    if (!device_is_ready(uart)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }

    rx = k_malloc(sizeof(*rx));
    if (rx) {
        rx->len = 0;
    } else {
        return -ENOMEM;
    }

    k_work_init_delayable(&uart_work, uart_work_handler);

    err = uart_callback_set(uart, uart_cb, NULL);
    if (err) {
        return err;
    }

    return uart_rx_enable(uart, rx->data, sizeof(rx->data), UART_RX_TIMEOUT);
}

void discovery_complete(struct bt_gatt_dm *dm, void *context) {
    struct bt_nus_client *nus = context;
    LOG_INF("Service discovery completed");

    bt_gatt_dm_data_print(dm);

    bt_nus_handles_assign(dm, nus);
    bt_nus_subscribe_receive(nus);

    bt_gatt_dm_data_release(dm);
}

void discovery_service_not_found(struct bt_conn *conn, void *context) {
    LOG_INF("Service not found");
}

void discovery_error(struct bt_conn *conn, int err, void *context) {
    LOG_WRN("Error while discovering GATT database: (%d)", err);
}

struct bt_gatt_dm_cb discovery_cb = {
    .completed = discovery_complete,
    .service_not_found = discovery_service_not_found,
    .error_found = discovery_error,
};

void gatt_discover(struct bt_conn *conn) {
    int err;

    if (conn != default_conn) {
        return;
    }

    err =
        bt_gatt_dm_start(conn, BT_UUID_NUS_SERVICE, &discovery_cb, &nus_client);
    if (err) {
        LOG_ERR(
            "could not start the discovery procedure, error "
            "code: %d",
            err);
    }
}

void exchange_func(struct bt_conn *conn, uint8_t err,
                   struct bt_gatt_exchange_params *params) {
    if (!err) {
        LOG_INF("MTU exchange done");
    } else {
        LOG_WRN("MTU exchange failed (err %" PRIu8 ")", err);
    }
}

void connected(struct bt_conn *conn, uint8_t conn_err) {
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (conn_err) {
        LOG_INF("Failed to connect to %s (%d)", addr, conn_err);

        if (default_conn == conn) {
            bt_conn_unref(default_conn);
            default_conn = NULL;

            err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
            if (err) {
                LOG_ERR("Scanning failed to start (err %d)", err);
            }
        }

        return;
    }

    LOG_INF("Connected: %s", addr);

    static struct bt_gatt_exchange_params exchange_params;

    exchange_params.func = exchange_func;
    err = bt_gatt_exchange_mtu(conn, &exchange_params);
    if (err) {
        LOG_WRN("MTU exchange failed (err %d)", err);
    }

    err = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (err) {
        LOG_WRN("Failed to set security: %d", err);

        gatt_discover(conn);
    }

    err = bt_scan_stop();
    if ((!err) && (err != -EALREADY)) {
        LOG_ERR("Stop LE scan failed (err %d)", err);
    }
}

void disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected: %s (reason %u)", addr, reason);

    if (default_conn != conn) {
        return;
    }

    bt_conn_unref(default_conn);
    default_conn = NULL;

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)", err);
    }
}

void security_changed(struct bt_conn *conn, bt_security_t level,
                      enum bt_security_err err) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err) {
        LOG_INF("Security changed: %s level %u", addr, level);
    } else {
        LOG_WRN("Security failed: %s level %u err %d", addr, level, err);
    }

    gatt_discover(conn);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {.connected = connected,
                                     .disconnected = disconnected,
                                     .security_changed = security_changed};

void scan_filter_match(struct bt_scan_device_info *device_info,
                       struct bt_scan_filter_match *filter_match,
                       bool connectable) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

    LOG_INF("Filters matched. Address: %s connectable: %d", addr, connectable);
}

void scan_connecting_error(struct bt_scan_device_info *device_info) {
    LOG_WRN("Connecting failed");
}

void scan_connecting(struct bt_scan_device_info *device_info,
                     struct bt_conn *conn) {
    default_conn = bt_conn_ref(conn);
}

int nus_client_init(void) {
    int err;
    struct bt_nus_client_init_param init = {.cb = {
                                                .received = ble_data_received,
                                                .sent = ble_data_sent,
                                            }};

    err = bt_nus_client_init(&nus_client, &init);
    if (err) {
        LOG_ERR("NUS Client initialization failed (err %d)", err);
        return err;
    }

    LOG_INF("NUS Client module initialized");
    return err;
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, scan_connecting_error,
                scan_connecting);

int scan_init(void) {
    int err;
    struct bt_scan_init_param scan_init = {
        .connect_if_match = 1,
    };

    bt_scan_init(&scan_init);
    bt_scan_cb_register(&scan_cb);

    err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_NUS_SERVICE);
    if (err) {
        LOG_ERR("Scanning filters cannot be set (err %d)", err);
        return err;
    }

    err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
    if (err) {
        LOG_ERR("Filters cannot be turned on (err %d)", err);
        return err;
    }

    LOG_INF("Scan module initialized");
    return err;
}

void auth_cancel(struct bt_conn *conn) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing cancelled: %s", addr);
}

void pairing_complete(struct bt_conn *conn, bool bonded) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}

void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_WRN("Pairing failed conn: %s, reason %d", addr, reason);
}

struct bt_conn_auth_cb conn_auth_callbacks = {
    .cancel = auth_cancel,
};

struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    .pairing_complete = pairing_complete, .pairing_failed = pairing_failed};

int bluetooth_init(void) {
    int err;

    err = bt_conn_auth_cb_register(&conn_auth_callbacks);
    if (err) {
        LOG_ERR("Failed to register authorization callbacks.");
        return 0;
    }

    err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
    if (err) {
        printk("Failed to register authorization info callbacks.\n");
        return 0;
    }

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return 0;
    }
    LOG_INF("Bluetooth initialized");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    err = uart_init();
    if (err != 0) {
        LOG_ERR("uart_init failed (err %d)", err);
        return 0;
    }

    err = scan_init();
    if (err != 0) {
        LOG_ERR("scan_init failed (err %d)", err);
        return 0;
    }

    err = nus_client_init();
    if (err != 0) {
        LOG_ERR("nus_client_init failed (err %d)", err);
        return 0;
    }

    printk("Starting Bluetooth Central UART example\n");

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)", err);
        return 0;
    }

    LOG_INF("Scanning successfully started");

    return 0;
}

int bluetooth_main(void) {
    int err;

    for (;;) {
        /* Wait indefinitely for data to be sent over Bluetooth */
        struct uart_data_t *buf = k_fifo_get(&fifo_uart_rx_data, K_FOREVER);

        err = bt_nus_client_send(&nus_client, buf->data, buf->len);
        if (err) {
            LOG_WRN(
                "Failed to send data over BLE connection"
                "(err %d)",
                err);
        }

        err = k_sem_take(&nus_write_sem, NUS_WRITE_TIMEOUT);
        if (err) {
            LOG_WRN("NUS send timeout");
        }

        k_free(buf);
    }
}
