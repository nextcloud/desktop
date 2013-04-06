/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2012      by Andreas Schneider <asn@cryptomilk.org>
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

#ifndef _CSYNC_MISC_H
#define _CSYNC_MISC_H

#include <csync.h>

char *csync_get_user_home_dir(void);
char *csync_get_local_username(void);

int csync_fnmatch(__const char *__pattern, __const char *__name, int __flags);

CSYNC_STATUS csync_errno_to_status(int error, CSYNC_STATUS default_status);

#endif /* _CSYNC_MISC_H */
