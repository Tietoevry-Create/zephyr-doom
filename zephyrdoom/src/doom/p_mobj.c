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
 * Moving object handling. Spawn functions.
 */

#include <stdio.h>

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
#include "sounds.h"

#include "st_stuff.h"
#include "hu_stuff.h"

#include "s_sound.h"

#include "doomstat.h"

/*
 * NRFD-TODO
 * Optimize MAX_MOBJ.
 */
#define MAX_MOBJ 290

mobj_t mobjs[MAX_MOBJ];

void G_PlayerReborn(int player);
void P_SpawnMapThing(mapthing_t *mthing);

/*
 * Use a heuristic approach to detect infinite state cycles. Count the number
 * of times the loop in P_SetMobjState() executes and exit with an error once
 * an arbitrary very large limit is reached.
 */
#define MOBJ_CYCLE_LIMIT 1000000

/* Returns true if the mobj is still present. */
boolean P_SetMobjState(mobj_t *mobj, statenum_t state)
{
    const state_t *st;
    int cycle_counter = 0;

    do
    {
        if (state == S_NULL)
        {
            mobj->state = (state_t *)S_NULL;
            P_RemoveMobj(mobj);
            return false;
        }

        st = &states[state];
        mobj->state = st;
        mobj->tics = st->tics;
        mobj->sprite = st->sprite;

        /*
         * // NRFD-TODO
         * mobj->frame = st->frame;
         */

        /*
         * Modified handling.
         * Call action functions when the state is set.
         */
        if (st->action.acp1)
            st->action.acp1(mobj);

        state = st->nextstate;

        if (cycle_counter++ > MOBJ_CYCLE_LIMIT)
        {
            I_Error("P_SetMobjState: Infinite state cycle detected!");
        }
    } while (!mobj->tics);

    return true;
}

void P_ExplodeMissile(mobj_t *mo)
{
    mo->momx = mo->momy = mo->momz = 0;

    P_SetMobjState(mo, mobjinfo[mo->type].deathstate);

    mo->tics -= P_Random() & 3;

    if (mo->tics < 1)
        mo->tics = 1;

    mo->flags &= ~MF_MISSILE;

    if (mo->info->deathsound)
        S_StartSound(mo, mo->info->deathsound);
}

#define STOPSPEED 0x1000
#define FRICTION 0xe800

void P_XYMovement(mobj_t *mo)
{
    fixed_t ptryx;
    fixed_t ptryy;
    player_t *player;
    fixed_t xmove;
    fixed_t ymove;

    if (!mo->momx && !mo->momy)
    {
        if (mo->flags & MF_SKULLFLY)
        {
            /* The skull slammed into something */
            mo->flags &= ~MF_SKULLFLY;
            mo->momx = mo->momy = mo->momz = 0;

            P_SetMobjState(mo, mo->info->spawnstate);
        }
        return;
    }

    player = mo->player;

    if (mo->momx > MAXMOVE)
        mo->momx = MAXMOVE;
    else if (mo->momx < -MAXMOVE)
        mo->momx = -MAXMOVE;

    if (mo->momy > MAXMOVE)
        mo->momy = MAXMOVE;
    else if (mo->momy < -MAXMOVE)
        mo->momy = -MAXMOVE;

    xmove = mo->momx;
    ymove = mo->momy;

    do
    {
        if (xmove > MAXMOVE / 2 || ymove > MAXMOVE / 2)
        {
            ptryx = mo->x + xmove / 2;
            ptryy = mo->y + ymove / 2;
            xmove >>= 1;
            ymove >>= 1;
        }
        else
        {
            ptryx = mo->x + xmove;
            ptryy = mo->y + ymove;
            xmove = ymove = 0;
        }

        if (!P_TryMove(mo, ptryx, ptryy))
        {
            /* Blocked move */
            if (mo->player)
            {
                /* Try to slide along it */
                P_SlideMove(mo);
            }
            else if (mo->flags & MF_MISSILE)
            {
                /* Explode a missile */
                if (ceilingline &&
                    LineBackSector(ceilingline) &&
                    LineBackSector(ceilingline)->ceilingpic == skyflatnum)
                {
                    /*
                     * Hack to prevent missiles exploding against the sky.
                     * Does not handle sky floors.
                     */
                    P_RemoveMobj(mo);
                    return;
                }
                P_ExplodeMissile(mo);
            }
            else
                mo->momx = mo->momy = 0;
        }
    } while (xmove || ymove);

    /* Slow down */
    if (player && player->cheats & CF_NOMOMENTUM)
    {
        /* Debug option for no sliding at all */
        mo->momx = mo->momy = 0;
        return;
    }

    if (mo->flags & (MF_MISSILE | MF_SKULLFLY))
        /* No friction for missiles ever */
        return;

    if (mo->z > mo->floorz)
        /* No friction when airborne */
        return;

    if (mo->flags & MF_CORPSE)
    {
        /*
         * Do not stop sliding.
         * If halfway off a step with some momentum.
         */
        if (mo->momx > FRACUNIT / 4 || mo->momx < -FRACUNIT / 4 || mo->momy > FRACUNIT / 4 || mo->momy < -FRACUNIT / 4)
        {
            if (mo->floorz != mo->subsector->sector->floorheight)
                return;
        }
    }

    if (mo->momx > -STOPSPEED && mo->momx < STOPSPEED && mo->momy > -STOPSPEED && mo->momy < STOPSPEED && (!player || (player->cmd.forwardmove == 0 && player->cmd.sidemove == 0)))
    {
        /* If in a walking frame, stop moving */
        if (player && (unsigned)((player->mo->state - states) - S_PLAY_RUN1) < 4)
            P_SetMobjState(player->mo, S_PLAY);

        mo->momx = 0;
        mo->momy = 0;
    }
    else
    {
        mo->momx = FixedMul(mo->momx, FRICTION);
        mo->momy = FixedMul(mo->momy, FRICTION);
    }
}

void P_ZMovement(mobj_t *mo)
{
    fixed_t dist;
    fixed_t delta;

    /* Check for smooth step up */
    if (mo->player && mo->z < mo->floorz)
    {
        mo->player->viewheight -= mo->floorz - mo->z;

        mo->player->deltaviewheight = (VIEWHEIGHT - mo->player->viewheight) >> 3;
    }

    /* Adjust height */
    mo->z += mo->momz;

    if (mo->flags & MF_FLOAT && mo->target)
    {
        /* Float down towards target if too close */
        if (!(mo->flags & MF_SKULLFLY) && !(mo->flags & MF_INFLOAT))
        {
            dist = P_AproxDistance(mo->x - mo->target->x,
                                   mo->y - mo->target->y);

            delta = (mo->target->z + (mo->height >> 1)) - mo->z;

            if (delta < 0 && dist < -(delta * 3))
                mo->z -= FLOATSPEED;
            else if (delta > 0 && dist < (delta * 3))
                mo->z += FLOATSPEED;
        }
    }

    /* Clip movement */
    if (mo->z <= mo->floorz)
    {
        /*
         * Hit the floor.
         *
         * Note (id):
         * Somebody left this after the setting momz to 0,
         * kinda useless there.
         *
         * cph - This was the a bug in the linuxdoom-1.10 source which
         * caused it not to sync Doom 2 v1.9 demos. Someone
         * added the above comment and moved up the following code. So
         * demos would desync in close lost soul fights.
         * Note that this only applies to original Doom 1 or Doom2 demos - not
         * Final Doom and Ultimate Doom. So we test demo_compatibility *and*
         * gamemission. (Note we assume that Doom1 is always Ult Doom, which
         * seems to hold for most published demos.)
         *
         * fraggle - cph got the logic here slightly wrong. There are three
         * versions of Doom 1.9:
         * The version used in registered doom 1.9 + doom2 - no bounce.
         * The version used in ultimate doom - has bounce.
         * The version used in final doom - has bounce.
         *
         * So we need to check that this is either retail or commercial
         * (but not doom2).
         */

        int correct_lost_soul_bounce = gameversion >= exe_ultimate;

        if (correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
        {
            /* The skull slammed into something */
            mo->momz = -mo->momz;
        }

        if (mo->momz < 0)
        {
            if (mo->player && mo->momz < -GRAVITY * 8)
            {
                /*
                 * Squat down.
                 * Decrease viewheight for a moment
                 * after hitting the ground (hard),
                 * and utter appropriate sound.
                 */
                mo->player->deltaviewheight = mo->momz >> 3;
                S_StartSound(mo, sfx_oof);
            }
            mo->momz = 0;
        }
        mo->z = mo->floorz;

        /*
         * cph 2001/05/26
         * See lost soul bouncing comment above. We need this here for bug
         * compatibility with original Doom2 v1.9 - if a soul is charging and
         * hit by a raising floor this incorrectly reverses its Y momentum.
         */
        if (!correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
            mo->momz = -mo->momz;

        if ((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }
    else if (!(mo->flags & MF_NOGRAVITY))
    {
        if (mo->momz == 0)
            mo->momz = -GRAVITY * 2;
        else
            mo->momz -= GRAVITY;
    }

    if (mo->z + mo->height > mo->ceilingz)
    {
        /* Hit the ceiling */
        if (mo->momz > 0)
            mo->momz = 0;
        {
            mo->z = mo->ceilingz - mo->height;
        }

        if (mo->flags & MF_SKULLFLY)
        {
            /* The skull slammed into something */
            mo->momz = -mo->momz;
        }

        if ((mo->flags & MF_MISSILE) && !(mo->flags & MF_NOCLIP))
        {
            P_ExplodeMissile(mo);
            return;
        }
    }
}

void P_NightmareRespawn(mobj_t *mobj)
{
    /*
     * // NRFD-TODO
     * fixed_t x;
     * fixed_t y;
     * fixed_t z;
     * subsector_t *ss;
     * mobj_t *mo;
     * mapthing_t *mthing;
     *
     * x = mobj->spawnpoint.x << FRACBITS;
     * y = mobj->spawnpoint.y << FRACBITS;
     *
     * // Something is occupying it's position
     * if (!P_CheckPosition(mobj, x, y))
     *     return; // no respwan
     *
     * // Spawn a teleport fog at old spot.
     * // Because of removal of the body?
     * mo = P_SpawnMobj(mobj->x,
     *                  mobj->y,
     *                  mobj->subsector->sector->floorheight, MT_TFOG);
     *
     * // Initiate teleport sound
     * S_StartSound(mo, sfx_telept);
     *
     * // Spawn a teleport fog at the new spot
     * ss = R_PointInSubsector(x, y);
     *
     * mo = P_SpawnMobj(x, y, ss->sector->floorheight, MT_TFOG);
     *
     * S_StartSound(mo, sfx_telept);
     *
     * // Spawn the new monster
     * mthing = &mobj->spawnpoint;
     *
     * // Spawn it
     * if (mobj->info->flags & MF_SPAWNCEILING)
     *     z = ONCEILINGZ;
     * else
     *     z = ONFLOORZ;
     *
     * // Inherit attributes from deceased one
     * mo = P_SpawnMobj(x, y, z, mobj->type);
     * mo->spawnpoint = mobj->spawnpoint;
     * mo->angle = ANG45 * (mthing->angle / 45);
     *
     * if (mthing->options & MTF_AMBUSH)
     *     mo->flags |= MF_AMBUSH;
     *
     * mo->reactiontime = 18;
     *
     * // Remove the old monster,
     * P_RemoveMobj(mobj);
     */
}

void P_MobjThinker(mobj_t *mobj)
{
    /* Momentum movement */
    if (mobj->momx || mobj->momy || (mobj->flags & MF_SKULLFLY))
    {
        P_XYMovement(mobj);

        /*
         * TODO
         * Decent NOP/NULL/Nil function pointer please.
         */
        if (mobj->thinker.function.acv == (actionf_v)(-1))
            /* mobj was removed */
            return;
    }
    if ((mobj->z != mobj->floorz) || mobj->momz)
    {
        P_ZMovement(mobj);

        /*
         * TODO
         * Decent NOP/NULL/Nil function pointer please.
         */
        if (mobj->thinker.function.acv == (actionf_v)(-1))
            /* mobj was removed */
            return;
    }

    /*
     * Cycle through states,
     * calling action functions at transitions.
     */
    if (mobj->tics != -1)
    {
        mobj->tics--;

        /* You can cycle through multiple states in a tic */
        if (!mobj->tics)
            if (!P_SetMobjState(mobj, mobj->state->nextstate))
                /* Freed itself */
                return;
    }
    else
    {
        /* Check for nightmare respawn */
        if (!(mobj->flags & MF_COUNTKILL))
            return;

        if (!respawnmonsters)
            return;

        mobj->movecount++;

        if (mobj->movecount < 12 * TICRATE)
            return;

        if (leveltime & 31)
            return;

        if (P_Random() > 4)
            return;

        P_NightmareRespawn(mobj);
    }
}

void P_InitMobjs(int num)
{
    printf("P_InitMobjs %d\n", num);
    int i;
    for (i = 0; i < MAX_MOBJ; i++)
    {
        memset(&mobjs[i], 0, sizeof(*mobjs));
        mobjs[i].type = MT_FREE;
    }
}

mobj_t *P_AllocMobj(void)
{
    mobj_t *mobj;
    int i;
    for (i = 0; i < MAX_MOBJ; i++)
    {
        if (mobjs[i].type == MT_FREE)
        {
            mobj = &mobjs[i];
            memset(mobj, 0, sizeof(*mobj));
            mobj->type = MT_ALLOC;
            return mobj;
        }
    }

    printf("P_AllocMobj: Out of free mobjs\n");
    mobj = Z_Malloc(sizeof(*mobj), PU_LEVEL, NULL);
    memset(mobj, 0, sizeof(*mobj));
    return mobj;
}

void P_FreeMobj(mobj_t *mobj)
{
    if (mobj >= mobjs && mobj < &mobjs[MAX_MOBJ])
    {
        /* In pre-allocated buffer */
        mobj->type = MT_FREE;
    }
    else
    {
        Z_Free(mobj);
        /* Dynamically allocated */
    }
}

mobj_t *P_SpawnMobj(fixed_t x, fixed_t y, fixed_t z, mobjtype_t type)
{
    mobj_t *mobj;
    const state_t *st;
    const mobjinfo_t *info;

    mobj = P_AllocMobj();
    info = &mobjinfo[type];
    mobj->type = type;
    mobj->info = info;
    mobj->x = x;
    mobj->y = y;
    mobj->radius = info->radius;
    mobj->height = info->height;
    mobj->flags = info->flags;
    mobj->health = info->spawnhealth;

    if (gameskill != sk_nightmare)
        mobj->reactiontime = info->reactiontime;

    mobj->lastlook = P_Random() % MAXPLAYERS;

    /*
     * Do not set the state with P_SetMobjState,
     * because action routines can not be called yet.
     */
    st = &states[info->spawnstate];

    mobj->state = st;
    mobj->tics = st->tics;
    mobj->sprite = st->sprite;

    /*
     * // NRFD-TODO
     * mobj->frame = st->frame;
     */

    /* Set subsector and/or block links */
    P_SetThingPosition(mobj);

    mobj->floorz = mobj->subsector->sector->floorheight;
    mobj->ceilingz = mobj->subsector->sector->ceilingheight;

    if (z == ONFLOORZ)
        mobj->z = mobj->floorz;
    else if (z == ONCEILINGZ)
        mobj->z = mobj->ceilingz - mobj->info->height;
    else
        mobj->z = z;

    mobj->thinker.function.acp1 = (actionf_p1)P_MobjThinker;

    P_AddThinker(&mobj->thinker);
    return mobj;
}

mapthing_t itemrespawnque[ITEMQUESIZE];
int itemrespawntime[ITEMQUESIZE];
int iquehead;
int iquetail;

void P_RemoveMobj(mobj_t *mobj)
{
    /*
     * // NRFD-TODO
     * // Respawn.
     * if ((mobj->flags & MF_SPECIAL) && !(mobj->flags & MF_DROPPED) && (mobj->type != MT_INV) && (mobj->type != MT_INS))
     * {
     *     itemrespawnque[iquehead] = mobj->spawnpoint;
     *     itemrespawntime[iquehead] = leveltime;
     *     iquehead = (iquehead + 1) & (ITEMQUESIZE - 1);
     *
     *     // Lose one off the end
     *     if (iquehead == iquetail)
     *         iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
     * }
     */

    /* Unlink from sector and block lists */
    P_UnsetThingPosition(mobj);

    /* Stop any playing sound */
    S_StopSound(mobj);

    /* Free block */
    P_RemoveThinkerMobj((thinker_t *)mobj);
}

void P_RespawnSpecials(void)
{
    fixed_t x;
    fixed_t y;
    fixed_t z;

    subsector_t *ss;
    mobj_t *mo;
    mapthing_t *mthing;

    int i;

    /* Only respawn items in deathmatch */
    if (deathmatch != 2)
        return; //

    /* Nothing left to respawn */
    if (iquehead == iquetail)
        return;

    /* Wait at least 30 seconds */
    if (leveltime - itemrespawntime[iquetail] < 30 * TICRATE)
        return;

    mthing = &itemrespawnque[iquetail];

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    /* Spawn a teleport fog at the new spot */
    ss = R_PointInSubsector(x, y);
    mo = P_SpawnMobj(x, y, ss->sector->floorheight, MT_IFOG);
    S_StartSound(mo, sfx_itmbk);

    /* Find which type to spawn */
    for (i = 0; i < NUMMOBJTYPES; i++)
    {
        if (mthing->type == mobjinfo[i].doomednum)
            break;
    }

    if (i >= NUMMOBJTYPES)
    {
        I_Error("P_RespawnSpecials: Failed to find mobj type with doomednum "
                "%d when respawning thing. This would cause a buffer overrun "
                "in vanilla Doom",
                mthing->type);
    }

    /* Spawn it */
    if (mobjinfo[i].flags & MF_SPAWNCEILING)
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    mo = P_SpawnMobj(x, y, z, i);

    /*
     * // NRFD-TODO
     * Nightmare.
     * mo->spawnpoint = *mthing;
     */

    mo->angle = ANG45 * (mthing->angle / 45);

    /* Pull it from the que */
    iquetail = (iquetail + 1) & (ITEMQUESIZE - 1);
}

/*
* Called when a player is spawned on the level.
* Most of the player structure stays unchanged
* between levels.
/*/
void P_SpawnPlayer(mapthing_t *mthing)
{
    player_t *p;
    fixed_t x;
    fixed_t y;
    fixed_t z;

    mobj_t *mobj;

    int i;

    if (mthing->type == 0)
    {
        return;
    }

    /* Not playing */
    if (!playeringame[mthing->type - 1])
        return;

    p = &players[mthing->type - 1];

    if (p->playerstate == PST_REBORN)
        G_PlayerReborn(mthing->type - 1);

    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;
    z = ONFLOORZ;
    mobj = P_SpawnMobj(x, y, z, MT_PLAYER);

    /* Set color translations for player sprites */
    if (mthing->type > 1)
        mobj->flags |= (mthing->type - 1) << MF_TRANSSHIFT;

    mobj->angle = ANG45 * (mthing->angle / 45);
    mobj->player = p;
    mobj->health = p->health;

    p->mo = mobj;
    p->playerstate = PST_LIVE;
    p->refire = 0;
    p->message = NULL;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->viewheight = VIEWHEIGHT;

    /* Setup gun psprite */
    P_SetupPsprites(p);

    /* Give all cards in death match mode */
    if (deathmatch)
        for (i = 0; i < NUMCARDS; i++)
            p->cards[i] = true;

    if (mthing->type - 1 == consoleplayer)
    {
        /* Wake up the status bar */
        ST_Start();

        /* Wake up the heads up text */
        HU_Start();
    }
}

/*
 * The fields of the mapthing should
 * already be in host byte order.
 */
void P_SpawnMapThing(mapthing_t *mthing)
{
    int i;
    int bit;
    mobj_t *mobj;
    fixed_t x;
    fixed_t y;
    fixed_t z;

    /* Count deathmatch start positions */
    if (mthing->type == 11)
    {
        if (deathmatch_p < &deathmatchstarts[10])
        {
            memcpy(deathmatch_p, mthing, sizeof(*mthing));
            deathmatch_p++;
        }
        return;
    }

    if (mthing->type <= 0)
    {
        /*
         * Thing type 0 is actually "player -1 start".
         * For some reason, Vanilla Doom accepts/ignores this.
         */
        return;
    }

    /* Check for players specially */
    if (mthing->type <= 4)
    {
        /* Save spots for respawning in network games */
        playerstarts[mthing->type - 1] = *mthing;
        if (!deathmatch)
            P_SpawnPlayer(mthing);

        return;
    }

    /* Check for apropriate skill level */
    if (!netgame && (mthing->options & 16))
        return;

    if (gameskill == sk_baby)
        bit = 1;
    else if (gameskill == sk_nightmare)
        bit = 4;
    else
        bit = 1 << (gameskill - 1);

    if (!(mthing->options & bit))
        return;

    /* Find which type to spawn */
    for (i = 0; i < NUMMOBJTYPES; i++)
        if (mthing->type == mobjinfo[i].doomednum)
            break;

    if (i == NUMMOBJTYPES)
        I_Error("P_SpawnMapThing: Unknown type %i at (%i, %i)",
                mthing->type,
                mthing->x, mthing->y);

    /* Don't spawn keycards and players in deathmatch */
    if (deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
        return;

    /* Don't spawn any monsters if -nomonsters */
    if (nomonsters && (i == MT_SKULL || (mobjinfo[i].flags & MF_COUNTKILL)))
    {
        return;
    }

    /* Spawn it */
    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (mobjinfo[i].flags & MF_SPAWNCEILING)
        z = ONCEILINGZ;
    else
        z = ONFLOORZ;

    mobj = P_SpawnMobj(x, y, z, i);

    /*
     * // NRFD-TODO
     * // Nightmare
     * mobj->spawnpoint = *mthing;
     */

    if (mobj->tics > 0)
        mobj->tics = 1 + (P_Random() % mobj->tics);
    if (mobj->flags & MF_COUNTKILL)
        totalkills++;
    if (mobj->flags & MF_COUNTITEM)
        totalitems++;

    mobj->angle = ANG45 * (mthing->angle / 45);
    if (mthing->options & MTF_AMBUSH)
        mobj->flags |= MF_AMBUSH;
}

/* Game spawn functions. */

extern fixed_t attackrange;

void P_SpawnPuff(fixed_t x, fixed_t y, fixed_t z)
{
    mobj_t *th;

    z += (P_SubRandom() << 10);

    th = P_SpawnMobj(x, y, z, MT_PUFF);
    th->momz = FRACUNIT;
    th->tics -= P_Random() & 3;

    if (th->tics < 1)
        th->tics = 1;

    /* Don't make punches spark on the wall */
    if (attackrange == MELEERANGE)
        P_SetMobjState(th, S_PUFF3);
}

void P_SpawnBlood(fixed_t x, fixed_t y, fixed_t z, int damage)
{
    mobj_t *th;

    z += (P_SubRandom() << 10);
    th = P_SpawnMobj(x, y, z, MT_BLOOD);
    th->momz = FRACUNIT * 2;
    th->tics -= P_Random() & 3;

    if (th->tics < 1)
        th->tics = 1;

    if (damage <= 12 && damage >= 9)
        P_SetMobjState(th, S_BLOOD2);
    else if (damage < 9)
        P_SetMobjState(th, S_BLOOD3);
}

/*
 * Moves the missile forward a bit
 * and possibly explodes it right there.
 */
void P_CheckMissileSpawn(mobj_t *th)
{
    th->tics -= P_Random() & 3;
    if (th->tics < 1)
        th->tics = 1;

    /*
     * Move a little forward so an angle can
     * be computed if it immediately explodes.
     */
    th->x += (th->momx >> 1);
    th->y += (th->momy >> 1);
    th->z += (th->momz >> 1);

    if (!P_TryMove(th, th->x, th->y))
        P_ExplodeMissile(th);
}

/*
 * Certain functions assume that a mobj_t pointer is non-NULL,
 * causing a crash in some situations where it is NULL. Vanilla
 * Doom did not crash because of the lack of proper memory
 * protection. This function substitutes NULL pointers for
 * pointers to a dummy mobj, to avoid a crash.
 */

/*
 * NRFD-TODO
 * This might crash if code tries to modify this object.
 */
const mobj_t dummy_mobj;

mobj_t *P_SubstNullMobj(mobj_t *mobj)
{
    if (mobj == NULL)
    {
        return (mobj_t *)&dummy_mobj;

        /*
        // TODO
        static mobj_t dummy_mobj;

        dummy_mobj.x = 0;
        dummy_mobj.y = 0;
        dummy_mobj.z = 0;
        dummy_mobj.flags = 0;

        mobj = &dummy_mobj;
        */
    }

    return mobj;
}

mobj_t *P_SpawnMissile(mobj_t *source, mobj_t *dest, mobjtype_t type)
{
    mobj_t *th;
    angle_t an;
    int dist;

    th = P_SpawnMobj(source->x,
                     source->y,
                     source->z + 4 * 8 * FRACUNIT, type);

    if (th->info->seesound)
        S_StartSound(th, th->info->seesound);

    /* Where it came from */
    th->target = source;
    an = R_PointToAngle2(source->x, source->y, dest->x, dest->y);

    /* Fuzzy player */
    if (dest->flags & MF_SHADOW)
        an += P_SubRandom() << 20;

    th->angle = an;
    an >>= ANGLETOFINESHIFT;
    th->momx = FixedMul(th->info->speed, finecosine[an]);
    th->momy = FixedMul(th->info->speed, finesine[an]);

    dist = P_AproxDistance(dest->x - source->x, dest->y - source->y);
    dist = dist / th->info->speed;

    if (dist < 1)
        dist = 1;

    th->momz = (dest->z - source->z) / dist;
    P_CheckMissileSpawn(th);

    return th;
}

/* Tries to aim at a nearby monster. */
void P_SpawnPlayerMissile(mobj_t *source, mobjtype_t type)
{
    mobj_t *th;
    angle_t an;

    fixed_t x;
    fixed_t y;
    fixed_t z;
    fixed_t slope;

    /* See which target is to be aimed at */
    an = source->angle;
    slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);

    if (!linetarget)
    {
        an += 1 << 26;
        slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);

        if (!linetarget)
        {
            an -= 2 << 26;
            slope = P_AimLineAttack(source, an, 16 * 64 * FRACUNIT);
        }

        if (!linetarget)
        {
            an = source->angle;
            slope = 0;
        }
    }

    x = source->x;
    y = source->y;
    z = source->z + 4 * 8 * FRACUNIT;

    th = P_SpawnMobj(x, y, z, type);

    if (th->info->seesound)
        S_StartSound(th, th->info->seesound);

    th->target = source;
    th->angle = an;
    th->momx = FixedMul(th->info->speed,
                        finecosine[an >> ANGLETOFINESHIFT]);
    th->momy = FixedMul(th->info->speed,
                        finesine[an >> ANGLETOFINESHIFT]);
    th->momz = FixedMul(th->info->speed, slope);

    P_CheckMissileSpawn(th);
}
