#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
#include <zephyr/fs/fs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct wad_sd_file {
#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
    struct fs_file_t file;
#endif
    bool is_open;
};

void wad_sd_init(struct wad_sd_file* sd);

/*
 * Open a file for reading from the SD card, if SD+FS support is enabled and
 * the system indicates the SD card is present.
 *
 * - If *no_sdcard is non-zero, prints the existing "skipping file open" message
 *   and returns 0 with sd->is_open = false.
 * - On open failure, prints the existing "couldn't open" message and returns
 *   the fs error code.
 * - If SD/FS is not enabled in the build, forces *no_sdcard = 1 and returns 0.
 */
int wad_sd_open_read(struct wad_sd_file* sd, const char* filename,
                     int* no_sdcard);

int wad_sd_seek_set(struct wad_sd_file* sd, off_t offset);

/* Returns number of bytes read, or a negative error code. */
int wad_sd_read(struct wad_sd_file* sd, void* buf, size_t len);

void wad_sd_close(struct wad_sd_file* sd);

static inline bool wad_sd_is_open(const struct wad_sd_file* sd) {
    return sd != NULL && sd->is_open;
}

#ifdef __cplusplus
}
#endif
