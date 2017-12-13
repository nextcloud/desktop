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

#include "std/c_private.h"
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

#if defined(Q_CC_GNU) && !defined(Q_CC_INTEL) && !defined(Q_CC_CLANG) && (__GNUC__ * 100 + __GNUC_MINOR__ < 408)
// openSuse 12.3 didn't like enum bitfields.
#define BITFIELD(size)
#elif defined(Q_CC_MSVC)
// MSVC stores enum and bool as signed, so we need to add a bit for the sign
#define BITFIELD(size) :(size+1)
#else
#define BITFIELD(size) :size
#endif

enum csync_status_codes_e {
  CSYNC_STATUS_OK         = 0,

  CSYNC_STATUS_ERROR      = 1024, /* don't use this code,
                                     */
  CSYNC_STATUS_UNSUCCESSFUL,       /* Unspecific problem happend */
  CSYNC_STATUS_STATEDB_LOAD_ERROR, /* Statedb can not be loaded. */
  CSYNC_STATUS_UPDATE_ERROR,       /* general update or discovery error */
  CSYNC_STATUS_TIMEOUT,            /* UNUSED */
  CSYNC_STATUS_HTTP_ERROR,         /* UNUSED */
  CSYNC_STATUS_PERMISSION_DENIED,  /*  */
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

typedef enum csync_status_codes_e CSYNC_STATUS;

#ifndef likely
# define likely(x) (x)
#endif
#ifndef unlikely
# define unlikely(x) (x)
#endif

#define CSYNC_STATUS_IS_OK(x) (likely((x) == CSYNC_STATUS_OK))
#define CSYNC_STATUS_IS_ERR(x) (unlikely((x) >= CSYNC_STATUS_ERROR))
#define CSYNC_STATUS_IS_EQUAL(x, y) ((x) == (y))

/**
  * Instruction enum. In the file traversal structure, it describes
  * the csync state of a file.
  */
enum csync_instructions_e {
  CSYNC_INSTRUCTION_NONE            = 0x00000000,  /* Nothing to do (UPDATE|RECONCILE) */
  CSYNC_INSTRUCTION_EVAL            = 0x00000001,  /* There was changed compared to the DB (UPDATE) */
  CSYNC_INSTRUCTION_REMOVE          = 0x00000002,  /* The file need to be removed (RECONCILE) */
  CSYNC_INSTRUCTION_RENAME          = 0x00000004,  /* The file need to be renamed (RECONCILE) */
  CSYNC_INSTRUCTION_EVAL_RENAME     = 0x00000800,  /* The file is new, it is the destination of a rename (UPDATE) */
  CSYNC_INSTRUCTION_NEW             = 0x00000008,  /* The file is new compared to the db (UPDATE) */
  CSYNC_INSTRUCTION_CONFLICT        = 0x00000010,  /* The file need to be downloaded because it is a conflict (RECONCILE) */
  CSYNC_INSTRUCTION_IGNORE          = 0x00000020,  /* The file is ignored (UPDATE|RECONCILE) */
  CSYNC_INSTRUCTION_SYNC            = 0x00000040,  /* The file need to be pushed to the other remote (RECONCILE) */
  CSYNC_INSTRUCTION_STAT_ERROR      = 0x00000080,
  CSYNC_INSTRUCTION_ERROR           = 0x00000100,
  CSYNC_INSTRUCTION_TYPE_CHANGE     = 0x00000200,  /* Like NEW, but deletes the old entity first (RECONCILE)
                                                      Used when the type of something changes from directory to file
                                                      or back. */
  CSYNC_INSTRUCTION_UPDATE_METADATA = 0x00000400,  /* If the etag has been updated and need to be writen to the db,
                                                      but without any propagation (UPDATE|RECONCILE) */
};

// This enum is used with BITFIELD(3) and BITFIELD(4) in several places.
// Also, this value is stored in the database, so beware of value changes.
enum ItemType {
    ItemTypeFile = 0,
    ItemTypeSoftLink = 1,
    ItemTypeDirectory = 2,
    ItemTypeSkip = 3,
    ItemTypePlaceholder = 4,
    ItemTypePlaceholderDownload = 5
};


#define FILE_ID_BUF_SIZE 36

// currently specified at https://github.com/owncloud/core/issues/8322 are 9 to 10
#define REMOTE_PERM_BUF_SIZE 15

typedef struct csync_file_stat_s csync_file_stat_t;

struct OCSYNC_EXPORT csync_file_stat_s {
  time_t modtime;
  int64_t size;
  uint64_t inode;

  OCC::RemotePermissions remotePerm;
  ItemType type BITFIELD(4);
  bool child_modified BITFIELD(1);
  bool has_ignored_files BITFIELD(1); // Specify that a directory, or child directory contains ignored files.
  bool is_hidden BITFIELD(1); // Not saved in the DB, only used during discovery for local files.

  QByteArray path;
  QByteArray rename_path;
  QByteArray etag;
  QByteArray file_id;
  QByteArray directDownloadUrl;
  QByteArray directDownloadCookies;
  QByteArray original_path; // only set if locale conversion fails

  // In the local tree, this can hold a checksum and its type if it is
  //   computed during discovery for some reason.
  // In the remote tree, this will have the server checksum, if available.
  // In both cases, the format is "SHA1:baff".
  QByteArray checksumHeader;

  CSYNC_STATUS error_status;

  enum csync_instructions_e instruction; /* u32 */

  csync_file_stat_s()
    : modtime(0)
    , size(0)
    , inode(0)
    , type(ItemTypeSkip)
    , child_modified(false)
    , has_ignored_files(false)
    , is_hidden(false)
    , error_status(CSYNC_STATUS_OK)
    , instruction(CSYNC_INSTRUCTION_NONE)
  { }

  static std::unique_ptr<csync_file_stat_t> fromSyncJournalFileRecord(const OCC::SyncJournalFileRecord &rec);
};

/**
 * csync handle
 */
typedef struct csync_s CSYNC;

typedef int (*csync_auth_callback) (const char *prompt, char *buf, size_t len,
    int echo, int verify, void *userdata);

typedef void (*csync_update_callback) (bool local,
                                    const char *dirUrl,
                                    void *userdata);

typedef void csync_vio_handle_t;
typedef csync_vio_handle_t* (*csync_vio_opendir_hook) (const char *url,
                                    void *userdata);
typedef std::unique_ptr<csync_file_stat_t> (*csync_vio_readdir_hook) (csync_vio_handle_t *dhhandle,
                                                              void *userdata);
typedef void (*csync_vio_closedir_hook) (csync_vio_handle_t *dhhandle,
                                                              void *userdata);

/* Compute the checksum of the given \a checksumTypeId for \a path. */
typedef QByteArray (*csync_checksum_hook)(
    const QByteArray &path, const QByteArray &otherChecksumHeader, void *userdata);

/**
 * @brief Update detection
 *
 * @param ctx  The context to run the update detection on.
 *
 * @return  0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_update(CSYNC *ctx);

/**
 * @brief Reconciliation
 *
 * @param ctx  The context to run the reconciliation on.
 *
 * @return  0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_reconcile(CSYNC *ctx);

/**
 * @brief Get the userdata saved in the context.
 *
 * @param ctx           The csync context.
 *
 * @return              The userdata saved in the context, NULL if an error
 *                      occurred.
 */
void *csync_get_userdata(CSYNC *ctx);

/**
 * @brief Save userdata to the context which is passed to the auth
 * callback function.
 *
 * @param ctx           The csync context.
 *
 * @param userdata      The userdata to be stored in the context.
 *
 * @return              0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_set_userdata(CSYNC *ctx, void *userdata);

/**
 * @brief Get the authentication callback set.
 *
 * @param ctx           The csync context.
 *
 * @return              The authentication callback set or NULL if an error
 *                      occurred.
 */
csync_auth_callback OCSYNC_EXPORT csync_get_auth_callback(CSYNC *ctx);

/**
 * @brief Set the authentication callback.
 *
 * @param ctx           The csync context.
 *
 * @param cb            The authentication callback.
 *
 * @return              0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_set_auth_callback(CSYNC *ctx, csync_auth_callback cb);

/* Used for special modes or debugging */
CSYNC_STATUS OCSYNC_EXPORT csync_get_status(CSYNC *ctx);

/* Used for special modes or debugging */
int OCSYNC_EXPORT csync_set_status(CSYNC *ctx, int status);

using csync_treewalk_visit_func = std::function<int(csync_file_stat_t *cur, csync_file_stat_t *other)>;

/**
 * @brief Walk the local file tree and call a visitor function for each file.
 *
 * @param ctx           The csync context.
 * @param visitor       A callback function to handle the file info.
 *
 * @return              0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_walk_local_tree(CSYNC *ctx, const csync_treewalk_visit_func &visitor);

/**
 * @brief Walk the remote file tree and call a visitor function for each file.
 *
 * @param ctx           The csync context.
 * @param visitor       A callback function to handle the file info.
 *
 * @return              0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_walk_remote_tree(CSYNC *ctx, const csync_treewalk_visit_func &visitor);

/**
 * @brief Get the csync status string.
 *
 * @param ctx            The csync context.
 *
 * @return               A const pointer to a string with more precise status info.
 */
const char OCSYNC_EXPORT *csync_get_status_string(CSYNC *ctx);

/**
 * @brief Aborts the current sync run as soon as possible. Can be called from another thread.
 *
 * @param ctx           The csync context.
 */
void OCSYNC_EXPORT csync_request_abort(CSYNC *ctx);

/**
 * @brief Clears the abort flag. Can be called from another thread.
 *
 * @param ctx           The csync context.
 */
void OCSYNC_EXPORT csync_resume(CSYNC *ctx);

/**
 * @brief Checks for the abort flag, to be used from the modules.
 *
 * @param ctx           The csync context.
 */
int  OCSYNC_EXPORT csync_abort_requested(CSYNC *ctx);

time_t OCSYNC_EXPORT oc_httpdate_parse( const char *date );

/**
 * }@
 */
#endif /* _CSYNC_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
