/*
 * Joystick backend wrapper.
 *
 * This project only needs the wired shield backend on FRDM-MCXN947.
 * The implementation lives in src/n_rjoy_shield.c.
 */

int n_rjoy_backend_init(void);
void n_rjoy_backend_read(void);

int N_rjoy_init(void) { return n_rjoy_backend_init(); }

void N_rjoy_read(void) { n_rjoy_backend_read(); }
