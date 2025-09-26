/*
 * Copyright (c) 2019 - 2020, Nordic Semiconductor ASA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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

#include <stdbool.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

#include "FT810.h"
#include "board_config.h"

static const struct device *spi_dev;
static struct spi_config spi_cfg;
static const struct device *gpio0_dev;

#define BUF_MAXCNT 8

volatile int display_spi_tip;  // transfer-in-progress
volatile uint8_t display_spi_txd_buf[BUF_MAXCNT];
volatile uint8_t display_spi_rxd_buf[BUF_MAXCNT];

void N_display_gpiote_end_to_cs() {
    /* No-op in Zephyr SPI path; CS is handled manually via GPIO */
}

void N_display_gpiote_clear() { /* No-op in Zephyr SPI path */ }

void N_display_spi_init() {
    /* Use SPI4 directly */
    spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi4));
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

    /* Start at 16 MHz for reliable init; can switch to 32 MHz after */
    spi_cfg.frequency = 16000000U;
    spi_cfg.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
    spi_cfg.slave = 0;
    /* Manual CS via GPIO */

    display_spi_tip = 0;
}

void N_display_power_reset() {
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_PD_N, 0);
    k_msleep(50);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_PD_N, 1);
    k_msleep(50);
}

void N_display_spi_setup(int txdMaxCnt, volatile uint8_t *txdPtr, int rxdMaxCnt,
                         volatile uint8_t *rxdPtr) {
    /* No-op with Zephyr SPI path; handled in transfer functions */
}

void N_display_spi_transfer_finish() {
    /* No pending async transfer in Zephyr path */
}

void N_display_spi_transfer_start() {
    N_display_spi_transfer_finish();
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
}

void N_display_spi_transfer_data() {
    /* This function performed a blocking transfer previously; no-op here */
}

void N_display_spi_transfer_data_end() {
    /* No async tail transfer; the combined transfer handles CS */
}

void N_display_spi_transfer_end() {
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
}

void N_display_spi_transfer() {
    /* This function is a generic transfer in original code; here it's unused.
     */
}

void N_display_spi_cmd(uint8_t b1, uint8_t b2) {
    uint8_t buf[3];
    buf[0] = b1;
    buf[1] = b2;
    buf[2] = 0x00;
    struct spi_buf txb = {.buf = buf, .len = sizeof(buf)};
    const struct spi_buf_set tx = {.buffers = &txb, .count = 1};
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
}

void N_display_spi_wr8(uint32_t addr, uint8_t data) {
    uint8_t *addrBytes = (uint8_t *)&addr;
    uint8_t buf[4];
    buf[0] = addrBytes[2] | 0x80;
    buf[1] = addrBytes[1];
    buf[2] = addrBytes[0];
    buf[3] = data;
    struct spi_buf txb = {.buf = buf, .len = sizeof(buf)};
    const struct spi_buf_set tx = {.buffers = &txb, .count = 1};
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
}

void N_display_spi_wr16(uint32_t addr, uint16_t data) {
    uint8_t *addrBytes = (uint8_t *)&addr;
    uint8_t *dataBytes = (uint8_t *)&data;
    uint8_t buf[5];
    buf[0] = addrBytes[2] | 0x80;
    buf[1] = addrBytes[1];
    buf[2] = addrBytes[0];
    buf[3] = dataBytes[0];
    buf[4] = dataBytes[1];
    struct spi_buf txb = {.buf = buf, .len = sizeof(buf)};
    const struct spi_buf_set tx = {.buffers = &txb, .count = 1};
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
}

void N_display_spi_wr32(uint32_t addr, uint32_t data) {
    uint8_t *addrBytes = (uint8_t *)&addr;
    uint8_t *dataBytes = (uint8_t *)&data;
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
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
}

void N_display_spi_wr(uint32_t addr, int dataSize, uint8_t *data) {
    uint8_t *addrBytes = (uint8_t *)&addr;
    uint8_t addr_buf[3];
    addr_buf[0] = addrBytes[2] | 0x80;
    addr_buf[1] = addrBytes[1];
    addr_buf[2] = addrBytes[0];
    struct spi_buf bufs[2] = {
        {.buf = addr_buf, .len = sizeof(addr_buf)},
        {.buf = data, .len = (size_t)dataSize},
    };
    const struct spi_buf_set tx = {.buffers = bufs, .count = 2};
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
}

// void N_display_spi_transfer_data_async() {
//   NRF_DISPLAY_SPIM->EVENTS_END = 0;
//   NRF_DISPLAY_SPIM->TASKS_START = 1;
//   while (NRF_DISPLAY_SPIM->EVENTS_END == 0) {
//   }
// }

// void N_display_spi_wr_async(uint32_t addr, int dataSize, uint8_t *data) {
//   uint8_t *addrBytes = (uint8_t*)&addr;

//   // Assuming MCU is Little-Endian
//   display_spi_txd_buf[0] = addrBytes[2] | 0x80;
//   display_spi_txd_buf[1] = addrBytes[1];
//   display_spi_txd_buf[2] = addrBytes[0];

//   N_display_spi_transfer_start();

//   N_display_spi_setup(3, display_spi_txd_buf, 0, NULL);
//   N_display_spi_transfer_data();

//   volatile uint8_t *dataPtr = (volatile uint8_t*)data;
//   N_display_spi_setup(dataSize, dataPtr, 0, NULL);
//   N_display_spi_transfer_data();

//   N_display_spi_transfer_end();
// }

uint8_t N_display_spi_rd8(uint32_t addr) {
    uint8_t *addrBytes = (uint8_t *)&addr;
    uint8_t hdr[4];
    uint8_t data = 0xFF;
    hdr[0] = addrBytes[2];
    hdr[1] = addrBytes[1];
    hdr[2] = addrBytes[0];
    hdr[3] = 0x00; /* dummy */
    struct spi_buf tx_buf = {.buf = hdr, .len = sizeof(hdr)};
    const struct spi_buf_set tx = {.buffers = &tx_buf, .count = 1};
    struct spi_buf rx_buf = {.buf = &data, .len = 1};
    const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 0);
    (void)spi_write(spi_dev, &spi_cfg, &tx);
    (void)spi_read(spi_dev, &spi_cfg, &rx);
    gpio_pin_set(gpio0_dev, DISPLAY_PIN_CS_N, 1);
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

    /* After FT810 init is complete, increase SPI to 32 MHz to match old build
     */
    spi_cfg.frequency = 32000000U;

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
