/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <debug/cpu_load.h>
#include <ff.h>
#include <hal/nrf_gpio.h>
#include <nrfx_clock.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>

#include "bluetooth_control.h"

LOG_MODULE_REGISTER(doom_main, CONFIG_DOOM_MAIN_LOG_LEVEL);

int no_sdcard = 1;
#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

static FATFS fat_fs;
static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};
static const char* disk_mount_pt = DISK_MOUNT_PT;

void clock_initialization() {
    nrfx_clock_hfclk_start();
    nrf_clock_hfclk_div_set(NRF_CLOCK_S, NRF_CLOCK_HFCLK_DIV_1);
    nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK192M, NRF_CLOCK_HFCLK_DIV_1);
}

static int lsdir(const char* path) {
    int res;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;
    int count = 0;

    fs_dir_t_init(&dirp);

    res = fs_opendir(&dirp, path);
    if (res) {
        printk("Error opening dir %s [%d]\n", path, res);
        return res;
    }

    printk("\nListing dir %s ...\n", path);
    for (;;) {
        res = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (res || entry.name[0] == 0) {
            break;
        }

        if (entry.type == FS_DIR_ENTRY_DIR) {
            printk("[DIR ] %s\n", entry.name);
        } else {
            printk("[FILE] %s (size = %zu)\n", entry.name, entry.size);
        }
        count++;
    }

    fs_closedir(&dirp);
    if (res == 0) {
        res = count;
    }

    return res;
}

int main(void) {
    LOG_INF("BOARD STARTING %s", CONFIG_BOARD);    

    clock_initialization();

    uint32_t hfclkctrl = NRF_CLOCK_S->HFCLKCTRL;
    printf("HFCLK_S: %d\n", hfclkctrl);

    NRF_CACHE_S->ENABLE = 1;

    mp.mnt_point = disk_mount_pt;

    int res = fs_mount(&mp);

    if (res == FR_OK) {
        printk("Disk mounted.\n");
        no_sdcard = 0;
        lsdir(disk_mount_pt);
    } else {
        printk("Error mounting disk.\n");
    }

    N_ButtonsInit();

    N_I2S_init();

    M_ArgvInit();

    int err = bluetooth_control_init();
    if (err) {
        LOG_ERR("Bluetooth control initialization failed.");
        return 0;
    }

    D_DoomMain();

    while (true) {
        __WFE();
    }

    return 0;
}

