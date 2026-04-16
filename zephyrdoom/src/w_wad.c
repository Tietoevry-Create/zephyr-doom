//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      Handles WAD file header, directory, lump I/O.
//

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifndef SEGGER
#include <strings.h>
#endif

#include "doomtype.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "n_buttons.h"
#include "w_wad.h"
#include "z_zone.h"

typedef PACKED_STRUCT({
    char identification[4];
    int numlumps;
    int infotableofs;
}) wadinfo_t;

typedef PACKED_STRUCT({
    int filepos;
    int size;
    char name[8];
}) filelump_t;

#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#if defined(CONFIG_FILE_SYSTEM)
#include <zephyr/fs/fs.h>
#endif
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#if defined(CONFIG_DISK_ACCESS)
#include <zephyr/storage/disk_access.h>
#endif
#include <zephyr/sys/util.h>

#include "n_mem.h"
#include "n_qspi.h"

LOG_MODULE_REGISTER(w_wad, LOG_LEVEL_INF);

// --- QSPI Flash Configuration ---
#if DT_NODE_HAS_STATUS(DT_ALIAS(spi_flash0), okay)
#define FLASH_NODE DT_ALIAS(spi_flash0)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(mx25r64), okay)
#define FLASH_NODE DT_NODELABEL(mx25r64)
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(w25q64jvssiq), okay)
#define FLASH_NODE DT_NODELABEL(w25q64jvssiq)
#else
#error "Unsupported board: no supported external flash devicetree node found."
#endif

#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"

extern int no_sdcard;
#define MAX_NUMLUMPS 1300
unsigned short numlumps = 0;

filelump_t* filelumps;

static lumpindex_t* lumphash = NULL;
static lumpindex_t* lumpnext = NULL;

int first_lump_pos;

#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM) &&          \
    defined(CONFIG_FEATURE_DOOM_LEDS) && defined(CONFIG_SOC_NRF5340_CPUAPP) && \
    DT_HAS_ALIAS(led1)
#define WAD_LED_FLASH_ENABLED 1
static const struct gpio_dt_spec wad_led =
    GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static struct k_timer wad_led_timer;
static bool wad_led_init_done;
#else
#define WAD_LED_FLASH_ENABLED 0
#endif

static bool wad_header_valid(const wadinfo_t* header) {
    return !strncmp(header->identification, "IWAD", 4) ||
           !strncmp(header->identification, "PWAD", 4);
}

#if WAD_LED_FLASH_ENABLED
static void wad_led_timer_expiry(struct k_timer* timer) {
    ARG_UNUSED(timer);
    gpio_pin_toggle_dt(&wad_led);
}

static void wad_led_flash_start(void) {
    if (!device_is_ready(wad_led.port)) {
        return;
    }
    if (!wad_led_init_done) {
        if (gpio_pin_configure_dt(&wad_led, GPIO_OUTPUT_INACTIVE) != 0) {
            return;
        }
        k_timer_init(&wad_led_timer, wad_led_timer_expiry, NULL);
        wad_led_init_done = true;
    }
    k_timer_start(&wad_led_timer, K_NO_WAIT, K_MSEC(75));
}

static void wad_led_flash_stop(void) {
    if (!wad_led_init_done) {
        return;
    }
    k_timer_stop(&wad_led_timer);
    gpio_pin_set_dt(&wad_led, 0);
}
#else
static inline void wad_led_flash_start(void) {}
static inline void wad_led_flash_stop(void) {}
#endif

unsigned int W_LumpNameHash(const char* s) {
    // This is the djb2 string hash function, modded to work on strings
    // that have a maximum length of 8.

    unsigned int result = 5381;
    unsigned int i;

    for (i = 0; i < 8 && s[i] != '\0'; ++i) {
        result = ((result << 5) ^ result) ^ toupper(s[i]);
    }

    return result;
}

wad_file_t* W_AddFile(char* filename) {
    lumpindex_t i;
    wad_file_t* wad_file_data;
    boolean do_wad_transfer = false;

    const struct device* flash_dev = DEVICE_DT_GET(FLASH_NODE);
    if (!device_is_ready(flash_dev)) {
        LOG_ERR("Flash device %s is not ready", flash_dev->name);
        return NULL;
    }

    N_ReadButtons();
    I_Sleep(1);
    N_ReadButtons();

    if (N_ButtonState(3)) {
        do_wad_transfer = true;
    }

    if (numlumps != 0) {
        I_Error("Only one wad file supported\n");
    }

    printf("W_AddFile: Reading %s\n", filename);

#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
    struct fs_file_t fs_file;
    fs_file_t_init(&fs_file);

    if (!no_sdcard) {
        int rc = fs_open(&fs_file, filename, FS_O_READ);
        if (rc != 0) {
            printf(" couldn't open %s (err %d)\n", filename, rc);
            return NULL;
        }
    } else {
        printf("no_sdcard = 1 - skipping file open\n");
    }
#else
    no_sdcard = 1;
#endif

    wad_file_data = Z_Malloc(sizeof(wad_file_t), PU_STATIC, 0);

    if (strcasecmp(filename + strlen(filename) - 3, "wad")) {
        I_Error("NRFD-TODO: W_AddFile\n");
    } else {
        // Copy entire WAD file to Flash memory
        long file_size = 4196366;

        int num_blocks =
            (file_size + N_QSPI_BLOCK_SIZE - 1) / N_QSPI_BLOCK_SIZE;
        N_qspi_reserve_blocks(num_blocks);

#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
        if (!no_sdcard) {
            uint8_t* block_data = N_malloc(N_QSPI_BLOCK_SIZE);
            int block_loc = 0;
            boolean data_mismatch = do_wad_transfer;
            boolean led_flash_started = false;

            if (data_mismatch) {
                printf("Uploading WAD data to QSPI flash memory..");
                block_loc = 0;
                wad_led_flash_start();
                led_flash_started = true;

                for (i = 0; i < num_blocks; i++) {
                    printf("Copying block %d of %d\n", i, num_blocks);
                    int block_next = block_loc + N_QSPI_BLOCK_SIZE;
                    int block_size = block_next > file_size
                                         ? (file_size % N_QSPI_BLOCK_SIZE)
                                         : N_QSPI_BLOCK_SIZE;

                    // Read from SD card
                    fs_seek(&fs_file, block_loc, FS_SEEK_SET);
                    int bytes_read = fs_read(&fs_file, block_data, block_size);
                    if (bytes_read < 0) {
                        printf("Error reading file: %d\n", bytes_read);
                        if (led_flash_started) {
                            wad_led_flash_stop();
                        }
                        N_free(block_data);
                        fs_close(&fs_file);
                        return NULL;
                    }

                    printf("First 40b of block: \n");
                    int pt = 0;
                    int tmp = 0;
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 10; j++) {
                            printf("%x ", block_data[pt]);
                            pt++;
                        }
                        printf("| ");
                        for (int j = 0; j < 10; j++) {
                            if (block_data[tmp] >= 33 &&
                                block_data[tmp] <= 126) {
                                printf("%c ", block_data[tmp]);
                            } else {
                                printf(". ");
                            }
                            tmp++;
                        }
                        printf("\n");
                    }
                    printf("\n");

                    // Write to QSPI flash
                    off_t flash_offset = block_loc;

                    // Erase full 64KB block before writing (required: erase
                    // size aligned)
                    int rc =
                        flash_erase(flash_dev, flash_offset, N_QSPI_BLOCK_SIZE);
                    if (rc != 0) {
                        printf("Flash erase failed (err %d)\n", rc);
                        if (led_flash_started) {
                            wad_led_flash_stop();
                        }
                        N_free(block_data);
                        fs_close(&fs_file);
                        return NULL;
                    }

                    // Write data to flash
                    rc = flash_write(flash_dev, flash_offset, block_data,
                                     block_size);
                    if (rc != 0) {
                        printf("Flash write failed (err %d)\n", rc);
                        if (led_flash_started) {
                            wad_led_flash_stop();
                        }
                        N_free(block_data);
                        fs_close(&fs_file);
                        return NULL;
                    }

                    // Read back for verification
                    rc = flash_read(flash_dev, flash_offset, block_data,
                                    block_size);
                    if (rc != 0) {
                        printf("Flash read failed (err %d)\n", rc);
                        if (led_flash_started) {
                            wad_led_flash_stop();
                        }
                        N_free(block_data);
                        fs_close(&fs_file);
                        return NULL;
                    }

                    printf("First 40b of block in flash: \n");
                    pt = 0;
                    tmp = 0;
                    for (int i = 0; i < 4; i++) {
                        for (int j = 0; j < 10; j++) {
                            printf("%x ", block_data[pt]);
                            pt++;
                        }
                        printf("| ");
                        for (int j = 0; j < 10; j++) {
                            if (block_data[tmp] >= 33 &&
                                block_data[tmp] <= 126) {
                                printf("%c ", block_data[tmp]);
                            } else {
                                printf(". ");
                            }
                            tmp++;
                        }
                        printf("\n");
                    }

                    block_loc = block_next;
                }
                if (led_flash_started) {
                    wad_led_flash_stop();
                }
            }
            N_free(block_data);
#if defined(CONFIG_FEATURE_DOOM_SD) && defined(CONFIG_FILE_SYSTEM)
            fs_close(&fs_file);
#endif
        }
#endif /* CONFIG_FILE_SYSTEM */

        wadinfo_t existing_header = {0};
        uint8_t* xip_ptr = (uint8_t*)N_qspi_data_pointer(0);
        int header_rc =
            flash_read(flash_dev, 0, &existing_header, sizeof(existing_header));
        printf("Flash device: %s\n", flash_dev->name);
        if (header_rc != 0) {
            printf("Flash header read failed (err %d)\n", header_rc);
            printf("Flash header via XIP pointer: %x %x %x %x\n", xip_ptr[0],
                   xip_ptr[1], xip_ptr[2], xip_ptr[3]);
        } else if (!wad_header_valid(&existing_header)) {
            printf("Flash header invalid via flash_read: %x %x %x %x\n",
                   existing_header.identification[0],
                   existing_header.identification[1],
                   existing_header.identification[2],
                   existing_header.identification[3]);
            printf("Flash header via XIP pointer: %x %x %x %x\n", xip_ptr[0],
                   xip_ptr[1], xip_ptr[2], xip_ptr[3]);
        }

        // The WAD lives in the QSPI XIP mapping. Use that view for normal
        // reads because flash_read() fails on this device path.
        wadinfo_t* header_ptr = (wadinfo_t*)xip_ptr;

        printf("Dumping flash data: \n");
        int pt = 0;
        int tmp = 0;
        for (int i = 0; i < 30; i++) {
            for (int j = 0; j < 10; j++) {
                printf("%x ", xip_ptr[pt]);
                pt++;
            }
            printf("| ");
            for (int j = 0; j < 10; j++) {
                if (xip_ptr[tmp] >= 33 && xip_ptr[tmp] <= 126) {
                    printf("%c ", xip_ptr[tmp]);
                } else {
                    printf(". ");
                }
                tmp++;
            }
            printf("\n\n");
        }

        printf("Header: %x %x %x %x\n", header_ptr->identification[0],
               header_ptr->identification[1], header_ptr->identification[2],
               header_ptr->identification[3]);

        if (strncmp(header_ptr->identification, "IWAD", 4)) {
            // Homebrew levels?
            if (strncmp(header_ptr->identification, "PWAD", 4)) {
                I_Error("Wad file %s doesn't have IWAD or PWAD id\n", filename);
            }
        }

        header_ptr->numlumps = LONG(header_ptr->numlumps);

        // Vanilla Doom doesn't like WADs with more than 4046 lumps
        if (!strncmp(header_ptr->identification, "PWAD", 4) &&
            header_ptr->numlumps > 4046) {
            I_Error(
                "Error: Vanilla limit for lumps in a WAD is 4046, "
                "PWAD %s has %d",
                filename, header_ptr->numlumps);
        }

        header_ptr->infotableofs = LONG(header_ptr->infotableofs);

        printf("WAD header_ptr\n");
        printf("ID: %.4s\n", header_ptr->identification);
        printf("Num lumps: %d\n", header_ptr->numlumps);
        printf("Info table: %d\n", header_ptr->infotableofs);

        if (numlumps != 0) {
            I_Error("NRFD-TODO: Multiple WADs not supported yet\n");
        }

        if ((numlumps + header_ptr->numlumps) > MAX_NUMLUMPS) {
            I_Error("W_AddFile: MAX_NUMLUMPS reached\n");
        }

        first_lump_pos = header_ptr->infotableofs;
        filelumps = (filelump_t*)N_qspi_data_pointer(first_lump_pos);
        numlumps += header_ptr->numlumps;

        wad_file_data->path = filename;
        wad_file_data->length = file_size;
    }

    if (lumphash != NULL) {
        Z_Free(lumphash);
        lumphash = NULL;
    }
    if (lumpnext != NULL) {
        Z_Free(lumpnext);
        lumpnext = NULL;
    }

    return wad_file_data;
}

void* W_LumpDataPointer(lumpindex_t lump) {
    return N_qspi_data_pointer(LONG(filelumps[lump].filepos));
}

int W_NumLumps(void) { return numlumps; }

lumpindex_t W_CheckNumForName(const char* name) {
    lumpindex_t i;

    if (numlumps > 0 && lumphash != NULL && lumpnext != NULL) {
        lumpindex_t bucket = W_LumpNameHash(name) % numlumps;

        for (i = lumphash[bucket]; i >= 0; i = lumpnext[i]) {
            if (!strncasecmp(filelumps[i].name, name, 8)) {
                return i;
            }
        }

        return -1;
    }

    for (i = numlumps - 1; i >= 0; --i) {
        if (!strncasecmp(filelumps[i].name, name, 8)) {
            return i;
        }
    }

    return -1;
}

lumpindex_t W_GetNumForName(const char* name) {
    lumpindex_t i;

    i = W_CheckNumForName(name);

    if (i < 0) {
        I_Error("W_GetNumForName: %s not found!", name);
    }

    return i;
}

char* W_LumpName(lumpindex_t lump) { return filelumps[lump].name; }

int W_LumpLength(lumpindex_t lump) {
    if (lump >= numlumps) {
        I_Error("W_LumpLength: %i >= numlumps", lump);
    }

    return LONG(filelumps[lump].size);
}

void W_ReadLump(lumpindex_t lump, void* dest) {
    if (lump >= numlumps) {
        I_Error("W_ReadLump: %i >= numlumps", lump);
    }
    filelump_t* filelump = &filelumps[lump];
    void* ptr = N_qspi_data_pointer(LONG(filelump->filepos));
    memcpy(dest, ptr, LONG(filelump->size));
}

//
// W_CacheLumpNum
//
// Load a lump into memory and return a pointer to a buffer containing
// the lump data.
//
// 'tag' is the type of zone memory buffer to allocate for the lump
// (usually PU_STATIC or PU_CACHE).  If the lump is loaded as
// PU_STATIC, it should be released back using W_ReleaseLumpNum
// when no longer needed (do not use Z_ChangeTag).
//

void* W_CacheLumpNum(lumpindex_t lumpnum, int tag) {
    ARG_UNUSED(tag);

    if ((unsigned)lumpnum >= numlumps) {
        I_Error("W_CacheLumpNum: %i >= numlumps", lumpnum);
    }

    return W_LumpDataPointer(lumpnum);
}

void* W_CacheLumpName(char* name, int tag) {
    return W_CacheLumpNum(W_GetNumForName(name), tag);
}

//
// Release a lump back to the cache, so that it can be reused later
// without having to read from disk again, or alternatively, discarded
// if we run out of memory.
//
// Back in Vanilla Doom, this was just done using Z_ChangeTag
// directly, but now that we have WAD mmap, things are a bit more
// complicated ...
//

void W_ReleaseLumpNum(lumpindex_t lumpnum) {
    if ((unsigned)lumpnum >= numlumps) {
        I_Error("W_ReleaseLumpNum: %i >= numlumps", lumpnum);
    }
}

void W_ReleaseLumpName(char* name) { W_ReleaseLumpNum(W_GetNumForName(name)); }

void W_GenerateHashTable(void) {
    lumpindex_t i;

    if (numlumps <= 0) {
        return;
    }

    if (lumphash != NULL) {
        Z_Free(lumphash);
        lumphash = NULL;
    }
    if (lumpnext != NULL) {
        Z_Free(lumpnext);
        lumpnext = NULL;
    }

    lumphash = Z_Malloc(sizeof(lumpindex_t) * numlumps, PU_STATIC, 0);
    lumpnext = Z_Malloc(sizeof(lumpindex_t) * numlumps, PU_STATIC, 0);

    for (i = 0; i < numlumps; ++i) {
        lumphash[i] = -1;
        lumpnext[i] = -1;
    }

    for (i = 0; i < numlumps; ++i) {
        lumpindex_t bucket = W_LumpNameHash(filelumps[i].name) % numlumps;
        lumpnext[i] = lumphash[bucket];
        lumphash[bucket] = i;
    }
}

// The Doom reload hack. The idea here is that if you give a WAD file to -file
// prefixed with the ~ hack, that WAD file will be reloaded each time a new
// level is loaded. This lets you use a level editor in parallel and make
// incremental changes to the level you're working on without having to restart
// the game after every change.
// But: the reload feature is a fragile hack...
void W_Reload(void) { printf("NRFD-TODO: W_Reload\n"); }
