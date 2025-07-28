/*
 * Copyright(C) 1993-1996 Id Software, Inc.
 * Copyright(C) 2005-2014 Simon Howard, Andrey Budko
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
 * Movement, collision handling.
 * Shooting and aiming.
 */

#include <stdio.h>
#include <stdlib.h>

#include "deh_misc.h"

#include "m_bbox.h"
#include "m_random.h"
#include "i_system.h"

#include "doomdef.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_local.h"

#include "s_sound.h"

/* State. */
#include "doomstat.h"
#include "r_state.h"

/* Data. */
#include "sounds.h"

/*
 * Spechit overrun magic value.
 *
 * This is the value used by PrBoom-plus. I think the value below is
 * actually better and works with more demos. However, I think
 * it's better for the spechits emulation to be compatible with
 * PrBoom-plus, at least so that the big spechits emulation list
 * on Doomworld can also be used with Chocolate Doom.
 */
#define DEFAULT_SPECHIT_MAGIC 0x01C09C98

/*
 * // This is from a post by myk on the Doomworld forums,
 * // outputted from entryway's spechit_magic generator for
 * // s205n546.lmp. The _exact_ value of this isn't too
 * // important, as long as it is in the right general
 * // range, it will usually work. Otherwise, we can use
 * // the generator (hacked doom2.exe) and provide it
 * // with -spechit.
 * #define DEFAULT_SPECHIT_MAGIC 0x84f968e8
 */

fixed_t tmbbox[4];
mobj_t *tmthing;
int tmflags;
fixed_t tmx;
fixed_t tmy;

/*
 * If "floatok" true, move would be ok.
 * If within "tmfloorz - tmceilingz".
 */
boolean floatok;

fixed_t tmfloorz;
fixed_t tmceilingz;
fixed_t tmdropoffz;

/*
 * Keep track of the line that lowers the ceiling,
 * so missiles don't explode against sky hack walls.
 */
line_t *ceilingline;

/*
 * Keep track of special lines as they are hit,
 * but don't process them until the move is proven valid.
 */
line_t *spechit[MAXSPECIALCROSS];
int numspechit;

/* Teleport move. */

boolean PIT_StompThing(mobj_t *thing)
{
    fixed_t blockdist;

    if (!(thing->flags & MF_SHOOTABLE))
        return true;

    blockdist = thing->radius + tmthing->radius;

    if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
    {
        /* Didn't hit it */
        return true;
    }

    /* Don't clip against self */
    if (thing == tmthing)
        return true;

    /* Monsters don't stomp things except on boss level */
    if (!tmthing->player && gamemap != 30)
        return false;

    P_DamageMobj(thing, tmthing, tmthing, 10000);

    return true;
}

boolean P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y)
{
    int xl;
    int xh;
    int yl;
    int yh;
    int bx;
    int by;

    subsector_t *newsubsec;

    /* Kill anything occupying the position */
    tmthing = thing;
    tmflags = thing->flags;

    tmx = x;
    tmy = y;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsubsec = R_PointInSubsector(x, y);
    ceilingline = NULL;

    /*
     * The base floor/ceiling is from the subsector
     * that contains the point.
     * Any contacted lines the step closer together
     * will adjust them.
     */
    tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
    tmceilingz = newsubsec->sector->ceilingheight;

    validcount++;
    numspechit = 0;

    /* Stomp on any things contacted */
    xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            if (!P_BlockThingsIterator(bx, by, PIT_StompThing))
                return false;

    /* The move is ok, so link the thing into its new position */
    P_UnsetThingPosition(thing);

    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);

    return true;
}

/* Movement iterator functions. */

static void SpechitOverrun(line_t *ld);

/* Adjusts tmfloorz and tmceilingz as lines are contacted. */
boolean PIT_CheckLine(line_t *ld)
{
    fixed_t *ld_bbox = LineBBox(ld);

    if (tmbbox[BOXRIGHT] <= ld_bbox[BOXLEFT] || tmbbox[BOXLEFT] >= ld_bbox[BOXRIGHT] || tmbbox[BOXTOP] <= ld_bbox[BOXBOTTOM] || tmbbox[BOXBOTTOM] >= ld_bbox[BOXTOP])
        return true;

    if (P_BoxOnLineSide(tmbbox, ld) != -1)
        return true;

    /*
     * A line has been hit.
     *
     * The moving thing's destination position will cross the given line.
     * If this should not be allowed, return false.
     * If the line is special, keep track of it
     * to process later if the move is proven ok.
     * NOTE - Specials are NOT sorted by order,
     * so two special lines that are only 8 pixels apart
     * could be crossed in either order.
     */
    if (!LineBackSector(ld))
        /* One sided line */
        return false;

    if (!(tmthing->flags & MF_MISSILE))
    {
        if (LineFlags(ld) & ML_BLOCKING)
            /* Explicitly blocking everything */
            return false;

        if (!tmthing->player && (LineFlags(ld) & ML_BLOCKMONSTERS))
            /* Block monsters only */
            return false;
    }

    /* Set openrange, opentop, openbottom */
    P_LineOpening(ld);

    /* Adjust floor / ceiling heights */
    if (opentop < tmceilingz)
    {
        tmceilingz = opentop;
        ceilingline = ld;
    }

    if (openbottom > tmfloorz)
        tmfloorz = openbottom;

    if (lowfloor < tmdropoffz)
        tmdropoffz = lowfloor;

    /* If contacted a special line, add it to the list */
    if (ld->special)
    {
        spechit[numspechit] = ld;
        numspechit++;

        /* fraggle: spechits overrun emulation code from prboom-plus */
        if (numspechit > MAXSPECIALCROSS_ORIGINAL)
        {
            SpechitOverrun(ld);
        }
    }

    return true;
}

boolean PIT_CheckThing(mobj_t *thing)
{
    fixed_t blockdist;
    boolean solid;
    int damage;

    if (!(thing->flags & (MF_SOLID | MF_SPECIAL | MF_SHOOTABLE)))
        return true;

    blockdist = thing->radius + tmthing->radius;

    if (abs(thing->x - tmx) >= blockdist || abs(thing->y - tmy) >= blockdist)
    {
        /* Didn't hit it */
        return true;
    }

    /* Don't clip against self */
    if (thing == tmthing)
        return true;

    /* Check for skulls slamming into things */
    if (tmthing->flags & MF_SKULLFLY)
    {
        damage = ((P_Random() % 8) + 1) * tmthing->info->damage;

        P_DamageMobj(thing, tmthing, tmthing, damage);

        tmthing->flags &= ~MF_SKULLFLY;
        tmthing->momx = tmthing->momy = tmthing->momz = 0;

        P_SetMobjState(tmthing, tmthing->info->spawnstate);

        /* Stop moving */
        return false;
    }

    /* Missiles can hit other things */
    if (tmthing->flags & MF_MISSILE)
    {
        /* See if it went over / under */
        if (tmthing->z > thing->z + thing->height)
            /* Overhead */
            return true;
        if (tmthing->z + tmthing->height < thing->z)
            /* Underneath */
            return true;

        if (tmthing->target && (tmthing->target->type == thing->type ||
                                (tmthing->target->type == MT_KNIGHT && thing->type == MT_BRUISER) ||
                                (tmthing->target->type == MT_BRUISER && thing->type == MT_KNIGHT)))
        {
            /* Don't hit same species as originator */
            if (thing == tmthing->target)
                return true;

            /*
             * sdh: Add deh_species_infighting here. We can override the
             * "monsters of the same species cant hurt each other" behavior
             * through dehacked patches.
             */
            if (thing->type != MT_PLAYER && !deh_species_infighting)
            {
                /*
                 * Explode, but do no damage.
                 * Let players missile other players.
                 */
                return false;
            }
        }

        if (!(thing->flags & MF_SHOOTABLE))
        {
            /* Didn't do any damage */
            return !(thing->flags & MF_SOLID);
        }

        /* Damage / explode */
        damage = ((P_Random() % 8) + 1) * tmthing->info->damage;
        P_DamageMobj(thing, tmthing, tmthing->target, damage);

        /* Don't traverse any more */
        return false;
    }

    /* Check for special pickup */
    if (thing->flags & MF_SPECIAL)
    {
        solid = (thing->flags & MF_SOLID) != 0;
        if (tmflags & MF_PICKUP)
        {
            /* Can remove thing */
            P_TouchSpecialThing(thing, tmthing);
        }
        return !solid;
    }

    return !(thing->flags & MF_SOLID);
}

/* Movement clipping. */

/*
 * This is purely informative, nothing is modified
 * (except things picked up).
 *
 * In:
 * A mobj_t (can be valid or invalid).
 * A position to be checked
 * (doesn't need to be related to the mobj_t->x,y).
 *
 * During:
 * Special things are touched if MF_PICKUP
 * early out on solid lines.
 *
 * Out:
 * newsubsec
 * floorz
 * ceilingz
 * tmdropoffz
 * Tthe lowest point contacted
 * (monsters won't move to a dropoff)/
 * speciallines[]
 * numspeciallines
 */
boolean P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y)
{
    int xl;
    int xh;
    int yl;
    int yh;
    int bx;
    int by;
    subsector_t *newsubsec;

    tmthing = thing;
    tmflags = thing->flags;

    tmx = x;
    tmy = y;

    tmbbox[BOXTOP] = y + tmthing->radius;
    tmbbox[BOXBOTTOM] = y - tmthing->radius;
    tmbbox[BOXRIGHT] = x + tmthing->radius;
    tmbbox[BOXLEFT] = x - tmthing->radius;

    newsubsec = R_PointInSubsector(x, y);
    ceilingline = NULL;

    /*
     * The base floor / ceiling is from the subsector
     * that contains the point.
     * Any contacted lines the step closer together
     * will adjust them.
     */
    tmfloorz = tmdropoffz = newsubsec->sector->floorheight;
    tmceilingz = newsubsec->sector->ceilingheight;

    validcount++;
    numspechit = 0;

    if (tmflags & MF_NOCLIP)
        return true;

    /*
     * Check things first, possibly picking things up.
     * The bounding box is extended by MAXRADIUS
     * because mobj_ts are grouped into mapblocks
     * based on their origin point, and can overlap
     * into adjacent blocks by up to MAXRADIUS units.
     */
    xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS) >> MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS) >> MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS) >> MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS) >> MAPBLOCKSHIFT;

    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            if (!P_BlockThingsIterator(bx, by, PIT_CheckThing))
                return false;

    /* Check lines */
    xl = (tmbbox[BOXLEFT] - bmaporgx) >> MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx) >> MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy) >> MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy) >> MAPBLOCKSHIFT;

    for (bx = xl; bx <= xh; bx++)
        for (by = yl; by <= yh; by++)
            if (!P_BlockLinesIterator(bx, by, PIT_CheckLine))
                return false;

    return true;
}

/*
 * Attempt to move to a new position,
 * crossing special lines unless MF_TELEPORT is set.
 */
boolean P_TryMove(mobj_t *thing, fixed_t x, fixed_t y)
{
    fixed_t oldx;
    fixed_t oldy;
    int side;
    int oldside;
    line_t *ld;

    floatok = false;
    if (!P_CheckPosition(thing, x, y))
        /* Solid wall or thing */
        return false;

    if (!(thing->flags & MF_NOCLIP))
    {
        if (tmceilingz - tmfloorz < thing->height)
            /* Doesn't fit */
            return false;

        floatok = true;

        if (!(thing->flags & MF_TELEPORT) && tmceilingz - thing->z < thing->height)
            /* mobj must lower itself to fit */
            return false;

        if (!(thing->flags & MF_TELEPORT) && tmfloorz - thing->z > 24 * FRACUNIT)
            /* Too big a step up */
            return false;

        if (!(thing->flags & (MF_DROPOFF | MF_FLOAT)) && tmfloorz - tmdropoffz > 24 * FRACUNIT)
            /* Don't stand over a dropoff */
            return false;
    }

    /* The move is ok, so link the thing into its new position */
    P_UnsetThingPosition(thing);

    oldx = thing->x;
    oldy = thing->y;
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;
    thing->x = x;
    thing->y = y;

    P_SetThingPosition(thing);

    /* If any special lines were hit, do the effect */
    if (!(thing->flags & (MF_TELEPORT | MF_NOCLIP)))
    {
        while (numspechit--)
        {
            /* See if the line was crossed */
            ld = spechit[numspechit];
            side = P_PointOnLineSide(thing->x, thing->y, ld);
            oldside = P_PointOnLineSide(oldx, oldy, ld);
            if (side != oldside)
            {
                if (ld->special)
                    P_CrossSpecialLine(ld - lines, oldside, thing);
            }
        }
    }

    return true;
}

/*
 * Takes a valid thing and adjusts the thing->floorz,
 * thing->ceilingz, and possibly thing->z.
 * This is called for all nearby monsters
 * whenever a sector changes height.
 * If the thing doesn't fit,
 * the z will be set to the lowest value
 * and false will be returned.
 */
boolean P_ThingHeightClip(mobj_t *thing)
{
    boolean onfloor;

    onfloor = (thing->z == thing->floorz);

    P_CheckPosition(thing, thing->x, thing->y);

    /* What about stranding a monster partially off an edge */
    thing->floorz = tmfloorz;
    thing->ceilingz = tmceilingz;

    if (onfloor)
    {
        /* Walking monsters rise and fall with the floor */
        thing->z = thing->floorz;
    }
    else
    {
        /* Don't adjust a floating monster unless forced to */
        if (thing->z + thing->height > thing->ceilingz)
            thing->z = thing->ceilingz - thing->height;
    }

    if (thing->ceilingz - thing->floorz < thing->height)
        return false;

    return true;
}

/*
 * Slide move.
 * Allows the player to slide along any angled walls.
 */
fixed_t bestslidefrac;
fixed_t secondslidefrac;

line_t *bestslideline;
line_t *secondslideline;

mobj_t *slidemo;

fixed_t tmxmove;
fixed_t tmymove;

/*
 * Adjusts the xmove / ymove,
 * so that the next move will slide along the wall.
 */
void P_HitSlideLine(line_t *ld)
{
    int side;

    angle_t lineangle;
    angle_t moveangle;
    angle_t deltaangle;

    fixed_t movelen;
    fixed_t newlen;
    slopetype_t slopetype = LineSlopeType(ld);

    if (slopetype == ST_HORIZONTAL)
    {
        tmymove = 0;
        return;
    }

    if (slopetype == ST_VERTICAL)
    {
        tmxmove = 0;
        return;
    }

    side = P_PointOnLineSide(slidemo->x, slidemo->y, ld);

    vector_t lv = LineVector(ld);

    lineangle = R_PointToAngle2(0, 0, lv.dx, lv.dy);

    if (side == 1)
        lineangle += ANG180;

    moveangle = R_PointToAngle2(0, 0, tmxmove, tmymove);
    deltaangle = moveangle - lineangle;

    if (deltaangle > ANG180)
        deltaangle += ANG180;
    /* I_Error ("SlideLine: ang>ANG180"); */

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;

    movelen = P_AproxDistance(tmxmove, tmymove);
    newlen = FixedMul(movelen, finecosine[deltaangle]);

    tmxmove = FixedMul(newlen, finecosine[lineangle]);
    tmymove = FixedMul(newlen, finesine[lineangle]);
}

boolean PTR_SlideTraverse(intercept_t *in)
{
    line_t *li;

    if (!in->isaline)
        I_Error("PTR_SlideTraverse: not a line?");

    li = in->d.line;

    if (!(LineFlags(li) & ML_TWOSIDED))
    {
        if (P_PointOnLineSide(slidemo->x, slidemo->y, li))
        {
            /* don't hit the back side */
            return true;
        }
        goto isblocking;
    }

    /* Set openrange, opentop, openbottom */
    P_LineOpening(li);

    if (openrange < slidemo->height)
        /* Doesn't fit */
        goto isblocking;

    if (opentop - slidemo->z < slidemo->height)
        /* mobj is too high */
        goto isblocking;

    if (openbottom - slidemo->z > 24 * FRACUNIT)
        /* Too big a step up */
        goto isblocking;

    /* This line doesn't block movement */
    return true;

/*
 * The line does block movement,
 * see if it is closer than best so far.
 */
isblocking:
    if (in->frac < bestslidefrac)
    {
        secondslidefrac = bestslidefrac;
        secondslideline = bestslideline;
        bestslidefrac = in->frac;
        bestslideline = li;
    }

    /* Stop */
    return false;
}

/*
 * The momx / momy move is bad, so try to slide
 * along a wall.
 * Find the first line hit, move flush to it,
 * and slide along it.
 *
 * This is a kludgy mess.
 */
void P_SlideMove(mobj_t *mo)
{
    fixed_t leadx;
    fixed_t leady;
    fixed_t trailx;
    fixed_t traily;
    fixed_t newx;
    fixed_t newy;
    int hitcount;

    slidemo = mo;
    hitcount = 0;

retry:
    if (++hitcount == 3)
        /* Don't loop forever */
        goto stairstep;

    /* Trace along the three leading corners */
    if (mo->momx > 0)
    {
        leadx = mo->x + mo->radius;
        trailx = mo->x - mo->radius;
    }
    else
    {
        leadx = mo->x - mo->radius;
        trailx = mo->x + mo->radius;
    }

    if (mo->momy > 0)
    {
        leady = mo->y + mo->radius;
        traily = mo->y - mo->radius;
    }
    else
    {
        leady = mo->y - mo->radius;
        traily = mo->y + mo->radius;
    }

    bestslidefrac = FRACUNIT + 1;

    P_PathTraverse(leadx, leady, leadx + mo->momx, leady + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(trailx, leady, trailx + mo->momx, leady + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);
    P_PathTraverse(leadx, traily, leadx + mo->momx, traily + mo->momy,
                   PT_ADDLINES, PTR_SlideTraverse);

    /* Move up to the wall */
    if (bestslidefrac == FRACUNIT + 1)
    {
        /* he move most have hit the middle, so stairstep */
    stairstep:
        if (!P_TryMove(mo, mo->x, mo->y + mo->momy))
            P_TryMove(mo, mo->x + mo->momx, mo->y);
        return;
    }

    /* Fudge a bit to make sure it doesn't hit */
    bestslidefrac -= 0x800;
    if (bestslidefrac > 0)
    {
        newx = FixedMul(mo->momx, bestslidefrac);
        newy = FixedMul(mo->momy, bestslidefrac);

        if (!P_TryMove(mo, mo->x + newx, mo->y + newy))
            goto stairstep;
    }

    /*
     * Now continue along the wall.
     * First calculate remainder.
     */
    bestslidefrac = FRACUNIT - (bestslidefrac + 0x800);

    if (bestslidefrac > FRACUNIT)
        bestslidefrac = FRACUNIT;

    if (bestslidefrac <= 0)
        return;

    tmxmove = FixedMul(mo->momx, bestslidefrac);
    tmymove = FixedMul(mo->momy, bestslidefrac);

    /* Clip the moves */
    P_HitSlideLine(bestslideline);

    mo->momx = tmxmove;
    mo->momy = tmymove;

    if (!P_TryMove(mo, mo->x + tmxmove, mo->y + tmymove))
    {
        goto retry;
    }
}

/* Who got hit (or NULL). */
mobj_t *linetarget;
mobj_t *shootthing;

/*
 * Height if not aiming up or down.
 * Use slope for monsters?
 */
fixed_t shootz;

int la_damage;
fixed_t attackrange;

fixed_t aimslope;

/* Slopes to top and bottom of target. */
extern fixed_t topslope;
extern fixed_t bottomslope;

/* Sets linetaget and aimslope when a target is aimed at. */
boolean PTR_AimTraverse(intercept_t *in)
{
    line_t *li;
    mobj_t *th;
    fixed_t slope;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;
    fixed_t dist;

    if (in->isaline)
    {
        li = in->d.line;
        sector_t *li_fs = LineFrontSector(li);
        sector_t *li_bs = LineBackSector(li);

        if (!(LineFlags(li) & ML_TWOSIDED))
            /* Stop */
            return false;

        /*
         * Crosses a two sided line.
         * A two sided line will restrict
         * the possible target ranges.
         */
        P_LineOpening(li);

        if (openbottom >= opentop)
            /* Stop */
            return false;

        dist = FixedMul(attackrange, in->frac);

        if (li_bs == NULL || li_fs->floorheight != li_bs->floorheight)
        {
            slope = FixedDiv(openbottom - shootz, dist);
            if (slope > bottomslope)
                bottomslope = slope;
        }

        if (li_bs == NULL || li_fs->ceilingheight != li_bs->ceilingheight)
        {
            slope = FixedDiv(opentop - shootz, dist);
            if (slope < topslope)
                topslope = slope;
        }

        if (topslope <= bottomslope)
            /* Stop */
            return false;

        /* Shot continues */
        return true;
    }

    /* Shoot a thing */
    th = in->d.thing;
    if (th == shootthing)
        /* Can't shoot self */
        return true;

    if (!(th->flags & MF_SHOOTABLE))
        /* Corpse or something */
        return true;

    /* Check angles to see if the thing can be aimed at */
    dist = FixedMul(attackrange, in->frac);
    thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

    if (thingtopslope < bottomslope)
        /* Shot over the thing */
        return true;

    thingbottomslope = FixedDiv(th->z - shootz, dist);

    if (thingbottomslope > topslope)
        /* Shot under the thing */
        return true;

    /* This thing can be hit */
    if (thingtopslope > topslope)
        thingtopslope = topslope;

    if (thingbottomslope < bottomslope)
        thingbottomslope = bottomslope;

    aimslope = (thingtopslope + thingbottomslope) / 2;
    linetarget = th;

    /* Don't go any farther */
    return false;
}

boolean PTR_ShootTraverse(intercept_t *in)
{
    fixed_t x;
    fixed_t y;
    fixed_t z;
    fixed_t frac;

    line_t *li;

    mobj_t *th;

    fixed_t slope;
    fixed_t dist;
    fixed_t thingtopslope;
    fixed_t thingbottomslope;

    if (in->isaline)
    {
        li = in->d.line;
        sector_t *li_fs = LineFrontSector(li);
        sector_t *li_bs = LineBackSector(li);

        if (li->special)
            P_ShootSpecialLine(shootthing, li);

        if (!(LineFlags(li) & ML_TWOSIDED))
            goto hitline;

        /* Crosses a two sided line */
        P_LineOpening(li);

        dist = FixedMul(attackrange, in->frac);

        /*
         * e6y: Emulation of missed back side on two-sided lines.
         * backsector can be NULL when emulating missing back side.
         */
        if (li_bs == NULL)
        {
            slope = FixedDiv(openbottom - shootz, dist);
            if (slope > aimslope)
                goto hitline;

            slope = FixedDiv(opentop - shootz, dist);
            if (slope < aimslope)
                goto hitline;
        }
        else
        {
            if (li_fs->floorheight != li_bs->floorheight)
            {
                slope = FixedDiv(openbottom - shootz, dist);
                if (slope > aimslope)
                    goto hitline;
            }

            if (li_fs->ceilingheight != li_bs->ceilingheight)
            {
                slope = FixedDiv(opentop - shootz, dist);
                if (slope < aimslope)
                    goto hitline;
            }
        }

        /* Shot continues */
        return true;

    /* Hit line */
    hitline:
        /* Position a bit closer */
        frac = in->frac - FixedDiv(4 * FRACUNIT, attackrange);
        x = trace.x + FixedMul(trace.dx, frac);
        y = trace.y + FixedMul(trace.dy, frac);
        z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

        if (li_fs->ceilingpic == skyflatnum)
        {
            /* Don't shoot the sky */
            if (z > li_fs->ceilingheight)
                return false;

            /* It's a sky hack wall */
            if (li_bs && li_bs->ceilingpic == skyflatnum)
                return false;
        }

        /* Spawn bullet puffs */
        P_SpawnPuff(x, y, z);

        /* Don't go any farther */
        return false;
    }

    /* Shoot a thing */
    th = in->d.thing;
    if (th == shootthing)
        /* Can't shoot self */
        return true;

    if (!(th->flags & MF_SHOOTABLE))
        /* Corpse or something */
        return true;

    /* Check angles to see if the thing can be aimed at */
    dist = FixedMul(attackrange, in->frac);
    thingtopslope = FixedDiv(th->z + th->height - shootz, dist);

    if (thingtopslope < aimslope)
        /* Shot over the thing */
        return true;

    thingbottomslope = FixedDiv(th->z - shootz, dist);

    if (thingbottomslope > aimslope)
        /* Shot under the thing */
        return true;

    /* Hit thing. Position a bit closer. */
    frac = in->frac - FixedDiv(10 * FRACUNIT, attackrange);

    x = trace.x + FixedMul(trace.dx, frac);
    y = trace.y + FixedMul(trace.dy, frac);
    z = shootz + FixedMul(aimslope, FixedMul(frac, attackrange));

    /*
     * Spawn bullet puffs or blod spots,
     * depending on target type.
     */
    if (in->d.thing->flags & MF_NOBLOOD)
        P_SpawnPuff(x, y, z);
    else
        P_SpawnBlood(x, y, z, la_damage);

    if (la_damage)
        P_DamageMobj(th, shootthing, shootthing, la_damage);

    /* Don't go any farther */
    return false;
}

fixed_t P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance)
{
    fixed_t x2;
    fixed_t y2;

    t1 = P_SubstNullMobj(t1);

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;

    x2 = t1->x + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->y + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;

    /* Can't shoot outside view angles */
    topslope = (SCREENHEIGHT / 2) * FRACUNIT / (SCREENWIDTH / 2);
    bottomslope = -(SCREENHEIGHT / 2) * FRACUNIT / (SCREENWIDTH / 2);

    attackrange = distance;
    linetarget = NULL;

    P_PathTraverse(t1->x, t1->y,
                   x2, y2,
                   PT_ADDLINES | PT_ADDTHINGS,
                   PTR_AimTraverse);

    if (linetarget)
        return aimslope;

    return 0;
}

/* If damage == 0, it is just a test trace that will leave linetarget set. */
void P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance, fixed_t slope,
                  int damage)
{
    fixed_t x2;
    fixed_t y2;

    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;
    x2 = t1->x + (distance >> FRACBITS) * finecosine[angle];
    y2 = t1->y + (distance >> FRACBITS) * finesine[angle];
    shootz = t1->z + (t1->height >> 1) + 8 * FRACUNIT;
    attackrange = distance;
    aimslope = slope;

    P_PathTraverse(t1->x, t1->y,
                   x2, y2,
                   PT_ADDLINES | PT_ADDTHINGS,
                   PTR_ShootTraverse);
}

/* Use lines. */
mobj_t *usething;

boolean PTR_UseTraverse(intercept_t *in)
{
    int side;

    if (!in->d.line->special)
    {
        P_LineOpening(in->d.line);
        if (openrange <= 0)
        {
            S_StartSound(usething, sfx_noway);

            /* Can't use through a wall */
            return false;
        }
        /* Not a special line, but keep checking */
        return true;
    }

    side = 0;
    if (P_PointOnLineSide(usething->x, usething->y, in->d.line) == 1)
        side = 1;

    /* Don't use back side */
    /* return false; */

    P_UseSpecialLine(usething, in->d.line, side);

    /* Can't use for than one special line in a row */
    return false;
}

/* Looks for special lines in front of the player to activate. */
void P_UseLines(player_t *player)
{
    int angle;
    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    usething = player->mo;

    angle = player->mo->angle >> ANGLETOFINESHIFT;

    x1 = player->mo->x;
    y1 = player->mo->y;
    x2 = x1 + (USERANGE >> FRACBITS) * finecosine[angle];
    y2 = y1 + (USERANGE >> FRACBITS) * finesine[angle];

    P_PathTraverse(x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse);
}

/* Radius attack. */
mobj_t *bombsource;
mobj_t *bombspot;
int bombdamage;

/* "bombsource" is the creature that caused the explosion at "bombspot". */
boolean PIT_RadiusAttack(mobj_t *thing)
{
    fixed_t dx;
    fixed_t dy;
    fixed_t dist;

    if (!(thing->flags & MF_SHOOTABLE))
        return true;

    /*
     * Boss spider and cyborg
     * take no damage from concussion.
     */
    if (thing->type == MT_CYBORG || thing->type == MT_SPIDER)
        return true;

    dx = abs(thing->x - bombspot->x);
    dy = abs(thing->y - bombspot->y);

    dist = dx > dy ? dx : dy;
    dist = (dist - thing->radius) >> FRACBITS;

    if (dist < 0)
        dist = 0;

    if (dist >= bombdamage)
        /* Out of range */
        return true;

    if (P_CheckSight(thing, bombspot))
    {
        /* Must be in direct path */
        P_DamageMobj(thing, bombspot, bombsource, bombdamage - dist);
    }

    return true;
}

/* Source is the creature that caused the explosion at spot. */
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage)
{
    int x;
    int y;

    int xl;
    int xh;
    int yl;
    int yh;

    fixed_t dist;

    dist = (damage + MAXRADIUS) << FRACBITS;
    yh = (spot->y + dist - bmaporgy) >> MAPBLOCKSHIFT;
    yl = (spot->y - dist - bmaporgy) >> MAPBLOCKSHIFT;
    xh = (spot->x + dist - bmaporgx) >> MAPBLOCKSHIFT;
    xl = (spot->x - dist - bmaporgx) >> MAPBLOCKSHIFT;
    bombspot = spot;
    bombsource = source;
    bombdamage = damage;

    for (y = yl; y <= yh; y++)
        for (x = xl; x <= xh; x++)
            P_BlockThingsIterator(x, y, PIT_RadiusAttack);
}

/*
 * Sector height changing.
 * After modifying a sectors floor or ceiling height,
 * call this routine to adjust the positions
 * of all things that touch the sector.
 *
 * If anything doesn't fit anymore, true will be returned.
 * If crunch is true, they will take damage as they are being crushed.
 * If crunch is false, you should set the sector height back
 * the way it was and call P_ChangeSector again to undo the changes.
 */
boolean crushchange;
boolean nofit;

boolean PIT_ChangeSector(mobj_t *thing)
{
    mobj_t *mo;

    if (P_ThingHeightClip(thing))
    {
        /* Keep checking */
        return true;
    }

    /* Crunch bodies to giblets */
    if (thing->health <= 0)
    {
        P_SetMobjState(thing, S_GIBS);

        thing->flags &= ~MF_SOLID;
        thing->height = 0;
        thing->radius = 0;

        /* Keep checking */
        return true;
    }

    /* Crunch dropped items */
    if (thing->flags & MF_DROPPED)
    {
        P_RemoveMobj(thing);

        /* Keep checking */
        return true;
    }

    if (!(thing->flags & MF_SHOOTABLE))
    {
        /* Assume it is bloody gibs or something */
        return true;
    }

    nofit = true;

    if (crushchange && !(leveltime & 3))
    {
        P_DamageMobj(thing, NULL, NULL, 10);

        /* Spray blood in a random direction */
        mo = P_SpawnMobj(thing->x,
                         thing->y,
                         thing->z + thing->height / 2, MT_BLOOD);

        mo->momx = P_SubRandom() << 12;
        mo->momy = P_SubRandom() << 12;
    }

    /* Keep checking (crush other things) */
    return true;
}

boolean P_ChangeSector(sector_t *sector, boolean crunch)
{
    int x;
    int y;

    nofit = false;
    crushchange = crunch;

    /* Re-check heights for all things near the moving sector */
    for (x = sector->blockbox[BOXLEFT]; x <= sector->blockbox[BOXRIGHT]; x++)
        for (y = sector->blockbox[BOXBOTTOM]; y <= sector->blockbox[BOXTOP]; y++)
            P_BlockThingsIterator(x, y, PIT_ChangeSector);

    return nofit;
}

/*
 * Code to emulate the behavior of Vanilla Doom when encountering an overrun
 * of the spechit array. This is by Andrey Budko (e6y) and comes from his
 * PrBoom plus port. A big thanks to Andrey for this.
 */
static void SpechitOverrun(line_t *ld)
{
    static unsigned int baseaddr = 0;
    unsigned int addr;

    if (baseaddr == 0)
    {
        int p;

        /*
         * This is the first time we have had an overrun. Work out
         * what base address we are going to use.
         * Allow a spechit value to be specified on the command line.
         */
        //!
        // @category compat
        // @arg <n>
        //
        // Use the specified magic value when emulating spechit overruns.
        //
        p = M_CheckParmWithArgs("-spechit", 1);

        if (p > 0)
        {
            M_StrToInt(myargv[p + 1], (int *)&baseaddr);
        }
        else
        {
            baseaddr = DEFAULT_SPECHIT_MAGIC;
        }
    }

    /* Calculate address used in doom2.exe */
    addr = baseaddr + (ld - lines) * 0x3E;

    switch (numspechit)
    {
    case 9:
    case 10:
    case 11:
    case 12:
        tmbbox[numspechit - 9] = addr;
        break;
    case 13:
        crushchange = addr;
        break;
    case 14:
        nofit = addr;
        break;
    default:
        fprintf(stderr, "SpechitOverrun: Warning: unable to emulate"
                        "an overrun where numspechit=%i\n",
                numspechit);
        break;
    }
}
