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
#include <config.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSYNC_STRINGIFY(s) CSYNC_TOSTRING(s)
#define CSYNC_TOSTRING(s) #s

/* csync version macros */
#define CSYNC_VERSION_INT(a, b, c) ((a) << 16 | (b) << 8 | (c))
#define CSYNC_VERSION_DOT(a, b, c) a ##.## b ##.## c
#define CSYNC_VERSION(a, b, c) CSYNC_VERSION_DOT(a, b, c)

/* csync version */
#define LIBCSYNC_VERSION_MAJOR  0
#define LIBCSYNC_VERSION_MINOR  50
#define LIBCSYNC_VERSION_MICRO  0

#define LIBCSYNC_VERSION_INT CSYNC_VERSION_INT(LIBCSYNC_VERSION_MAJOR, \
                                           LIBCSYNC_VERSION_MINOR, \
                                           LIBCSYNC_VERSION_MICRO)
#define LIBCSYNC_VERSION     CSYNC_VERSION(LIBCSYNC_VERSION_MAJOR, \
                                           LIBCSYNC_VERSION_MINOR, \
                                           LIBCSYNC_VERSION_MICRO)

/*
 * csync file declarations
 */
#define CSYNC_CONF_DIR ".csync"
#define CSYNC_CONF_FILE "csync.conf"
#define CSYNC_EXCLUDE_FILE "csync_exclude.conf"
#define CSYNC_LOCK_FILE "lock"

/**
  * Instruction enum. In the file traversal structure, it describes
  * the csync state of a file.
  */
enum csync_status_codes_e {
  CSYNC_STATUS_OK         = 0,

  CSYNC_STATUS_ERROR      = 1024, /* don't use this code,
                                     just use in csync_status_ok */
  CSYNC_STATUS_UNSUCCESSFUL,
  CSYNC_STATUS_NO_LOCK,
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
  CSYNC_INSTRUCTION_NONE       = 0x00000000,
  CSYNC_INSTRUCTION_EVAL       = 0x00000001,
  CSYNC_INSTRUCTION_REMOVE     = 0x00000002,
  CSYNC_INSTRUCTION_RENAME     = 0x00000004,
  CSYNC_INSTRUCTION_NEW        = 0x00000008,
  CSYNC_INSTRUCTION_CONFLICT   = 0x00000010,
  CSYNC_INSTRUCTION_IGNORE     = 0x00000020,
  CSYNC_INSTRUCTION_SYNC       = 0x00000040,
  CSYNC_INSTRUCTION_STAT_ERROR = 0x00000080,
  CSYNC_INSTRUCTION_ERROR      = 0x00000100,
  /* instructions for the propagator */
  CSYNC_INSTRUCTION_DELETED    = 0x00000200,
  CSYNC_INSTRUCTION_UPDATED    = 0x00000400
};

/**
 * CSync File Traversal structure.
 *
 * This structure is passed to the visitor function for every file
 * which is seen.
 * Note: The file size is missing here because type off_t is depending
 *       on the large file support in your build. Make sure to check
 *       that cmake and the callback app are compiled with the same
 *       setting for it, such as:
 *       -D_LARGEFILE64_SOURCE or -D_LARGEFILE_SOURCE
 *
 */
struct csync_tree_walk_file_s {
    const char *path;
    /* off_t       size; */
    time_t      modtime;
#ifdef _WIN32
    uint32_t    uid;
    uint32_t    gid;
#else
    uid_t       uid;
    gid_t       gid;
#endif
    mode_t      mode;
    int         type;
    enum csync_instructions_e instruction;
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

/**
 * @brief Check internal csync status.
 *
 * @param csync  The context to check.
 *
 * @return  true if status is error free, false for error states.
 */
bool csync_status_ok(CSYNC *ctx);

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
 * This function loads the configuration, the statedb and locks the client.
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
 * Writes the statedb, unlocks csync and frees the memory.
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
 * @brief Get the config directory.
 *
 * @param ctx          The csync context.
 *
 * @return             The path of the config directory or NULL on error.
 */
const char *csync_get_config_dir(CSYNC *ctx);

/**
 * @brief Change the config directory.
 *
 * @param ctx           The csync context.
 *
 * @param path          The path to the new config directory.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_set_config_dir(CSYNC *ctx, const char *path);

/**
 * @brief Remove the complete config directory.
 *
 * @param ctx           The csync context.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_remove_config_dir(CSYNC *ctx);

/**
 * @brief Enable the usage of the statedb. It is enabled by default.
 *
 * @param ctx           The csync context.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_enable_statedb(CSYNC *ctx);

/**
 * @brief Disable the usage of the statedb. It is enabled by default.
 *
 * @param ctx           The csync context.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_disable_statedb(CSYNC *ctx);

/**
 * @brief Check if the statedb usage is enabled.
 *
 * @param ctx           The csync context.
 *
 * @return              1 if it is enabled, 0 if it is disabled.
 */
int csync_is_statedb_disabled(CSYNC *ctx);

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

/**
 * @brief Get the path of the statedb file used.
 *
 * @param ctx           The csync context.
 *
 * @return              The path to the statedb file, NULL if an error occured.
 */
const char *csync_get_statedb_file(CSYNC *ctx);

/**
 * @brief Enable the creation of backup copys if files are changed on both sides
 *
 * @param ctx           The csync context.
 *
 * @return              0 on success, less than 0 if an error occured.
 */
int csync_enable_conflictcopys(CSYNC *ctx);

/**
  * @brief Flag to tell csync that only a local run is intended. Call before csync_init
  *
  * @param local_only   Bool flag to indicate local only mode.
  *
  * @return             0 on success, less than 0 if an error occured.
  */
int csync_set_local_only( CSYNC *ctx, bool local_only );

/**
  * @brief Retrieve the flag to tell csync that only a local run is intended.
  *
  * @return             1: stay local only, 0: local and remote mode
  */
bool csync_get_local_only( CSYNC *ctx );

/* Used for special modes or debugging */
int csync_get_status(CSYNC *ctx);

/* Used for special modes or debugging */
int csync_set_status(CSYNC *ctx, int status);

typedef int csync_treewalk_visit_func(TREE_WALK_FILE* ,void*);

/**
 * @brief Walk the local file tree and call a visitor function for each file.
 *
 * @param ctx           The csync context.
 * @param visitor       A callback function to handle the file info.
 * @param filter        A filter, built from and'ed csync_instructions_e
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


/*
 * Progress callbacks.
 */
enum csync_notify_type_e { CSYNC_NOTIFY_START_DOWNLOAD, CSYNC_NOTIFY_START_UPLOAD,
                           CSYNC_NOTIFY_PROGRESS, CSYNC_NOTIFY_FINISHED_DOWNLOAD,
                           CSYNC_NOTIFY_FINISHED_UPLOAD };

/**
 * @brief Callback definition for individual file progress callback.
 *
 * @param remote_url    The currently handled file.
 *
 * @param kind          The type of progress.
 *
 * @param o1            The current transmitted bytes.
 *
 * @param o2            The size of the file.
 */
typedef void (*csync_file_progress_callback) (const char *remote_url, enum csync_notify_type_e kind,
                                              long long o1, long long o2, void *userdata);

/**
 * @brief Set a progress callback for individual files.
 *
 * This callback reports about up- or download progress of a individual file.
 *
 * @param ctx           The csync context.
 *
 * @param cb            The callback
 */
int csync_set_file_progress_callback(CSYNC* ctx, csync_file_progress_callback cb);

/**
 * @brief Callback definition for overall progress callback.
 *
 * @param file_no       The current number of up- or downloaded files.
 *
 * @param file_cnt      The overall number of files to transmit.
 *
 * @param o1            The current transmitted bytes.
 *
 * @param o2            The overall sum of bytes to transmit.
 *
 * @param userdata      The user data pointer.
 */
typedef void (*csync_overall_progress_callback) (const char *file_name,
                                                 int file_no,
                                                 int file_cnt,
                                                 long long o1,
                                                 long long o2,
                                                 void *userdata);

/**
 * @brief Set a progress callback for the overall files.
 *
 * This callback reports about overall up- or download progress.
 *
 * @param ctx           The csync context.
 *
 * @param cb            The callback
 */
int csync_set_overall_progress_callback (CSYNC* ctx, csync_overall_progress_callback cb);

#ifdef __cplusplus
}
#endif

/**
 * }@
 */
#endif /* _CSYNC_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
