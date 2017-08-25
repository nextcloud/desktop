/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2012      by Olivier Goffart <ogoffart@woboq.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include "csync.h"

/* Return the final destination path of a given patch in case of renames */
char OCSYNC_EXPORT *csync_rename_adjust_path(CSYNC *ctx, const char *path);
/* Return the source of a given path in case of renames */
char OCSYNC_EXPORT *csync_rename_adjust_path_source(CSYNC *ctx, const char *path);
void OCSYNC_EXPORT csync_rename_destroy(CSYNC *ctx);
void OCSYNC_EXPORT csync_rename_record(CSYNC *ctx, const char *from, const char *to);
/*  Return the amount of renamed item recorded */
bool OCSYNC_EXPORT csync_rename_count(CSYNC *ctx);
