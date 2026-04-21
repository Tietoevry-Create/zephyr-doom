#include "wad_sd.h"

#include <errno.h>
#include <stdio.h>

void wad_sd_init(struct wad_sd_file* sd) {
    if (sd == NULL) {
        return;
    }
    sd->is_open = false;

#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
    fs_file_t_init(&sd->file);
#endif
}

int wad_sd_open_read(struct wad_sd_file* sd, const char* filename,
                     int* no_sdcard) {
    if (sd == NULL || filename == NULL || no_sdcard == NULL) {
        return -EINVAL;
    }

#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
    wad_sd_init(sd);

    if (*no_sdcard) {
        printf("no_sdcard = 1 - skipping file open\n");
        return 0;
    }

    int rc = fs_open(&sd->file, filename, FS_O_READ);
    if (rc != 0) {
        printf(" couldn't open %s (err %d)\n", filename, rc);
        sd->is_open = false;
        return rc;
    }

    sd->is_open = true;
    return 0;
#else
    *no_sdcard = 1;
    (void)sd;
    (void)filename;
    wad_sd_init(sd);
    return 0;
#endif
}

int wad_sd_seek_set(struct wad_sd_file* sd, off_t offset) {
#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
    if (!wad_sd_is_open(sd)) {
        return -ENOENT;
    }
    return fs_seek(&sd->file, offset, FS_SEEK_SET);
#else
    (void)sd;
    (void)offset;
    return -ENOTSUP;
#endif
}

int wad_sd_read(struct wad_sd_file* sd, void* buf, size_t len) {
#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
    if (!wad_sd_is_open(sd)) {
        return -ENOENT;
    }
    return fs_read(&sd->file, buf, len);
#else
    (void)sd;
    (void)buf;
    (void)len;
    return -ENOTSUP;
#endif
}

void wad_sd_close(struct wad_sd_file* sd) {
#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
    if (sd == NULL) {
        return;
    }
    if (sd->is_open) {
        (void)fs_close(&sd->file);
        sd->is_open = false;
    }
#else
    (void)sd;
#endif
}
