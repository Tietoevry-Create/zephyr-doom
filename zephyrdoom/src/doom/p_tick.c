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
//      Archiving: SaveGame I/O.
//      Thinker, Ticker.
//


#include "z_zone.h"
#include "p_local.h"

#include "doomstat.h"


int     leveltime;

//
// THINKERS
// All thinkers should be allocated by Z_Malloc
// so they can be operated on uniformly.
// The actual structures will vary in size,
// but the first element must be thinker_t.
//



// Both the head and tail of the thinker list.
thinker_t       thinkercap;


//
// P_InitThinkers
//
void P_InitThinkers (void)
{
    printf("P_InitThinkers\n");
    thinkercap.prev = thinkercap.next  = &thinkercap;
}



#include "compiler_abstraction.h"

//
// P_AddThinker
// Adds a new thinker at the end of the list.
//
void P_AddThinker (thinker_t* thinker)
{
    thinkercap.prev->next = thinker;
    __ASM volatile("": : :"memory");
    thinker->next = &thinkercap;
    __ASM volatile("": : :"memory");
    thinker->prev = thinkercap.prev;
    __ASM volatile("": : :"memory");
    thinkercap.prev = thinker;
}



//
// P_RemoveThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
void P_RemoveThinker (thinker_t* thinker)
{
  // FIXME: NOP.
  thinker->function.acv = (actionf_v)(-1);
}

void P_RemoveThinkerMobj (thinker_t* thinker)
{
  thinker->function.acv = (actionf_v)(-2);
}



//
// P_AllocateThinker
// Allocates memory and adds a new thinker at the end of the list.
//
void P_AllocateThinker (thinker_t*      thinker)
{
}

void P_FreeMobj (mobj_t* mobj);

//
// P_RunThinkers
//
void P_RunThinkers (void)
{
    thinker_t *currentthinker, *nextthinker;

    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
        if ( currentthinker->function.acv == (actionf_v)(-1) )
        {
            // time to remove it
            // printf("Free thinker\n");
            nextthinker = currentthinker->next;
            currentthinker->next->prev = currentthinker->prev;
            currentthinker->prev->next = currentthinker->next;
            Z_Free(currentthinker);
        }
        else if ( currentthinker->function.acv == (actionf_v)(-2) )
        {
            // time to remove it
            // printf("Free mobj\n");
            nextthinker = currentthinker->next;
            currentthinker->next->prev = currentthinker->prev;
            currentthinker->prev->next = currentthinker->next;
            P_FreeMobj((mobj_t*)currentthinker);
        }
        else
        {
            if (currentthinker->function.acp1)
                currentthinker->function.acp1 (currentthinker);
            nextthinker = currentthinker->next;
        }
        currentthinker = nextthinker;
    }
}



//
// P_Ticker
//

void P_Ticker (void)
{
    int         i;

    // run the tic
    if (paused)
        return;

    // pause if in menu and at least one tic has been run
    if ( !netgame
         && menuactive
         && !demoplayback
         && players[consoleplayer].viewz != 1)
    {
        return;
    }


    for (i=0 ; i<MAXPLAYERS ; i++)
        if (playeringame[i])
            P_PlayerThink (&players[i]);

    P_RunThinkers ();
    P_UpdateSpecials ();
    P_RespawnSpecials ();

    // for par times
    leveltime++;
}
