//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
//  System specific interface stuff.
//


#ifndef __D_TICCMD__
#define __D_TICCMD__

#include "doomtype.h"


// The data sampled per tick (single player)
// and transmitted to other peers (multiplayer).
// Mainly movements/button commands per game tick,
// plus a checksum for internal state consistency.

typedef struct   __attribute__((packed))
{
    signed char forwardmove;    // *2048 for move
    signed char sidemove;   // *2048 for move
    short angleturn;            // <<16 for angle delta
    byte chatchar;
    byte buttons;
    // villsa [STRIFE] according to the asm,
    // consistancy is a short, not a byte
    byte consistancy;           // checks for net game

    // villsa - Strife specific:

    // These Strife/Heretic/Hexen fields are never populated or transmitted in
    // a Doom game, but the full Chocolate Doom net serializer (net_structrw.c)
    // references them, so they must exist when networking is compiled in. The
    // fork excludes them on RAM-constrained targets that don't build the net
    // code (NRFD-EXLUDE).
#if defined(CONFIG_FEATURE_DOOM_NET)
    byte buttons2;
    int inventory;

    // Heretic/Hexen specific:

    byte lookfly;               // look/fly up/down/centering
    byte arti;                  // artitype_t to use
#endif
} ticcmd_t;



#endif
