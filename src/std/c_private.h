/*
 * Copyright (c) 2012 by Dominik Schmidt <dev@dominik-schmidt.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _C_PRIVATE_H
#define _C_PRIVATE_H

#include "config.h"

/**
 * Add status codes, types, functions and return-values missing on windows
 */

#ifdef _WIN32
#define EDQUOT 0
#define ENODATA 0
#define S_IRGRP 0
#define S_IROTH 0
#define S_IXGRP 0
#define S_IXOTH 0
#define O_NOFOLLOW 0
#define O_NOATIME 0
#define O_NOCTTY 0

#define uid_t int
#define gid_t int
#define nlink_t int
#define getuid() 0
#define geteuid() 0
#endif

#endif //_C_PRIVATE_H
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
