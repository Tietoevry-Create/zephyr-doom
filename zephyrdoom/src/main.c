/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <debug/cpu_load.h>
#include <ff.h>
#include <hal/nrf_gpio.h>
#include <nrfx_clock.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
// #include <zephyr/sd/sd.h>
#include <zephyr/storage/disk_access.h>

#include "deh_str.h"

LOG_MODULE_REGISTER(doom_main, CONFIG_DOOM_MAIN_LOG_LEVEL);

#include "bluetooth_control.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define GPIO0 ((NRF_GPIO_Type*)0x50842500UL)
#define GPIO1 ((NRF_GPIO_Type*)0x50842800UL)

void clock_initialization() {
    nrfx_clock_hfclk_start();
    nrf_clock_hfclk_div_set(NRF_CLOCK_S, NRF_CLOCK_HFCLK_DIV_1);
    nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK192M, NRF_CLOCK_HFCLK_DIV_1);
    // nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK, NRF_CLOCK_HFCLK_DIV_1);
}

#define SD_ROOT_PATH "/SD:/"
/* Maximum length for path support by Windows file system */
#define PATH_MAX_LEN 260
#define K_SEM_OPER_TIMEOUT_MS 500
K_SEM_DEFINE(m_sem_sd_oper_ongoing, 1, 1);

static const char* sd_root_path = "/SD:";
static bool sd_init_success;

static FATFS fat_fs;

static struct fs_mount_t mnt_pt = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};

int sd_card_init(void) {
    int ret;
    static const char* sd_dev = "SD";
    uint64_t sd_card_size_bytes;
    uint32_t sector_count;
    size_t sector_size;

    ret = disk_access_init(sd_dev);
    if (ret) {
        LOG_DBG("SD card init failed, please check if SD card inserted");
        return -ENODEV;
    }

    ret = disk_access_ioctl(sd_dev, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count);
    if (ret) {
        LOG_ERR("Unable to get sector count");
        return ret;
    }

    LOG_DBG("Sector count: %d", sector_count);

    ret = disk_access_ioctl(sd_dev, DISK_IOCTL_GET_SECTOR_SIZE, &sector_size);
    if (ret) {
        LOG_ERR("Unable to get sector size");
        return ret;
    }

    LOG_DBG("Sector size: %d bytes", sector_size);

    sd_card_size_bytes = (uint64_t)sector_count * sector_size;

    LOG_INF("SD card volume size: %d MB", (uint32_t)(sd_card_size_bytes >> 20));

    mnt_pt.mnt_point = sd_root_path;

    ret = fs_mount(&mnt_pt);
    if (ret) {
        LOG_ERR("Mnt. disk failed, could be format issue. should be FAT/exFAT");
        return ret;
    }

    sd_init_success = true;

    return 0;
}

int sd_card_list_files(char const* const path, char* buf, size_t* buf_size) {
    int ret;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;
    char abs_path_name[PATH_MAX_LEN + 1] = SD_ROOT_PATH;
    size_t used_buf_size = 0;

    if (k_sem_count_get(&m_sem_sd_oper_ongoing) <= 0) {
        LOG_ERR("SD operation ongoing");
        return -EPERM;
    }

    k_sem_take(&m_sem_sd_oper_ongoing, K_MSEC(K_SEM_OPER_TIMEOUT_MS));

    if (!sd_init_success) {
        k_sem_give(&m_sem_sd_oper_ongoing);
        return -ENODEV;
    }

    fs_dir_t_init(&dirp);

    if (path == NULL) {
        ret = fs_opendir(&dirp, sd_root_path);
        if (ret) {
            LOG_ERR("Open SD card root dir failed");
            k_sem_give(&m_sem_sd_oper_ongoing);
            return ret;
        }
    } else {
        if (strlen(path) > CONFIG_FS_FATFS_MAX_LFN) {
            LOG_ERR("Path is too long");
            k_sem_give(&m_sem_sd_oper_ongoing);
            // return -FR_INVALID_NAME;
            return -1;
        }

        strcat(abs_path_name, path);

        ret = fs_opendir(&dirp, abs_path_name);
        if (ret) {
            LOG_ERR("Open assigned path failed");
            k_sem_give(&m_sem_sd_oper_ongoing);
            return ret;
        }
    }

    while (1) {
        ret = fs_readdir(&dirp, &entry);
        if (ret) {
            k_sem_give(&m_sem_sd_oper_ongoing);
            return ret;
        }

        if (entry.name[0] == 0) {
            break;
        }

        if (buf != NULL) {
            size_t remaining_buf_size = *buf_size - used_buf_size;
            ssize_t len = snprintk(
                &buf[used_buf_size], remaining_buf_size, "[%s]\t%s\n",
                entry.type == FS_DIR_ENTRY_DIR ? "DIR " : "FILE", entry.name);

            if (len >= remaining_buf_size) {
                LOG_ERR("Failed to append to buffer, error: %d", len);
                k_sem_give(&m_sem_sd_oper_ongoing);
                return -EINVAL;
            }

            used_buf_size += len;
        }

        LOG_INF("[%s] %s", entry.type == FS_DIR_ENTRY_DIR ? "DIR " : "FILE",
                entry.name);
    }

    ret = fs_closedir(&dirp);
    if (ret) {
        LOG_ERR("Close SD card root dir failed");
        k_sem_give(&m_sem_sd_oper_ongoing);
        return ret;
    }

    *buf_size = used_buf_size;
    k_sem_give(&m_sem_sd_oper_ongoing);
    return 0;
}


int main(void) {
    LOG_INF("BOARD STARTING %s", CONFIG_BOARD);

    cpu_load_init();

    clock_initialization();

    uint32_t hfclkctrl = NRF_CLOCK_S->HFCLKCTRL;
    printf("HFCLK_S: %d\n", hfclkctrl);

    NRF_CACHE_S->ENABLE = 1;

    // sd_card_init(); // TODO: Get this working (all references to N_fs have been commented out in w_wad and m_misc) N_qspi_init();

    // if (!no_sdcard) {
    //     N_fs_init();
    //     printf("\n\n");
    //     printf("----------------------------------\n");
    //     printf("NFS Initialized\n");
    //     printf("---------------------------------\n");
    // }

    N_ButtonsInit();

    M_ArgvInit();

    bluetooth_init();

    D_DoomMain();

    while (true) {
        __WFE();
    }

    int ret;

    if (!gpio_is_ready_dt(&led)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }

    while (1) {
        sd_card_list_files(NULL, NULL, NULL);
        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0) {
            return 0;
        }
        k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}
