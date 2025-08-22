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

#ifndef __D_PLAYER__
#define __D_PLAYER__

/*
 * The player data structure depends on a number
 * of other structs: items (internal inventory),
 * animation states (closely tied to the sprites
 * used to represent them, unfortunately).
 */
#include "d_items.h"
#include "p_pspr.h"

/*
 * In addition, the player is just a special
 * case of the generic moving object/actor.
 */
#include "p_mobj.h"

/*
 * Finally, for odd reasons, the player input
 * is buffered within the player data struct,
 * as commands per game tick.
 */
#include "d_ticcmd.h"

#include "net_defs.h"

/* Player states. */
typedef enum
{
    /* Playing or camping */
    PST_LIVE,

    /* Dead on the ground, view follows killer */
    PST_DEAD,

    /* Ready to restart/respawn */
    PST_REBORN
} playerstate_t;

/* Player internal flags, for cheats and debug. */
typedef enum
{
    /* No clipping, walk through barriers */
    CF_NOCLIP = 1,

    /* No damage, no health loss */
    CF_GODMODE = 2,

    /* Not really a cheat, just a debug aid */
    CF_NOMOMENTUM = 4
} cheat_t;

/* Extended player object info - player_t */
typedef struct __attribute__((packed)) player_s
{
    mobj_t *mo;
    playerstate_t playerstate;
    ticcmd_t cmd;

    /*
     * Determine POV, including viewpoint bobbing during movement.
     * Focal origin above r.z
     */
    fixed_t viewz;

    /* Base height above floor for viewz */
    fixed_t viewheight;

    /* Bob/squat speed */
    fixed_t deltaviewheight;

    /* Bounded/Scaled total momentum */
    fixed_t bob;

    /*
     * This is only used between levels,
     * mo->health is used during levels.
     * NRFD-NOTE: Changed from int to short
     */
    short health;

    /* NRFD-NOTE: Changed from int to short */
    short armorpoints;

    /*
     * Armor type is 0-2.
     * NRFD-NOTD: Changed from int to char
     */
    char armortype;

    /* Power ups. invinc and invis are tic counters. */
    int powers[NUMPOWERS];
    boolean cards[NUMCARDS];
    boolean backpack;

    /*
     * Frags, kills of other players.
     * NRFD-NOTE: Changed from int to short
     */
    short frags[MAXPLAYERS];
    weapontype_t readyweapon;

    /* Is wp_nochange if not changing */
    weapontype_t pendingweapon;

    /* NRFD-NOTE: Changed from int to short */
    short weaponowned[NUMWEAPONS];

    /* NRFD-NOTE: Changed from int to short */
    short ammo[NUMAMMO];

    /* NRFD-NOTE: Changed from int to short */
    short maxammo[NUMAMMO];

    /*
     * True if button down last tic.
     * NRFD-NOTE: Changed from int to boolean
     */
    boolean attackdown;

    /* NRFD-NOTE: Changed from int to boolean */
    boolean usedown;

    /* Bit flags, for cheats and debug. See cheat_t, above. */
    int cheats;

    /*
     * Refired shots are less accurate.
     * NRFD-NOTE: Changed from int to short
     */
    short refire;

    /*
     * For intermission stats.
     * NRFD-NOTE: Changed from int to short
     */
    short killcount;

    /* NRFD-NOTE: Changed from int to short */
    short itemcount;

    /* NRFD-NOTE: Changed from int to short */
    short secretcount;

    /* Hint messages */
    char *message;

    /* For screen flashing (red or bright) */
    int damagecount;
    int bonuscount;

    /* Who did damage (NULL for floors/ceilings) */
    mobj_t *attacker;

    /* So gun flashes light up areas */
    int8_t extralight;

    /*
     * Current PLAYPAL, can be set to REDCOLORMAP for pain, etc.
     * NRFD-NOTE: Changed from int to short
     */
    short fixedcolormap;

    /*
     * Player skin colorshift, 0-3 for which color to draw player.
     * NRFD-NOTE: Changed from int to short
     */
    short colormap;

    /* Overlay view sprites (gun, etc.) */
    pspdef_t psprites[NUMPSPRITES];

    /* True if secret level has been done */
    boolean didsecret;
} player_t;

/*
 * Intermission.
 * Structure passed e.g. to WI_Start(wb)
 */
typedef struct
{
    /* Whether the player is in game */
    boolean in;

    /* Player stats, kills, collected items etc. */
    int skills;
    int sitems;
    int ssecret;
    int stime;
    int frags[4];

    /* Current score on entry, modified on return */
    int score;
} wbplayerstruct_t;

typedef struct
{
    /* Episode (0-2) */
    int epsd;

    /* If true, splash the secret level */
    boolean didsecret;

    /* Previous and next levels, origin 0 */
    int last;
    int next;

    int maxkills;
    int maxitems;
    int maxsecret;
    int maxfrags;

    /* The par time */
    int partime;

    /* Index of this player in game */
    int pnum;

    wbplayerstruct_t plyr[MAXPLAYERS];
} wbstartstruct_t;

#endif /* __D_PLAYER__ */
