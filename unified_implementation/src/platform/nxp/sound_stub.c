/*
 * FRDM minimal build: provide a stub sound module so i_sound.c links,
 * but keep audio effectively disabled.
 */

#include "i_sound.h"

static boolean stub_init(boolean use_sfx_prefix) {
    (void)use_sfx_prefix;
    return false;
}
static void stub_shutdown(void) {}
static int stub_get_sfx_lump(sfxinfo_t* sfxinfo) {
    (void)sfxinfo;
    return 0;
}
static void stub_update(void) {}
static void stub_update_params(int channel, int vol, int sep) {
    (void)channel;
    (void)vol;
    (void)sep;
}
static int stub_start_sound(sfxinfo_t* sfxinfo, int channel, int vol, int sep,
                            int pitch) {
    (void)sfxinfo;
    (void)channel;
    (void)vol;
    (void)sep;
    (void)pitch;
    return 0;
}
static void stub_stop_sound(int channel) { (void)channel; }
static boolean stub_is_playing(int channel) {
    (void)channel;
    return false;
}
static void stub_cache_sounds(sfxinfo_t* sounds, int num_sounds) {
    (void)sounds;
    (void)num_sounds;
}

static snddevice_t stub_devices[] = {SNDDEVICE_SB};

sound_module_t sound_i2s_module = {
    stub_devices,       1,
    stub_init,          stub_shutdown,
    stub_get_sfx_lump,  stub_update,
    stub_update_params, stub_start_sound,
    stub_stop_sound,    stub_is_playing,
    stub_cache_sounds,
};
