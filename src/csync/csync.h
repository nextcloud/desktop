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

/**
 * @file csync.h
 *
 * @brief Application developer interface for csync.
 *
 * @defgroup csyncPublicAPI csync public API
 *
 * @{
 */

#ifndef _CSYNC_H
#define _CSYNC_H

#include "ocsynclib.h"

#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <config_csync.h>
#include <functional>
#include <memory>
#include <QByteArray>
#include "common/remotepermissions.h"

namespace OCC {
class SyncJournalFileRecord;
}

namespace CSyncEnums {
OCSYNC_EXPORT Q_NAMESPACE

    enum csync_status_codes_e : uint16_t {
        CSYNC_STATUS_OK = 0,

        CSYNC_STATUS_ERROR = 1024, /* don't use this code,
                                    */
        CSYNC_STATUS_UNSUCCESSFUL, /* Unspecific problem happend */
        CSYNC_STATUS_STATEDB_LOAD_ERROR, /* Statedb can not be loaded. */
        CSYNC_STATUS_UPDATE_ERROR, /* general update or discovery error */
        CSYNC_STATUS_TIMEOUT, /* UNUSED */
        CSYNC_STATUS_HTTP_ERROR, /* UNUSED */
        CSYNC_STATUS_PERMISSION_DENIED, /*  */
        CSYNC_STATUS_NOT_FOUND,
        CSYNC_STATUS_FILE_EXISTS,
        CSYNC_STATUS_OUT_OF_SPACE,
        CSYNC_STATUS_SERVICE_UNAVAILABLE,
        CSYNC_STATUS_STORAGE_UNAVAILABLE,
        CSYNC_STATUS_FILE_SIZE_ERROR,
        CSYNC_STATUS_OPENDIR_ERROR,
        CSYNC_STATUS_READDIR_ERROR,
        CSYNC_STATUS_OPEN_ERROR,
        CSYNC_STATUS_ABORTED,
        /* Codes for file individual status: */
        CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK,
        CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST,
        CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS,
        CSYNC_STATUS_INDIVIDUAL_TRAILING_SPACE,
        CSYNC_STATUS_INDIVIDUAL_EXCLUDE_LONG_FILENAME,
        CSYNC_STATUS_INDIVIDUAL_EXCLUDE_HIDDEN,
        CSYNC_STATUS_INVALID_CHARACTERS,
        CSYNC_STATUS_INDIVIDUAL_STAT_FAILED,
        CSYNC_STATUS_FORBIDDEN,
        CSYNC_STATUS_INDIVIDUAL_TOO_DEEP,
        CSYNC_STATUS_INDIVIDUAL_IS_CONFLICT_FILE,
        CSYNC_STATUS_INDIVIDUAL_CANNOT_ENCODE
    };
Q_ENUM_NS(csync_status_codes_e)

/**
  * Instruction enum. In the file traversal structure, it describes
  * the csync state of a file.
  */
// clang-format off
enum SyncInstruction : uint16_t {
    CSYNC_INSTRUCTION_NONE            = 0,       /* Nothing to do (UPDATE|RECONCILE) */
    CSYNC_INSTRUCTION_REMOVE          = 1 << 1,  /* The file need to be removed (RECONCILE) */
    CSYNC_INSTRUCTION_RENAME          = 1 << 2,  /* The file need to be renamed (RECONCILE) */
    CSYNC_INSTRUCTION_NEW             = 1 << 3,  /* The file is new compared to the db (UPDATE) */
    CSYNC_INSTRUCTION_CONFLICT        = 1 << 4,  /* The file need to be downloaded because it is a conflict (RECONCILE) */
    CSYNC_INSTRUCTION_IGNORE          = 1 << 5,  /* The file is ignored (UPDATE|RECONCILE) */
    CSYNC_INSTRUCTION_SYNC            = 1 << 6,  /* The file need to be pushed to the other remote (RECONCILE) */
    CSYNC_INSTRUCTION_ERROR           = 1 << 8,
    CSYNC_INSTRUCTION_TYPE_CHANGE     = 1 << 9,  /* Like NEW, but deletes the old entity first (RECONCILE)
                                                    Used when the type of something changes from directory to file
                                                    or back. */
    CSYNC_INSTRUCTION_UPDATE_METADATA = 1 << 10, /* If the etag has been updated and need to be writen to the db,
                                                    but without any propagation (UPDATE|RECONCILE) */
};
// clang-format on
Q_FLAG_NS(SyncInstruction)
Q_DECLARE_FLAGS(SyncInstructions, SyncInstruction)
Q_DECLARE_OPERATORS_FOR_FLAGS(SyncInstructions)

// This enum is used with BITFIELD(3) and BITFIELD(4) in several places.
// Also, this value is stored in the database, so beware of value changes.
enum ItemType : uint8_t {
    ItemTypeFile = 0,
    ItemTypeSoftLink = 1,
    ItemTypeDirectory = 2,
    ItemTypeSkip = 3,

    /** The file is a dehydrated placeholder, meaning data isn't available locally */
    ItemTypeVirtualFile = 4,

    /** A ItemTypeVirtualFile that wants to be hydrated.
     *
     * Actions may put this in the db as a request to a future sync, such as
     * implicit hydration (when the user wants to access file data) when using
     * suffix vfs. For pin-state driven hydrations changing the database is
     * not necessary.
     *
     * For some vfs plugins the placeholder files on disk may be marked for
     * (de-)hydration (like with a file attribute) and then the local discovery
     * will return this item type.
     *
     * The discovery will also use this item type to mark entries for hydration
     * if an item's pin state mandates it, such as when encountering a AlwaysLocal
     * file that is dehydrated.
     */
    ItemTypeVirtualFileDownload = 5,

    /** A ItemTypeFile that wants to be dehydrated.
     *
     * Similar to ItemTypeVirtualFileDownload, but there's currently no situation
     * where it's stored in the database since there is no action that triggers a
     * file dehydration without changing the pin state.
     */
    ItemTypeVirtualFileDehydration = 6,
};
Q_ENUM_NS(ItemType)
}

using namespace CSyncEnums;
using CSYNC_STATUS = CSyncEnums::csync_status_codes_e;

struct OCSYNC_EXPORT csync_file_stat_t
{
    time_t modtime = 0;
    int64_t size = 0;
    uint64_t inode = 0;

    ItemType type = ItemTypeSkip;
    bool is_hidden = false; // Not saved in the DB, only used during discovery for local files.

    QString path;
};

OCSYNC_EXPORT QDebug operator<<(QDebug debug, const SyncInstructions &job);

/**
 * }@
 */
#endif /* _CSYNC_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
