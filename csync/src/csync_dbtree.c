/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2012      by Klaas Freitag <freitag@owncloud.com>
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

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "csync_dbtree.h"
#include "c_lib.h"
#include "csync_private.h"
#include "csync_statedb.h"
#include "csync_util.h"
#include "c_macro.h"


#define CSYNC_LOG_CATEGORY_NAME "csync.dbtree"
#include "csync_log.h"

struct dir_listing {
    c_list_t *list;
    unsigned int cnt;
    c_list_t *entry;
    char *dir;
};

csync_vio_method_handle_t *csync_dbtree_opendir(CSYNC *ctx, const char *name)
{

    char *column = NULL;
    const char *path = NULL;
    csync_vio_file_stat_t *fs = NULL;
    unsigned int c = 0;
    c_strlist_t *list = NULL;
    struct dir_listing *listing = NULL;

     /* "phash INTEGER(8),"
       "pathlen INTEGER,"
       "path VARCHAR(4096),"
       "inode INTEGER,"
       "uid INTEGER,"
       "gid INTEGER,"
       "mode INTEGER,"
       "modtime INTEGER(8),"
       "type INTEGER,"
       "md5 VARCHAR(32),"  // That's the etag
     */

    int col_count = 10;
    if( strlen(name) < strlen(ctx->remote.uri)+1) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "name does not contain remote uri!");
        return NULL;
    }

    path = name + strlen(ctx->remote.uri)+1;

    list = csync_statedb_get_below_path(ctx, path);

    if( ! list ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Query result list is NULL!");
        return NULL;
    }
    /* list count must be a multiple of col_count */
    if( list->count % col_count != 0 ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Wrong size of query result list");
        c_strlist_destroy( list );
        return NULL;
    }

    listing = c_malloc(sizeof(struct dir_listing));
    ZERO_STRUCTP(listing);
    if( listing == NULL ) {
        c_strlist_destroy( list );
        errno = ENOMEM;
        return NULL;
    }

    listing->dir = c_strdup(path);

    for( c = 0; c < (list->count / col_count); c++) {
        int base = c*col_count;
        int cnt = 0;
        int tpath_len = 0;
        int type = 0;

        char *tpath = list->vector[base+1];
        /* check if the result points to a file directly below the search path
         * by checking if there is another / in the result.
         * If yes, skip it.
         * FIXME: Find a better filter solution here.
         */
        tpath += strlen(path)+1; /* jump over the search path */
        tpath_len = strlen( tpath );
        while( cnt < tpath_len ) {
            if(*(tpath+cnt) == '/') {
                /* CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Skipping entry: %s", list->vector[base+1]); */
                break;
            }
            cnt++;
        }
        if( cnt < tpath_len ) continue;

        if (!list->vector[base+8][0])
            continue; /* If etag is empty, the file was removed on the server */

        fs = csync_vio_file_stat_new();
        fs->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

        column = list->vector[base+0]; /* phash    */

        column = list->vector[base+1]; /* path     */
        fs->name = c_strdup(column+strlen(path)+1);

        column = list->vector[base+2]; /* inode    */
        fs->inode = atoll(column);
        fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_INODE;

        column = list->vector[base+3]; /* uid      */
        fs->uid = atoi(column);
        fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_UID;

        column = list->vector[base+4]; /* gid      */
        fs->gid = atoi(column);
        fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_GID;

        column = list->vector[base+5]; /* mode     */
        fs->mode = atoi(column);
        // fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_M;

        column = list->vector[base+6]; /* modtime  */
        fs->mtime = strtoul(column, NULL, 10);
        fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

        column = list->vector[base+7]; /* type     */
        type = atoi(column);
        /* Attention: the type of csync_ftw_type_e which is the source for
         * the database entry is different from csync_vio_file_type_e which
         * is the target file type here. Mapping is needed!
         */
        switch( type ) {
        case CSYNC_FTW_TYPE_DIR:
            fs->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
            break;
        case CSYNC_FTW_TYPE_FILE:
            fs->type = CSYNC_VIO_FILE_TYPE_REGULAR;
            break;
        case CSYNC_FTW_TYPE_SLINK:
            fs->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
            break;
        default:
            fs->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
        }
        fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

        column = list->vector[base+8]; /* etag */
        fs->etag = c_strdup(column);
        fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ETAG;

        column = list->vector[base+9]; /* file id */
        csync_vio_file_stat_set_file_id(fs, column);

        /* store into result list. */
        listing->list = c_list_append( listing->list, fs );
        listing->cnt++;
    }

    if(listing->cnt)
        listing->entry = c_list_first( listing->list );

    c_strlist_destroy( list );

    return listing;
}

int csync_dbtree_closedir(CSYNC *ctx, csync_vio_method_handle_t *dhandle)
{
    struct dir_listing *dl = NULL;
    int rc = 0;
    (void) ctx;

    if( dhandle != NULL ) {
        dl = (struct dir_listing*) dhandle;

        c_list_free(dl->list);
        SAFE_FREE(dl->dir);
        SAFE_FREE(dl);
    }

    return rc;
}

csync_vio_file_stat_t *csync_dbtree_readdir(CSYNC *ctx, csync_vio_method_handle_t *dhandle)
{
    csync_vio_file_stat_t *fs = NULL;
    struct dir_listing *dl = NULL;
    (void) ctx;

    if( dhandle != NULL ) {
        dl = (struct dir_listing*) dhandle;
        if( dl->entry != NULL ) {
            fs = (csync_vio_file_stat_t*) dl->entry->data;

            dl->entry = c_list_next( dl->entry);
        }
    }

    return fs;
}

/* vim: set ts=8 sw=2 et cindent: */
