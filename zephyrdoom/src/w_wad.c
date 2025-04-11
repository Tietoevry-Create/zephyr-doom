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
#include <stdlib.h>
#include <string.h>
#ifndef SEGGER
#include <strings.h>
#endif

#include "doomtype.h"

#include "i_timer.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_video.h"
#include "m_misc.h"
#include "v_diskicon.h"
#include "z_zone.h"

#include "w_wad.h"

#include "n_buttons.h"

typedef PACKED_STRUCT (
{
    // Should be "IWAD" or "PWAD".
    char                identification[4];
    int                 numlumps;
    int                 infotableofs;
}) wadinfo_t;


typedef PACKED_STRUCT (
{
    int                 filepos;
    int                 size;
    char                name[8];
}) filelump_t;


#include "n_fs.h"
#include "n_qspi.h"
#include "n_mem.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h> // For MIN
#include <string.h>        // For strncmp, strlen, strcasecmp, memcmp
#include <stdlib.h>        // For Z_Malloc (assuming this is like malloc) - replaced with k_malloc

// --- Required Includes from both examples ---
#include <zephyr/storage/disk_access.h> // If disk init/mount is done elsewhere

LOG_MODULE_REGISTER(w_wad, LOG_LEVEL_INF);

// --- QSPI Flash Configuration ---
#define FLASH_NODE DT_ALIAS(spi_flash0)

#if !DT_NODE_HAS_STATUS(FLASH_NODE, okay)
#error "Unsupported board: spi_flash0 devicetree alias is not defined or disabled."
#endif

// --- Filesystem Configuration (Assuming FATFS as in the first example) ---
// Adapt if using EXT2 or another filesystem
#define DISK_DRIVE_NAME "SD"
#define DISK_MOUNT_PT "/" DISK_DRIVE_NAME ":"


extern int no_sdcard; //NRFD-NOTE: from main.c
//
// GLOBALS
//

// wad_file_t *wad_file = NULL; // NRFD

// Location of each lump on disk.
#define MAX_NUMLUMPS 1300
// lumpinfo_t lumpinfo[MAX_NUMLUMPS];
unsigned short numlumps = 0;

filelump_t *filelumps;

// Hash table for fast lookups
static lumpindex_t *lumphash = NULL;

N_FILE wad_file;
int first_lump_pos;

/*
int debugLumpCount = 0;
int debugLumpNums[12];
void *debugLumpCache[12];
*/

// Variables for the reload hack: filename of the PWAD to reload, and the
// lumps from WADs before the reload file, so we can resent numlumps and
// load the file again.
/* NRFD-EXCLUDE
static wad_file_t *reloadhandle = NULL;
static lumpinfo_t *reloadlumps = NULL;
static char *reloadname = NULL;
static int reloadlump = -1;
*/

// Hash function used for lump names.
unsigned int W_LumpNameHash(const char *s)
{
    // This is the djb2 string hash function, modded to work on strings
    // that have a maximum length of 8.

    unsigned int result = 5381;
    unsigned int i;

    for (i=0; i < 8 && s[i] != '\0'; ++i)
    {
        result = ((result << 5) ^ result ) ^ toupper(s[i]);
    }

    return result;
}

wad_file_t *W_AddFile (char *filename)
{
    // wadinfo_t header; // This was read via direct pointer later
    // lumpindex_t i; // loop counter 'i' redefined below
    wad_file_t *wad_file_data = NULL;
    int rc = 0; // Result code

    struct fs_file_t sd_file;
    struct fs_dirent file_entry;
    off_t file_size = 0;

    const struct device *flash_dev = DEVICE_DT_GET(FLASH_NODE);
    struct flash_pages_info page_info;
    size_t flash_erase_block_size = 0; // Typically sector size
    uint8_t *transfer_buffer = NULL;
    size_t buffer_size = 0; // Will be set to flash_erase_block_size

    boolean do_wad_transfer = false; // Keep original transfer logic trigger

    // --- Check Flash Device ---
    if (!device_is_ready(flash_dev)) {
        LOG_ERR("Flash device %s is not ready", flash_dev->name);
        return NULL;
    }
    LOG_INF("Flash device %s found and ready.", flash_dev->name);

    // --- Get Flash Erase Block Size ---
    // We need this to allocate buffer and perform erases efficiently
    rc = flash_get_page_info_by_offs(flash_dev, 0, &page_info);
    if (rc != 0) {
        LOG_ERR("Failed to get flash page info (err %d)", rc);
        return NULL;
    }
    flash_erase_block_size = page_info.size;
    if (flash_erase_block_size == 0) {
         LOG_ERR("Invalid flash erase block size (0). Check driver/DT.");
         return NULL;
    }
    buffer_size = flash_erase_block_size; // Use erase block size for buffer
    LOG_INF("Flash erase block (sector) size: %zu bytes", flash_erase_block_size);


    // --- Original Button Check Logic (Optional) ---
    N_ReadButtons();
    I_Sleep(1);
    N_ReadButtons();
    if (N_ButtonState(0)) {
        do_wad_transfer = true;
        LOG_INF("Button pressed, forcing WAD transfer to QSPI flash.");
    } else {
        do_wad_transfer = false;
    }


    // --- Check for existing WADs (Original Logic) ---
     if (numlumps != 0) {
         // This error suggests the original system only loaded one WAD at a time directly into memory/flash map.
         // Depending on how you manage lumpinfo, this might still be relevant.
         I_Error("W_AddFile: Only one WAD file supported in this configuration.\n");
         // If you adapt lumpinfo management, you might remove this check.
         return NULL; // Or handle appending lumps if logic is changed
     }

    LOG_INF("W_AddFile: Processing %s", filename);

    // --- Open File on SD Card ---
    fs_file_t_init(&sd_file);
    rc = fs_open(&sd_file, filename, FS_O_READ);
    if (rc < 0) {
        LOG_ERR("Couldn't open SD file %s (err %d)", filename, rc);
        return NULL;
    }
    LOG_INF("Opened SD file: %s", filename);

    // --- Get File Size ---
    rc = fs_stat(filename, &file_entry);
    if (rc < 0) {
        LOG_ERR("Couldn't get file stats for %s (err %d)", filename, rc);
        fs_close(&sd_file);
        return NULL;
    }
    file_size = file_entry.size;
    if (file_size <= 0) {
         LOG_ERR("File %s is empty or size could not be determined.", filename);
         fs_close(&sd_file);
         return NULL;
    }
    LOG_INF("File size: %lld bytes", (long long)file_size);


    // --- Allocate WAD metadata structure ---
    // Using Z_Malloc as in original, assuming it maps to k_malloc or similar
    wad_file_data = Z_Malloc(sizeof(wad_file_t), PU_STATIC, 0);
    if (!wad_file_data) {
        LOG_ERR("Failed to allocate memory for wad_file_data");
        fs_close(&sd_file);
        return NULL;
    }
    // Initialize basic wad_file_data info
    wad_file_data->path = filename; // Note: filename pointer might not be valid long-term if it's transient
    wad_file_data->length = (long)file_size; // Store file size


    // --- Check if it's a WAD file (Original Logic) ---
    // This check happens before transfer in original, keep it here.
    if (strcasecmp(filename + strlen(filename) - 3, "wad"))
    {
        // Original code handled single lump files differently.
        // This example focuses on WAD transfer. Add single lump handling if needed.
        LOG_ERR("File %s does not have '.wad' extension. Single lump files not handled in this example.", filename);
        Z_Free(wad_file_data); // Free metadata allocated earlier
        fs_close(&sd_file);
        return NULL; // Or implement single lump logic
    }
    else // It's a WAD file, proceed with potential transfer
    {
        // --- Allocate Transfer Buffer ---
        transfer_buffer = k_malloc(buffer_size);
        if (!transfer_buffer) {
            LOG_ERR("Failed to allocate transfer buffer (%zu bytes)", buffer_size);
            Z_Free(wad_file_data);
            fs_close(&sd_file);
            return NULL;
        }
        LOG_INF("Transfer buffer allocated (%zu bytes).", buffer_size);

        // --- Transfer Data if Needed ---
        // Original code had a check for data mismatch here.
        // We use the 'do_wad_transfer' flag determined earlier.
        if (do_wad_transfer) {
            LOG_INF("Starting WAD transfer from SD card to QSPI flash...");

            off_t current_flash_offset = 0;
            size_t bytes_remaining = file_size;
            size_t bytes_transferred = 0;
            uint8_t *verify_buffer = k_malloc(buffer_size);
            
            if (!verify_buffer) {
                LOG_ERR("Failed to allocate verification buffer");
                rc = -ENOMEM;
                goto transfer_cleanup;
            }

            while (bytes_remaining > 0) {
                size_t bytes_to_read = MIN(bytes_remaining, buffer_size);
                ssize_t bytes_read;

                // --- Read block from SD Card ---
                bytes_read = fs_read(&sd_file, transfer_buffer, bytes_to_read);
                if (bytes_read < 0) {
                    LOG_ERR("Failed to read from SD file %s (err %d)", filename, (int)bytes_read);
                    rc = (int)bytes_read;
                    k_free(verify_buffer);
                    goto transfer_cleanup; // Go to cleanup section
                }
                if (bytes_read == 0) {
                    // Should not happen if file_size > 0 and bytes_remaining > 0
                    LOG_ERR("Unexpected end of file reached for %s", filename);
                    rc = -1; // Input/output error
                    k_free(verify_buffer);
                    goto transfer_cleanup;
                }

                // Print first 40 bytes of the buffer (or less if buffer is smaller)
                LOG_INF("First bytes of transfer buffer:");
                for (int i = 0; i < MIN(40, bytes_read); i++) {
                    printk("%02X ", transfer_buffer[i]);
                    if ((i + 1) % 8 == 0) printk(" ");
                    if ((i + 1) % 16 == 0) printk("\n");
                }
                printk("\n");

                // Read existing data from flash to verify if it's the same
                rc = flash_read(flash_dev, current_flash_offset, verify_buffer, bytes_read);
                if (rc != 0) {
                    LOG_ERR("Failed to read from flash at offset %lld (err %d)", (long long)current_flash_offset, rc);
                    k_free(verify_buffer);
                    goto transfer_cleanup;
                }

                // Compare data before writing
                if (memcmp(transfer_buffer, verify_buffer, bytes_read) == 0) {
                    LOG_INF("Data at offset %lld already matches - skipping write", (long long)current_flash_offset);
                } else {
                    LOG_INF("Data mismatch at offset %lld - erasing and writing", (long long)current_flash_offset);

                    // --- Erase Necessary Flash Sector(s) BEFORE Writing ---
                    rc = flash_erase(flash_dev, current_flash_offset, flash_erase_block_size);
                    if (rc != 0) {
                        LOG_ERR("Failed to erase flash at offset %lld (err %d)", (long long)current_flash_offset, rc);
                        k_free(verify_buffer);
                        goto transfer_cleanup;
                    }

                    // --- Write block to QSPI Flash ---
                    rc = flash_write(flash_dev, current_flash_offset, transfer_buffer, bytes_read);
                    if (rc != 0) {
                        LOG_ERR("Failed to write to flash at offset %lld (err %d)", (long long)current_flash_offset, rc);
                        k_free(verify_buffer);
                        goto transfer_cleanup;
                    }
                }

                // --- Update counters ---
                bytes_remaining -= bytes_read;
                current_flash_offset += bytes_read;
                bytes_transferred += bytes_read;

                // Optional: Add progress indicator
                LOG_INF("Transferred %zu / %lld bytes...", bytes_transferred, (long long)file_size);
            }

            k_free(verify_buffer);

            LOG_INF("WAD transfer completed successfully (%lld bytes written).", (long long)bytes_transferred);

        } else {
             LOG_INF("Skipping WAD transfer to QSPI flash (do_wad_transfer is false). Assuming existing data is valid.");
             // If skipping, the rest of the function assumes flash already holds the correct data.
        }


transfer_cleanup: // Label for cleanup after transfer attempt
        k_free(transfer_buffer); // Free buffer regardless of success/failure of transfer
        transfer_buffer = NULL; // Avoid double free

        if (rc != 0) { // If an error occurred during transfer loop
             LOG_ERR("WAD transfer failed with error %d.", rc);
             Z_Free(wad_file_data);
             fs_close(&sd_file);
             return NULL;
        }

        // --- IMPORTANT: WAD Parsing Logic Needs Rework ---
        // The following code from the original W_AddFile relies on getting
        // direct pointers into the QSPI flash memory (N_qspi_data_pointer).
        // The standard Zephyr flash API (flash_read) does NOT provide this.
        // You MUST rewrite this section to:
        // 1. Allocate memory buffers (e.g., using k_malloc).
        // 2. Use flash_read() to copy data from flash (header, lump directory)
        //    into these buffers.
        // 3. Parse the data from the memory buffers.
        // 4. Adjust lumpinfo setup to store data read from flash, or pointers
        //    to allocated buffers containing the data, rather than direct flash pointers.

        LOG_WRN("------ Starting WAD parsing section - Requires rework! ------");
        LOG_WRN("Original code used direct flash pointers (N_qspi_data_pointer).");
        LOG_WRN("You must replace N_qspi_data_pointer calls with flash_read() into buffers.");

        // --- Start of original WAD parsing logic (NEEDS REWORK) ---

        // Example: Reading header (REWORK NEEDED)
        wadinfo_t header_buffer; // Allocate a buffer on stack or heap
        rc = flash_read(flash_dev, 0, &header_buffer, sizeof(header_buffer));
        if (rc != 0) {
            LOG_ERR("Failed to read WAD header from flash (err %d)", rc);
            Z_Free(wad_file_data);
            fs_close(&sd_file);
            return NULL;
        }
        // Now use header_buffer instead of header_ptr
        wadinfo_t *header_ptr = &header_buffer; // Pointer to buffer for compatibility with original code style

        // Original debug print (uses buffer now)
        LOG_INF("Header ID read from flash: %.4s", header_ptr->identification);

        // Original WAD identification check (uses buffer now)
        if (strncmp(header_ptr->identification,"IWAD",4) && strncmp(header_ptr->identification,"PWAD",4)) {
             LOG_ERR("Wad file %s doesn't have IWAD or PWAD id (read from flash)", filename);
             I_Error("Wad file %s doesn't have IWAD or PWAD id\n", filename); // Original error call
             Z_Free(wad_file_data);
             fs_close(&sd_file);
             return NULL; // Or handle appropriately
        }

        // Convert fields from Little Endian (as they are stored in WAD) to CPU format
        header_ptr->numlumps = LONG(header_ptr->numlumps);
        header_ptr->infotableofs = LONG(header_ptr->infotableofs);

        // Original header info print
        LOG_INF("WAD Header (from flash buffer):");
        LOG_INF("  ID: %.4s", header_ptr->identification);
        LOG_INF("  Num lumps: %d", header_ptr->numlumps);
        LOG_INF("  Info table offset: %d", header_ptr->infotableofs);


        // Example: Setting up lump directory pointer (REWORK NEEDED)
        // Original: filelumps = (filelump_t*)N_qspi_data_pointer(header_ptr->infotableofs);
        // Rework: You need to calculate the size of the lump directory, allocate a buffer,
        //         and use flash_read to load the directory into that buffer.
        size_t dir_size = header_ptr->numlumps * sizeof(filelump_t);
        filelump_t *lump_dir_buffer = k_malloc(dir_size);
        if (!lump_dir_buffer) {
             LOG_ERR("Failed to allocate buffer for lump directory (%zu bytes)", dir_size);
             Z_Free(wad_file_data);
             fs_close(&sd_file);
             return NULL;
        }
        rc = flash_read(flash_dev, header_ptr->infotableofs, lump_dir_buffer, dir_size);
         if (rc != 0) {
             LOG_ERR("Failed to read lump directory from flash (err %d)", rc);
             k_free(lump_dir_buffer);
             Z_Free(wad_file_data);
             fs_close(&sd_file);
             return NULL;
         }
         LOG_INF("Lump directory read from flash into buffer at %p", lump_dir_buffer);

        // Point the global/static filelumps to the buffer *for this load*.
        // Consider how multiple WADs or dynamic loading affects this if filelumps is truly global.
        filelumps = lump_dir_buffer; // DANGER: If W_AddFile is called again, this buffer needs managing/freeing.
                                     // A better approach might involve dynamic allocation within lumpinfo itself.

        // Update global numlumps count
        numlumps += header_ptr->numlumps; // Assuming numlumps was 0 initially based on earlier check

        // --- Loop through lumps (original logic adapted for buffer) ---
        // The original code iterated and set up lumpinfo[]. The critical change
        // is that lump_p->cache CANNOT be a direct flash pointer. It should either be
        // NULL (requiring W_CacheLumpNum to read from flash on demand) or point to
        // data pre-cached from flash into RAM (if memory permits).

        LOG_WRN("Lump setup needs review: lumpinfo[].cache cannot be a direct flash pointer.");
        // Example - Setting cache to NULL (on-demand loading needed later):
        /*
        for (int i = 0; i < header_ptr->numlumps; i++) {
            lumpinfo_t *lump_p = &lumpinfo[startlump + i]; // Assuming startlump is 0 here
            filelump_t *filelump_entry = &filelumps[i]; // From the buffer we read

            lump_p->position = LONG(filelump_entry->filepos); // Store flash offset
            lump_p->size = LONG(filelump_entry->size);
            strncpy(lump_p->name, filelump_entry->name, 8);
            lump_p->name[8] = '\0'; // Ensure null termination

            lump_p->cache = NULL; // *** CRITICAL: No direct pointer possible ***
                                  // W_CacheLumpNum must implement flash_read using lump_p->position

            // LOG_INF("Lump %.8s: size %d, flash_offset %d", lump_p->name, lump_p->size, lump_p->position);
        }
        */
        LOG_WRN("--- End of WAD parsing section needing rework ---");

        // --- End of original WAD parsing logic ---
    }

    // --- Cleanup and Final Steps ---
    fs_close(&sd_file); // Close the file handle

    // Original lumphash cleanup
    if (lumphash != NULL)
    {
        // Assuming Z_Free maps to k_free or similar
        Z_Free(lumphash);
        lumphash = NULL;
    }

    LOG_INF("W_AddFile completed for %s", filename);
    return wad_file_data; // Return metadata pointer
}


void *W_LumpDataPointer(lumpindex_t lump)
{
    return N_qspi_data_pointer(LONG(filelumps[lump].filepos));
}

//
// W_NumLumps
//
int W_NumLumps (void)
{
    return numlumps;
}

//
// W_CheckNumForName
// Returns -1 if name not found.
//

lumpindex_t W_CheckNumForName(const char* name)
{
    lumpindex_t i;

    // Do we have a hash table yet?

    /* NRFD-TODO: lump hash table */
    /*
    if (lumphash != NULL)
    {
        int hash;

        // We do! Excellent.

        hash = W_LumpNameHash(name) % numlumps;

        for (i = lumphash[hash]; i != -1; i = lumpinfo[i]->next)
        {
            if (!strncasecmp(lumpinfo[i]->name, name, 8))
            {
                return i;
            }
        }
    }
    else*/
    {
        // We don't have a hash table generate yet. Linear search :-(
        //
        // scan backwards so patch lump files take precedence

        for (i = numlumps - 1; i >= 0; --i)
        {
            if (!strncasecmp(filelumps[i].name, name, 8))
            // if (!strncasecmp(lumpinfo[i].name, name, 8))
            {
                return i;
            }
        }
    }

    // TFB. Not found.

    return -1;
}

//
// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
lumpindex_t W_GetNumForName(const char* name)
{
    lumpindex_t i;

    i = W_CheckNumForName (name);

    if (i < 0)
    {
        I_Error ("W_GetNumForName: %s not found!", name);
    }
 
    return i;
}

char *W_LumpName(lumpindex_t lump)
{
    return filelumps[lump].name;
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//
int W_LumpLength(lumpindex_t lump)
{
    if (lump >= numlumps)
    {
        I_Error ("W_LumpLength: %i >= numlumps", lump);
    }

    // return lumpinfo[lump].size;
    return LONG(filelumps[lump].size);
}



//
// W_ReadLump
// Loads the lump into the given buffer,
//  which must be >= W_LumpLength().
//
void W_ReadLump(lumpindex_t lump, void *dest)
{
    if (lump >= numlumps)
    {
        I_Error ("W_ReadLump: %i >= numlumps", lump);
    }

    // lumpinfo_t *l;
    // l = &lumpinfo[lump];
    // printf("W_ReadLump(dummy): %.8s\n", l->name);

    // V_BeginRead(l->size);
    filelump_t *filelump = &filelumps[lump];
    void *ptr = N_qspi_data_pointer(LONG(filelump->filepos));
    memcpy(dest, ptr, LONG(filelump->size));

    // memcpy(dest, l->cache, l->size);

    /* NRFD-EXCLUDE

    // int c;
    // printf("Read lump at %d with size %d to %X\n", l->position, l->size, (unsigned int)(dest));
    // c = W_Read(l->wad_file, l->position, dest, l->size);
    // c = W_Read(wad_file, l->position, dest, l->size);

    if (c < l->size)
    {
        I_Error("W_ReadLump: only read %i of %i on lump %i",
                c, l->size, lump);
    }*/
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

void *W_CacheLumpNum(lumpindex_t lumpnum, int tag)
{
    byte *result;
    // lumpinfo_t *lump;

    if ((unsigned)lumpnum >= numlumps)
    {
        I_Error ("W_CacheLumpNum: %i >= numlumps", lumpnum);
    }

    // lump = &lumpinfo[lumpnum];

    // Get the pointer to return.  If the lump is in a Memory-mapped
    // file, we can just return a pointer to within the memory-mapped
    // region.  If the lump is in an ordinary file, we may already
    // have it cached; otherwise, load it into memory.

    // result = lump->cache;
    /*
    for (int i=0;i<debugLumpCount;i++) {
        if (debugLumpNums[i]==lumpnum) {
            byte *cache = debugLumpCache[i];
            byte *qspi_data = W_LumpDataPointer(lumpnum);
            for (int j=0;j<filelumps[lumpnum].size;j++) {
                if (cache[j] != qspi_data[j]) {
                    printf("X");
                }
            }
            return cache;
        }
    }
    */
    result = W_LumpDataPointer(lumpnum);
    // N_ldbg("W_CacheLumpNum: %.8s\n", lump->name);

    /* NRFD-EXCLUDE:
    if (lump->wad_file->mapped != NULL)
    {
        // Memory mapped file, return from the mmapped region.

        result = lump->wad_file->mapped + lump->position;
    }
    else if (lump->cache != NULL)
    {
        // Already cached, so just switch the zone tag.

        result = lump->cache;
        Z_ChangeTag(lump->cache, tag);
    }
    else
    {
        // Not yet loaded, so load it now
        lump->cache = Z_Malloc(W_LumpLength(lumpnum), tag, &lump->cache);
        W_ReadLump (lumpnum, lump->cache);
        result = lump->cache;
    }
    */

    return result;
}



//
// W_CacheLumpName
//
void *W_CacheLumpName(char *name, int tag)
{
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

void W_ReleaseLumpNum(lumpindex_t lumpnum)
{

    if ((unsigned)lumpnum >= numlumps)
    {
        I_Error ("W_ReleaseLumpNum: %i >= numlumps", lumpnum);
    }

    /* NRFD-EXCLUDE

    lumpinfo_t *lump;
    lump = &lumpinfo[lumpnum];

    if (lump->wad_file->mapped != NULL)
    {
        // Memory-mapped file, so nothing needs to be done here.
    }
    else
    {
        Z_ChangeTag(lump->cache, PU_CACHE);
    }
    */
}

void W_ReleaseLumpName(char *name)
{
    W_ReleaseLumpNum(W_GetNumForName(name));
}

#if 0

//
// W_Profile
//
int             info[2500][10];
int             profilecount;

void W_Profile (void)
{
    int         i;
    memblock_t* block;
    void*       ptr;
    char        ch;
    FILE*       f;
    int         j;
    char        name[9];
        
        
    for (i=0 ; i<numlumps ; i++)
    {   
        ptr = lumpinfo[i].cache;
        if (!ptr)
        {
            ch = ' ';
            continue;
        }
        else
        {
            block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
            if (block->tag < PU_PURGELEVEL)
                ch = 'S';
            else
                ch = 'P';
        }
        info[i][profilecount] = ch;
    }
    profilecount++;
        
    f = fopen ("waddump.txt","w");
    name[8] = 0;

    for (i=0 ; i<numlumps ; i++)
    {
        memcpy (name,lumpinfo[i].name,8);

        for (j=0 ; j<8 ; j++)
            if (!name[j])
                break;

        for ( ; j<8 ; j++)
            name[j] = ' ';

        fprintf (f,"%s ",name);

        for (j=0 ; j<profilecount ; j++)
            fprintf (f,"    %c",info[i][j]);

        fprintf (f,"\n");
    }
    fclose (f);
}


#endif

// Generate a hash table for fast lookups

void W_GenerateHashTable(void)
{
    lumpindex_t i;
    printf("NRDF-TODO? W_GenerateHashTable\n");

    /*
    // Free the old hash table, if there is one:
    if (lumphash != NULL)
    {
        Z_Free(lumphash);
    }

    // Generate hash table
    if (numlumps > 0)
    {
        lumphash = Z_Malloc(sizeof(lumpindex_t) * numlumps, PU_STATIC, NULL);

        for (i = 0; i < numlumps; ++i)
        {
            lumphash[i] = -1;
        }

        for (i = 0; i < numlumps; ++i)
        {
            unsigned int hash;

            hash = W_LumpNameHash(lumpinfo[i]->name) % numlumps;

            // Hook into the hash table

            lumpinfo[i]->next = lumphash[hash];
            lumphash[hash] = i;
        }
    }
    */

    // All done!
}

// The Doom reload hack. The idea here is that if you give a WAD file to -file
// prefixed with the ~ hack, that WAD file will be reloaded each time a new
// level is loaded. This lets you use a level editor in parallel and make
// incremental changes to the level you're working on without having to restart
// the game after every change.
// But: the reload feature is a fragile hack...
void W_Reload(void)
{
    /* NRFD-EXCLUDE
    char *filename;
    lumpindex_t i;

    if (reloadname == NULL)
    {
        return;
    }

    // We must free any lumps being cached from the PWAD we're about to reload:
    for (i = reloadlump; i < numlumps; ++i)
    {
        if (lumpinfo[i]->cache != NULL)
        {
            Z_Free(lumpinfo[i]->cache);
        }
    }

    // Reset numlumps to remove the reload WAD file:
    numlumps = reloadlump;

    // Now reload the WAD file.
    filename = reloadname;

    W_CloseFile(reloadhandle);
    free(reloadlumps);

    reloadname = NULL;
    reloadlump = -1;
    reloadhandle = NULL;
    W_AddFile(filename);
    free(filename);

    // The WAD directory has changed, so we have to regenerate the
    // fast lookup hashtable:
    W_GenerateHashTable();
    */
}

/*
void W_DebugLump(int lump)
{
    return;
    if (lump != 561) return;

    filelump_t *filelump = &filelumps[lump];

    printf("W_DebugLump: %d size: %d\n", lump, filelump->size);

    byte *cache = N_malloc(filelump->size);
    
    // N_fs_read(wad_file, filelump->filepos, cache, filelump->size);
    byte *qspi_data = W_LumpDataPointer(lump);
    for (int i=0;i<filelump->size;i++) {
        cache[i] = qspi_data[i];
    }

    debugLumpNums[debugLumpCount] = lump;
    debugLumpCache[debugLumpCount] = cache;
    debugLumpCount++;
}
*/