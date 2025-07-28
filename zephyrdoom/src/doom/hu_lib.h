/*
 * Copyright(C) 1993-1996 Id Software, Inc.
 * Copyright(C) 2005-2014 Simon Howard
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __HULIB__
#define __HULIB__

/* We are referring to patches. */
#include "r_defs.h"

/* Font stuff. */
#define HU_CHARERASE KEY_BACKSPACE

#define HU_MAXLINES 4
#define HU_MAXLINELENGTH 80

/*
 * Typedefs of widgets
 * NRFD-NOTE: int changed to short in structs below.
 */

/*
 * Text Line widget.
 * Parent of Scrolling Text and Input Text widgets.
 */
typedef struct __attribute__((packed))
{
    /* Left justified position of scrolling text window */
    short x;
    short y;

    patch_t **f;                  /* Font */
    short sc;                     /* Start character */
    char l[HU_MAXLINELENGTH + 1]; /* Line of text */
    short len;                    /* Current line length */

    /* Whether this line needs to be updated */
    short needsupdate;
} hu_textline_t;

/*
 * Scrolling Text window widget.
 * Child of Text Line widget.
 */
typedef struct __attribute__((packed))
{
    hu_textline_t l[HU_MAXLINES]; /* Text lines to draw */
    short h;                      /* Height in lines */
    short cl;                     /* Current line number */

    /* Pointer to boolean stating whether to update window */
    boolean *on;

    /* Last value of *->on */
    boolean laston;
} hu_stext_t;

/*
 * Input Text Line widget.
 * Child of Text Line widget.
 */
typedef struct __attribute__((packed))
{
    /* Text line to input on */
    hu_textline_t l;

    /* Left margin past which I am not to delete characters */
    short lm;

    /* Pointer to boolean stating whether to update window */
    boolean *on;

    /* Last value of *->on */
    boolean laston;
} hu_itext_t;

/* Widget creation, access, and update routines. */

/* Initializes heads-up widget library */
void HUlib_init(void);

/* Textline code. */

/* Clear a line of text. */
void HUlib_clearTextLine(hu_textline_t *t);

void HUlib_initTextLine(hu_textline_t *t, int x, int y, patch_t **f, int sc);

/* Returns success. */
boolean HUlib_addCharToTextLine(hu_textline_t *t, char ch);

/* Returns success. */
boolean HUlib_delCharFromTextLine(hu_textline_t *t);

/* Draws tline. */
void HUlib_drawTextLine(hu_textline_t *l, boolean drawcursor);

/* Erases text line. */
void HUlib_eraseTextLine(hu_textline_t *l);

/* Scrolling Text window widget routines. */

void HUlib_initSText(hu_stext_t *s, int x, int y, int h, patch_t **font,
                     int startchar, boolean *on);

/* Add a new line. */
void HUlib_addLineToSText(hu_stext_t *s);

void HUlib_addMessageToSText(hu_stext_t *s, char *prefix, char *msg);

/* Draws stext. */
void HUlib_drawSText(hu_stext_t *s);

/* Erases all stext lines */
void HUlib_eraseSText(hu_stext_t *s);

/* Input Text Line widget routines. */
void HUlib_initIText(hu_itext_t *it, int x, int y, patch_t **font,
                     int startchar, boolean *on);

/* Enforces left margin. */
void HUlib_delCharFromIText(hu_itext_t *it);

/* Enforces left margin. */
void HUlib_eraseLineFromIText(hu_itext_t *it);

/* Resets line and left margin. */
void HUlib_resetIText(hu_itext_t *it);

/* Left of left margin */
void HUlib_addPrefixToIText(hu_itext_t *it, char *str);

/* Whether eaten. */
boolean HUlib_keyInIText(hu_itext_t *it, unsigned char ch);

void HUlib_drawIText(hu_itext_t *it);

/* Erases all itext lines. */
void HUlib_eraseIText(hu_itext_t *it);

#endif /* __HULIB__ */
