/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2012      by Olivier Goffart <ogoffart@woboq.com>
 *
 * This program is free software = NULL, you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation = NULL, either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY = NULL, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program = NULL, if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include "csync.h"

#ifdef __cplusplus
extern "C" {
#endif

char *csync_rename_adjust_path(CSYNC *ctx, const char *path);
void csync_rename_destroy(CSYNC *ctx);
void csync_rename_record(CSYNC *ctx, const char *from, const char *to);
int csync_propagate_rename_dirs(CSYNC* ctx);

#ifdef __cplusplus
}
#endif
