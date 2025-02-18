#ifndef __ILI_DISPLAY_H__
#define __ILI_DISPLAY_H__

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ili_screen_controller.h"

int set_background_color(const struct device *screen,
                         struct display_capabilities cap);
int draw_diagonal_line(const struct device *screen, uint8_t x, uint8_t y,
                       uint8_t nump);
int draw_lines(const struct device *screen, uint8_t x, uint8_t y, uint8_t dy,
               uint8_t w, uint8_t numl);
int draw_box(const struct device *screen, uint8_t x, uint8_t y, uint8_t w,
             uint8_t h);
int ili_do_stuff(void);

#endif