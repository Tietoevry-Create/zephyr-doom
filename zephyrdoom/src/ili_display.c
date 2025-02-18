/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ili_display.h"

#include "ili_screen_controller.h"

LOG_MODULE_REGISTER(Lesson5_Exercise2, LOG_LEVEL_INF);

#define BKGCOLOR 0x0000U   // 16-bit RGB value for background
#define HLINECOLOR 0xF800  // 16-bit RGB value for line-color (5-6-5 RGB)
#define DLINECOLOR \
    0x001F  // 16-bit RGB value for pixelated diagonal line (5-6-5 RGB)
#define BOXCOLOR 0xFFE0  // 16-bit RGB value for box-color (5-6-5 RGB)
#define DELAY 20         // Delay in ms for visual effect

int set_background_color(const struct device *screen,
                         struct display_capabilities cap) {
    uint8_t *buf;
    size_t buf_size;
    uint16_t h_step = 1;
    int err;
    struct display_buffer_descriptor buf_desc;

    /* Set buffer size for one row and allocate buffer (1 pixel = 2 bytes) */
    buf_size = cap.x_resolution;
    buf_size *= 2;
    buf = k_malloc(buf_size);
    if (buf == NULL) {
        LOG_ERR("set_background_color: Memory allocation error");
        return -1;
    }

    /* Fill the buffer with greyscale and set buffer descriptor */
    (void)memset(buf, BKGCOLOR, buf_size);
    buf_desc.buf_size = buf_size;
    buf_desc.width = cap.x_resolution;
    buf_desc.height = h_step;
    buf_desc.pitch = cap.x_resolution;

    for (int idx = 0; idx < cap.y_resolution; idx += h_step) {
        err = screen_write(screen, 0, idx, &buf_desc, buf);
        if (err < 0) {
            return err;
        }
    }
    k_free(buf);
    return 0;
}

int draw_diagonal_line(const struct device *screen, uint8_t x, uint8_t y,
                       uint8_t nump) {
    uint8_t *buf;
    uint8_t buf_size = 1;
    int err;
    struct display_buffer_descriptor buf_desc;
    buf_size *= 2;

    /* Set buffer and descriptor for a single pixel */
    buf = k_malloc(buf_size);
    if (buf == NULL) {
        LOG_ERR("draw_diagonal_line: Memory allocation error");
        return -1;
    }

    buf_desc.buf_size = buf_size;
    buf_desc.width = buf_size;
    buf_desc.height = 1;
    buf_desc.pitch = buf_size;

    /* Fill the color buffer */
    fill_buf_16bit(DLINECOLOR, buf, buf_size);

    LOG_INF("Displaying diagonal line, starting pixel (x, y) = %2d, %2d", x, y);
    for (int i = 0; i < nump; i++) {
        err = screen_write(screen, x++, y++, &buf_desc, buf);
        if (err < 0) {
            return err;
        }
        display_blanking_off(screen);
        k_msleep(DELAY);
    }
    k_free(buf);
    LOG_INF("Last pixel (x, y)= %2d, %2d", x, y);
    return 0;
}

int draw_lines(const struct device *screen, uint8_t x, uint8_t y, uint8_t dy,
               uint8_t w, uint8_t numl) {
    uint8_t *buf;
    size_t buf_size;
    int err;
    struct display_buffer_descriptor buf_desc;

    buf_size = w;
    buf_size *= 2;

    /* Set buffer and descriptor for a single line */
    buf = k_malloc(buf_size);
    if (buf == NULL) {
        LOG_ERR("draw_lines: Memory allocation error");
        return -1;
    }
    buf_desc.buf_size = buf_size;
    buf_desc.width = buf_size;
    buf_desc.height = 1;
    buf_desc.pitch = buf_size;

    /* Fill the color buffer */
    fill_buf_16bit(HLINECOLOR, buf, buf_size);

    LOG_INF(
        "Displaying series of lines (1 line at a time), starting line (x, y) = "
        "%2d, %2d",
        x, y);
    for (int i = 0; i < numl; i++) {
        err = screen_write(screen, x, y, &buf_desc, buf);
        if (err < 0) {
            k_free(buf);
            return err;
        }
        y -= dy;
        display_blanking_off(screen);
        k_msleep(DELAY);
    }
    k_free(buf);
    LOG_INF("Last line (x, y)= %2d, %2d", x, y);
    return 0;
}

int draw_box(const struct device *screen, uint8_t x, uint8_t y, uint8_t w,
             uint8_t h) {
    uint8_t *buf;
    size_t buf_size;
    int err;
    struct display_buffer_descriptor buf_desc;

    /* Set buffer and descriptor for rectangular box */
    buf_size = w * h;
    buf_size *= 2;
    buf = k_malloc(buf_size);
    if (buf == NULL) {
        LOG_ERR("draw_box: Memory allocation Error");
        return -1;
    }
    buf_desc.buf_size = buf_size;
    buf_desc.width = w;
    buf_desc.height = h;
    buf_desc.pitch = w;

    /* Fill the color buffer */
    fill_buf_16bit(BOXCOLOR, buf, buf_size);

    LOG_INF("Displaying a rectangular box, top left corner (x,y) = %2d, %2d", x,
            y);
    err = screen_write(screen, x, y, &buf_desc, buf);
    if (err < 0) {
        k_free(buf);
        return err;
    }

    k_free(buf);
    LOG_INF("Bottom right corner (x, y) of box = %2d, %2d", x + w, y + h);
    display_blanking_off(screen);

    return 0;
}

int draw_pixel(const struct device *screen, uint8_t x, uint8_t y,
               uint32_t color) {
    uint8_t *buf;
    uint8_t buf_size = 2;  // 1 pixel (2 bytes)
    int err;
    struct display_buffer_descriptor buf_desc;

    // Each pixel uses 2 bytes (16-bit color), so buffer size is 2 bytes
    buf_size *= 2;

    // Allocate memory for the pixel buffer
    buf = k_malloc(buf_size);
    if (buf == NULL) {
        LOG_ERR("draw_pixel: Memory allocation error");
        return -1;
    }

    // Set the buffer descriptor for a single pixel
    buf_desc.buf_size = buf_size;
    buf_desc.width = buf_size;
    buf_desc.height = 1;
    buf_desc.pitch = buf_size;

    // Fill the buffer with the specified color (16-bit RGB)
    fill_buf_16bit(color, buf, buf_size);

    // Write the pixel to the screen at the specified (x, y) location
    err = screen_write(screen, x, y, &buf_desc, buf);
    if (err < 0) {
        k_free(buf);
        return err;
    }

    // Free the allocated memory after use
    k_free(buf);

    // Turn off display blanking to ensure the pixel is visible
    display_blanking_off(screen);

    return 0;
}

int ili_do_stuff(void) {
    int err;

    /* Initialize variables to suitable values (as per screen size) */
    size_t x = 0, y = 0, dy = 0;
    size_t w = 80, h = 40;
    const struct device *screen;
    struct display_capabilities cap;
    uint8_t num_pixels = 100;
    uint8_t num_lines = 25;

    screen = DEVICE_DT_GET(DT_NODELABEL(ili9340));
    if (!device_is_ready(screen)) {
        LOG_ERR("Device %s not found; Aborting", screen->name);
        return 0;
    }

    /* Setting orientation 1 (W=320 H=240) */
    err = display_set_orientation(screen, 1);
    if (err < 0) {
        LOG_ERR("Error %d", err);
        return 0;
    }

    /* Setting pixel format 1 (16 bit) */
    err = display_set_pixel_format(screen, PANEL_PIXEL_FORMAT_RGB_565);
    if (err < 0) {
        LOG_ERR("Error %d", err);
        return 0;
    }

    /* Obtain and display screen capabilities */
    display_get_capabilities(screen, &cap);

    LOG_INF(
        "Display Sample on %s: Orientation=%d,  Pixel-format=%d,   X-res=%d,   "
        "Y-res=%d",
        screen->name, cap.current_orientation, cap.current_pixel_format,
        cap.x_resolution, cap.y_resolution);

    err = set_background_color(screen, cap);
    if (err < 0) {
        LOG_ERR("Error %d", err);
        return 0;
    }

    /* Display a diagonal line (pixel by pixel) */
    x = 20;
    y = 20;
    err = draw_diagonal_line(screen, x, y, num_pixels);
    if (err < 0) {
        LOG_ERR("Error %d", err);
        return 0;
    }

    /* Display a series of lines (1 line at a time) */
    x = 100;
    y = 100;
    dy = 3;
    err = draw_lines(screen, x, y, dy, w, num_lines);
    if (err < 0) {
        LOG_ERR("Error %d", err);
        return 0;
    }

    /* Display a rectangular box */
    x = 30;
    y = 130;
    err = draw_box(screen, x, y, w, h);
    if (err < 0) {
        LOG_ERR("Error %d", err);
        return 0;
    }

    // Draw a square using draw_pixel
    for (int row = 0; row < 10; row++) {
        for (int col = 0; col < 10; col++) {
            draw_pixel(screen, 100 + row, 100 + col,
                       0x07E0);  // Green (5-6-5 RGB)
        }
    }

    return 0;
}