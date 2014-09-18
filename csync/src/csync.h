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

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <config_csync.h>

#include "csync_version.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
  * Instruction enum. In the file traversal structure, it describes
  * the csync state of a file.
  */
enum csync_status_codes_e {
  CSYNC_STATUS_OK         = 0,

  CSYNC_STATUS_ERROR      = 1024, /* don't use this code,
                                     */
  CSYNC_STATUS_UNSUCCESSFUL,
  CSYNC_STATUS_NO_LOCK, /* OBSOLETE  does not happen anymore */
  CSYNC_STATUS_STATEDB_LOAD_ERROR,
  CSYNC_STATUS_STATEDB_WRITE_ERROR,
  CSYNC_STATUS_NO_MODULE,
  CSYNC_STATUS_TIMESKEW,
  CSYNC_STATUS_FILESYSTEM_UNKNOWN,
  CSYNC_STATUS_TREE_ERROR,
  CSYNC_STATUS_MEMORY_ERROR,
  CSYNC_STATUS_PARAM_ERROR,
  CSYNC_STATUS_UPDATE_ERROR,
  CSYNC_STATUS_RECONCILE_ERROR,
  CSYNC_STATUS_PROPAGATE_ERROR,
  CSYNC_STATUS_REMOTE_ACCESS_ERROR,
  CSYNC_STATUS_REMOTE_CREATE_ERROR,
  CSYNC_STATUS_REMOTE_STAT_ERROR,
  CSYNC_STATUS_LOCAL_CREATE_ERROR,
  CSYNC_STATUS_LOCAL_STAT_ERROR,
  CSYNC_STATUS_PROXY_ERROR,
  CSYNC_STATUS_LOOKUP_ERROR,
  CSYNC_STATUS_SERVER_AUTH_ERROR,
  CSYNC_STATUS_PROXY_AUTH_ERROR,
  CSYNC_STATUS_CONNECT_ERROR,
  CSYNC_STATUS_TIMEOUT,
  CSYNC_STATUS_HTTP_ERROR,
  CSYNC_STATUS_PERMISSION_DENIED,
  CSYNC_STATUS_NOT_FOUND,
  CSYNC_STATUS_FILE_EXISTS,
  CSYNC_STATUS_OUT_OF_SPACE,
  CSYNC_STATUS_QUOTA_EXCEEDED,
  CSYNC_STATUS_SERVICE_UNAVAILABLE,
  CSYNC_STATUS_FILE_SIZE_ERROR,
  CSYNC_STATUS_CONTEXT_LOST,
  CSYNC_STATUS_MERGE_FILETREE_ERROR,
  CSYNC_STATUS_CSYNC_STATUS_ERROR,
  CSYNC_STATUS_OPENDIR_ERROR,
  CSYNC_STATUS_READDIR_ERROR,
  CSYNC_STATUS_OPEN_ERROR,
  CSYNC_STATUS_ABORTED,
    /* Codes for file individual status: */
    CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK,
    CSYNC_STATUS_INDIVIDUAL_IS_HARDLINK,
    CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST,
    CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS,
    CYSNC_STATUS_FILE_LOCKED_OR_OPEN
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


enum csync_instructions_e {
  CSYNC_INSTRUCTION_NONE       = 0x00000000,  /* Nothing to do (UPDATE|RECONCILE) */
  CSYNC_INSTRUCTION_EVAL       = 0x00000001,  /* There was changed compared to the DB (UPDATE) */
  CSYNC_INSTRUCTION_REMOVE     = 0x00000002,  /* The file need to be removed (RECONCILE) */
  CSYNC_INSTRUCTION_RENAME     = 0x00000004,  /* The file need to be renamed (RECONCILE) */
  CSYNC_INSTRUCTION_EVAL_RENAME= 0x00000800,  /* The file is new, it is the destination of a rename (UPDATE) */
  CSYNC_INSTRUCTION_NEW        = 0x00000008,  /* The file is new compared to the db (UPDATE) */
  CSYNC_INSTRUCTION_CONFLICT   = 0x00000010,  /* The file need to be downloaded because it is a conflict (RECONCILE) */
  CSYNC_INSTRUCTION_IGNORE     = 0x00000020,  /* The file is ignored (UPDATE|RECONCILE) */
  CSYNC_INSTRUCTION_SYNC       = 0x00000040,  /* The file need to be pushed to the other remote (RECONCILE) */
  CSYNC_INSTRUCTION_STAT_ERROR = 0x00000080,
  CSYNC_INSTRUCTION_ERROR      = 0x00000100
};

enum csync_ftw_type_e {
    CSYNC_FTW_TYPE_FILE,
    CSYNC_FTW_TYPE_SLINK,
    CSYNC_FTW_TYPE_DIR,
    CSYNC_FTW_TYPE_SKIP
};


/**
 * CSync File Traversal structure.
 *
 * This structure is passed to the visitor function for every file
 * which is seen.
 *
 */

struct csync_tree_walk_file_s {
    const char *path;
    int64_t     size;
    int64_t     inode;
    time_t      modtime;
    mode_t      mode;
    enum csync_ftw_type_e     type;
    enum csync_instructions_e instruction;

    /* For directories: If the etag has been updated and need to be writen on the db */
    int         should_update_etag;

    const char *rename_path;
    const char *etag;
    const char *file_id;
    const char *remotePerm;
    char *directDownloadUrl;
    char *directDownloadCookies;
    struct {
        int64_t     size;
        time_t      modtime;
        const char *etag;
        const char *file_id;
        enum csync_instructions_e instruction;
    } other;

    CSYNC_STATUS error_status;
};
typedef struct csync_tree_walk_file_s TREE_WALK_FILE;

/**
 * csync handle
 */
typedef struct csync_s CSYNC;

typedef int (*csync_auth_callback) (const char *prompt, char *buf, size_t len,
    int echo, int verify, void *userdata);

typedef void (*csync_log_callback) (int verbosity,
                                    const char *function,
                                    const char *buffer,
                                    void *userdata);

typedef void (*csync_update_callback) (bool local,
                                    const char *dirUrl,
                                    void *userdata);

/**
 * @brief Allocate a csync context.
 *
 * @param csync  The context variable to allocate.
 *
 * @return  0 on success, less than 0 if an error occured.
 */
int csync_create(CSYNC **csync, const char *local, const char *remote);

/**
 * @brief Initialize the file synchronizer.
 *
 * This function loads the configuration
 *
 * @param ctx  The context to initialize.
 *
 * @return  0 on success, less than 0 if an error occured.
 */
int csync_init(CSYNC *ctx);

/**
 * @brief Update detection
 *
 * @param ctx  The context to run the update detection on.
 *
 * @return  0 on success, less than 0 if an error occured.
 */
int csync_update(CSYNC *ctx);

/**
 * @brief Reconciliation
 *
 * @param ctx  The context to run the reconciliation on.
 *
 * @return  0 on success, less than 0 if an error occured.
 */
int csync_reconcile(CSYNC *ctx);

/**
 * @brief Propagation
 *
 * @param ctx  The context to run the propagation on.
 *
 * @return  0 on success, less than 0 if an error occured.
 */
int csync_propagate(CSYNC *ctx);

/**
 * @brief Commit the sync results to journal
 *
 * @param ctx  The context to commit.
 *
 * @return  0 on success, less than 0 if an error occured.
 */
int csync_commit(CSYNC *ctx);

/**
 * @brief Destroy the csync context
 *
 * frees the memory.
 *
 * @param ctx  The context to destroy.
 *
 * @return  0 on success, less than 0 if an error occured.
 */
int csync_destroy(CSYNC *ctx);

/**
 * @brief Check if csync is the required version or get the version
 * string.
 *
 * @param req_version   The version required.
 *
 * @return              If the version of csync is newer than the version
 *                      required it will return a version string.
 *                      NULL if the version is older.
 *
 * Example:
 *
 * @code
 *  if (csync_version(CSYNC_VERSION_INT(0,42,1)) == NULL) {
 *    fprintf(stderr, "libcsync version is too old!\n");
 *    exit(1);
 *  }
 *
 *  if (debug) {
 *    printf("csync %s\n", csync_version(0));
 *  }
 * @endcode
 */
const char *csync_version(int req_version);

/**
 * @brief Add an additional exclude list.
 *
 * @param ctx           The context to add the exclude list.
 *
 * @param path          The path pointing to the file.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_add_exclude_list(CSYNC *ctx, const char *path);

/**
 * @brief Removes all items imported from exclude lists.
 *
 * @param ctx           The context to add the exclude list.
 */
void csync_clear_exclude_list(CSYNC *ctx);

/**
 * @brief Get the userdata saved in the context.
 *
 * @param ctx           The csync context.
 *
 * @return              The userdata saved in the context, NULL if an error
 *                      occured.
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
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_set_userdata(CSYNC *ctx, void *userdata);

/**
 * @brief Get the authentication callback set.
 *
 * @param ctx           The csync context.
 *
 * @return              The authentication callback set or NULL if an error
 *                      occured.
 */
csync_auth_callback csync_get_auth_callback(CSYNC *ctx);

/**
 * @brief Set the authentication callback.
 *
 * @param ctx           The csync context.
 *
 * @param cb            The authentication callback.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_set_auth_callback(CSYNC *ctx, csync_auth_callback cb);

/**
 * @brief Set the log level.
 *
 * @param[in]  level  The log verbosity.
 *
 * @return 0 on success, < 0 if an error occured.
 */
int csync_set_log_level(int level);

/**
 * @brief Get the log verbosity
 *
 * @return            The log verbosity, -1 on error.
 */
int csync_get_log_level(void);

/**
 * @brief Get the logging callback set.
 *
 * @return              The logging callback set or NULL if an error
 *                      occured.
 */
csync_log_callback csync_get_log_callback(void);

/**
 * @brief Set the logging callback.
 *
 * @param cb            The logging callback.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_set_log_callback(csync_log_callback cb);

/**
 * @brief get the userdata set for the logging callback.
 *
 * @return              The userdata or NULL.
 */
void *csync_get_log_userdata(void);

/**
 * @brief Set the userdata passed to the logging callback.
 *
 * @param[in]  data     The userdata to set.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_set_log_userdata(void *data);

/* Used for special modes or debugging */
CSYNC_STATUS csync_get_status(CSYNC *ctx);

/* Used for special modes or debugging */
int csync_set_status(CSYNC *ctx, int status);

typedef int csync_treewalk_visit_func(TREE_WALK_FILE* ,void*);

/**
 * @brief Walk the local file tree and call a visitor function for each file.
 *
 * @param ctx           The csync context.
 * @param visitor       A callback function to handle the file info.
 * @param filter        A filter, built from or'ed csync_instructions_e
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_walk_local_tree(CSYNC *ctx, csync_treewalk_visit_func *visitor, int filter);

/**
 * @brief Walk the remote file tree and call a visitor function for each file.
 *
 * @param ctx           The csync context.
 * @param visitor       A callback function to handle the file info.
 * @param filter        A filter, built from and'ed csync_instructions_e
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_walk_remote_tree(CSYNC *ctx, csync_treewalk_visit_func *visitor, int filter);

/**
 * @brief Get the csync status string.
 *
 * @param ctx            The csync context.
 *
 * @return               A const pointer to a string with more precise status info.
 */
const char *csync_get_status_string(CSYNC *ctx);

#ifdef WITH_ICONV
/**
 * @brief Set iconv source codec for filenames.
 *
 * @param from          Source codec.
 *
 * @return              0 on success, or an iconv error number.
 */
int csync_set_iconv_codec(const char *from);
#endif

/**
 * @brief Set a property to module
 *
 * @param ctx           The csync context.
 *
 * @param key           The property key
 *
 * @param value         An opaque pointer to the data.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_set_module_property(CSYNC *ctx, const char *key, void *value);

/**
 * @brief Aborts the current sync run as soon as possible. Can be called from another thread.
 *
 * @param ctx           The csync context.
 */
void csync_request_abort(CSYNC *ctx);

/**
 * @brief Clears the abort flag. Can be called from another thread.
 *
 * @param ctx           The csync context.
 */
void csync_resume(CSYNC *ctx);

/**
 * @brief Checks for the abort flag, to be used from the modules.
 *
 * @param ctx           The csync context.
 */
int  csync_abort_requested(CSYNC *ctx);

/**
 * Specify if it is allowed to read the remote tree from the DB (default to enabled)
 */
int csync_set_read_from_db(CSYNC* ctx, int enabled);

#ifdef __cplusplus
}
#endif

/**
 * }@
 */
#endif /* _CSYNC_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
