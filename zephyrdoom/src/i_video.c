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
//	DOOM graphics stuff for NRF.
//


// NRFD-EXCLUDE: #include "icon.c"

#include <string.h>
#include <hal/nrf_gpio.h>

#include "doom_config.h"
#include "d_loop.h"
#include "deh_str.h"
#include "doomtype.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "n_buttons.h"
#include "n_display.h"
#include "n_uart.h"
#include "n_rjoy.h"

#include "FT810.h"

#define GPIO0 ((NRF_GPIO_Type*) 0x50842500UL)
#define GPIO1 ((NRF_GPIO_Type*) 0x50842800UL)


static uint32_t pixel_format;


// display has been set up?

static boolean initialized = false;

// disable mouse?

const boolean nomouse = true;
const int usemouse = 1;

// Save screenshots in PNG format.

// int png_screenshots = 0;

// SDL video driver name

// char *video_driver = "";

// Window position:

// char *window_position = "center";

// SDL display number on which to run.

// int video_display = 0;

// Screen width and height, from configuration file.

// int window_width = SCREENWIDTH * 2;
// int window_height = SCREENHEIGHT_4_3 * 2;

// Fullscreen mode, 0x0 for SDL_WINDOW_FULLSCREEN_DESKTOP.

// int fullscreen_width = 0, fullscreen_height = 0;

// Maximum number of pixels to use for intermediate scale buffer.

// static int max_scaling_buffer_pixels = 16000000;

// Run in full screen mode?  (int type for config code)

// int fullscreen = true;

// Aspect ratio correction mode

// int aspect_ratio_correct = true;
// static int actualheight;

// Force integer scales for resolution-independent rendering

// int integer_scaling = false;

// VGA Porch palette change emulation

// const int vga_porch_flash = false;

// Force software rendering, for systems which lack effective hardware
// acceleration

// int force_software_renderer = false;

// Time to wait for the screen to settle on startup before starting the
// game (ms)

// const int startup_delay = 1000;

// Grab the mouse? (int type for config code). nograbmouse_override allows
// this to be temporarily disabled via the command line.

// const int grabmouse = true;
// const boolean nograbmouse_override = false;

// The screen buffer; this is modified to draw things to the screen

pixel_t *I_VideoBuffer; //[320*200];
pixel_t *I_VideoBackBuffer; //[320*200];
pixel_t I_VideoBuffers[2][320*200];

uint8_t  display_pal[DISPLAY_PALETTE_SIZE];

static int current_dl;

// Memory references for display driver memory
static uint32_t display_vbuffer_locs[3]; // Frame buffer
static uint32_t display_palette_locs[3]; // Pallette

// If true, game is running as a screensaver

const boolean screensaver_mode = false;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen

const boolean screenvisible = true;

// If true, we display dots at the bottom of the screen to
// indicate FPS.

const boolean display_fps_dots = false;

// If this is true, the screen is rendered but not blitted to the
// video buffer.

const boolean noblit = false;

// Gamma correction level to use

byte usegamma = 1;

// Joystick/gamepad hysteresis
unsigned int joywait = 0;

void PrintVBuffer(void)
{
    // pixel_t *vb = I_VideoBuffer;
    // printf("%.2X %.2X %.2X %2X %.2X %.2X %.2X %2X\n",
    //     vb[0], vb[1], vb[2], vb[3],
    //     vb[4], vb[5], vb[6], vb[7]);
    // printf("%.2X %.2X %.2X %2X %.2X %.2X %.2X %2X\n",
    //     vb[1*SCREENWIDTH+0], vb[1*SCREENWIDTH+1], vb[1*SCREENWIDTH+2], vb[1*SCREENWIDTH+3],
    //     vb[1*SCREENWIDTH+4], vb[1*SCREENWIDTH+5], vb[1*SCREENWIDTH+6], vb[1*SCREENWIDTH+7]);
}

/**
 * Write byte on pins and inform LCD - write data to LCD
 *
 * @param byte data to send
 * @param data_pin true if sending data, false if sending command
 *
 * @return  void
 */
void write_byte(uint8_t byte, bool data_pin)
{
    // Check wheter we are sending data or command - flip pin 6 on GPIO0 accordingly
    if (data_pin) {
        NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 6)) | (data_pin << 6);
    } else {
        NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 6));
    }


    // Prepare byte - we have to swap bit 1,2 with 7,8, because the LCD board has it flipped
    uint16_t aligned_byte = byte;
    aligned_byte |= ((aligned_byte & 3) << 8);
    aligned_byte >>= 2;
    // Push byte onto PINs via register directly (starting with bit 4 - pin 4 on GPIO1)
    NRF_P1_S->OUT = (NRF_P1_S->OUT & ~(0xFF << 4)) | (aligned_byte << 4);

    // Inform LCD that it can read it pin values - flip pin 5 on GPIO0 LOW then HIGH
    // LCD controller reads on rising edge
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 5));
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 5)) | (1 << 5);
}

/**
 * Initialization function stolen from Arduino driver, no idea what it does.
 *
 * @return  void
 */
void init_lcd(void)
{
    //TODO: Find out why exactly we doing each step (commands) from datasheet
    //gpio_pin_set_raw(gpio_dev0, 25, 1);
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 25)) | (1 << 25);
    I_Sleep(5);
    //gpio_pin_set_raw(gpio_dev0, 25, 0);
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 25));
    I_Sleep(15);
    //gpio_pin_set_raw(gpio_dev0, 25, 1);
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 25)) | (1 << 25);
    I_Sleep(15);

    //gpio_pin_set_raw(gpio_dev0, 7, 1);
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 7)) | (1 << 7);
    //gpio_pin_set_raw(gpio_dev0, 5, 1);
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 5)) | (1 << 5);
    //gpio_pin_set_raw(gpio_dev0, 7, 0); // CS
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 7));

    write_byte(0xF9, false);
    write_byte(0x00, true);
    write_byte(0x08, true);
    write_byte(0xC0, false);
    write_byte(0x19, true);
    write_byte(0x1A, true);
    write_byte(0xC1, false);
    write_byte(0x45, true);
    write_byte(0X00, true);
    write_byte(0xC2, false);
    write_byte(0x33, true);
    write_byte(0xC5, false);
    write_byte(0x00, true);
    write_byte(0x28, true);
    write_byte(0xB1, false);
    write_byte(0x90, true);
    write_byte(0x11, true);
    write_byte(0xB4, false);
    write_byte(0x02, true);
    write_byte(0xB6, false);
    write_byte(0x00, true);
    write_byte(0x42, true);
    write_byte(0x3B, true);
    write_byte(0xB7, false);
    write_byte(0x07, true);
    write_byte(0xE0, false);
    write_byte(0x1F, true);
    write_byte(0x25, true);
    write_byte(0x22, true);
    write_byte(0x0B, true);
    write_byte(0x06, true);
    write_byte(0x0A, true);
    write_byte(0x4E, true);
    write_byte(0xC6, true);
    write_byte(0x39, true);
    write_byte(0x00, true);
    write_byte(0x00, true);
    write_byte(0x00, true);
    write_byte(0x00, true);
    write_byte(0x00, true);
    write_byte(0x00, true);
    write_byte(0xE1, false);
    write_byte(0x1F, true);
    write_byte(0x3F, true);
    write_byte(0x3F, true);
    write_byte(0x0F, true);
    write_byte(0x1F, true);
    write_byte(0x0F, true);
    write_byte(0x46, true);
    write_byte(0x49, true);
    write_byte(0x31, true);
    write_byte(0x05, true);
    write_byte(0x09, true);
    write_byte(0x03, true);
    write_byte(0x1C, true);
    write_byte(0x1A, true);
    write_byte(0x00, true);
    write_byte(0xF1, false);
    write_byte(0x36, true);
    write_byte(0x04, true);
    write_byte(0x00, true);
    write_byte(0x3C, true);
    write_byte(0x0F, true);
    write_byte(0x0F, true);
    write_byte(0xA4, true);
    write_byte(0x02, true);
    write_byte(0xF2, false);
    write_byte(0x18, true);
    write_byte(0xA3, true);
    write_byte(0x12, true);
    write_byte(0x02, true);
    write_byte(0x32, true);
    write_byte(0x12, true);
    write_byte(0xFF, true);
    write_byte(0x32, true);
    write_byte(0x00, true);
    write_byte(0xF4, false);
    write_byte(0x40, true);
    write_byte(0x00, true);
    write_byte(0x08, true);
    write_byte(0x91, true);
    write_byte(0x04, true);
    write_byte(0xF8, false);
    write_byte(0x21, true);
    write_byte(0x04, true);
    write_byte(0x36, false);
    write_byte(0x48, true);
    write_byte(0x3A, false);
    write_byte(0x55, true);

    write_byte(0x11, false); // Exit Sleep
    I_Sleep(120);
    write_byte(0x29, false); // Display on
    printf("GPIO Display initialized\n");
}


/**
 * Address_set function stolen from Arduino driver that
 * sets starting and ending position for future image data
 *
 * @param   x1  Starting position X
 * @param   y1  Starting position Y
 * @param   x2  Ending position X
 * @param   y2  Ending position Y
 *
 * @return  void
 */
void Address_set(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
    // Todo: figure out exactly what's happening in here
    write_byte(0x2a, false);
    write_byte(x1 >> 8, true);
    write_byte(x1, true);
    write_byte(x2 >> 8, true);
    write_byte(x2, true);
    write_byte(0x2b, false);
    write_byte(y1 >> 8, true);
    write_byte(y1, true);
    write_byte(y2 >> 8, true);
    write_byte(y2, true);
    write_byte(0x2c, false);
}


/**
 * Fill provided framebuffer with provided color in RGB16 (5-6-5) format
 *
 * @param   fb      Framebuffer array
 * @param   color   Color in RGB16 (5-6-5) format
 *
 * @return  void
 */
void framebuffer_fill(uint16_t fb[], uint16_t color) {
    for (int i = 0; i < 320*201; i++) {
        fb[i] = color;
    }
}


/**
 * Send provided framebuffer into LCD.
 * This function allows to skip repeated write_byte invokation for every pixel twice and can
 * save some time.
 *
 * @param   fb      Framebuffer array
 *
 * @return  void
 */
void framebuffer_send(uint8_t fb[], int len) {
    Address_set(0, 0, 320, 199);
    write_byte(0x02c, false); //write_memory_start
    // digitalWrite(LCD_RS,HIGH);
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 6)) | (1 << 6); // CS pin 6
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 7));

    NRF_P0_S->OUT = (NRF_P0_S->OUT | (1 << 6));

    for (int j = 199; j >= 0; j--) {
        for (int k= 0; k < 320; k++) {
            int i = k + j * 320;
            // This generates too much tint and lowers contrast significantly
            /*uint16_t cur = ((display_pal[fb[i]*4+0]>>3)&0x1f) << 11 |
                            ((display_pal[fb[i]*4+1]>>3)&0x3f)<<5 |
                            ((display_pal[fb[i]*4+2]>>3)&0x1f);*/
            /*uint16_t cur = (display_pal[fb[i]*4+0]>>4)<<11 |
                            (display_pal[fb[i]*4+1]>>3)<<5 |
                            (display_pal[fb[i]*4+2]>>4);*/

            // Best RGB888 -> RGB565 conversion
            uint16_t cur = ((display_pal[fb[i]*4+0] * 249 + 1024) >> 11) << 11 |
                            ((display_pal[fb[i]*4+1] * 253 + 512) >> 10) <<5 |
                            ((display_pal[fb[i]*4+2] * 249 + 1024) >> 11);

            uint16_t aligned_byte = (uint8_t) (cur >> 8);
            aligned_byte |= ((aligned_byte & 3) << 8);
            aligned_byte >>= 2;

            NRF_P1_S->OUT = (NRF_P1_S->OUT & ~(0xFF << 4)) | (aligned_byte << 4);
            NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 5));
            NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 5)) | (1 << 5);

            aligned_byte = (uint8_t) cur;
            aligned_byte |= ((aligned_byte & 3) << 8);
            aligned_byte >>= 2;
            NRF_P1_S->OUT = (NRF_P1_S->OUT & ~(0xFF << 4)) | (aligned_byte << 4);

            NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 5));
            NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 5)) | (1 << 5);
        }
    }
    NRF_P0_S->OUT = (NRF_P0_S->OUT & ~(1 << 7)) | (1 << 7); // CS pin 7
}


// Set the variable controlling FPS dots.

void I_DisplayFPSDots(boolean dots_on)
{
    // display_fps_dots = dots_on;
}


void I_ShutdownGraphics(void)
{
    if (initialized)
    {
        initialized = false;
    }
}



//
// I_StartFrame
//
void I_StartFrame (void)
{
    // N_display_spi_transfer_finish();
}

static void I_ToggleFullScreen(void)
{
    //NRFD-EXCLUDE
}

void I_GenerateEvents(void)
{
    N_ReadButtons();
    // N_ReadUart(); // TODO: why

    //printf("%d %d\n", joywait, I_GetTime());
    if (joywait < I_GetTime())
    {
        joywait = 0;
        N_rjoy_read();
        //I_UpdateJoystick();
    }

    /* NRFD-TODO: mouse?
    if (usemouse && !nomouse)
    {
        I_ReadMouse();
    }*/
}

//
// I_StartTic
//
void I_StartTic (void)
{
    N_ldbg("NRFD-TODO: I_StartTic\n");
    if (!initialized)
    {
        return;
    }

    I_GenerateEvents();

}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

void I_WriteDisplayList(uint32_t pal_loc, uint32_t display_loc)
{
    /*dl_start();

    dl(FT810_CLEAR_COLOR_RGB(0x00, 0x00, 0x00));
    dl(FT810_CLEAR(1,1,1));  // Clear color, stencil, tag
    dl(FT810_CLEAR_COLOR_RGB(0x00, 0x00, 0x00));
    dl(FT810_CLEAR(1,0,0));  // Clear color

    dl(FT810_BITMAP_HANDLE(0)) ;
    dl(FT810_BITMAP_LAYOUT(PALETTED8, SCREENWIDTH, SCREENHEIGHT)) ;
    int16_t trans_a = (int16_t)(256/2.0);
    int16_t trans_b = (int16_t)(256/2.4);
    dl(FT810_BITMAP_TRANSFORM_A(trans_a));
    dl(FT810_BITMAP_TRANSFORM_E(trans_b));
    dl(FT810_BITMAP_SIZE(NEAREST, BORDER, BORDER, 640&0x1FF, 480));
    dl(FT810_BITMAP_SIZE_H(640>>9, 0));
    dl(FT810_BITMAP_SOURCE(display_loc)) ;

    dl(FT810_COLOR_RGB(0xFF, 0xFF, 0xFF));

    dl(FT810_BEGIN(BITMAPS));
    {
        dl(FT810_VERTEX_TRANSLATE_X(80*16));
        // dl(FT810_VERTEX_TRANSLATE_Y(0*16));

        dl(FT810_BLEND_FUNC(ONE, ZERO));

        dl(FT810_COLOR_MASK(0,0,0,1));
        dl(FT810_PALETTE_SOURCE(pal_loc+3));
        dl(FT810_VERTEX2II(0, 0, 0, 0));

        dl(FT810_BLEND_FUNC(DST_ALPHA, ONE_MINUS_DST_ALPHA));
        dl(FT810_COLOR_MASK(1,0,0,0));
        dl(FT810_PALETTE_SOURCE(pal_loc));
        dl(FT810_VERTEX2II(0, 0, 0, 0));

        dl(FT810_COLOR_MASK(0,1,0,0));
        dl(FT810_PALETTE_SOURCE(pal_loc+1));
        dl(FT810_VERTEX2II(0, 0, 0, 0));

        dl(FT810_COLOR_MASK(0,0,1,0));
        dl(FT810_PALETTE_SOURCE(pal_loc+2));
        dl(FT810_VERTEX2II(0, 0, 0, 0));
    }
    dl(FT810_END());

    dl_end();

    N_display_dlswap_frame();*/
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
    static int lasttic;
    int tics;
    int i;

    framebuffer_send((uint8_t*)I_VideoBuffer, SCREENWIDTH*SCREENHEIGHT);

    /*// draws little dots on the bottom of the screen
    if (display_fps_dots)
    {
        i = I_GetTime();
        tics = i - lasttic;
        lasttic = i;
        if (tics > 20) tics = 20;

        for (i=0 ; i<tics*4 ; i+=4)
            I_VideoBuffer[ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
        for ( ; i<20*4 ; i+=4)
            I_VideoBuffer[ (SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
    }

    // Draw disk icon before blit, if necessary.
    // NRFD_TODO: V_DrawDiskIcon();

    // Wait for previous frame buffer transfer to finish
    N_display_spi_transfer_finish();

    // Instruct display to start drawing previous frame
    I_WriteDisplayList(display_palette_locs[current_dl], display_vbuffer_locs[current_dl]);

    current_dl = (current_dl+1)%3;

    // Do complete palette data transfer
    N_display_spi_wr(display_palette_locs[current_dl], DISPLAY_PALETTE_SIZE, display_pal);
    N_display_spi_transfer_finish();

    // Start frame buffer transfer
    N_display_spi_wr(display_vbuffer_locs[current_dl], SCREENWIDTH*SCREENHEIGHT, (uint8_t*)I_VideoBuffer);*/

    // Restore background and undo the disk indicator, if it was drawn.
    // NRFD-TODO: V_RestoreDiskBackground();
}


//
// I_ReadScreen
//
void I_ReadScreen (pixel_t* scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH*SCREENHEIGHT*sizeof(*scr));
}


//
// I_SetPalette
//
void I_SetPalette (byte *doompalette)
{
    int i;
    // printf("I_SetPalette %X\n", (unsigned int)(doompalette));

    // Convert Doom palette to FT810 palette

    // TODO: Do conversion right before transferring to save memory?
    for (i=0; i<256; ++i)
    {
        // Zero out the bottom two bits of each channel - the PC VGA
        // controller only supports 6 bits of accuracy.

        uint8_t r = gammatable[usegamma][*doompalette++] & ~3;
        uint8_t g = gammatable[usegamma][*doompalette++] & ~3;
        uint8_t b = gammatable[usegamma][*doompalette++] & ~3;
        display_pal[i*4+0] = r;
        display_pal[i*4+1] = g;
        display_pal[i*4+2] = b;
        display_pal[i*4+3] = 0xFF;

    }
}

/* NRFD-EXCLUDE
// Given an RGB value, find the closest matching palette index.

int I_GetPaletteIndex(int r, int g, int b)
{
    int best, best_diff, diff;
    int i;

    best = 0; best_diff = INT_MAX;

    for (i = 0; i < 256; ++i)
    {
        diff = (r - palette[i].r) * (r - palette[i].r)
             + (g - palette[i].g) * (g - palette[i].g)
             + (b - palette[i].b) * (b - palette[i].b);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }

        if (diff == 0)
        {
            break;
        }
    }

    return best;
}

    */

//
// Set the window title
//

void I_SetWindowTitle(char *title)
{
    /* NRFD-EXCLUDE */
}


void I_GraphicsCheckCommandLine(void)
{
    int i;

    //!
    // @category video
    // @vanilla
    //
    // Disable blitting the screen.
    //

    // NRFD-TODO?
    // noblit = M_CheckParm ("-noblit");

    // //!
    // // @category video
    // //
    // // Don't grab the mouse when running in windowed mode.
    // //

    // nograbmouse_override = M_ParmExists("-nograbmouse");

    //!
    // @category video
    //
    // Disable the mouse.
    //
    // NRFD-EXCLUDE
    // nomouse = M_CheckParm("-nomouse") > 0;
}



void I_InitGraphics(void)
{
    printf("I_InitGraphics\n");

    init_lcd();

    current_dl = 1;

    I_VideoBuffer = I_VideoBuffers[1];
    I_VideoBackBuffer = I_VideoBuffers[0];
    initialized = true;
}

// Bind all variables controlling video options into the configuration
// file system.
void I_BindVideoVariables(void)
{
    // NRFD-TODO:?
    // M_BindIntVariable("use_mouse",                 &usemouse);
    // M_BindIntVariable("fullscreen",                &fullscreen);
    // M_BindIntVariable("video_display",             &video_display);
    // M_BindIntVariable("aspect_ratio_correct",      &aspect_ratio_correct);
    // M_BindIntVariable("integer_scaling",           &integer_scaling);
    // M_BindIntVariable("vga_porch_flash",           &vga_porch_flash);
    // M_BindIntVariable("startup_delay",             &startup_delay);
    // M_BindIntVariable("fullscreen_width",          &fullscreen_width);
    // M_BindIntVariable("fullscreen_height",         &fullscreen_height);
    // M_BindIntVariable("force_software_renderer",   &force_software_renderer);
    // M_BindIntVariable("max_scaling_buffer_pixels", &max_scaling_buffer_pixels);
    // M_BindIntVariable("window_width",              &window_width);
    // M_BindIntVariable("window_height",             &window_height);
    // M_BindIntVariable("grabmouse",                 &grabmouse);
    // M_BindStringVariable("video_driver",           &video_driver);
    // M_BindStringVariable("window_position",        &window_position);
    // M_BindIntVariable("usegamma",                  &usegamma);
    // M_BindIntVariable("png_screenshots",           &png_screenshots);
}

// Rename to StartUpdate?
void I_ClearVideoBuffer(void)
{
    // N_display_spi_transfer_finish();

    // Swap buffers
    if (I_VideoBuffer == I_VideoBuffers[0]) {
        I_VideoBuffer = I_VideoBuffers[1];
        I_VideoBackBuffer = I_VideoBuffers[0];
    }
    else {
        I_VideoBuffer = I_VideoBuffers[0];
        I_VideoBackBuffer = I_VideoBuffers[1];
    }
    V_RestoreBuffer();

    /*for (int i=0; i<SCREENHEIGHT*SCREENWIDTH; i++) {
        I_VideoBuffer[i] = 251; // pink
    }*/
}