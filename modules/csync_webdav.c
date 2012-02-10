/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2011      by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012      by Klaas Freitag <freitag@owncloud.com>
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
#include <neon/ne_props.h>
#include <neon/ne_auth.h>
#include <neon/ne_dates.h>

#include "c_lib.h"
#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#define DEBUG_WEBDAV(x) printf x

enum resource_type {
    resr_normal = 0,
    resr_collection,
    resr_reference,
    resr_error
};

#ifdef HAVE_UNSIGNED_LONG_LONG
typedef unsigned long long dav_size_t;
#define FMT_DAV_SIZE_T "ll"
#ifdef HAVE_STRTOULL
#define DAV_STRTOL strtoull
#endif
#else
typedef unsigned long dav_size_t;
#define FMT_DAV_SIZE_T "l"
#endif

#ifndef DAV_STRTOL
#define DAV_STRTOL strtol
#endif

/* Struct to store data for each resource found during an opendir operation.
 * It represents a single file entry.
 */
typedef struct resource {
    char *uri;           /* The complete uri */
    char *name;          /* The filename only */

    enum resource_type type;
    dav_size_t         size;
    time_t             modtime;

    struct resource    *next;
} resource;

/* Struct to hold the context of a WebDAV PropFind operation to fetch
 * a directory listing from the server.
 */
struct listdir_context {
    struct resource *list;          /* The list of result resources */
    struct resource *currResource;   /* A pointer to the current resource */
    const char      *target;         /* Request-URI of the PROPFIND */
    unsigned int     include_target; /* Do you want the uri in the result list? */
    unsigned int     result_count;   /* number of elements stored in list */
};

/*
 * context to store info about a temp file for GET and PUT requests
 * which store the data in a local file to save memory and secure the
 * transmission.
 */
struct transfer_context {
    ne_request *req;            /* the neon request */
    int         fd;             /* file descriptor of the file to read or write from */
    char        *tmpFileName;   /* the name of the temp file */
    size_t      bytes_written;  /* the amount of bytes written or read */
    char        method[4];      /* the HTTP method, either PUT or GET  */
};

/* Struct with the WebDAV session */
struct dav_session_s {
    ne_session *ctx;
    char *user;
    char *pwd;
};

/* The list of properties that is fetched in PropFind on a collection */
static const ne_propname ls_props[] = {
    { "DAV:", "getlastmodified" },
    { "DAV:", "getcontentlength" },
    { "DAV:", "resourcetype" },
    { "DAV:", "getcontenttype" },
    { NULL, NULL }
};

/*
 * local variables.
 */

struct dav_session_s dav_session; /* The DAV Session, initialised in dav_connect */
int _connected;                   /* flag to indicate if a connection exists, ie.
                                     the dav_session is valid */
csync_vio_file_stat_t _fs;

/* ***************************************************************************** */
static int ne_session_error_errno(ne_session *session)
{
    const char *p = ne_get_error(session);
    char *q;
    int err;

    err = strtol(p, &q, 10);
    if (p == q) {
        return EIO;
    }
    DEBUG_WEBDAV(("Session error string %s\n", p));
    DEBUG_WEBDAV(("Session Error: %d\n", err ));

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

/*
 * Authentication callback. Is set by ne_set_server_auth to be called
 * from the neon lib to authenticate a request.
 */
static int ne_auth( void *userdata, const char *realm, int attempt,
                    char *username, char *password)
{
    (void) userdata;
    (void) realm;

    // DEBUG_WEBDAV(( "Authentication required %s\n", realm ));
    if( username && password ) {
        strncpy( username, dav_session.user, NE_ABUFSIZ);
        strncpy( password, dav_session.pwd, NE_ABUFSIZ );
        DEBUG_WEBDAV(( "Authentication required %s\n", username ));
    }
    return attempt;
}

/*
 * Connect to a DAV server
 * This function sets the flag _connected if the connection is established
 * and returns if the flag is set, so calling it frequently is save.
 */
static int dav_connect(const char *base_url) {
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
    dav_session.user = user;
    dav_session.pwd = passwd;

    DEBUG_WEBDAV(("* scheme %s\n", scheme ));
    DEBUG_WEBDAV(("* user %s\n", user ));
    // DEBUG_WEBDAV(("passwd %s\n", passwd ));
    DEBUG_WEBDAV(("* host %s\n", host ));
    DEBUG_WEBDAV(("* port %d\n", port ));
    DEBUG_WEBDAV(("* path %s\n", path ));
    if (rc < 0) {
        goto out;
    }

    rc = ne_uri_parse(base_url, &uri);
    DEBUG_WEBDAV(("ne_parse_result: %d\n", rc ));

    if (rc < 0) {
        return -1;
    }

    if (uri.port == 0) {
        uri.port = ne_uri_defaultport(uri.scheme);
    }

    rc = ne_sock_init();
    DEBUG_WEBDAV(("ne_sock_init: %d\n", rc ));
    if (rc < 0) {
        return -1;
    }

    /* FIXME: Check for https connections */
    dav_session.ctx = ne_session_create(uri.scheme, uri.host, uri.port);

    if (dav_session.ctx == NULL) {
        return -1;
    }

    ne_set_read_timeout(dav_session.ctx, timeout);

    ne_set_server_auth(dav_session.ctx, ne_auth, 0 );

    _connected = 1;
    rc = 0;
out:
    SAFE_FREE(scheme);
    // SAFE_FREE(user);
    // SAFE_FREE(passwd);
    SAFE_FREE(host);
    SAFE_FREE(path);

    return rc;
}

/*
 * helper function to sort the result resource list. Called from the
 * results function to build up the result list.
 */
static int compare_resource(const struct resource *r1,
                            const struct resource *r2)
{
    /* DEBUG_WEBDAV(( "C1 %d %d\n", r1, r2 )); */
    int re = -1;
    /* Sort errors first, then collections, then alphabetically */
    if (r1->type == resr_error) {
        // return -1;
    } else if (r2->type == resr_error) {
        re = 1;
    } else if (r1->type == resr_collection) {
        if (r2->type != resr_collection) {
            // return -1;
        } else {
            re = strcmp(r1->uri, r2->uri);
        }
    } else {
        if (r2->type != resr_collection) {
            re = strcmp(r1->uri, r2->uri);
        } else {
            re = 1;
        }
    }
    /* DEBUG_WEBDAV(( "C2 = %d\n", re )); */
    return re;

}

/*
 * result parsing list.
 * This function is called to parse the result of the propfind request
 * to list directories on the WebDAV server. I takes a single resource
 * and fills a resource struct and stores it to the result list which
 * is stored in the listdir_context.
 */
static void results(void *userdata,
                    const ne_uri *uri,
                    const ne_prop_result_set *set)
{
    struct listdir_context *fetchCtx = userdata;
    struct resource *current = 0;
    struct resource *previous = 0;
    struct resource *newres = 0;
    const char *clength, *modtime = NULL;
    const char *resourcetype = NULL;
    const char *contenttype = NULL;
    const ne_status *status = NULL;
    const char *path = uri->path;

    (void) status;

    DEBUG_WEBDAV(("** propfind result found: %s\n", path ));
    if( ! fetchCtx->target ) {
        DEBUG_WEBDAV(("error: target must not be zero!\n" ));
        return;
    }

    if (ne_path_compare(fetchCtx->target, path) == 0 && !fetchCtx->include_target) {
        /* This is the target URI */
        DEBUG_WEBDAV(( "Skipping target resource.\n"));
        /* Free the private structure. */
        return;
    }

    /* Fill the resource structure with the data about the file */
    newres = c_malloc(sizeof(struct resource)); // ne_propset_private(set);
    newres->uri =  c_strdup(path);
    newres->name = c_basename( path );

    modtime      = ne_propset_value( set, &ls_props[0] );
    clength      = ne_propset_value( set, &ls_props[1] );
    resourcetype = ne_propset_value( set, &ls_props[2] );
    contenttype  = ne_propset_value( set, &ls_props[3] );
    DEBUG_WEBDAV(("Contenttype: %s\n", contenttype ? contenttype : "" ));

    newres->type = resr_normal;
    if( clength == NULL && resourcetype && strncmp( resourcetype, "<DAV:collection>", 16 ) == 0) {
        newres->type = resr_collection;
    }

    if (modtime)
        newres->modtime = ne_httpdate_parse(modtime);

    if (clength) {
        char *p;

        newres->size = DAV_STRTOL(clength, &p, 10);
        if (*p) {
            newres->size = 0;
        }
    }

    if( contenttype ) {

    }
    /* put the new resource into the result list */
    for (current = fetchCtx->list, previous = NULL; current != NULL;
         previous = current, current=current->next) {
        if (compare_resource(current, newres) >= 0) {
            break;
        }
    }
    if (previous) {
        previous->next = newres;
    } else {
        fetchCtx->list = newres;
    }
    newres->next = current;

    fetchCtx->result_count = fetchCtx->result_count + 1;
    DEBUG_WEBDAV(( "results for URI %s: %d %d\n", newres->name, (int)newres->size, (int)newres->modtime ));
    // DEBUG_WEBDAV(( "Leaving result for resource %s\n", newres->uri ));

}

/*
 * fetches a resource list from the WebDAV server. This is equivalent to list dir.
 */

static int fetch_resource_list( const char *uri,
                                int depth,
                                struct listdir_context *fetchCtx )
{
    int ret = 0;

    DEBUG_WEBDAV(("Connected to uri %s\n", uri ));

    /* do a propfind request and parse the results in the results function, set as callback */
    ret = ne_simple_propfind( dav_session.ctx, uri, depth, ls_props, results, fetchCtx );

    if( ret == NE_OK ) {
        DEBUG_WEBDAV(("Simple propfind OK.\n" ));
        fetchCtx->currResource = fetchCtx->list;
    } else {
        ret = ne_session_error_errno( dav_session.ctx );
        DEBUG_WEBDAV(("ne_simple_propfind failed: %d\n", ret ));
    }

    return ret;
}


/*
 * file functions
 */
static ssize_t _write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
    struct transfer_context *writeCtx = NULL;
    size_t written = 0;

    if (fhandle == NULL) {
        return -1;
    }

    writeCtx = (struct transfer_context*) fhandle;
    if( writeCtx->fd > -1 ) {
        written = write( writeCtx->fd, buf, count );
        if( written != count ) {
            DEBUG_WEBDAV(("Written bytes not equal to count\n"));
        } else {
            writeCtx->bytes_written = writeCtx->bytes_written + written;
            DEBUG_WEBDAV(("Successfully written %d\n", written ));
        }
    } else {
        /* problem: the file descriptor is not valid. */
        DEBUG_WEBDAV(("Not a valid file descriptor in write\n"));
    }

    DEBUG_WEBDAV(("Wrote %d bytes.\n", written ));
    return written;
}

static csync_vio_method_handle_t *_open(const char *durl,
                                        int flags,
                                        mode_t mode) {
    const char *uri;
    int put = 0;
    int rc = 0;
    struct transfer_context *writeCtx;

    (void) mode; /* unused on webdav server */
    DEBUG_WEBDAV(( "=> open called!\n"));
    writeCtx = c_malloc( sizeof(struct transfer_context) );
    writeCtx->bytes_written = 0;

    /* uri = ne_path_escape(durl);
     * escaping lets the ne_request_create fail, even though its documented
     * differently :-(
     */
    uri = durl;

    if (uri == NULL) {
        return NULL;
    }
    dav_connect( uri );

    if (flags & O_WRONLY) {
        put = 1;
    }
    if (flags & O_RDWR) {
        put = 1;
    }
    if (flags & O_CREAT) {
        put = 1;
    }

    /* open a temp file to store the incoming data */
    writeCtx->tmpFileName = c_strdup( "/tmp/csync.XXXXXX" );
    writeCtx->fd = mkstemp( writeCtx->tmpFileName );
    DEBUG_WEBDAV(("opening temp directory %s\n", writeCtx->tmpFileName ));

    if (put) {
        DEBUG_WEBDAV(("PUT request!\n"));

        writeCtx->req = ne_request_create(dav_session.ctx, "PUT", uri);
        if( writeCtx->req ) {
            rc = ne_begin_request( writeCtx->req );
            DEBUG_WEBDAV(("PUT request2!\n"));
            if (rc != NE_OK) {
                DEBUG_WEBDAV(("Can not open a request, bailing out.\n"));
                return NULL;
            }
        }

        strncpy( writeCtx->method, "PUT", 3 );
    } else {
        DEBUG_WEBDAV(("GET request!\n"));

        writeCtx->req = 0;
        strncpy( writeCtx->method, "GET", 3 );
        /* Download the data into a local temp file. */
        rc = ne_get( dav_session.ctx, uri, writeCtx->fd );
        if( rc != NE_OK ) {
            ne_error_to_errno( rc );
            DEBUG_WEBDAV(("Download to local file failed.\n"));
            return NULL;
        }
        if( close( writeCtx->fd ) == -1 ) {
            DEBUG_WEBDAV(("Close of local download file failed.\n"));
            writeCtx->fd = -1;
            return NULL;
        }

        writeCtx->fd = -1;
    }

    return (csync_vio_method_handle_t *) writeCtx;
}

static csync_vio_method_handle_t *_creat(const char *durl, mode_t mode) {

    csync_vio_method_handle_t *handle = _open(durl, O_CREAT|O_WRONLY|O_TRUNC, mode);

    /* on create, the file needs to be created empty */
    _write( handle, NULL, 0 );

    return handle;
}

static int _close(csync_vio_method_handle_t *fhandle) {
    struct transfer_context *writeCtx;
    struct stat st;
    int rc;
    int ret = 0;

    writeCtx = (struct transfer_context*) fhandle;

    if (fhandle == NULL) {
        ret = -1;
    }

    /* handle the PUT request, means write to the WebDAV server */
    if( ret != -1 && strcmp( writeCtx->method, "PUT" ) == 0 ) {

        /* if there is a valid file descriptor, close it, reopen in read mode and start the PUT request */
        if( writeCtx->fd > -1 ) {
            if( close( writeCtx->fd ) < 0 ) {
                DEBUG_WEBDAV(("Could not close file %s\n", writeCtx->tmpFileName ));
                ret = -1;
            }

            /* and open it again to read from */
            if (( writeCtx->fd = open( writeCtx->tmpFileName, O_RDONLY )) < 0) {
                ret = -1;
            } else {
                if (fstat( writeCtx->fd, &st ) < 0) {
                    DEBUG_WEBDAV(("Could not stat file %s\n", writeCtx->tmpFileName ));
                    ret = -1;
                }

                /* successfully opened for read. Now start the request via ne_put */
                ne_set_request_body_fd( writeCtx->req, writeCtx->fd, 0, st.st_size );
                rc = ne_request_dispatch( writeCtx->req );
                if (rc == NE_OK) {
                    if ( ne_get_status( writeCtx->req )->klass != 2 ) {
                        DEBUG_WEBDAV(("Error - PUT status value no 2xx\n"));
                    }
                }
            }
        }
        ne_request_destroy( writeCtx->req );
    } else  {
        /* Its a GET request, not much to do in close. */
        if( writeCtx->fd > -1) {
            close( writeCtx->fd );
        }
    }
    /* Remove the local file. */
    unlink( writeCtx->tmpFileName );

    /* free mem. Note that the request mem is freed by the ne_request_destroy call */
    SAFE_FREE( writeCtx->tmpFileName );
    SAFE_FREE( writeCtx );

    return ret;
}

static ssize_t _read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
    struct transfer_context *writeCtx;
    ssize_t len = 0;
    struct stat st;

    writeCtx = (struct transfer_context*) fhandle;

    DEBUG_WEBDAV(( "read called on %s!\n", writeCtx->tmpFileName ));
    if( ! fhandle ) {
        return -1;
    }

    if( writeCtx->fd == -1 ) {
        /* open the downloaded file to read from */
        if (( writeCtx->fd = open( writeCtx->tmpFileName, O_RDONLY )) < 0) {
             DEBUG_WEBDAV(("Could not open local file %s\n", writeCtx->tmpFileName ));
            return -1;
        } else {
            if (fstat( writeCtx->fd, &st ) < 0) {
                DEBUG_WEBDAV(("Could not stat file %s\n", writeCtx->tmpFileName ));
                return -1;
            }

            DEBUG_WEBDAV(("local downlaod file size=%d\n", (int) st.st_size ));
        }
    }

    if( writeCtx->fd ) {
        len = read( writeCtx->fd, buf, count );
        writeCtx->bytes_written = writeCtx->bytes_written + len;
    }

    DEBUG_WEBDAV(( "read len: %d\n", len ));

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
static csync_vio_method_handle_t *_opendir(const char *uri) {

    ne_uri neuri;
    int rc;
    struct listdir_context *fetchCtx = NULL;
    struct resource *reslist = NULL;

    DEBUG_WEBDAV(("opendir method called on %s\n", uri ));

    dav_connect( uri );

    rc = ne_uri_parse(uri, &neuri);
    DEBUG_WEBDAV(("ne_parse_result: %d\n", rc ));

    if (rc < 0) {
        return NULL;
    }

    fetchCtx = c_malloc( sizeof( struct listdir_context ));

    fetchCtx->list = reslist;
    fetchCtx->target = neuri.path;
    fetchCtx->include_target = 0;
    fetchCtx->currResource = NULL;

    rc = fetch_resource_list( uri, NE_DEPTH_ONE, fetchCtx );
    if( rc != NE_OK ) {
        errno = ne_error_to_errno( rc );
        return NULL;
    } else {
        fetchCtx->currResource = fetchCtx->list;
        DEBUG_WEBDAV(("opendir returning handle %p\n", (void*) fetchCtx ));
        return fetchCtx;
    }
}

static int _closedir(csync_vio_method_handle_t *dhandle) {

    struct listdir_context *fetchCtx = dhandle;
    struct resource *r = fetchCtx->list;
    struct resource *rnext = NULL;

    DEBUG_WEBDAV(("closedir method called %p!\n", dhandle));

    while( r ) {
        rnext = r->next;
        SAFE_FREE(r->uri);
        SAFE_FREE(r->name);
        SAFE_FREE(r);
        r = rnext;
    }
    SAFE_FREE( dhandle );
    return 0;
}

/*
 * helper: convert a resource struct to file_stat struct.
 */
static csync_vio_file_stat_t *resourceToFileStat( struct resource *res )
{
    csync_vio_file_stat_t *lfs = NULL;

    if( ! res ) {
        return NULL;
    }

    lfs = c_malloc(sizeof(csync_vio_file_stat_t));
    if (lfs == NULL) {
        // free readdir list?
        return NULL;
    }

    lfs->name = c_strdup( res->name );

    lfs->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;
    if( res->type == resr_normal ) {
        lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
        lfs->type = CSYNC_VIO_FILE_TYPE_REGULAR;
    } else if( res->type == resr_collection ) {
        lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
        lfs->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
    }

    lfs->mtime = res->modtime;
    lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
    lfs->size  = res->size;
    lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

    return lfs;
}

static csync_vio_file_stat_t *_readdir(csync_vio_method_handle_t *dhandle) {

    struct listdir_context *fetchCtx = dhandle;
    csync_vio_file_stat_t *lfs = NULL;

    if( fetchCtx->currResource ) {
        DEBUG_WEBDAV(("readdir method called for %s!\n", fetchCtx->currResource->uri));
    } else {
        /* DEBUG_WEBDAV(("An empty dir or at end\n")); */
        return NULL;
    }

    if( fetchCtx && fetchCtx->currResource ) {
        /* FIXME: Who frees the allocated mem for lfs, allocated in the helper func? */
        lfs = resourceToFileStat( fetchCtx->currResource );

        // set pointer to next element
        fetchCtx->currResource = fetchCtx->currResource->next;

        /* fill the static stat buf as input for the stat function */
        _fs.name   = lfs->name;
        _fs.mtime  = lfs->mtime;
        _fs.fields = lfs->fields;
        _fs.type   = lfs->type;
        _fs.size   = lfs->size;
    }

    DEBUG_WEBDAV(("LFS fields: %s: %d\n", lfs->name, lfs->type ));
    return lfs;
}

static int _mkdir(const char *uri, mode_t mode) {
    char *suri;
    int rc;

    (void) mode; /* unused */

    suri = ne_path_escape(uri);
    if (suri == NULL) {
        return -1;
    }

    rc = dav_connect(suri);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }

    /* the suri path is required to have a trailing slash */
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

    rc = dav_connect(suri);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }

    rc = ne_delete(dav_session.ctx, suri);
    if (rc != 0) {
        errno = ne_error_to_errno(rc);
        return -1;
    }

    return 0;
}

/* WebDAV does not deliver permissions. We set a default here. */
static int _stat_perms( int type ) {
    int ret = 0;

    if( type == CSYNC_VIO_FILE_TYPE_DIRECTORY ) {
        DEBUG_WEBDAV(("Setting mode in stat (dir)\n"));
        /* directory permissions */
        ret = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR /* directory, rwx for user */
                | S_IRGRP | S_IXGRP                       /* rx for group */
                | S_IROTH | S_IXOTH;                      /* rx for others */
    } else {
        /* regualar file permissions */
        DEBUG_WEBDAV(("Setting mode in stat (file)\n"));
        ret = S_IFREG | S_IRUSR | S_IWUSR /* regular file, user read & write */
                | S_IRGRP                         /* group read perm */
                | S_IROTH;                        /* others read perm */
    }
    return ret;
}

static int _stat(const char *uri, csync_vio_file_stat_t *buf) {
    /* get props:
     *   modtime
     *   creattime
     *   size
     */
    int rc = 0;
#define DONT_CHEAT
#ifdef DONT_CHEAT
    csync_vio_file_stat_t *lfs = NULL;
    struct listdir_context  *fetchCtx = NULL;
    ne_uri neuri;
#else
    time_t now = time(NULL);
#endif

    DEBUG_WEBDAV(("__stat__ %s called\n", uri ));

    buf->name = c_strdup(c_basename(uri));
    if (buf->name == NULL) {
        csync_vio_file_stat_destroy(buf);
        errno = ENOMEM;
        return -1;
    }

    /* check if the data in the static 'cache' fs is for the same file.
     * The cache is filled by readdir which is often called directly before
     * stat. If the cache matches, a http call is saved.
     */
    if( _fs.name && strcmp( buf->name, _fs.name ) == 0 ) {
        buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;

        buf->fields = _fs.fields;
        buf->type   = _fs.type;
        buf->mtime  = _fs.mtime;
        buf->size   = _fs.size;
        buf->mode   = _stat_perms( _fs.type );
    } else {
        // fetch data via a propfind call.
        DEBUG_WEBDAV(("I have no stat cache, call propfind.\n"));
#ifdef DONT_CHEAT

        fetchCtx = c_malloc( sizeof( struct listdir_context ));

        rc = ne_uri_parse(uri, &neuri);
        // DEBUG_WEBDAV(("ne_parse_result: %d\n", rc ));

        if (rc < 0) {
            errno = ne_error_to_errno( rc );
            return -1;
        }
        // fetchCtx->list = reslist;
        fetchCtx->target = neuri.path;
        fetchCtx->include_target = 1;
        fetchCtx->currResource = NULL;

        DEBUG_WEBDAV(("fetchCtx good.\n" ));

        rc = fetch_resource_list( uri, NE_DEPTH_ONE, fetchCtx );
        if( rc != NE_OK ) {
            errno = ne_error_to_errno( rc );
            return -1;
        }
        fetchCtx->currResource = fetchCtx->list;

        if( fetchCtx ) {
            lfs = resourceToFileStat( fetchCtx->currResource );
            if( lfs ) {
                buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;

                buf->fields = lfs->fields;
                buf->type   = lfs->type;
                buf->mtime  = lfs->mtime;
                buf->size   = lfs->size;
                buf->mode   = _stat_perms( lfs->type );
            }
        }
#else
        // FIXME: Cheat for the time check...
        buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
        buf->type = CSYNC_VIO_FILE_TYPE_REGULAR;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
        buf->mtime = now;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
        buf->size = 0;
#endif
    }
    DEBUG_WEBDAV(("STAT result: %s, type=%d\n", buf->name, buf->type ));

    return 0;
}

static int _rename(const char *olduri, const char *newuri) {
    const char *ouri;
    ne_uri targetUri;
    int rc;

    ouri = ne_path_escape(olduri);
    if (ouri == NULL) {
        return -1;
    }

    rc = ne_uri_parse(newuri, &targetUri);
    DEBUG_WEBDAV(("ne_parse_result: %d\n", rc ));
    if (rc < 0) {
        return -1;
    }

    rc = dav_connect(ouri);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }

    rc = ne_move(dav_session.ctx, 1, ouri, targetUri.path );
    DEBUG_WEBDAV(("MOVE: %s => %s: %d\n", olduri, newuri, rc ));
    if (rc != NE_OK ) {
        ne_session_error_errno(dav_session.ctx);
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

    rc = dav_connect(suri);
    if (rc < 0) {
        errno = EINVAL;
        return -1;
    }

    rc = ne_delete(dav_session.ctx, suri);
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

    ne_proppatch_operation ops[2];
    ne_propname pname;
    int rc = NE_OK;
    char val[255];

    if( ! uri || !times ) {
        errno = 1;
        return -1; // FIXME: Find good errno
    }
    pname.nspace = NULL;
    pname.name = "LastModified";

    snprintf( val, sizeof(val), "%ld", times->tv_sec );
    DEBUG_WEBDAV(("===> Setting LastModified to %s\n", val ));

    ops[0].name = &pname;
    ops[0].type = ne_propset;
    ops[0].value = val;

    ops[1].name = NULL;

    rc = ne_proppatch( dav_session.ctx, uri, ops );
    if( rc != NE_OK ) {
        errno = ne_error_to_errno( rc );
    }
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
