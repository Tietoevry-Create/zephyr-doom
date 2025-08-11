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
 *
 * DESCRIPTION:
 * The status bar widget code.
 */

#ifndef __STLIB__
#define __STLIB__

/* We are referring to patches. */
#include "r_defs.h"

/* Typedefs of widgets. */

/*
 * Number widget.
 *
 * NRFD-NOTE: int changed to short in structs below
 */
typedef struct __attribute__((packed))
{
    /* Upper right-hand corner of the number (right-justified) */
    short x;
    short y;

    /* Max # of digits in number */
    short width;

    /* Last number value */
    short oldnum;

    /* Pointer to current value */
    short *num;

    /* Pointer to boolean stating whether to update number */
    boolean *on;

    /* List of patches for 0-9 */
    patch_t **p;

    /* User data */
    int data;
} st_number_t;

/*
 * Percent widget ("child" of number widget,
 * or, more precisely, contains a number widget.)
 */
typedef struct
{
    /* Number information */
    st_number_t n;

    /* Percent sign graphic */
    patch_t *p;
} st_percent_t;

/* Multiple icon widget. */
typedef struct __attribute__((packed))
{
    /* Center justified location of icons */
    short x;
    short y;

    /* Last icon number */
    short oldinum;

    /* Pointer to current icon */
    short *inum;

    /* Pointer to boolean stating whether to update icon */
    boolean *on;

    /* List of icons */
    patch_t **p;

    /* User data */
    int data;
} st_multicon_t;

/* Binary icon widget. */
typedef struct __attribute__((packed))
{
    /* Center justified location of icon */
    short x;
    short y;

    /* Last icon value */
    boolean oldval;

    /* Pointer to current icon status */
    boolean *val;

    /* Pointer to boolean stating whether to update icon */
    boolean *on;

    /* Icon */
    patch_t *p;

    /* User data */
    int data;
} st_binicon_t;

/* Widget creation, access, and update routines. */

/*
 * Initializes widget library.
 * More precisely, initialize STMINUS, everything else is done somewhere else.
 */
void STlib_init(void);

/* NRFD-NOTE: int changed to short in functions below. */

/* Number widget routines. */
void STlib_initNum(st_number_t *n, short x, short y, patch_t **pl, short *num,
                   boolean *on, short width);

void STlib_updateNum(st_number_t *n, boolean refresh);

/* Percent widget routines. */
void STlib_initPercent(st_percent_t *p, short x, short y, patch_t **pl,
                       short *num, boolean *on, patch_t *percent);

void STlib_updatePercent(st_percent_t *per, int refresh);

/* Multiple Icon widget routines. */
void STlib_initMultIcon(st_multicon_t *mi, short x, short y, patch_t **il,
                        short *inum, boolean *on);

void STlib_updateMultIcon(st_multicon_t *mi, boolean refresh);

/* Binary icon widget routines. */
void STlib_initBinIcon(st_binicon_t *b, short x, short y, patch_t *i,
                       boolean *val, boolean *on);

void STlib_updateBinIcon(st_binicon_t *bi, boolean refresh);

#endif /* __STLIB__ */
