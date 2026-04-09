#include "n_display.h"

/*
 * Display stub backend.
 *
 * Used when CONFIG_FEATURE_DOOM_DISPLAY is disabled.
 *
 * Semantics (Option A): Doom continues to render internally, but no display
 * controller is initialized and no SPI transfers are performed.
 */

static uint32_t fake_ram_ptr;

void N_display_spi_init(void) {}

void N_display_power_reset(void) {}

void N_display_spi_transfer_finish(void) {}

void N_display_spi_cmd(uint8_t b1, uint8_t b2) {
    (void)b1;
    (void)b2;
}

void N_display_spi_wr8(uint32_t addr, uint8_t data) {
    (void)addr;
    (void)data;
}

void N_display_spi_wr16(uint32_t addr, uint16_t data) {
    (void)addr;
    (void)data;
}

void N_display_spi_wr32(uint32_t addr, uint32_t data) {
    (void)addr;
    (void)data;
}

void N_display_spi_wr(uint32_t addr, int dataSize, uint8_t* data) {
    (void)addr;
    (void)dataSize;
    (void)data;
}

uint8_t N_display_spi_rd8(uint32_t addr) {
    (void)addr;
    return 0;
}

void N_display_init(void) { fake_ram_ptr = 0; }

void N_display_wakeup(void) {}

uint32_t N_display_ram_alloc(size_t size) {
    /* Provide deterministic, aligned “RAM” offsets for FT810 command building.
     */
    uint32_t addr = fake_ram_ptr;
    fake_ram_ptr += (uint32_t)((size + 3u) & ~3u);
    return addr;
}

void N_display_dlswap_frame(void) {}

void dl_start(void) {}

void dl(uint32_t cmd) { (void)cmd; }

void dl_end(void) {}
