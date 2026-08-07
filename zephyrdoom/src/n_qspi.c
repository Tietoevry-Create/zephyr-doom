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

/*
 * External flash backend using Zephyr flash API + XIP mapping.
 *
 * Reads of WAD/texture data use XIP (via N_qspi_data_pointer()), while erase/
 * write/read operations go through Zephyr's flash driver.
 */

#include "n_qspi.h"

#undef PACKED_STRUCT
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

void I_Error(char* error, ...);

// Keep in sync with the external flash selection used by w_wad.c.
#if DT_NODE_HAS_STATUS(DT_ALIAS(spi_flash0), okay)
#define DOOM_FLASH_NODE DT_ALIAS(spi_flash0)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(mx25r64), okay)
#define DOOM_FLASH_NODE DT_NODELABEL(mx25r64)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(w25q64jvssiq), okay)
#define DOOM_FLASH_NODE DT_NODELABEL(w25q64jvssiq)
#else
#error "Unsupported board: no supported external flash devicetree node found."
#endif

static const struct device* flash_dev;
static size_t qspi_next_loc;

static const struct device* ensure_flash_dev(void) {
    if (flash_dev != NULL) {
        return flash_dev;
    }

    flash_dev = DEVICE_DT_GET(DOOM_FLASH_NODE);
    if (!device_is_ready(flash_dev)) {
        flash_dev = NULL;
        I_Error("N_qspi: external flash device not ready");
    }

    return flash_dev;
}

void N_qspi_wait(void) {
    // Zephyr flash APIs are synchronous for our use; keep a small delay to
    // match historical behavior and avoid hammering the controller.
    k_msleep(1);
}

void N_qspi_init(void) {
    (void)ensure_flash_dev();
    qspi_next_loc = 0;
}

void N_qspi_erase_block(size_t loc) {
    const struct device* dev = ensure_flash_dev();
    int rc = flash_erase(dev, loc, N_QSPI_BLOCK_SIZE);
    if (rc != 0) {
        I_Error("N_qspi: flash_erase failed (%d)", rc);
    }
    N_qspi_wait();
}

void N_qspi_write(size_t loc, void* buffer, size_t size) {
    const struct device* dev = ensure_flash_dev();
    int rc = flash_write(dev, loc, buffer, size);
    if (rc != 0) {
        I_Error("N_qspi: flash_write failed (%d)", rc);
    }
    N_qspi_wait();
}

void N_qspi_write_block(size_t loc, void* buffer, size_t size) {
    if (size > N_QSPI_BLOCK_SIZE) {
        I_Error("N_qspi_write_block: Tried to write block > 64KB");
    }

    N_qspi_erase_block(loc);
    N_qspi_write(loc, buffer, size);
}

void N_qspi_read(size_t loc, void* buffer, size_t size) {
    const struct device* dev = ensure_flash_dev();
    int rc = flash_read(dev, loc, buffer, size);
    if (rc != 0) {
        I_Error("N_qspi: flash_read failed (%d)", rc);
    }
    N_qspi_wait();
}

void* N_qspi_data_pointer(size_t loc) {
    return (void*)(N_QSPI_XIP_START_ADDR + loc);
}

void N_qspi_reserve_blocks(size_t block_count) {
    qspi_next_loc += block_count * N_QSPI_BLOCK_SIZE;
}

size_t N_qspi_alloc_block(void) {
    size_t loc = qspi_next_loc;
    qspi_next_loc += N_QSPI_BLOCK_SIZE;
    return loc;
}
