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

    csync_owncloud_redirect_callback_t redir_callback;
};

struct csync_owncloud_ctx_s {
    CSYNC *csync_ctx;

    // For the WebDAV connection
    struct dav_session_s dav_session; /* The DAV Session, initialised in dav_connect */
    int _connected;                   /* flag to indicate if a connection exists, ie.
                                         the dav_session is valid */
};

typedef struct csync_owncloud_ctx_s csync_owncloud_ctx_t;
//typedef csync_owncloud_ctx_t* csync_owncloud_ctx_p;

void set_errno_from_http_errcode( int err );
void set_error_message( csync_owncloud_ctx_t *ctx, const char *msg );
void set_errno_from_neon_errcode(csync_owncloud_ctx_t *ctx, int neon_code );
int http_result_code_from_session(csync_owncloud_ctx_t *ctx);
void set_errno_from_session(csync_owncloud_ctx_t *ctx);

time_t oc_httpdate_parse( const char *date );

const char* csync_owncloud_get_platform(void);

char *_cleanPath( const char* uri );

#endif // CSYNC_OWNCLOUD_PRIVATE_H
