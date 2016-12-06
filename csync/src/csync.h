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

#ifdef __cplusplus
extern "C" {
#endif

struct csync_client_certs_s {
  char *certificatePath;
  char *certificatePasswd;
};  
  
enum csync_status_codes_e {
  CSYNC_STATUS_OK         = 0,

  CSYNC_STATUS_ERROR      = 1024, /* don't use this code,
                                     */
  CSYNC_STATUS_UNSUCCESSFUL,       /* Unspecific problem happend */
  CSYNC_STATUS_NO_LOCK,            /* OBSOLETE  does not happen anymore */
  CSYNC_STATUS_STATEDB_LOAD_ERROR, /* Statedb can not be loaded. */
  CSYNC_STATUS_STATEDB_CORRUPTED,  /* Statedb is corrupted */
  CSYNC_STATUS_NO_MODULE,          /* URL passed to csync does not start with owncloud:// or ownclouds:// */
  CSYNC_STATUS_TIMESKEW,           /* OBSOLETE */
  CSYNC_STATUS_FILESYSTEM_UNKNOWN, /* UNUSED */
  CSYNC_STATUS_TREE_ERROR,         /* csync trees could not be created */
  CSYNC_STATUS_MEMORY_ERROR,       /* not enough memory problem */
  CSYNC_STATUS_PARAM_ERROR,        /* parameter is zero where not expected */
  CSYNC_STATUS_UPDATE_ERROR,       /* general update or discovery error */
  CSYNC_STATUS_RECONCILE_ERROR,    /* general reconcile error */
  CSYNC_STATUS_PROPAGATE_ERROR,    /* OBSOLETE */
  CSYNC_STATUS_REMOTE_ACCESS_ERROR, /* UNUSED */
  CSYNC_STATUS_REMOTE_CREATE_ERROR, /* UNUSED */
  CSYNC_STATUS_REMOTE_STAT_ERROR,  /* UNUSED */
  CSYNC_STATUS_LOCAL_CREATE_ERROR, /* UNUSED */
  CSYNC_STATUS_LOCAL_STAT_ERROR,   /* UNUSED */
  CSYNC_STATUS_PROXY_ERROR,        /* UNUSED */
  CSYNC_STATUS_LOOKUP_ERROR,       /* Neon fails to find proxy. Almost OBSOLETE */
  CSYNC_STATUS_SERVER_AUTH_ERROR,  /* UNUSED */
  CSYNC_STATUS_PROXY_AUTH_ERROR,   /* UNUSED */
  CSYNC_STATUS_CONNECT_ERROR,      /* neon driven connection failed */
  CSYNC_STATUS_TIMEOUT,            /* UNUSED */
  CSYNC_STATUS_HTTP_ERROR,         /* UNUSED */
  CSYNC_STATUS_PERMISSION_DENIED,  /*  */
  CSYNC_STATUS_NOT_FOUND,
  CSYNC_STATUS_FILE_EXISTS,
  CSYNC_STATUS_OUT_OF_SPACE,
  CSYNC_STATUS_QUOTA_EXCEEDED, /* UNUSED */
  CSYNC_STATUS_SERVICE_UNAVAILABLE,
  CSYNC_STATUS_STORAGE_UNAVAILABLE,
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
    CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST,
    CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS,
    CSYNC_STATUS_INDIVIDUAL_TRAILING_SPACE,
    CSYNC_STATUS_INDIVIDUAL_EXCLUDE_LONG_FILENAME,
    CYSNC_STATUS_FILE_LOCKED_OR_OPEN,
    CSYNC_STATUS_INDIVIDUAL_EXCLUDE_HIDDEN,
    CSYNC_STATUS_INVALID_CHARACTERS,
    CSYNC_STATUS_INDIVIDUAL_STAT_FAILED,
    CSYNC_STATUS_FORBIDDEN
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

enum csync_ftw_type_e {
    CSYNC_FTW_TYPE_FILE,
    CSYNC_FTW_TYPE_SLINK,
    CSYNC_FTW_TYPE_DIR,
    CSYNC_FTW_TYPE_SKIP
};


#define FILE_ID_BUF_SIZE 36

// currently specified at https://github.com/owncloud/core/issues/8322 are 9 to 10
#define REMOTE_PERM_BUF_SIZE 15

typedef struct csync_vio_file_stat_s csync_vio_file_stat_t;

enum csync_vio_file_flags_e {
  CSYNC_VIO_FILE_FLAGS_NONE = 0,
  CSYNC_VIO_FILE_FLAGS_SYMLINK = 1 << 0,
  CSYNC_VIO_FILE_FLAGS_HIDDEN = 1 << 1
};

enum csync_vio_file_type_e {
  CSYNC_VIO_FILE_TYPE_UNKNOWN,
  CSYNC_VIO_FILE_TYPE_REGULAR,
  CSYNC_VIO_FILE_TYPE_DIRECTORY,
  CSYNC_VIO_FILE_TYPE_FIFO,
  CSYNC_VIO_FILE_TYPE_SOCKET,
  CSYNC_VIO_FILE_TYPE_CHARACTER_DEVICE,
  CSYNC_VIO_FILE_TYPE_BLOCK_DEVICE,
  CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK
};

enum csync_vio_file_stat_fields_e {
  CSYNC_VIO_FILE_STAT_FIELDS_NONE = 0,
  CSYNC_VIO_FILE_STAT_FIELDS_TYPE = 1 << 0,
  CSYNC_VIO_FILE_STAT_FIELDS_MODE = 1 << 1, // local POSIX mode
  CSYNC_VIO_FILE_STAT_FIELDS_FLAGS = 1 << 2,
//  CSYNC_VIO_FILE_STAT_FIELDS_DEVICE = 1 << 3,
  CSYNC_VIO_FILE_STAT_FIELDS_INODE = 1 << 4,
//  CSYNC_VIO_FILE_STAT_FIELDS_LINK_COUNT = 1 << 5,
  CSYNC_VIO_FILE_STAT_FIELDS_SIZE = 1 << 6,
//  CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_COUNT = 1 << 7, /* will be removed */
//  CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_SIZE = 1 << 8,  /* will be removed */
  CSYNC_VIO_FILE_STAT_FIELDS_ATIME = 1 << 9,
  CSYNC_VIO_FILE_STAT_FIELDS_MTIME = 1 << 10,
  CSYNC_VIO_FILE_STAT_FIELDS_CTIME = 1 << 11,
//  CSYNC_VIO_FILE_STAT_FIELDS_SYMLINK_NAME = 1 << 12,
//  CSYNC_VIO_FILE_STAT_FIELDS_CHECKSUM = 1 << 13,
//  CSYNC_VIO_FILE_STAT_FIELDS_ACL = 1 << 14,
//  CSYNC_VIO_FILE_STAT_FIELDS_UID = 1 << 15,
//  CSYNC_VIO_FILE_STAT_FIELDS_GID = 1 << 16,
  CSYNC_VIO_FILE_STAT_FIELDS_ETAG = 1 << 17,
  CSYNC_VIO_FILE_STAT_FIELDS_FILE_ID = 1 << 18,
  CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADURL = 1 << 19,
  CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADCOOKIES = 1 << 20,
  CSYNC_VIO_FILE_STAT_FIELDS_PERM = 1 << 21 // remote oC perm

};


struct csync_vio_file_stat_s {
  char *name;
  char *etag; // FIXME: Should this be inlined like file_id and perm?
  char file_id[FILE_ID_BUF_SIZE+1];
  char *directDownloadUrl;
  char *directDownloadCookies;
  char remotePerm[REMOTE_PERM_BUF_SIZE+1];

  time_t atime;
  time_t mtime;
  time_t ctime;

  int64_t size;

  mode_t mode;

  uint64_t inode;

  int fields; // actually enum csync_vio_file_stat_fields_e fields;
  enum csync_vio_file_type_e type;

  enum csync_vio_file_flags_e flags;

  char *original_name; // only set if locale conversion fails
};

csync_vio_file_stat_t OCSYNC_EXPORT *csync_vio_file_stat_new(void);
csync_vio_file_stat_t OCSYNC_EXPORT *csync_vio_file_stat_copy(csync_vio_file_stat_t *file_stat);

void OCSYNC_EXPORT csync_vio_file_stat_destroy(csync_vio_file_stat_t *fstat);

void OCSYNC_EXPORT csync_vio_file_stat_set_file_id( csync_vio_file_stat_t* dst, const char* src );

void OCSYNC_EXPORT csync_vio_set_file_id(char* dst, const char *src );


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

    /* For directories: Does it have children that were ignored (hidden or ignore pattern) */
    int         has_ignored_files;

    const char *rename_path;
    const char *etag;
    const char *file_id;
    const char *remotePerm;
    char *directDownloadUrl;
    char *directDownloadCookies;

    const char *checksum;
    uint32_t checksumTypeId;

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

typedef void csync_vio_handle_t;
typedef csync_vio_handle_t* (*csync_vio_opendir_hook) (const char *url,
                                    void *userdata);
typedef csync_vio_file_stat_t* (*csync_vio_readdir_hook) (csync_vio_handle_t *dhhandle,
                                                              void *userdata);
typedef void (*csync_vio_closedir_hook) (csync_vio_handle_t *dhhandle,
                                                              void *userdata);
typedef int (*csync_vio_stat_hook) (csync_vio_handle_t *dhhandle,
                                                              void *userdata);

/* Compute the checksum of the given \a checksumTypeId for \a path. */
typedef const char* (*csync_checksum_hook) (
        const char *path, uint32_t checksumTypeId, void *userdata);

/**
 * @brief Allocate a csync context.
 *
 * @param csync  The context variable to allocate.
 */
void OCSYNC_EXPORT csync_create(CSYNC **csync, const char *local);

/**
 * @brief Initialize the file synchronizer.
 *
 * This function loads the configuration
 *
 * @param ctx  The context to initialize.
 */
void OCSYNC_EXPORT csync_init(CSYNC *ctx, const char *db_file);

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
 * @brief Re-initializes the csync context
 *
 * @param ctx  The context to commit.
 *
 * @return  0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_commit(CSYNC *ctx);

/**
 * @brief Destroy the csync context
 *
 * frees the memory.
 *
 * @param ctx  The context to destroy.
 *
 * @return  0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_destroy(CSYNC *ctx);

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

/**
 * @brief Set the log level.
 *
 * @param[in]  level  The log verbosity.
 *
 * @return 0 on success, < 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_set_log_level(int level);

/**
 * @brief Get the log verbosity
 *
 * @return            The log verbosity, -1 on error.
 */
int OCSYNC_EXPORT csync_get_log_level(void);

/**
 * @brief Get the logging callback set.
 *
 * @return              The logging callback set or NULL if an error
 *                      occurred.
 */
csync_log_callback OCSYNC_EXPORT csync_get_log_callback(void);

/**
 * @brief Set the logging callback.
 *
 * @param cb            The logging callback.
 *
 * @return              0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_set_log_callback(csync_log_callback cb);

/**
 * @brief get the userdata set for the logging callback.
 *
 * @return              The userdata or NULL.
 */
void OCSYNC_EXPORT *csync_get_log_userdata(void);

/**
 * @brief Set the userdata passed to the logging callback.
 *
 * @param[in]  data     The userdata to set.
 *
 * @return              0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_set_log_userdata(void *data);

/* Used for special modes or debugging */
CSYNC_STATUS OCSYNC_EXPORT csync_get_status(CSYNC *ctx);

/* Used for special modes or debugging */
int OCSYNC_EXPORT csync_set_status(CSYNC *ctx, int status);

typedef int csync_treewalk_visit_func(TREE_WALK_FILE* ,void*);

/**
 * @brief Walk the local file tree and call a visitor function for each file.
 *
 * @param ctx           The csync context.
 * @param visitor       A callback function to handle the file info.
 * @param filter        A filter, built from or'ed csync_instructions_e
 *
 * @return              0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_walk_local_tree(CSYNC *ctx, csync_treewalk_visit_func *visitor, int filter);

/**
 * @brief Walk the remote file tree and call a visitor function for each file.
 *
 * @param ctx           The csync context.
 * @param visitor       A callback function to handle the file info.
 * @param filter        A filter, built from and'ed csync_instructions_e
 *
 * @return              0 on success, less than 0 if an error occurred.
 */
int OCSYNC_EXPORT csync_walk_remote_tree(CSYNC *ctx, csync_treewalk_visit_func *visitor, int filter);

/**
 * @brief Get the csync status string.
 *
 * @param ctx            The csync context.
 *
 * @return               A const pointer to a string with more precise status info.
 */
const char OCSYNC_EXPORT *csync_get_status_string(CSYNC *ctx);

#ifdef WITH_ICONV
/**
 * @brief Set iconv source codec for filenames.
 *
 * @param from          Source codec.
 *
 * @return              0 on success, or an iconv error number.
 */
int OCSYNC_EXPORT csync_set_iconv_codec(const char *from);
#endif

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

char OCSYNC_EXPORT *csync_normalize_etag(const char *);
time_t OCSYNC_EXPORT oc_httpdate_parse( const char *date );

#ifdef __cplusplus
}
#endif

/**
 * }@
 */
#endif /* _CSYNC_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
