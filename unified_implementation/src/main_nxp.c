/*
 * FRDM-MCXN947 main entrypoint: minimal bring-up (WAD-in-flash + display +
 * buttons).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(doom_main, LOG_LEVEL_INF);

int no_sdcard = 1;

void D_DoomMain(void);
void M_ArgvInit(void);
void N_ButtonsInit(void);

int main(void) {
    printk("BOARD STARTING %s\n", CONFIG_BOARD);

    /* No SD / filesystem on FRDM by default */
    no_sdcard = 1;

    N_ButtonsInit();
    M_ArgvInit();

    D_DoomMain();

    for (;;) {
        k_sleep(K_FOREVER);
    }
}
