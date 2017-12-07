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

/* Return the final destination path of a given patch in case of renames
 *
 * Does only map the parent directories. If the directory "A" is renamed to
 * "B" then this function will not map "A" to "B". Only "A/foo" -> "B/foo".
*/
QByteArray OCSYNC_EXPORT csync_rename_adjust_parent_path(CSYNC *ctx, const QByteArray &path);

/* Return the source of a given path in case of renames */
QByteArray OCSYNC_EXPORT csync_rename_adjust_parent_path_source(CSYNC *ctx, const QByteArray &path);

/* like the parent_path variant, but applying to the full path */
QByteArray OCSYNC_EXPORT csync_rename_adjust_full_path_source(CSYNC *ctx, const QByteArray &path);

void OCSYNC_EXPORT csync_rename_record(CSYNC *ctx, const QByteArray &from, const QByteArray &to);
/*  Return the amount of renamed item recorded */
bool OCSYNC_EXPORT csync_rename_count(CSYNC *ctx);
