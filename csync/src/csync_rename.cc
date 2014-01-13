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

extern "C" {
#include "csync_private.h"
}

#include <map>
#include <string>
#include <vector>
#include <algorithm>

static std::string _parentDir(const std::string &path) {
    int len = path.length();
    while(len > 0 && path[len-1]!='/') len--;
    while(len > 0 && path[len-1]=='/') len--;
    return path.substr(0, len);
}

struct csync_rename_s {
    static csync_rename_s *get(CSYNC *ctx) {
        if (!ctx->rename_info) {
            ctx->rename_info = new csync_rename_s;
        }
        return reinterpret_cast<csync_rename_s *>(ctx->rename_info);
    }

    std::map<std::string, std::string> folder_renamed_to; // map from->to

    struct renameop {
        csync_file_stat_t *st;
        bool operator<(const renameop &other) const {
            return strlen(st->destpath) < strlen(other.st->destpath);
        }
    };
    std::vector<renameop> todo;
};

static int _csync_rename_record(void *obj, void *data) {
    CSYNC *ctx = reinterpret_cast<CSYNC*>(data);
    csync_rename_s* d = csync_rename_s::get(ctx);
    csync_file_stat_t *st = reinterpret_cast<csync_file_stat_t *>(obj);

    if ( st->instruction != CSYNC_INSTRUCTION_RENAME)
        return 0;

    csync_rename_s::renameop op = { st };
    d->todo.push_back(op);
    return 0;
}

extern "C" {
void csync_rename_destroy(CSYNC* ctx)
{
    delete reinterpret_cast<csync_rename_s *>(ctx->rename_info);
    ctx->rename_info = 0;
}

void csync_rename_record(CSYNC* ctx, const char* from, const char* to)
{
    csync_rename_s::get(ctx)->folder_renamed_to[from] = to;
}

char* csync_rename_adjust_path(CSYNC* ctx, const char* path)
{
    csync_rename_s* d = csync_rename_s::get(ctx);
    for (std::string p = _parentDir(path); !p.empty(); p = _parentDir(p)) {
        std::map< std::string, std::string >::iterator it = d->folder_renamed_to.find(p);
        if (it != d->folder_renamed_to.end()) {
            std::string rep = it->second + (path + p.length());
            return c_strdup(rep.c_str());
        }
    }
    return c_strdup(path);
}

};
