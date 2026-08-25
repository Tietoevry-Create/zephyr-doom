/* Native (native_sim) WAD + composite-texture storage backend.
 *
 * - The IWAD is embedded as a C array (doom_wad / doom_wad_len). Lump reads go
 *   through N_qspi_data_pointer(loc) -> &doom_wad[loc], as the XIP path expects.
 * - Composite wall textures are generated at runtime (r_data.c) and must be
 *   stored somewhere readable. On hardware that is pre-baked external flash;
 *   on native we provide a writable RAM arena. Offsets >= arena_base address
 *   the arena; offsets below address the embedded WAD. */

#include "n_qspi.h"

#include "i_system.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char doom_wad[];
extern const unsigned int  doom_wad_len;

static const uint8_t *wad_base;

/* Arena for generated composite textures. 4 MB is plenty for doom1 (shareware).
 * If the I_Error below fires, raise this. */
#define COMPOSITE_ARENA_SIZE (4u * 1024u * 1024u)
static uint8_t *arena;
static size_t   arena_base;   /* block-aligned offset just past the WAD */
static size_t   arena_next;   /* next block offset handed out by alloc_block */

static size_t round_up_block(size_t v) {
    return (v + (N_QSPI_BLOCK_SIZE - 1)) & ~((size_t)(N_QSPI_BLOCK_SIZE - 1));
}

void N_qspi_init(void) {
    wad_base = (const uint8_t *)doom_wad;

    if (arena == NULL) {
        arena = (uint8_t *)malloc(COMPOSITE_ARENA_SIZE);
        if (arena == NULL) {
            I_Error("N_qspi_native: failed to allocate composite arena");
        }
    }
    arena_base = round_up_block((size_t)doom_wad_len);
    arena_next = 0;
}

void *N_qspi_data_pointer(size_t loc) {
    if (loc >= arena_base) {
        size_t off = loc - arena_base;
        if (off >= COMPOSITE_ARENA_SIZE) {
            I_Error("N_qspi_native: composite read past arena (off=%u)",
                    (unsigned)off);
        }
        return (void *)(arena + off);
    }
    return (void *)(wad_base + loc);
}

void N_qspi_wait(void) {}

void N_qspi_read(size_t loc, void *buffer, size_t size) {
    memcpy(buffer, N_qspi_data_pointer(loc), size);
}

void N_qspi_write(size_t loc, void *buffer, size_t size) {
    if (loc >= arena_base) {
        size_t off = loc - arena_base;
        if (off + size > COMPOSITE_ARENA_SIZE) {
            I_Error("N_qspi_native: composite write past arena (off=%u size=%u)",
                    (unsigned)off, (unsigned)size);
        }
        memcpy(arena + off, buffer, size);
    }
    /* A write with loc < arena_base would target the read-only WAD; ignore. */
}

void N_qspi_erase_block(size_t loc) { (void)loc; }

void N_qspi_write_block(size_t loc, void *buffer, size_t size) {
    N_qspi_write(loc, buffer, size);
}

void N_qspi_reserve_blocks(size_t block_count) {
    arena_next += block_count * N_QSPI_BLOCK_SIZE;
}

size_t N_qspi_alloc_block(void) {
    size_t loc = arena_base + arena_next;
    arena_next += N_QSPI_BLOCK_SIZE;
    return loc;
}
