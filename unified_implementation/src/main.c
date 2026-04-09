/*
 * Unified Zephyr main entrypoint.
 *
 * - On boards that enable FATFS+disk access, mounts the disk and lists files.
 * - On nRF5340, performs the known-good HFCLK/cache bring-up.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_FILE_SYSTEM) && defined(CONFIG_DISK_ACCESS) && \
    defined(CONFIG_FAT_FILESYSTEM_ELM)
#include <ff.h>
#include <zephyr/fs/fs.h>
#include <zephyr/storage/disk_access.h>
#endif

#if defined(CONFIG_SOC_NRF5340_CPUAPP)
#include <hal/nrf_cache.h>
#include <hal/nrf_clock.h>
#include <nrfx_clock.h>
#endif

#if defined(CONFIG_FEATURE_DOOM_BLE)
#include "bluetooth_control.h"
#endif
#if defined(CONFIG_FEATURE_DOOM_AUDIO)
#include "n_i2s.h"
#endif

LOG_MODULE_REGISTER(doom_main, CONFIG_DOOM_MAIN_LOG_LEVEL);

int no_sdcard = 1;

void D_DoomMain(void);
void M_ArgvInit(void);
void N_ButtonsInit(void);

#if defined(CONFIG_SOC_NRF5340_CPUAPP)
static void platform_clock_cache_init(void) {
    nrfx_clock_hfclk_start();
    nrf_clock_hfclk_div_set(NRF_CLOCK_S, NRF_CLOCK_HFCLK_DIV_1);
    nrfx_clock_divider_set(NRF_CLOCK_DOMAIN_HFCLK192M, NRF_CLOCK_HFCLK_DIV_1);

    NRF_CACHE_S->ENABLE = 1;
}
#else
static void platform_clock_cache_init(void) {}
#endif

#if defined(CONFIG_FILE_SYSTEM) && defined(CONFIG_DISK_ACCESS) && \
    defined(CONFIG_FAT_FILESYSTEM_ELM)
#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

static FATFS fat_fs;
static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .fs_data = &fat_fs,
};
static const char* disk_mount_pt = DISK_MOUNT_PT;

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

static void try_mount_disk(void) {
    mp.mnt_point = disk_mount_pt;

    int res = fs_mount(&mp);
    if (res == FR_OK) {
        printk("Disk mounted.\n");
        no_sdcard = 0;
        lsdir(disk_mount_pt);
    } else {
        printk("Error mounting disk (%d).\n", res);
    }
}
#else
static void try_mount_disk(void) { no_sdcard = 1; }
#endif

int main(void) {
    LOG_INF("BOARD STARTING %s", CONFIG_BOARD);

    platform_clock_cache_init();

#if defined(CONFIG_FEATURE_DOOM_SD)
    try_mount_disk();
#else
    no_sdcard = 1;
#endif

    N_ButtonsInit();
#if defined(CONFIG_FEATURE_DOOM_AUDIO)
    N_I2S_init();
#endif
    M_ArgvInit();

#if defined(CONFIG_FEATURE_DOOM_BLE)
    int err = bluetooth_control_init();
    if (err) {
        LOG_ERR("Bluetooth control initialization failed.");
        return 0;
    }
#endif

    D_DoomMain();

    for (;;) {
        k_sleep(K_FOREVER);
    }
}
