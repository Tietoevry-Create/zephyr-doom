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
//       Generate a checksum of the WAD directory.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i_system.h"
#include "m_misc.h"
#include "sha1.h"
#include "w_checksum.h"
#include "w_wad.h"

// Hash one WAD directory entry exactly the way Chocolate Doom does, so that a
// netgame peer loading the same IWAD computes the same digest. The fields and
// their order are load-bearing: name (as a NUL-terminated 9-byte buffer), the
// index of the WAD the lump came from, the lump's byte offset, and its size.
//
// This fork only ever has one WAD open (W_AddFile errors on a second), so the
// file index is always 0; upstream tracks a table of open files to number them.

static void ChecksumAddLump(sha1_context_t *sha1_context, lumpindex_t lump)
{
    char buf[9];

    M_StringCopy(buf, W_LumpName(lump), sizeof(buf));
    SHA1_UpdateString(sha1_context, buf);
    SHA1_UpdateInt32(sha1_context, 0);
    SHA1_UpdateInt32(sha1_context, W_LumpPosition(lump));
    SHA1_UpdateInt32(sha1_context, W_LumpLength(lump));
}

void W_Checksum(sha1_digest_t digest)
{
    sha1_context_t sha1_context;
    unsigned int i;

    SHA1_Init(&sha1_context);

    // Go through each entry in the WAD directory, adding information
    // about each entry to the SHA1 hash.

    for (i = 0; i < numlumps; ++i)
    {
        ChecksumAddLump(&sha1_context, (lumpindex_t) i);
    }

    SHA1_Final(digest, &sha1_context);
}
