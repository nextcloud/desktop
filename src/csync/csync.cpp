/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>
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

#include "config_csync.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "csync.h"


#include "common/syncjournalfilerecord.h"

std::unique_ptr<csync_file_stat_t> csync_file_stat_s::fromSyncJournalFileRecord(const OCC::SyncJournalFileRecord &rec)
{
    std::unique_ptr<csync_file_stat_t> st(new csync_file_stat_t);
    st->path = rec._path;
    st->inode = rec._inode;
    st->modtime = rec._modtime;
    st->type = static_cast<ItemType>(rec._type);
    st->etag = rec._etag;
    st->file_id = rec._fileId;
    st->remotePerm = rec._remotePerm;
    st->size = rec._fileSize;
    st->has_ignored_files = rec._serverHasIgnoredFiles;
    st->checksumHeader = rec._checksumHeader;
    return st;
}
