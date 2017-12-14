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

#include "csync_private.h"
#include "csync_rename.h"

#include <algorithm>

static ByteArrayRef _parentDir(const ByteArrayRef &path) {
    int len = path.length();
    while(len > 0 && path.at(len-1)!='/') len--;
    while(len > 0 && path.at(len-1)=='/') len--;
    return path.left(len);
}

void csync_rename_record(CSYNC* ctx, const QByteArray &from, const QByteArray &to)
{
    ctx->renames.folder_renamed_to[from] = to;
    ctx->renames.folder_renamed_from[to] = from;
}

QByteArray csync_rename_adjust_parent_path(CSYNC *ctx, const QByteArray &path)
{
    if (ctx->renames.folder_renamed_to.empty())
        return path;
    for (auto p = _parentDir(path); !p.isEmpty(); p = _parentDir(p)) {
        auto it = ctx->renames.folder_renamed_to.find(p);
        if (it != ctx->renames.folder_renamed_to.end()) {
            QByteArray rep = it->second + path.mid(p.length());
            return rep;
        }
    }
    return path;
}

QByteArray csync_rename_adjust_parent_path_source(CSYNC *ctx, const QByteArray &path)
{
    if (ctx->renames.folder_renamed_from.empty())
        return path;
    for (ByteArrayRef p = _parentDir(path); !p.isEmpty(); p = _parentDir(p)) {
        auto it = ctx->renames.folder_renamed_from.find(p);
        if (it != ctx->renames.folder_renamed_from.end()) {
            QByteArray rep = it->second + path.mid(p.length());
            return rep;
        }
    }
    return path;
}

QByteArray csync_rename_adjust_full_path_source(CSYNC *ctx, const QByteArray &path)
{
    if (ctx->renames.folder_renamed_from.empty())
        return path;
    for (ByteArrayRef p = path; !p.isEmpty(); p = _parentDir(p)) {
        auto it = ctx->renames.folder_renamed_from.find(p);
        if (it != ctx->renames.folder_renamed_from.end()) {
            QByteArray rep = it->second + path.mid(p.length());
            return rep;
        }
    }
    return path;
}

bool csync_rename_count(CSYNC *ctx) {
    return ctx->renames.folder_renamed_from.size();
}
