/*
 * Common FT810 display backend: Zephyr SPI + GPIO + optional cache flush.
 *
 * This replaces the older nRF-only register-level SPIM/GPIOTE/DPPI display
 * implementation and allows both nRF5340DK and FRDM-MCXN947 to share the same
 * `n_display.c`.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__has_include)
#if __has_include(<zephyr/cache.h>)
#include <zephyr/cache.h>
#define HAVE_SYS_CACHE_FLUSH 1
#elif __has_include(<zephyr/sys/cache.h>)
#include <zephyr/sys/cache.h>
#define HAVE_SYS_CACHE_FLUSH 1
#else
#define HAVE_SYS_CACHE_FLUSH 0
#endif
#else
#include <zephyr/cache.h>
#define HAVE_SYS_CACHE_FLUSH 1
#endif

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "FT810.h"
#include "board_config.h"

static const struct device* spi_dev;
static struct spi_config spi_cfg;
static const struct device* gpio0_dev;

/* Async framebuffer transfer support */
static struct k_thread display_spi_thread;
static K_THREAD_STACK_DEFINE(display_spi_stack, 1024);
static struct k_sem display_spi_start_sem;
static struct k_sem display_spi_done_sem;
static struct k_mutex display_spi_bus_mutex;
static volatile const uint8_t* display_async_data_ptr;
static volatile size_t display_async_data_len;
static volatile uint32_t display_async_addr;
static volatile int display_spi_tip;

static inline void display_spi_flush_tx_cache(const void* buf, size_t len) {
#if HAVE_SYS_CACHE_FLUSH
    /* Required when SPI driver uses DMA (EDMA reads RAM, not D-cache). */
    sys_cache_data_flush_range((void*)buf, len);
#else
    (void)buf;
    (void)len;
#endif
}

static void display_spi_thread_fn(void* p1, void* p2, void* p3) {
    (void)p1;
    (void)p2;
    (void)p3;

    for (;;) {
        k_sem_take(&display_spi_start_sem, K_FOREVER);

        const uint8_t* data_ptr = (const uint8_t*)display_async_data_ptr;
        size_t data_len = (size_t)display_async_data_len;
        uint32_t addr = (uint32_t)display_async_addr;

        k_mutex_lock(&display_spi_bus_mutex, K_FOREVER);

        /* CS low */
        gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);

        /* Chunk the transfer to avoid SPI/DMA length limitations. */
        while (data_len > 0) {
            size_t chunk_len = data_len;
            if (chunk_len > 16384) {
                chunk_len = 16384;
            }

            uint8_t* addrBytes = (uint8_t*)&addr;
            uint8_t hdr[3];
            hdr[0] = addrBytes[2] | 0x80;
            hdr[1] = addrBytes[1];
            hdr[2] = addrBytes[0];

            struct spi_buf bufs[2];
            bufs[0].buf = (void*)hdr;
            bufs[0].len = sizeof(hdr);
            bufs[1].buf = (void*)data_ptr;
            bufs[1].len = chunk_len;
            const struct spi_buf_set tx = {.buffers = bufs, .count = 2};

            (void)spi_write(spi_dev, &spi_cfg, &tx);

            addr += (uint32_t)chunk_len;
            data_ptr += chunk_len;
            data_len -= chunk_len;
        }

        /* CS high */
        gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
        k_mutex_unlock(&display_spi_bus_mutex);
        k_sem_give(&display_spi_done_sem);
    }
}

void N_display_gpiote_end_to_cs(void) {
    /* No-op in Zephyr SPI path; CS is handled manually via GPIO */
}

void N_display_gpiote_clear(void) { /* No-op in Zephyr SPI path */ }

void N_display_spi_init(void) {
    /* SPI controller selected via devicetree alias `spi4`.
     * - nRF5340DK: `spi4 = &spi4;`
     * - FRDM-MCXN947: `spi4 = &arduino_spi;`
     */
    spi_dev = DEVICE_DT_GET(DT_ALIAS(spi4));
    if (!device_is_ready(spi_dev)) {
        printk("SPI device not ready\n");
    }

    gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(gpio0_dev)) {
        printk("GPIO0 device not ready\n");
    }

    /* Configure CS and PD_N as outputs */
    gpio_pin_configure(gpio0_dev, DISPLAY_PIN_CS_N, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure(gpio0_dev, DISPLAY_PIN_PD_N, GPIO_OUTPUT_ACTIVE);

    /* SPI config: 8-bit, mode0, MSB first, 32MHz */
    spi_cfg.frequency = 32000000U;
    spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
    spi_cfg.slave = 0;

    /* Async SPI worker */
    k_sem_init(&display_spi_start_sem, 0, 1);
    k_sem_init(&display_spi_done_sem, 0, 1);
    k_mutex_init(&display_spi_bus_mutex);
    display_spi_tip = 0;
    k_thread_create(&display_spi_thread, display_spi_stack,
                    K_THREAD_STACK_SIZEOF(display_spi_stack),
                    display_spi_thread_fn, NULL, NULL, NULL, K_PRIO_PREEMPT(3),
                    0, K_NO_WAIT);
}

void N_display_power_reset(void) {
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_PD_N, 0);
    k_msleep(50);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_PD_N, 1);
    k_msleep(50);
}

void N_display_spi_setup(int txdMaxCnt, volatile uint8_t* txdPtr, int rxdMaxCnt,
                         volatile uint8_t* rxdPtr) {
    (void)txdMaxCnt;
    (void)txdPtr;
    (void)rxdMaxCnt;
    (void)rxdPtr;
}

void N_display_spi_transfer_finish(void) {
    if (display_spi_tip) {
        k_sem_take(&display_spi_done_sem, K_FOREVER);
        display_spi_tip = 0;
    }
}

static inline void display_spi_wait_idle(void) {
    N_display_spi_transfer_finish();
}

void N_display_spi_transfer_start(void) {
    N_display_spi_transfer_finish();
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
}

void N_display_spi_transfer_data(void) {
    /* No-op: synchronous transfers are done via spi_write/spi_read */
}

void N_display_spi_transfer_data_end(void) { /* No-op */ }

void N_display_spi_transfer_end(void) {
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
}

void N_display_spi_transfer(void) { /* Unused in Zephyr SPI backend */ }

void N_display_spi_cmd(uint8_t b1, uint8_t b2) {
    uint8_t buf[3];
    buf[0] = b1;
    buf[1] = b2;
    buf[2] = 0x00;
    struct spi_buf txb = {.buf = buf, .len = sizeof(buf)};
    const struct spi_buf_set tx = {.buffers = &txb, .count = 1};
    display_spi_wait_idle();
    k_mutex_lock(&display_spi_bus_mutex, K_FOREVER);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
    k_mutex_unlock(&display_spi_bus_mutex);
}

void N_display_spi_wr8(uint32_t addr, uint8_t data) {
    uint8_t* addrBytes = (uint8_t*)&addr;
    uint8_t buf[4];
    buf[0] = addrBytes[2] | 0x80;
    buf[1] = addrBytes[1];
    buf[2] = addrBytes[0];
    buf[3] = data;
    struct spi_buf txb = {.buf = buf, .len = sizeof(buf)};
    const struct spi_buf_set tx = {.buffers = &txb, .count = 1};
    display_spi_wait_idle();
    k_mutex_lock(&display_spi_bus_mutex, K_FOREVER);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
    k_mutex_unlock(&display_spi_bus_mutex);
}

void N_display_spi_wr16(uint32_t addr, uint16_t data) {
    uint8_t* addrBytes = (uint8_t*)&addr;
    uint8_t* dataBytes = (uint8_t*)&data;
    uint8_t buf[5];
    buf[0] = addrBytes[2] | 0x80;
    buf[1] = addrBytes[1];
    buf[2] = addrBytes[0];
    buf[3] = dataBytes[0];
    buf[4] = dataBytes[1];
    struct spi_buf txb = {.buf = buf, .len = sizeof(buf)};
    const struct spi_buf_set tx = {.buffers = &txb, .count = 1};
    display_spi_wait_idle();
    k_mutex_lock(&display_spi_bus_mutex, K_FOREVER);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
    k_mutex_unlock(&display_spi_bus_mutex);
}

void N_display_spi_wr32(uint32_t addr, uint32_t data) {
    uint8_t* addrBytes = (uint8_t*)&addr;
    uint8_t* dataBytes = (uint8_t*)&data;
    uint8_t buf[7];
    buf[0] = addrBytes[2] | 0x80;
    buf[1] = addrBytes[1];
    buf[2] = addrBytes[0];
    buf[3] = dataBytes[0];
    buf[4] = dataBytes[1];
    buf[5] = dataBytes[2];
    buf[6] = dataBytes[3];
    struct spi_buf txb = {.buf = buf, .len = sizeof(buf)};
    const struct spi_buf_set tx = {.buffers = &txb, .count = 1};
    display_spi_wait_idle();
    k_mutex_lock(&display_spi_bus_mutex, K_FOREVER);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
    k_mutex_unlock(&display_spi_bus_mutex);
}

void N_display_spi_wr(uint32_t addr, int dataSize, uint8_t* data) {
    if (dataSize >= 2048) {
        N_display_spi_transfer_finish();
        display_spi_flush_tx_cache(data, (size_t)dataSize);
        display_async_addr = addr;
        display_async_data_ptr = data;
        display_async_data_len = (size_t)dataSize;
        display_spi_tip = 1;
        k_sem_give(&display_spi_start_sem);
        return;
    }

    display_spi_flush_tx_cache(data, (size_t)dataSize);
    display_spi_wait_idle();
    k_mutex_lock(&display_spi_bus_mutex, K_FOREVER);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);

    const uint8_t* data_ptr = (const uint8_t*)data;
    size_t data_len = (size_t)dataSize;
    uint32_t cur_addr = addr;

    while (data_len > 0) {
        size_t chunk_len = data_len;
        if (chunk_len > 16384) {
            chunk_len = 16384;
        }

        uint8_t* addrBytes2 = (uint8_t*)&cur_addr;
        uint8_t hdr[3];
        hdr[0] = addrBytes2[2] | 0x80;
        hdr[1] = addrBytes2[1];
        hdr[2] = addrBytes2[0];

        struct spi_buf bufs[2] = {
            {.buf = hdr, .len = sizeof(hdr)},
            {.buf = (void*)data_ptr, .len = chunk_len},
        };
        const struct spi_buf_set tx = {.buffers = bufs, .count = 2};
        (void)spi_write(spi_dev, &spi_cfg, &tx);

        cur_addr += (uint32_t)chunk_len;
        data_ptr += chunk_len;
        data_len -= chunk_len;
    }

    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
    k_mutex_unlock(&display_spi_bus_mutex);
}

uint8_t N_display_spi_rd8(uint32_t addr) {
    uint8_t* addrBytes = (uint8_t*)&addr;
    uint8_t hdr[4];
    uint8_t data = 0xFF;
    hdr[0] = addrBytes[2];
    hdr[1] = addrBytes[1];
    hdr[2] = addrBytes[0];
    hdr[3] = 0x00;

    struct spi_buf tx_buf = {.buf = hdr, .len = sizeof(hdr)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    struct spi_buf rx_buf = {.buf = &data, .len = 1};
    const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

    display_spi_wait_idle();
    k_mutex_lock(&display_spi_bus_mutex, K_FOREVER);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    (void)spi_read(spi_dev, &spi_cfg, &rx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
    k_mutex_unlock(&display_spi_bus_mutex);
    return data;
}

/// --------

uint32_t ram_free_loc = FT810_RAM_G;

void N_display_wakeup() { N_display_spi_cmd(0x00, 0x00); }

void N_display_init() {
    N_display_spi_init();

    N_display_power_reset();

    N_display_wakeup();

    k_msleep(80);

    N_display_spi_cmd(FT810_CMD_CLKEXT, 0x00);

    k_msleep(80);

    printf("N_display_init - Display ID: %.2X\n",
           N_display_spi_rd8(FT810_REG_ID));
    printf("N_display_init - CPU Reset: %.2X\n",
           N_display_spi_rd8(FT810_REG_CPURESET));

    N_display_spi_wr16(FT810_REG_HSIZE, 800);  // Active width of LCD display
    N_display_spi_wr16(FT810_REG_VSIZE, 480);  // Active height of LCD display
    N_display_spi_wr16(FT810_REG_HCYCLE,
                       928);  // Total number of clocks per line
    N_display_spi_wr16(FT810_REG_HOFFSET, 88);  // Start of active line
    N_display_spi_wr16(FT810_REG_HSYNC0, 0);   // Start of horizontal sync pulse
    N_display_spi_wr16(FT810_REG_HSYNC1, 48);  // End of horizontal sync pulse
    N_display_spi_wr16(FT810_REG_VCYCLE,
                       525);  // Total number of lines per screen
    N_display_spi_wr16(FT810_REG_VOFFSET, 32);  // Start of active screen
    N_display_spi_wr16(FT810_REG_VSYNC0, 0);    // Start of vertical sync pulse
    N_display_spi_wr16(FT810_REG_VSYNC1, 3);    // End of vertical sync pulse
    N_display_spi_wr8(FT810_REG_SWIZZLE, 0);    // Define RGB output pins
    N_display_spi_wr8(FT810_REG_PCLK_POL, 1);   // Define active edge of PCLK

    uint8_t disGpio = N_display_spi_rd8(FT810_REG_GPIO);
    N_display_spi_wr8(FT810_REG_GPIO, disGpio | 0x80);

    N_display_spi_wr8(FT810_REG_PWM_DUTY, 0xFF);  // Backlight PWM duty cycle

    N_display_spi_wr8(FT810_REG_PCLK, 2);  // Pixel Clock

    N_display_spi_wr8(FT810_REG_ROTATE, 1);  // inverted (up-down)
}

uint32_t N_display_ram_alloc(size_t size) {
    uint32_t result = ram_free_loc;
    ram_free_loc += size;
    return result;
}

void N_display_dlswap_frame() {
    N_display_spi_wr32(FT810_REG_DLSWAP, FT810_DLSWAP_FRAME);
}

uint32_t display_dli = 0;

void dl_start() { display_dli = FT810_RAM_DL; }
void dl(uint32_t cmd) {
    N_display_spi_wr32(display_dli, cmd);
    display_dli += 4;
}
void dl_end() { N_display_spi_wr32(display_dli, FT810_DISPLAY()); }
