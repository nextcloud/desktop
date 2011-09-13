/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2011      by Andreas Schneider <asn@cryptomilk.org>
 *
 * This program is free software = NULL, you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation = NULL, either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY = NULL, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program = NULL, if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <neon/ne_basic.h>
#include <neon/ne_socket.h>
#include <neon/ne_session.h>
#include <neon/ne_request.h>

#include "c_lib.h"
#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#ifdef NDEBUG
#define DEBUG_WEBDAV(x)
#else
#define DEBUG_WEBDAV(x) printf x
#endif

struct dav_session_s {
    ne_session *ctx;
    char *base_uri;
    char *user;
    char *pwd;
};

struct dav_session_s dav_session;
int _connected;

csync_vio_file_stat_t fs;

static int ne_session_error_errno(ne_session *session)
{
    const char *p = ne_get_error(session);
    char *q;
    int err;

    err = strtol(p, &q, 10);
    if (p == q) {
        return EIO;
    }

    switch(err) {
        case 200:           /* OK */
        case 201:           /* Created */
        case 202:           /* Accepted */
        case 203:           /* Non-Authoritative Information */
        case 204:           /* No Content */
        case 205:           /* Reset Content */
        case 207:           /* Multi-Status */
        case 304:           /* Not Modified */
            return 0;
        case 401:           /* Unauthorized */
        case 402:           /* Payment Required */
        case 407:           /* Proxy Authentication Required */
            return EPERM;
        case 301:           /* Moved Permanently */
        case 303:           /* See Other */
        case 404:           /* Not Found */
        case 410:           /* Gone */
            return ENOENT;
        case 408:           /* Request Timeout */
        case 504:           /* Gateway Timeout */
            return EAGAIN;
        case 423:           /* Locked */
            return EACCES;
        case 400:           /* Bad Request */
        case 403:           /* Forbidden */
        case 405:           /* Method Not Allowed */
        case 409:           /* Conflict */
        case 411:           /* Length Required */
        case 412:           /* Precondition Failed */
        case 414:           /* Request-URI Too Long */
        case 415:           /* Unsupported Media Type */
        case 424:           /* Failed Dependency */
        case 501:           /* Not Implemented */
            return EINVAL;
        case 413:           /* Request Entity Too Large */
        case 507:           /* Insufficient Storage */
            return ENOSPC;
        case 206:           /* Partial Content */
        case 300:           /* Multiple Choices */
        case 302:           /* Found */
        case 305:           /* Use Proxy */
        case 306:           /* (Unused) */
        case 307:           /* Temporary Redirect */
        case 406:           /* Not Acceptable */
        case 416:           /* Requested Range Not Satisfiable */
        case 417:           /* Expectation Failed */
        case 422:           /* Unprocessable Entity */
        case 500:           /* Internal Server Error */
        case 502:           /* Bad Gateway */
        case 503:           /* Service Unavailable */
        case 505:           /* HTTP Version Not Supported */
            return EIO;
        default:
            return EIO;
    }

    return EIO;
}

static int ne_error_to_errno(int ne_err)
{
    switch (ne_err) {
        case NE_OK:
        case NE_ERROR:
            return 0;
        case NE_AUTH:
        case NE_PROXYAUTH:
            return EACCES;
        case NE_CONNECT:
        case NE_TIMEOUT:
        case NE_RETRY:
            return EAGAIN;
        case NE_FAILED:
            return EINVAL;
        case NE_REDIRECT:
            return ENOENT;
        case NE_LOOKUP:
            return EIO;
        default:
            return EIO;
    }

    return EIO;
}

static int ne_dav_connect(const char *base_url) {
    char *scheme = NULL;
    char *user = NULL;
    char *passwd = NULL;
    char *host = NULL;
    unsigned int port = 0;
    char *path = NULL;
    int timeout = 30;
    ne_uri uri;
    int rc;

    if (_connected) {
        return 0;
    }

    rc = c_parse_uri(base_url, &scheme, &user, &passwd, &host, &port, &path);
    if (rc < 0) {
        goto out;
    }

    rc = ne_uri_parse(base_url, &uri);
    if (rc < 0) {
        return -1;
    }

    if (uri.scheme == NULL) {
        uri.scheme = c_strdup("http");
    }

    if (uri.port == 0) {
        uri.port = ne_uri_defaultport(uri.scheme);
    }

    rc = ne_sock_init();
    if (rc < 0) {
        return -1;
    }

    dav_session.ctx = ne_session_create(uri.scheme, uri.host, uri.port);
    if (dav_session.ctx == NULL) {
        return -1;
    }

    ne_set_read_timeout(dav_session.ctx, timeout);

    /* ne_set_server_auth(); */

    _connected = 1;
    rc = 0;
out:
    SAFE_FREE(scheme);
    SAFE_FREE(user);
    SAFE_FREE(passwd);
    SAFE_FREE(host);
    SAFE_FREE(path);

    return rc;
}

/*
 * file functions
 */

static csync_vio_method_handle_t *_open(const char *durl,
                                        int flags,
                                        mode_t mode) {
    char *uri;
    ne_request *req;
    int put = 0;

    (void) mode; /* unused */

    uri = ne_path_escape(durl);
    if (uri == NULL) {
        return NULL;
    }

    if (flags & O_WRONLY) {
        put = 1;
    }
    if (flags & O_RDWR) {
        put = 1;
    }
    if (flags & O_CREAT) {
        put = 1;
    }

    /* FIXME */
    if (put) {
        req = ne_request_create(dav_session.ctx, "PUT", uri);
    } else {
        req = ne_request_create(dav_session.ctx, "GET", uri);
    }

    return (csync_vio_method_handle_t *) req;
}

static csync_vio_method_handle_t *_creat(const char *durl, mode_t mode) {
    return _open(durl, O_CREAT|O_WRONLY|O_TRUNC, mode);
}

static int _close(csync_vio_method_handle_t *fhandle) {
    ne_request *req;

    if (fhandle == NULL) {
        return -1;
    }
    req = (ne_request *)fhandle;

    ne_request_destroy(req);

    return 0;
}

static ssize_t _read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
    ne_request *req = (ne_request *)fhandle;
    const ne_status *const st = ne_get_status(req);
    ssize_t len;
    int rc;

    rc = ne_begin_request(req);
    if (rc != NE_OK) {
        return -1;
    }

    do {
        if (st->klass == 2) {
            rc = NE_OK;
            len = ne_read_response_block(req, buf, count);
            if (len < 0) {
                rc = NE_ERROR;
            }
        } else {
            rc = ne_discard_response(req);
            len = -1;
        }
    } while(rc == NE_RETRY);

    if (rc == NE_OK) {
        rc = ne_end_request(req);
    }

    return len;
}

static ssize_t _write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
    ne_request *req = (ne_request *)fhandle;
    ssize_t len = count;
    int rc;

    ne_set_request_body_buffer(req, buf, count);

    rc = ne_request_dispatch(req);
    if (rc == NE_OK && ne_get_status(req)->klass != 2) {
        len = -1;
    }

    return len;
}

static off_t _lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
    (void) fhandle;
    (void) offset;
    (void) whence;

    return -1;
}

/*
 * directory functions
 */

static csync_vio_method_handle_t *_opendir(const char *name) {
    (void) name;

    return NULL;
}

static int _closedir(csync_vio_method_handle_t *dhandle) {
    (void) dhandle;

    return 0;
}

static csync_vio_file_stat_t *_readdir(csync_vio_method_handle_t *dhandle) {
    (void) dhandle;

    return &fs;
}

static int _mkdir(const char *uri, mode_t mode) {
    char *suri;
    int rc;

    (void) mode; /* unused */

    suri = ne_path_escape(uri);
    if (suri == NULL) {
        return -1;
    }

    rc = ne_dav_connect(suri);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }

    rc = ne_mkcol(dav_session.ctx, suri);
    if (rc != 0) {
        errno = ne_error_to_errno(rc);
        return -1;
    }

    return 0;
}

static int _rmdir(const char *uri) {
    char *suri;
    int rc;

    suri = ne_path_escape(uri);
    if (suri == NULL) {
        return -1;
    }

    rc = ne_dav_connect(suri);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }

    rc = ne_delete(dav_session.ctx, uri);
    if (rc != 0) {
        errno = ne_error_to_errno(rc);
        return -1;
    }

    return 0;
}

static int _stat(const char *uri, csync_vio_file_stat_t *buf) {
    time_t now;

    /* get props:
     *   modtime
     *   creattime
     *   size
     */

    buf->name = c_basename(uri);
    if (buf->name == NULL) {
        csync_vio_file_stat_destroy(buf);
    }
    buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

    time(&now);
    buf->mtime = now;
    buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

    return 0;
}

static int _rename(const char *olduri, const char *newuri) {
    char *ouri;
    char *nuri;
    int rc;

    ouri = ne_path_escape(olduri);
    if (ouri == NULL) {
        return -1;
    }

    nuri = ne_path_escape(newuri);
    if (nuri == NULL) {
        return -1;
    }

    rc = ne_dav_connect(ouri);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }

    rc = ne_move(dav_session.ctx, 1, ouri, nuri);
    if (rc != 0) {
        errno = ne_error_to_errno(rc);
        return -1;
    }

    return 0;
}

static int _unlink(const char *uri) {
    char *suri;
    int rc;

    suri = ne_path_escape(uri);
    if (suri == NULL) {
        return -1;
    }

    rc = ne_dav_connect(suri);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }

    rc = ne_delete(dav_session.ctx, uri);
    if (rc != 0) {
        errno = ne_error_to_errno(rc);
        return -1;
    }

    return 0;
}

static int _chmod(const char *uri, mode_t mode) {
    (void) uri;
    (void) mode;

    return 0;
}

static int _chown(const char *uri, uid_t owner, gid_t group) {
    (void) uri;
    (void) owner;
    (void) group;

    return 0;
}

static int _utimes(const char *uri, const struct timeval *times) {
    (void) uri;
    (void) times;

    return 0;
}

csync_vio_method_t _method = {
    .method_table_size = sizeof(csync_vio_method_t),
    .open = _open,
    .creat = _creat,
    .close = _close,
    .read = _read,
    .write = _write,
    .lseek = _lseek,
    .opendir = _opendir,
    .closedir = _closedir,
    .readdir = _readdir,
    .mkdir = _mkdir,
    .rmdir = _rmdir,
    .stat = _stat,
    .rename = _rename,
    .unlink = _unlink,
    .chmod = _chmod,
    .chown = _chown,
    .utimes = _utimes
};

csync_vio_method_t *vio_module_init(const char *method_name, const char *args,
        csync_auth_callback cb, void *userdata) {
    DEBUG_WEBDAV(("csync_webdav - method_name: %s\n", method_name));
    DEBUG_WEBDAV(("csync_webdav - args: %s\n", args));

    (void) method_name;
    (void) args;
    (void) cb;
    (void) userdata;


    return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
    (void) method;
}

/* vim: set ts=4 sw=4 et cindent: */
