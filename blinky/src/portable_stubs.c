/*
 * Portable stubs for Nordic-specific functions removed in the minimal NXP
 * build. Provide no-op or simplified implementations so the Doom core links.
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "doomtype.h"
#include "i_sound.h"

LOG_MODULE_REGISTER(portable_stubs, LOG_LEVEL_INF);

/* Button / input replacements */
void N_ButtonsInit(void) {}
void N_ReadButtons(void) {}

/* Radio / joystick replacements */
int N_rjoy_init(void) { return 0; }
void N_rjoy_read(void) {}

/* Display replacements */
void N_display_spi_init(void) {}
void N_display_power_reset(void) {}
void N_display_spi_transfer_finish(void) {}
void N_display_spi_cmd(unsigned char b1, unsigned char b2) {}
void N_display_spi_wr8(unsigned int addr, unsigned char data) {}
void N_display_spi_wr16(unsigned int addr, unsigned short data) {}
void N_display_spi_wr32(unsigned int addr, unsigned int data) {}
void N_display_spi_wr(unsigned int addr, int dataSize, unsigned char* data) {}
unsigned char N_display_spi_rd8(unsigned int addr) { return 0; }
void N_display_init(void) {}
void N_display_wakeup(void) {}
unsigned int N_display_ram_alloc(size_t size) { return 0; }
void N_display_dlswap_frame(void) {}
void dl_start(void) {}
void dl(unsigned int cmd) {}
void dl_end(void) {}

/* Audio / I2S */
void N_I2S_init(void) {}

/* Filesystem / QSPI / memory stubs */
void N_qspi_wait(void) {}
void N_fs_init(void) {}

/* Hardfault hooks */
void N_hardfault_init(void) {}

/* Memory related stubs */
void N_mem_init(void) {}

/* SD / File listing fallback used in main */
/* Defined in main.c, so we don't need it here */
/*
int sd_card_list_files(char const* const path, char* buf, size_t* buf_size) {
    return -ENOTSUP;
}
*/

/* Bluetooth stub */
void bluetooth_main_xbox(void) {}

/* Missing stubs for linker errors */

/* Audio stubs */
static boolean N_SoundInit(boolean use_sfx_prefix) {
    LOG_INF("N_SoundInit called");
    return true;
}
static void N_SoundShutdown(void) {}
static int N_GetSfxLumpNum(sfxinfo_t* sfxinfo) { return sfxinfo->lumpnum; }
static void N_SoundUpdate(void) {}
static void N_UpdateSoundParams(int channel, int vol, int sep) {}
static int N_StartSound(sfxinfo_t* sfxinfo, int channel, int vol, int sep,
                        int pitch) {
    return 1;
}
static void N_StopSound(int channel) {}
static boolean N_SoundIsPlaying(int channel) { return false; }
static void N_CacheSounds(sfxinfo_t* sounds, int num_sounds) {}

sound_module_t sound_i2s_module = {
    .sound_devices = NULL,
    .num_sound_devices = 0,
    .Init = N_SoundInit,
    .Shutdown = N_SoundShutdown,
    .GetSfxLumpNum = N_GetSfxLumpNum,
    .Update = N_SoundUpdate,
    .UpdateSoundParams = N_UpdateSoundParams,
    .StartSound = N_StartSound,
    .StopSound = N_StopSound,
    .SoundIsPlaying = N_SoundIsPlaying,
    .CacheSounds = N_CacheSounds,
};

/* Input stubs */
int N_ButtonState(int idx) { return 0; }

/* QSPI / Memory stubs */
#define EXT_FLASH_BASE 0x80000000

void* N_malloc(size_t size) { return k_malloc(size); }
void N_qspi_reserve_blocks(int blocks) {}
void* N_qspi_data_pointer(int offset) {
    return (void*)(EXT_FLASH_BASE + offset);
}
void N_qspi_write_block(int offset, void* data) {}
void N_qspi_read(int offset, void* data, int size) {}
int N_qspi_alloc_block(void) { return 0; }
void N_qspi_erase_block(int offset) {}
