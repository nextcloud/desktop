/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2011      by Andreas Schneider <asn@cryptomilk.org>
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

#ifndef CSYNC_OWNCLOUD_PRIVATE_H
#define CSYNC_OWNCLOUD_PRIVATE_H

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "config_csync.h"
#ifdef NEON_WITH_LFS /* Switch on LFS in libneon. Never remove the NE_LFS! */
#define NE_LFS
#endif

#include <neon/ne_basic.h>
#include <neon/ne_socket.h>
#include <neon/ne_session.h>
#include <neon/ne_request.h>
#include <neon/ne_props.h>
#include <neon/ne_auth.h>
#include <neon/ne_dates.h>
#include <neon/ne_compress.h>
#include <neon/ne_redirect.h>


#include "c_rbtree.h"

#include "c_lib.h"
#include "csync.h"
#include "csync_misc.h"
#include "csync_macros.h"
#include "c_private.h"
#include "httpbf.h"

#include "vio/csync_vio_file_stat.h"
#include "vio/csync_vio.h"

#include "csync_log.h"

#include "csync_owncloud.h"


#define DEBUG_WEBDAV(...) csync_log( 9, "oc_module", __VA_ARGS__);

typedef int (*csync_owncloud_redirect_callback_t)(CSYNC* ctx, const char* uri);

/* Struct with the WebDAV session */
struct dav_session_s {
    ne_session *ctx;
    char *user;
    char *pwd;

    char *proxy_type;
    char *proxy_host;
    int   proxy_port;
    char *proxy_user;
    char *proxy_pwd;

    char *session_key;

    char *error_string;

    int read_timeout;

    bool no_recursive_propfind;

    csync_owncloud_redirect_callback_t redir_callback;
};

struct csync_owncloud_ctx_s {
    CSYNC *csync_ctx;

    // For the PROPFIND results
    bool is_first_propfind;
    struct listdir_context *propfind_cache;
    c_rbtree_t *propfind_recursive_cache;
    int propfind_recursive_cache_depth;
    int propfind_recursive_cache_file_count;
    int propfind_recursive_cache_folder_count;

    // For the WebDAV connection
    struct dav_session_s dav_session; /* The DAV Session, initialised in dav_connect */
    int _connected;                   /* flag to indicate if a connection exists, ie.
                                         the dav_session is valid */
};
typedef struct csync_owncloud_ctx_s csync_owncloud_ctx_t;
//typedef csync_owncloud_ctx_t* csync_owncloud_ctx_p;

enum resource_type {
    resr_normal = 0,
    resr_collection,
    resr_reference,
    resr_error
};

/* The list of properties that is fetched in PropFind on a collection */
static const ne_propname ls_props[] = {
    { "DAV:", "getlastmodified" },
    { "DAV:", "getcontentlength" },
    { "DAV:", "resourcetype" },
    { "DAV:", "getetag"},
    { "http://owncloud.org/ns", "id"},
    { NULL, NULL }
};

/* Struct to store data for each resource found during an opendir operation.
 * It represents a single file entry.
 */
typedef struct resource {
    char *uri;           /* The complete uri */
    char *name;          /* The filename only */

    enum resource_type type;
    int64_t              size;
    time_t             modtime;
    char*              md5;
    char               file_id[FILE_ID_BUF_SIZE+1];

    struct resource    *next;
} resource;

/* Struct to hold the context of a WebDAV PropFind operation to fetch
 * a directory listing from the server.
 */
struct listdir_context {
    struct resource *list;           /* The list of result resources */
    struct resource *currResource;   /* A pointer to the current resource */
    char            *target;        /* Request-URI of the PROPFIND */
    unsigned int     result_count;   /* number of elements stored in list */
    int ref; /* reference count, only destroy when it reaches 0 */
};


/* Values are propfind_recursive_element: */
struct propfind_recursive_element {
    struct resource *self;
    struct resource *children;
    struct propfind_recursive_element *parent;
};
typedef struct propfind_recursive_element propfind_recursive_element_t;

void clear_propfind_recursive_cache(csync_owncloud_ctx_t *ctx);
struct listdir_context *get_listdir_context_from_recursive_cache(csync_owncloud_ctx_t *ctx, const char *curi);
void fill_recursive_propfind_cache(csync_owncloud_ctx_t *ctx, const char *uri, const char *curi);
struct listdir_context *get_listdir_context_from_cache(csync_owncloud_ctx_t *ctx, const char *curi);
void fetch_resource_list_recursive(csync_owncloud_ctx_t *ctx, const char *uri, const char *curi);

void set_errno_from_http_errcode( int err );
void set_error_message( csync_owncloud_ctx_t *ctx, const char *msg );
void set_errno_from_neon_errcode(csync_owncloud_ctx_t *ctx, int neon_code );
int http_result_code_from_session(csync_owncloud_ctx_t *ctx);
void set_errno_from_session(csync_owncloud_ctx_t *ctx);

time_t oc_httpdate_parse( const char *date );

char *_cleanPath( const char* uri );

int _stat_perms( int type );
void resourceToFileStat( csync_vio_file_stat_t *lfs, struct resource *res );
void resource_free(struct resource* o);
struct resource* resource_dup(struct resource* o);
void free_fetchCtx( struct listdir_context *ctx );



#endif // CSYNC_OWNCLOUD_PRIVATE_H
