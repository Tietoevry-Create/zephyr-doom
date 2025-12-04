/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Nordic UART Bridge Service (NUS) sample
 */

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include <bluetooth/services/nus.h>

#include <dk_buttons_and_leds.h>

#include <zephyr/settings/settings.h>

#include <stdio.h>

#include <zephyr/logging/log.h>

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

#define LOG_MODULE_NAME peripheral_uart
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#define STACKSIZE CONFIG_BT_NUS_THREAD_STACK_SIZE
#define PRIORITY 7

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED DK_LED1
#define RUN_LED_BLINK_INTERVAL 1000

#define CON_STATUS_LED DK_LED2

#define KEY_PASSKEY_ACCEPT DK_BTN1_MSK
#define KEY_PASSKEY_REJECT DK_BTN2_MSK

#define UART_BUF_SIZE CONFIG_BT_NUS_UART_BUFFER_SIZE
#define UART_WAIT_FOR_BUF_DELAY K_MSEC(50)
#define UART_WAIT_FOR_RX CONFIG_BT_NUS_UART_RX_WAIT_TIME

static K_SEM_DEFINE(ble_init_ok, 0, 1);

static struct bt_conn *current_conn;
static struct bt_conn *auth_conn;

static const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(nordic_nus_uart));
static struct k_work_delayable uart_work;

struct uart_data_t
{
    uint8_t data[UART_BUF_SIZE];
    uint16_t len;
};

static K_FIFO_DEFINE(fifo_uart_tx_data);
static K_FIFO_DEFINE(fifo_uart_rx_data);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

#if CONFIG_BT_NUS_UART_ASYNC_ADAPTER
UART_ASYNC_ADAPTER_INST_DEFINE(async_adapter);
#else
static const struct device *const async_adapter;
#endif

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    ARG_UNUSED(dev);

    static size_t aborted_len;
    struct uart_data_t *buf;
    static uint8_t *aborted_buf;
    static bool disable_req;

    switch (evt->type)
    {
    case UART_TX_DONE:
        LOG_DBG("UART_TX_DONE");
        if ((evt->data.tx.len == 0) ||
            (!evt->data.tx.buf))
        {
            return;
        }

        if (aborted_buf)
        {
            buf = CONTAINER_OF(aborted_buf, struct uart_data_t,
                               data[0]);
            aborted_buf = NULL;
            aborted_len = 0;
        }
        else
        {
            buf = CONTAINER_OF(evt->data.tx.buf, struct uart_data_t,
                               data[0]);
        }

        k_free(buf);

        buf = k_fifo_get(&fifo_uart_tx_data, K_NO_WAIT);
        if (!buf)
        {
            return;
        }

        if (uart_tx(uart, buf->data, buf->len, SYS_FOREVER_MS))
        {
            LOG_WRN("Failed to send data over UART");
        }

        break;

    case UART_RX_RDY:
        LOG_DBG("UART_RX_RDY");
        buf = CONTAINER_OF(evt->data.rx.buf, struct uart_data_t, data[0]);
        buf->len += evt->data.rx.len;

        if (disable_req)
        {
            return;
        }

        if ((evt->data.rx.buf[buf->len - 1] == '\n') ||
            (evt->data.rx.buf[buf->len - 1] == '\r'))
        {
            disable_req = true;
            uart_rx_disable(uart);
        }

        break;

    case UART_RX_DISABLED:
        LOG_DBG("UART_RX_DISABLED");
        disable_req = false;

        buf = k_malloc(sizeof(*buf));
        if (buf)
        {
            buf->len = 0;
        }
        else
        {
            LOG_WRN("Not able to allocate UART receive buffer");
            k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
            return;
        }

        uart_rx_enable(uart, buf->data, sizeof(buf->data),
                       UART_WAIT_FOR_RX);

        break;

    case UART_RX_BUF_REQUEST:
        LOG_DBG("UART_RX_BUF_REQUEST");
        buf = k_malloc(sizeof(*buf));
        if (buf)
        {
            buf->len = 0;
            uart_rx_buf_rsp(uart, buf->data, sizeof(buf->data));
        }
        else
        {
            LOG_WRN("Not able to allocate UART receive buffer");
        }

        break;

    case UART_RX_BUF_RELEASED:
        LOG_DBG("UART_RX_BUF_RELEASED");
        buf = CONTAINER_OF(evt->data.rx_buf.buf, struct uart_data_t,
                           data[0]);

        if (buf->len > 0)
        {
            k_fifo_put(&fifo_uart_rx_data, buf);
        }
        else
        {
            k_free(buf);
        }

        break;

    case UART_TX_ABORTED:
        LOG_DBG("UART_TX_ABORTED");
        if (!aborted_buf)
        {
            aborted_buf = (uint8_t *)evt->data.tx.buf;
        }

        aborted_len += evt->data.tx.len;
        buf = CONTAINER_OF((void *)aborted_buf, struct uart_data_t,
                           data);

        uart_tx(uart, &buf->data[aborted_len],
                buf->len - aborted_len, SYS_FOREVER_MS);

        break;

    default:
        break;
    }
}

static void uart_work_handler(struct k_work *item)
{
    struct uart_data_t *buf;

    buf = k_malloc(sizeof(*buf));
    if (buf)
    {
        buf->len = 0;
    }
    else
    {
        LOG_WRN("Not able to allocate UART receive buffer");
        k_work_reschedule(&uart_work, UART_WAIT_FOR_BUF_DELAY);
        return;
    }

    uart_rx_enable(uart, buf->data, sizeof(buf->data), UART_WAIT_FOR_RX);
}

static bool uart_test_async_api(const struct device *dev)
{
    const struct uart_driver_api *api =
        (const struct uart_driver_api *)dev->api;

    return (api->callback_set != NULL);
}

static int uart_init(void)
{
    int err;
    struct uart_data_t *rx;
    struct uart_data_t *tx;

    if (!device_is_ready(uart))
    {
        return -ENODEV;
    }

    if (IS_ENABLED(CONFIG_USB_DEVICE_STACK))
    {
        err = usb_enable(NULL);
        if (err && (err != -EALREADY))
        {
            LOG_ERR("Failed to enable USB");
            return err;
        }
    }

    rx = k_malloc(sizeof(*rx));
    if (rx)
    {
        rx->len = 0;
    }
    else
    {
        return -ENOMEM;
    }

    k_work_init_delayable(&uart_work, uart_work_handler);

    if (IS_ENABLED(CONFIG_BT_NUS_UART_ASYNC_ADAPTER) && !uart_test_async_api(uart))
    {
        /* Implement API adapter */
        uart_async_adapter_init(async_adapter, uart);
        uart = async_adapter;
    }

    err = uart_callback_set(uart, uart_cb, NULL);
    if (err)
    {
        k_free(rx);
        LOG_ERR("Cannot initialize UART callback");
        return err;
    }

    if (IS_ENABLED(CONFIG_UART_LINE_CTRL))
    {
        LOG_INF("Wait for DTR");
        while (true)
        {
            uint32_t dtr = 0;

            uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr);
            if (dtr)
            {
                break;
            }
            /* Give CPU resources to low priority threads */
            k_sleep(K_MSEC(100));
        }
        LOG_INF("DTR set");
        err = uart_line_ctrl_set(uart, UART_LINE_CTRL_DCD, 1);
        if (err)
        {
            LOG_WRN("Failed to set DCD, ret code %d", err);
        }
        err = uart_line_ctrl_set(uart, UART_LINE_CTRL_DSR, 1);
        if (err)
        {
            LOG_WRN("Failed to set DSR, ret code %d", err);
        }
    }

    tx = k_malloc(sizeof(*tx));

    if (tx)
    {
        int pos;
        pos = snprintf(tx->data, sizeof(tx->data),
                       "Starting Nordic UART service example\r\n");

        if ((pos < 0) || (pos >= sizeof(tx->data)))
        {
            k_free(rx);
            k_free(tx);
            LOG_ERR("snprintf returned %d", pos);
            return -ENOMEM;
        }

        tx->len = pos;
    }
    else
    {
        k_free(rx);
        return -ENOMEM;
    }

    err = uart_tx(uart, tx->data, tx->len, SYS_FOREVER_MS);
    if (err)
    {
        k_free(rx);
        k_free(tx);
        LOG_ERR("Cannot display welcome message (err: %d)", err);
        return err;
    }

    err = uart_rx_enable(uart, rx->data, sizeof(rx->data), UART_WAIT_FOR_RX);
    if (err)
    {
        LOG_ERR("Cannot enable uart reception (err: %d)", err);
        /*
         * Free the rx buffer only because the tx buffer will be handled in the
         * callback
         */
        k_free(rx);
    }

    return err;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    if (err)
    {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    LOG_INF("Connected %s", addr);

    current_conn = bt_conn_ref(conn);

    dk_set_led_on(CON_STATUS_LED);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected: %s (reason %u)", addr, reason);

    if (auth_conn)
    {
        bt_conn_unref(auth_conn);
        auth_conn = NULL;
    }

    if (current_conn)
    {
        bt_conn_unref(current_conn);
        current_conn = NULL;
        dk_set_led_off(CON_STATUS_LED);
    }
}

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level,
                             enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err)
    {
        LOG_INF("Security changed: %s level %u", addr, level);
    }
    else
    {
        LOG_WRN("Security failed: %s level %u err %d", addr,
                level, err);
    }
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
    .security_changed = security_changed,
#endif
};

#if defined(CONFIG_BT_NUS_SECURITY_ENABLED)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Passkey for %s: %06u", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];

    auth_conn = bt_conn_ref(conn);

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Passkey for %s: %06u", addr, passkey);
    LOG_INF("Press Button 1 to confirm, Button 2 to reject.");
}

static void auth_cancel(struct bt_conn *conn)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing cancelled: %s", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing failed conn: %s, reason %d", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
    .passkey_display = auth_passkey_display,
    .passkey_confirm = auth_passkey_confirm,
    .cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
                          uint16_t len)
{
    char addr[BT_ADDR_LE_STR_LEN] = {0};

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));

    LOG_INF("Received data from: %s", addr);

    for (uint16_t pos = 0; pos != len;)
    {
        int err;
        struct uart_data_t *tx = k_malloc(sizeof(*tx));

        if (!tx)
        {
            LOG_WRN("Not able to allocate UART send data buffer");
            return;
        }

        /* Keep the last byte of TX buffer for potential LF char */
        size_t tx_data_size = sizeof(tx->data) - 1;

        if ((len - pos) > tx_data_size)
        {
            tx->len = tx_data_size;
        }
        else
        {
            tx->len = (len - pos);
        }

        memcpy(tx->data, &data[pos], tx->len);

        pos += tx->len;

        /*
         * Append the LF character when the CR character triggered transmission
         * from the peer
         */
        if ((pos == len) && (data[len - 1] == '\r'))
        {
            tx->data[tx->len] = '\n';
            tx->len++;
        }

        err = uart_tx(uart, tx->data, tx->len, SYS_FOREVER_MS);
        if (err)
        {
            k_fifo_put(&fifo_uart_tx_data, tx);
        }
    }
}

static struct bt_nus_cb nus_cb = {
    .received = bt_receive_cb,
};

void error(void)
{
    dk_set_leds_state(DK_ALL_LEDS_MSK, DK_NO_LEDS_MSK);

    while (true)
    {
        /* Spin for ever */
        k_sleep(K_MSEC(1000));
    }
}

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
static void num_comp_reply(bool accept)
{
    if (accept)
    {
        bt_conn_auth_passkey_confirm(auth_conn);
        LOG_INF("Numeric Match, conn %p", (void *)auth_conn);
    }
    else
    {
        bt_conn_auth_cancel(auth_conn);
        LOG_INF("Numeric Reject, conn %p", (void *)auth_conn);
    }

    bt_conn_unref(auth_conn);
    auth_conn = NULL;
}

void button_changed(uint32_t button_state, uint32_t has_changed)
{
    uint32_t buttons = button_state & has_changed;

    if (auth_conn)
    {
        if (buttons & KEY_PASSKEY_ACCEPT)
        {
            num_comp_reply(true);
        }

        if (buttons & KEY_PASSKEY_REJECT)
        {
            num_comp_reply(false);
        }
    }
}
#endif /* CONFIG_BT_NUS_SECURITY_ENABLED */

static void configure_gpio(void)
{
    int err;

#ifdef CONFIG_BT_NUS_SECURITY_ENABLED
    err = dk_buttons_init(button_changed);
    if (err)
    {
        LOG_ERR("Cannot init buttons (err: %d)", err);
    }
#endif

    err = dk_leds_init();
    if (err)
    {
        LOG_ERR("Cannot init LEDs (err: %d)", err);
    }
}

static const struct adc_dt_spec adc_chan0 =
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static const struct adc_dt_spec adc_chan1 =
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

int setup_adc_channel(const struct adc_dt_spec *adc_spec)
{
    int err;

    /* STEP 3.3 - validate that the ADC peripheral (SAADC) is ready */
    if (!adc_is_ready_dt(adc_spec))
    {
        LOG_ERR("ADC controller devivce %s not ready", adc_spec->dev->name);
        return -1;
    }
    /* STEP 3.4 - Setup the ADC channel */
    err = adc_channel_setup_dt(adc_spec);
    if (err < 0)
    {
        LOG_ERR("Could not setup channel #%d (%d)", adc_spec->channel_id, err);
        return err;
    }

    return 0;
}

int initialize_adc_sequence(const struct adc_dt_spec *adc_spec,
                            struct adc_sequence *sequence)
{
    int err;

    /* STEP 4.2 - Initialize the ADC sequence */
    err = adc_sequence_init_dt(adc_spec, sequence);
    if (err < 0)
    {
        LOG_ERR("Could not initalize sequnce");
        return err;
    }

    return 0;
}

#define BUTTON_A_PIN 14
#define BUTTON_B_PIN 23
#define BUTTON_C_PIN 12
#define BUTTON_D_PIN 17
#define BUTTON_E_PIN 1
#define BUTTON_F_PIN 13
int input_button_pins[] = {BUTTON_A_PIN, BUTTON_B_PIN};
int pullup_button_pins[] = {BUTTON_C_PIN, BUTTON_D_PIN, BUTTON_E_PIN, BUTTON_F_PIN};
static const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

int initialize_buttons()
{
    int err;

    for (int i = 0; i < sizeof(input_button_pins) / sizeof(input_button_pins[0]); i++)
    {
        err = gpio_pin_configure(gpio0, input_button_pins[i], GPIO_INPUT);
        if (err)
        {
            LOG_ERR("Error configuring pin %d", input_button_pins[i]);
            return err;
        }
    }

    for (int i = 0; i < sizeof(pullup_button_pins) / sizeof(pullup_button_pins[0]); i++)
    {
        err = gpio_pin_configure(gpio0, pullup_button_pins[i], GPIO_INPUT | GPIO_PULL_UP);
        if (err)
        {
            LOG_ERR("Error configuring pin %d", pullup_button_pins[i]);
            return err;
        }
    }

    return 0;
}

int is_button_A_pressed() { return gpio_pin_get(gpio0, BUTTON_A_PIN); }
int is_button_B_pressed() { return gpio_pin_get(gpio0, BUTTON_B_PIN); }
int is_button_C_pressed() { return gpio_pin_get(gpio0, BUTTON_C_PIN); }
int is_button_D_pressed() { return gpio_pin_get(gpio0, BUTTON_D_PIN); }
int is_button_E_pressed() { return gpio_pin_get(gpio0, BUTTON_E_PIN); }
int is_button_F_pressed() { return gpio_pin_get(gpio0, BUTTON_F_PIN); }

int main(void)
{
    int blink_status = 0;
    int err = 0;

    configure_gpio();

    err = uart_init();
    if (err)
    {
        error();
    }

    if (IS_ENABLED(CONFIG_BT_NUS_SECURITY_ENABLED))
    {
        err = bt_conn_auth_cb_register(&conn_auth_callbacks);
        if (err)
        {
            printk("Failed to register authorization callbacks.\n");
            return 0;
        }

        err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
        if (err)
        {
            printk("Failed to register authorization info callbacks.\n");
            return 0;
        }
    }

    err = bt_enable(NULL);
    if (err)
    {
        error();
    }

    LOG_INF("Bluetooth initialized");

    k_sem_give(&ble_init_ok);

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load();
    }

    err = bt_nus_init(&nus_cb);
    if (err)
    {
        LOG_ERR("Failed to initialize UART service (err: %d)", err);
        return 0;
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
                          ARRAY_SIZE(sd));
    if (err)
    {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return 0;
    }

    /*
     * STEP 4.1 - Define a variable of type adc_sequence and a buffer of type
     * uint16_t
     */
    int16_t adc_buf0;
    struct adc_sequence sequence0 = {
        .buffer = &adc_buf0,
        /* Buffer size in bytes, not number of samples */
        .buffer_size = sizeof(adc_buf0),
    };

    int16_t adc_buf1;
    struct adc_sequence sequence1 = {
        .buffer = &adc_buf1,
        .buffer_size = sizeof(adc_buf1),
    };

    err = setup_adc_channel(&adc_chan0);
    if (err < 0)
    {
        return err;
    }
    err = setup_adc_channel(&adc_chan1);
    if (err < 0)
    {
        return err;
    }

    err = initialize_adc_sequence(&adc_chan0, &sequence0);
    if (err < 0)
    {
        return err;
    }
    err = initialize_adc_sequence(&adc_chan1, &sequence1);
    if (err < 0)
    {
        return err;
    }

    err = initialize_buttons();
    if (err < 0)
    {
        return err;
    }

    for (;;)
    {
        dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
        k_sleep(K_MSEC(RUN_LED_BLINK_INTERVAL));

        /* Don't go any further until BLE is initialized */
        k_sem_take(&ble_init_ok, K_FOREVER);

        for (;;)
        {
            char buf[sizeof(adc_buf0) + sizeof(adc_buf1) + 6 * sizeof(bool)];

            err = adc_read(adc_chan0.dev, &sequence0);
            if (err < 0)
            {
                LOG_ERR("Could not read ADC (%d)", err);
                continue;
            }

            err = adc_read(adc_chan1.dev, &sequence1);
            if (err < 0)
            {
                LOG_ERR("Could not read ADC (%d)", err);
                continue;
            }

            /* Fill buff (Little Endian) */
            size_t offset = 0;
            memcpy(buf, &adc_buf0, sizeof(adc_buf0));
            offset += sizeof(adc_buf0);
            memcpy(buf + offset, &adc_buf1, sizeof(adc_buf1));
            offset += sizeof(adc_buf1);

            buf[offset] = is_button_A_pressed();
            offset += sizeof(bool);
            buf[offset] = is_button_B_pressed();
            offset += sizeof(bool);
            buf[offset] = is_button_C_pressed();
            offset += sizeof(bool);
            buf[offset] = is_button_D_pressed();
            offset += sizeof(bool);
            buf[offset] = is_button_E_pressed();
            offset += sizeof(bool);
            buf[offset] = is_button_F_pressed();
            offset += sizeof(bool);

            if (bt_nus_send(NULL, buf, offset))
            {
                LOG_WRN("Failed to send data over BLE connection");
            }

            k_sleep(K_MSEC(50));
        }
    }
}
