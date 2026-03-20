/*
 * FRDM external flash backend using Zephyr flash API and XIP mapping.
 */

#include "n_qspi.h"

#undef PACKED_STRUCT
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#if DT_NODE_HAS_STATUS(DT_NODELABEL(w25q64jvssiq), okay)
#define DOOM_FLASH_NODE DT_NODELABEL(w25q64jvssiq)
#elif DT_NODE_HAS_STATUS(DT_ALIAS(spi_flash0), okay)
#define DOOM_FLASH_NODE DT_ALIAS(spi_flash0)
#else
#error "No supported external flash node found for FRDM QSPI backend"
#endif

static const struct device* flash_dev;
static size_t qspi_next_loc;

static void ensure_flash_dev(void) {
    if (flash_dev != NULL) {
        return;
    }
    flash_dev = DEVICE_DT_GET(DOOM_FLASH_NODE);
    if (!device_is_ready(flash_dev)) {
        flash_dev = NULL;
    }
}

void N_qspi_wait(void) { k_msleep(1); }

void N_qspi_init(void) {
    ensure_flash_dev();
    qspi_next_loc = 0;
}

void N_qspi_reserve_blocks(size_t block_count) {
    qspi_next_loc += block_count * N_QSPI_BLOCK_SIZE;
}

size_t N_qspi_alloc_block(void) {
    size_t loc = qspi_next_loc;
    qspi_next_loc += N_QSPI_BLOCK_SIZE;
    return loc;
}

void* N_qspi_data_pointer(size_t loc) {
    return (void*)(N_QSPI_XIP_START_ADDR + loc);
}

void N_qspi_erase_block(size_t loc) {
    ensure_flash_dev();
    if (flash_dev == NULL) {
        return;
    }
    (void)flash_erase(flash_dev, loc, N_QSPI_BLOCK_SIZE);
}

void N_qspi_write(size_t loc, void* buffer, size_t size) {
    ensure_flash_dev();
    if (flash_dev == NULL) {
        return;
    }
    (void)flash_write(flash_dev, loc, buffer, size);
}

void N_qspi_write_block(size_t loc, void* buffer, size_t size) {
    N_qspi_erase_block(loc);
    N_qspi_write(loc, buffer, size);
}

void N_qspi_read(size_t loc, void* buffer, size_t size) {
    ensure_flash_dev();
    if (flash_dev == NULL) {
        return;
    }
    (void)flash_read(flash_dev, loc, buffer, size);
}
