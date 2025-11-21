/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ff.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
// #include <zephyr/sd/sd.h>
#include <zephyr/storage/disk_access.h>

LOG_MODULE_REGISTER(doom_main, CONFIG_DOOM_MAIN_LOG_LEVEL);

/* Bluetooth control disabled in portable core build */

/* Forward declarations from original headers to avoid pulling Nordic-specific
 * dependencies */
void N_ButtonsInit(void);
void N_I2S_init(void);
void M_ArgvInit(void);
void D_DoomMain(void);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

/* LED access not used in minimal build */

#define SD_ROOT_PATH "/SD:/"
/* Maximum length for path support by Windows file system */
#define PATH_MAX_LEN 260
#define K_SEM_OPER_TIMEOUT_MS 500
K_SEM_DEFINE(m_sem_sd_oper_ongoing, 1, 1);

void clock_initialization() { /* No-op for portable build */ }

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

    /* Minimal portable initialization */
    N_ButtonsInit();
    N_I2S_init();
    M_ArgvInit();

    /* Start DOOM */
    D_DoomMain();

    /* Idle forever */
    for (;;) {
        k_sleep(K_MSEC(SLEEP_TIME_MS));
    }

    return 0;
}
