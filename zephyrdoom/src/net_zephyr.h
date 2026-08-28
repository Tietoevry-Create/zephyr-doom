//
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
//     Networking module which uses Zephyr BSD sockets.
//     Protocol-compatible drop-in replacement for the upstream
//     Chocolate Doom net_sdl module (SDL_net based).
//
//     ORIGIN: new file, written for zephyr-doom as part of multiplayer
//     support; the interface mirrors Chocolate Doom 3.0.0's src/net_sdl.h.
//     See net_zephyr.c for details and docs/multiplayer-design.md for the
//     rationale.
//

#ifndef NET_ZEPHYR_H
#define NET_ZEPHYR_H

#include "net_defs.h"

extern net_module_t net_zephyr_module;

#endif /* #ifndef NET_ZEPHYR_H */
