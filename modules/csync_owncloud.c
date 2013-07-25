/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2011      by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012      by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>

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
#include <neon/ne_compress.h>
#include <neon/ne_redirect.h>

#include "c_lib.h"
#include "csync.h"
#include "c_private.h"
#include "csync_macros.h"
#include "csync_log.h"

#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#ifdef NDEBUG
#define DEBUG_WEBDAV(...)
#else
#define DEBUG_WEBDAV(...) csync_log( 9, "oc_module", __VA_ARGS__);
#endif

enum resource_type {
    resr_normal = 0,
    resr_collection,
    resr_reference,
    resr_error
};

#ifdef HAVE_UNSIGNED_LONG_LONG
typedef unsigned long long dav_size_t;
#ifdef HAVE_STRTOULL
#define DAV_STRTOL strtoull
#endif
#else
typedef unsigned long dav_size_t;
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
    struct resource *list;           /* The list of result resources */
    struct resource *currResource;   /* A pointer to the current resource */
    char            *target;        /* Request-URI of the PROPFIND */
    unsigned int     include_target; /* Do you want the uri in the result list? */
    unsigned int     result_count;   /* number of elements stored in list */
};


static void free_fetchCtx( struct listdir_context *ctx )
{
    struct resource *newres, *res;
    if( ! ctx ) return;
    newres = ctx->list;
    res = newres;

    SAFE_FREE(ctx->target);

    while( res ) {
        SAFE_FREE(res->uri);
        SAFE_FREE(res->name);

        newres = res->next;
        SAFE_FREE(res);
        res = newres;
    }
    SAFE_FREE(ctx);
}


/*
 * context to store info about a temp file for GET and PUT requests
 * which store the data in a local file to save memory and secure the
 * transmission.
 */
struct transfer_context {
  ne_request *req;            /* the neon request */
  int         fd;             /* file descriptor of the file to read or write from */
  const char  *method;        /* the HTTP method, either PUT or GET  */
  ne_decompress *decompress;  /* the decompress context */
  char        *url;
};

/* Struct with the WebDAV session */
struct dav_session_s {
    ne_session *ctx;
    char *user;
    char *pwd;

    char *error_string;
};

/* The list of properties that is fetched in PropFind on a collection */
static const ne_propname ls_props[] = {
    { "DAV:", "getlastmodified" },
    { "DAV:", "getcontentlength" },
    { "DAV:", "resourcetype" },
    { NULL, NULL }
};

/*
 * local variables.
 */

struct dav_session_s dav_session; /* The DAV Session, initialised in dav_connect */
int _connected;                   /* flag to indicate if a connection exists, ie.
                                     the dav_session is valid */
csync_vio_file_stat_t _fs;

csync_auth_callback _authcb;
csync_file_progress_callback    _file_progress_cb;

void *_userdata;

#define PUT_BUFFER_SIZE 1024*5

char _buffer[PUT_BUFFER_SIZE];

/* ***************************************************************************** */

static void set_error_message( const char *msg )
{
    SAFE_FREE(dav_session.error_string);
    if( msg )
        dav_session.error_string = c_strdup(msg);
}


static void set_errno_from_http_errcode( int err ) {
        int new_errno = 0;

    switch(err) {
    case 200:           /* OK */
    case 201:           /* Created */
    case 202:           /* Accepted */
    case 203:           /* Non-Authoritative Information */
    case 204:           /* No Content */
    case 205:           /* Reset Content */
    case 207:           /* Multi-Status */
    case 304:           /* Not Modified */
        new_errno = 0;
    case 401:           /* Unauthorized */
    case 402:           /* Payment Required */
    case 407:           /* Proxy Authentication Required */
        new_errno = EPERM;
    case 301:           /* Moved Permanently */
    case 303:           /* See Other */
    case 404:           /* Not Found */
    case 410:           /* Gone */
        new_errno = ENOENT;
    case 408:           /* Request Timeout */
    case 504:           /* Gateway Timeout */
        new_errno = EAGAIN;
    case 423:           /* Locked */
        new_errno = EACCES;
    case 405:
        new_errno = EEXIST;  /* Method Not Allowed */
    case 400:           /* Bad Request */
    case 403:           /* Forbidden */
    case 409:           /* Conflict */
    case 411:           /* Length Required */
    case 412:           /* Precondition Failed */
    case 414:           /* Request-URI Too Long */
    case 415:           /* Unsupported Media Type */
    case 424:           /* Failed Dependency */
    case 501:           /* Not Implemented */
        new_errno = EINVAL;
    case 413:           /* Request Entity Too Large */
    case 507:           /* Insufficient Storage */
        new_errno = ENOSPC;
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
    default:
        new_errno = EIO;
    }
    errno = new_errno;
}

static int http_result_code_from_session() {
    const char *p = ne_get_error( dav_session.ctx );
    char *q;
    int err;

    set_error_message(p); /* remember the error message */

    err = strtol(p, &q, 10);
    if (p == q) {
        err = ERRNO_ERROR_STRING;
    }
    return err;
}

static void set_errno_from_session() {
    int err = http_result_code_from_session();

    if( err == EIO || err == ERRNO_ERROR_STRING) {
        errno = err;
    } else {
        set_errno_from_http_errcode(err);
    }
}

static void set_errno_from_neon_errcode( int neon_code ) {

    if( neon_code != NE_OK ) {
        DEBUG_WEBDAV("Neon error code was %d", neon_code);
    }

    switch(neon_code) {
    case NE_OK:     /* Success, but still the possiblity of problems */
    case NE_ERROR:  /* Generic error; use ne_get_error(session) for message */
        set_errno_from_session(); /* Something wrong with http communication */
        break;
    case NE_LOOKUP:  /* Server or proxy hostname lookup failed */
        errno = ERRNO_LOOKUP_ERROR;
        break;
    case NE_AUTH:     /* User authentication failed on server */
        errno = ERRNO_USER_UNKNOWN_ON_SERVER;
        break;
    case NE_PROXYAUTH:  /* User authentication failed on proxy */
        errno = ERRNO_PROXY_AUTH;
        break;
    case NE_CONNECT:  /* Could not connect to server */
        errno = ERRNO_CONNECT;
        break;
    case NE_TIMEOUT:  /* Connection timed out */
        errno = ERRNO_TIMEOUT;
        break;
    case NE_FAILED:   /* The precondition failed */
        errno = ERRNO_PRECONDITION;
        break;
    case NE_RETRY:    /* Retry request (ne_end_request ONLY) */
        errno = ERRNO_RETRY;
        break;
    case NE_REDIRECT: /* See ne_redirect.h */
        errno = ERRNO_REDIRECT;
        break;
    default:
        errno = ERRNO_GENERAL_ERROR;
    }
}

/* cleanPath to return an escaped path of an uri */
static char *_cleanPath( const char* uri ) {
    int rc = 0;
    char *path = NULL;
    char *re = NULL;

    rc = c_parse_uri( uri, NULL, NULL, NULL, NULL, NULL, &path );
    if( rc  < 0 ) {
        DEBUG_WEBDAV("Unable to cleanPath %s", uri ? uri: "<zero>" );
        re = NULL;
    } else {
        re = ne_path_escape( path );
    }

    SAFE_FREE( path );
    return re;
}

/*
 * helper method to build up a user text for SSL problems, called from the
 * verify_sslcert callback.
 */
static void addSSLWarning( char *ptr, const char *warn, int len )
{
    char *concatHere = ptr;
    int remainingLen = 0;

    if( ! (warn && ptr )) return;
    remainingLen = len - strlen(ptr);
    if( remainingLen <= 0 ) return;
    concatHere = ptr + strlen(ptr);  /* put the write pointer to the end. */
    strncpy( concatHere, warn, remainingLen );
}

/*
 * Callback to verify the SSL certificate, called from libneon.
 * It analyzes the SSL problem, creates a user information text and passes
 * it to the csync callback to ask the user.
 */
#define LEN 4096
static int verify_sslcert(void *userdata, int failures,
                          const ne_ssl_certificate *cert)
{
    char problem[LEN];
    char buf[NE_ABUFSIZ];
    int ret = -1;

    (void) cert;
    memset( problem, 0, LEN );

    addSSLWarning( problem, "There are problems with the SSL certificate:\n", LEN );
    if( failures & NE_SSL_NOTYETVALID ) {
        addSSLWarning( problem, " * The certificate is not yet valid.\n", LEN );
    }
    if( failures & NE_SSL_EXPIRED ) {
        addSSLWarning( problem, " * The certificate has expired.\n", LEN );
    }

    if( failures & NE_SSL_UNTRUSTED ) {
        addSSLWarning( problem, " * The certificate is not trusted!\n", LEN );
    }
    if( failures & NE_SSL_IDMISMATCH ) {
        addSSLWarning( problem, " * The hostname for which the certificate was "
                       "issued does not match the hostname of the server\n", LEN );
    }
    if( failures & NE_SSL_BADCHAIN ) {
        addSSLWarning( problem, " * The certificate chain contained a certificate other than the server cert\n", LEN );
    }
    if( failures & NE_SSL_REVOKED ) {
        addSSLWarning( problem, " * The server certificate has been revoked by the issuing authority.\n", LEN );
    }

    addSSLWarning( problem, "Do you want to accept the certificate anyway?\nAnswer yes to do so and take the risk: ", LEN );

    if( _authcb ){
        /* call the csync callback */
        DEBUG_WEBDAV("Call the csync callback for SSL problems");
        memset( buf, 0, NE_ABUFSIZ );
        (*_authcb) ( problem, buf, NE_ABUFSIZ-1, 1, 0, userdata );
        if( strcmp( buf, "yes" ) == 0 ) {
            ret = 0;
        }
    }
    DEBUG_WEBDAV("## VERIFY_SSL CERT: %d", ret  );
    return ret;
}

/*
 * Authentication callback. Is set by ne_set_server_auth to be called
 * from the neon lib to authenticate a request.
 */
static int ne_auth( void *userdata, const char *realm, int attempt,
                    char *username, char *password)
{
    char buf[NE_ABUFSIZ] = {0};

    (void) userdata;
    (void) realm;

    /* DEBUG_WEBDAV( "Authentication required %s, realm ); */
    if( username && password ) {
        DEBUG_WEBDAV( "Authentication required %s", username );
        if( dav_session.user ) {
            /* allow user without password */
            snprintf(username, NE_ABUFSIZ, "%s", dav_session.user);

            if( dav_session.pwd ) {
                strncpy( password, dav_session.pwd, NE_ABUFSIZ );
            } else {
                (*_authcb) ("Enter your password: ", buf, NE_ABUFSIZ-1, 0, 0, userdata );
                snprintf(password, NE_ABUFSIZ, "%s", buf );
            }
        } else if( _authcb != NULL ){
            /* call the csync callback */
            DEBUG_WEBDAV("Call the csync callback for %s", realm );
            (*_authcb) ("Enter your username: ", buf, NE_ABUFSIZ-1, 1, 0, userdata );
            snprintf(username, NE_ABUFSIZ, "%s", buf );
            (*_authcb) ("Enter your password: ", buf, NE_ABUFSIZ-1, 0, 0, userdata );
            snprintf(password, NE_ABUFSIZ, "%s", buf );
        } else {
            DEBUG_WEBDAV("I can not authenticate!");
        }
    }
    return attempt;
}

/* called from neon */
static void ne_notify_status_cb (void *userdata, ne_session_status status,
                                 const ne_session_status_info *info)
{
    struct transfer_context *tc = (struct transfer_context*) userdata;

    if (_file_progress_cb && (status == ne_status_sending || status == ne_status_recving)) {
        if (info->sr.total > 0) {
            _file_progress_cb(tc->url, CSYNC_NOTIFY_PROGRESS,
                              info->sr.progress,
                              info->sr.total,
                              userdata);
        }
    }
}

/*
 * Connect to a DAV server
 * This function sets the flag _connected if the connection is established
 * and returns if the flag is set, so calling it frequently is save.
 */
static int dav_connect(const char *base_url) {
    int timeout = 30;
    int useSSL = 0;
    int rc;
    char protocol[6];
    char uaBuf[256];
    char *path = NULL;
    char *scheme = NULL;
    char *host = NULL;
    unsigned int port = 0;

    if (_connected) {
        return 0;
    }

    rc = c_parse_uri( base_url, &scheme, &dav_session.user, &dav_session.pwd, &host, &port, &path );
    if( rc < 0 ) {
        DEBUG_WEBDAV("Failed to parse uri %s", base_url );
        goto out;
    }

    DEBUG_WEBDAV("* scheme %s", scheme ? scheme : "empty");
    DEBUG_WEBDAV("* host %s", host ? host : "empty");
    DEBUG_WEBDAV("* port %u", port );
    DEBUG_WEBDAV("* path %s", path ? path : "empty");

    if( strcmp( scheme, "owncloud" ) == 0 ) {
        strncpy( protocol, "http", 6);
    } else if( strcmp( scheme, "ownclouds" ) == 0 ) {
        strncpy( protocol, "https", 6 );
        useSSL = 1;
    } else {
        strncpy( protocol, "", 6 );
        DEBUG_WEBDAV("Invalid scheme %s, go outa here!", scheme );
        rc = -1;
        goto out;
    }

    DEBUG_WEBDAV("* user %s", dav_session.user ? dav_session.user : "");

    if (port == 0) {
        port = ne_uri_defaultport(protocol);
    }

    rc = ne_sock_init();
    DEBUG_WEBDAV("ne_sock_init: %d", rc );
    if (rc < 0) {
        rc = -1;
        goto out;
    }

    dav_session.ctx = ne_session_create( protocol, host, port);

    if (dav_session.ctx == NULL) {
        DEBUG_WEBDAV("Session create with protocol %s failed", protocol );
        rc = -1;
        goto out;
    }

    ne_set_read_timeout(dav_session.ctx, timeout);
    snprintf( uaBuf, sizeof(uaBuf), "csyncoC/%s",CSYNC_STRINGIFY( LIBCSYNC_VERSION ));
    ne_set_useragent( dav_session.ctx, c_strdup( uaBuf ));
    ne_set_server_auth(dav_session.ctx, ne_auth, 0 );

    if( useSSL ) {
        if (!ne_has_support(NE_FEATURE_SSL)) {
            DEBUG_WEBDAV("Error: SSL is not enabled.");
            rc = -1;
            goto out;
        }

        ne_ssl_trust_default_ca( dav_session.ctx );
        ne_ssl_set_verify( dav_session.ctx, verify_sslcert, 0 );
    }

    _connected = 1;
    rc = 0;
out:
    SAFE_FREE(path);
    SAFE_FREE(host);
    SAFE_FREE(scheme);

    return rc;
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
    struct resource *newres = 0;
    const char *clength, *modtime = NULL;
    const char *resourcetype = NULL;
    const ne_status *status = NULL;
    char *path = ne_path_unescape( uri->path );

    (void) status;
    if( ! fetchCtx ) {
        DEBUG_WEBDAV("No valid fetchContext");
        return;
    }

    DEBUG_WEBDAV("** propfind result found: %s", path );
    if( ! fetchCtx->target ) {
        DEBUG_WEBDAV("error: target must not be zero!" );
        return;
    }

    if (ne_path_compare(fetchCtx->target, uri->path) == 0 && !fetchCtx->include_target) {
        /* This is the target URI */
        DEBUG_WEBDAV( "Skipping target resource.");
        /* Free the private structure. */
        SAFE_FREE( path );
        return;
    }

    /* Fill the resource structure with the data about the file */
    newres = c_malloc(sizeof(struct resource));
    newres->uri =  path; /* no need to strdup because ne_path_unescape already allocates */
    newres->name = c_basename( path );

    modtime      = ne_propset_value( set, &ls_props[0] );
    clength      = ne_propset_value( set, &ls_props[1] );
    resourcetype = ne_propset_value( set, &ls_props[2] );

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

    /* prepend the new resource to the result list */
    newres->next   = fetchCtx->list;
    fetchCtx->list = newres;
    fetchCtx->result_count = fetchCtx->result_count + 1;
    DEBUG_WEBDAV( "results for URI %s: %d %d", newres->name, (int)newres->size, (int)newres->type );
}

/*
 * fetches a resource list from the WebDAV server. This is equivalent to list dir.
 */

static int fetch_resource_list( const char *uri,
                                int depth,
                                struct listdir_context *fetchCtx )
{
  int ret = 0;
  ne_propfind_handler *hdl = NULL;
  ne_request *request = NULL;
  const char *content_type = NULL;
  char *curi = NULL;
  const ne_status *req_status = NULL;

  curi = _cleanPath( uri );

  if (!fetchCtx) {
    errno = ENOMEM;
    SAFE_FREE(curi);
    return -1;
  }
  fetchCtx->list = NULL;
  fetchCtx->target = curi;
  fetchCtx->currResource = NULL;

  /* do a propfind request and parse the results in the results function, set as callback */
  hdl = ne_propfind_create(dav_session.ctx, curi, depth);

  if(hdl) {
    ret = ne_propfind_named(hdl, ls_props, results, fetchCtx);
    request = ne_propfind_get_request( hdl );
    req_status = ne_get_status( request );
  }

  if( ret == NE_OK ) {
    fetchCtx->currResource = fetchCtx->list;
    /* Check the request status. */
    if( req_status && req_status->klass != 2 ) {
      set_errno_from_http_errcode(req_status->code);
      DEBUG_WEBDAV("ERROR: Request failed: status %d (%s)", req_status->code,
                   req_status->reason_phrase);
      ret = NE_CONNECT;
      set_error_message(req_status->reason_phrase);
    }
    DEBUG_WEBDAV("Simple propfind result code %d.", req_status ? req_status->code : -1);
  } else {
    if( ret == NE_ERROR && req_status->code == 404) {
      errno = ENOENT;
    } else {
      set_errno_from_neon_errcode(ret);
    }
  }

  if( ret == NE_OK ) {
    /* Check the content type. If the server has a problem, ie. database is gone or such,
        * the content type is not xml but a html error message. Stop on processing if it's
        * not XML.
        * FIXME: Generate user error message from the reply content.
        */
    content_type =  ne_get_response_header( request, "Content-Type" );
    if( !(content_type && c_streq(content_type, "application/xml; charset=utf-8") ) ) {
      DEBUG_WEBDAV("ERROR: Content type of propfind request not XML: %s.",
                   content_type ?  content_type: "<empty>");
      errno = ERRNO_WRONG_CONTENT;
      set_error_message("Server error: PROPFIND reply is not XML formatted!");
      ret = NE_CONNECT;
    }
  }
#ifndef NDEBUG
  if( ret != NE_OK ) {
    const char *err = ne_get_error(dav_session.ctx);
    DEBUG_WEBDAV("WRN: propfind named failed with %d, request error: %s", ret, err ? err : "<nil>");
  }
#endif
  if( hdl )
    ne_propfind_destroy(hdl);

#ifndef NDEBUG
  if (ret == NE_REDIRECT) {
    const ne_uri *redir_ne_uri = ne_redirect_location(dav_session.ctx);
    if (redir_ne_uri) {
      char *redir_uri = ne_uri_unparse(redir_ne_uri);
      DEBUG_WEBDAV("Permanently moved to %s", redir_uri);
    }
  }
#endif

  if( ret != NE_OK ) {
    free_fetchCtx(fetchCtx);
    return -1;
  }

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
    } else {
        DEBUG_WEBDAV("ERROR: Unknown resource type %d", res->type);
    }

    lfs->mtime = res->modtime;
    lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
    lfs->size  = res->size;
    lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

    return lfs;
}

/* WebDAV does not deliver permissions. Set a default here. */
static int _stat_perms( int type ) {
    int ret = 0;

    if( type == CSYNC_VIO_FILE_TYPE_DIRECTORY ) {
        /* DEBUG_WEBDAV("Setting mode in stat (dir)); */
        /* directory permissions */
        ret = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR /* directory, rwx for user */
                | S_IRGRP | S_IXGRP                       /* rx for group */
                | S_IROTH | S_IXOTH;                      /* rx for others */
    } else {
        /* regualar file permissions */
        /* DEBUG_WEBDAV("Setting mode in stat (file)); */
        ret = S_IFREG | S_IRUSR | S_IWUSR /* regular file, user read & write */
                | S_IRGRP                         /* group read perm */
                | S_IROTH;                        /* others read perm */
    }
    return ret;
}

/*
 * file functions
 */
static int owncloud_stat(const char *uri, csync_vio_file_stat_t *buf) {
    /* get props:
     *   modtime
     *   creattime
     *   size
     */
    int rc = 0;
    csync_vio_file_stat_t *lfs = NULL;
    struct listdir_context  *fetchCtx = NULL;
    char *curi = NULL;
    char *decodedUri = NULL;
    char strbuf[PATH_MAX +1];
    int len = 0;

    DEBUG_WEBDAV("owncloud_stat %s called", uri );

    buf->name = c_basename(uri);

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
        buf->fields  = CSYNC_VIO_FILE_STAT_FIELDS_NONE;
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
        /* fetch data via a propfind call. */
        fetchCtx = c_malloc( sizeof( struct listdir_context ));
        if( ! fetchCtx ) {
            errno = ENOMEM;
            return -1;
        }

        curi = _cleanPath( uri );

        DEBUG_WEBDAV("I have no stat cache, call propfind for %s.", curi );
        fetchCtx->list = NULL;
        fetchCtx->target = curi;
        fetchCtx->include_target = 1;
        fetchCtx->currResource = NULL;

        rc = fetch_resource_list( curi, NE_DEPTH_ONE, fetchCtx );
        if( rc != NE_OK ) {
          if( errno != ENOENT ) {
            set_errno_from_session();
          }
          DEBUG_WEBDAV("stat fails with errno %d", errno );

          return -1;
        }

        if( fetchCtx ) {
            struct resource *res = fetchCtx->list;
            while( res ) {
                /* remove trailing slashes */
                len = strlen(res->uri);
                while( len > 0 && res->uri[len-1] == '/' ) --len;
                memset( strbuf, 0, PATH_MAX+1);
                strncpy( strbuf, res->uri, len < PATH_MAX ? len : PATH_MAX );
                decodedUri = ne_path_unescape( curi ); /* allocates memory */
                if( c_streq(strbuf, decodedUri )) {
                    SAFE_FREE( decodedUri );
                    break;
                }
                res = res->next;
                SAFE_FREE( decodedUri );
            }
            DEBUG_WEBDAV("Working on file %s", res ? res->name : "NULL");

            lfs = resourceToFileStat( res );
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

                csync_vio_file_stat_destroy( lfs );
            }
            SAFE_FREE( fetchCtx );
        }
    }
    DEBUG_WEBDAV("STAT result: %s, type=%d", buf->name ? buf->name:"NULL",
                  buf->type );
    return 0;
}


/* Normally, this module uses get and put. But for creation of new files
 * with owncloud_creat, write is still needed.
 */
static ssize_t owncloud_write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {

  struct transfer_context *writeCtx;
  int rc = 0;
  int neon_stat;
  const ne_status *status;

  writeCtx = (struct transfer_context*) fhandle;

  if (fhandle == NULL) {
      errno = EBADF;
      rc = -1;
  }

  ne_set_request_body_buffer(writeCtx->req, buf, count );

  /* Start the request. */
  neon_stat = ne_request_dispatch( writeCtx->req );
  set_errno_from_neon_errcode( neon_stat );

  status = ne_get_status( writeCtx->req );
  if( status->klass != 2 ) {
    DEBUG_WEBDAV("sendfile request failed with http status %d!", status->code);
    set_errno_from_http_errcode( status->code );
    /* decide if soft error or hard error that stops the whole sync. */
    /* Currently all problems concerning one file are soft errors */
    if( status->klass == 4 /* Forbidden and stuff, soft error */ ) {
      rc = 1;
    } else if( status->klass == 5 /* Server errors and such */ ) {
      rc = 1; /* No Abort on individual file errors. */
    } else {
      rc = 1;
    }
  } else {
    DEBUG_WEBDAV("write request all ok, result code %d", status->code);
  }

  return rc;
}

static int uncompress_reader(void *userdata, const char *buf, size_t len)
{
   struct transfer_context *writeCtx = userdata;
   size_t written = 0;

   if( buf && writeCtx->fd ) {
       /* DEBUG_WEBDAV("Writing NON compressed %d bytes, len); */
       len = write(writeCtx->fd, buf, len);
       if( len != written ) {
           /* DEBUG_WEBDAV("WRN: uncompress_reader wrote wrong num of bytes"); */
       }
       return NE_OK;
   }
   return NE_ERROR;
}

static int compress_reader(void *userdata, const char *buf, size_t len)
{
   struct transfer_context *writeCtx = userdata;
   size_t written = 0;

   if( buf && writeCtx->fd ) {
       /* DEBUG_WEBDAV("Writing compressed %d bytes, len); */
       written = write(writeCtx->fd, buf, len);
       if( written != len ) {
           DEBUG_WEBDAV("WRN: compress reader wrote wrong len");
       }
       return NE_OK;
   }
   return NE_ERROR;
}

/*
 * This hook is called after the response is here from the server, but before
 * the response body is parsed. It decides if the response is compressed and
 * if it is it installs the compression reader accordingly.
 * If the response is not compressed, the normal response body reader is installed.
 */

static void install_content_reader( ne_request *req, void *userdata, const ne_status *status )
{
    const char *enc = NULL;
    struct transfer_context *writeCtx = userdata;

    (void) status;
    if( !writeCtx ) {
        DEBUG_WEBDAV("Error: install_content_reader called without valid write context!");
        return;
    }

    enc = ne_get_response_header( req, "Content-Encoding" );
    if( status && status->code != 200 ) {
      DEBUG_WEBDAV("Content encoding ist <%s> with status %d", enc ? enc : "empty",
                   status ? status->code : -1 );
    }

    if( enc && c_streq( enc, "gzip" )) {
        writeCtx->decompress = ne_decompress_reader( req, ne_accept_2xx,
                                                     compress_reader,     /* reader callback */
                                                     (void*) writeCtx );  /* userdata        */
    } else {
        ne_add_response_body_reader( req, ne_accept_2xx,
                                     uncompress_reader,
                                     (void*) writeCtx );
        writeCtx->decompress = NULL;
    }
}

static char*_lastDir = NULL;

/* capabilities are currently:
 *  bool atomar_copy_support
 *  bool put_support
 *  bool get_support
 */

static struct csync_vio_capabilities_s _owncloud_capabilities = {
    .atomar_copy_support = true,
    .get_support = true,
    .put_support = true,
};

static csync_vio_capabilities_t *owncloud_get_capabilities(void)
{
  return &_owncloud_capabilities;
}

static csync_vio_method_handle_t *owncloud_open(const char *durl,
                                                int flags,
                                                mode_t mode) {
    char *dir = NULL;
    char getUrl[PATH_MAX];
    int put = 0;
    int rc = NE_OK;
#ifdef _WIN32
    int gtp = 0;
    char tmpname[13];
    mbchar_t winTmp[PATH_MAX];
    mbchar_t *winUrlMB = NULL;
    char *winTmpUtf8 = NULL;
    csync_stat_t sb;
#endif

    struct transfer_context *writeCtx = NULL;
    csync_stat_t statBuf;
    memset( getUrl, '\0', PATH_MAX );

    (void) mode; /* unused on webdav server */
    DEBUG_WEBDAV( "=> open called for %s", durl );

    if( rc == NE_OK )
        dav_connect( durl );

    if (flags & O_WRONLY) {
        put = 1;
    }
    if (flags & O_RDWR) {
        put = 1;
    }
    if (flags & O_CREAT) {
        put = 1;
    }

    if( rc == NE_OK && put ) {
      /* check if the dir name exists. Otherwise return ENOENT */
      dir = c_dirname( durl );
      if (dir == NULL) {
        errno = ENOMEM;
        return NULL;
      }
      DEBUG_WEBDAV("Stating directory %s", dir );
      if( c_streq( dir, _lastDir )) {
        DEBUG_WEBDAV("Dir %s is there, we know it already.", dir);
      } else {
        if( owncloud_stat( dir, (csync_vio_method_handle_t*)(&statBuf) ) == 0 ) {
          DEBUG_WEBDAV("Directory of file to open exists.");
          SAFE_FREE( _lastDir );
          _lastDir = c_strdup(dir);

        } else {
          DEBUG_WEBDAV("Directory %s of file to open does NOT exist.", dir );
          /* the directory does not exist. That is an ENOENT */
          errno = ENOENT;
          SAFE_FREE( dir );
          return NULL;
        }
      }
    }

    writeCtx = c_malloc( sizeof(struct transfer_context) );

    writeCtx->url = _cleanPath( durl );
    if( ! writeCtx->url ) {
        DEBUG_WEBDAV("Failed to clean path for %s", durl );
        errno = EACCES;
        rc = NE_ERROR;
    }

    if( rc == NE_OK && put) {
        DEBUG_WEBDAV("PUT request on %s!", writeCtx->url);
        writeCtx->req = ne_request_create(dav_session.ctx, "PUT", writeCtx->url);
        writeCtx->method = "PUT";
    }

    if( rc == NE_OK && ! put ) {
        writeCtx->req = 0;
        writeCtx->method = "GET";

        /* the download via the get function requires a full uri */
        snprintf( getUrl, PATH_MAX, "%s://%s%s", ne_get_scheme( dav_session.ctx),
                  ne_get_server_hostport( dav_session.ctx ), writeCtx->url );
        DEBUG_WEBDAV("GET request on %s", getUrl );

        writeCtx->req = ne_request_create( dav_session.ctx, "GET", getUrl );

        /* Call the progress callback */
        if (_file_progress_cb) {
            ne_set_notifier(dav_session.ctx, ne_notify_status_cb, writeCtx);
            _file_progress_cb( writeCtx->url, CSYNC_NOTIFY_START_DOWNLOAD, 0 , 0, _userdata);
        }
    }

    if( rc != NE_OK ) {
        SAFE_FREE( writeCtx );
        writeCtx = NULL;
    }

    SAFE_FREE( dir );

    return (csync_vio_method_handle_t *) writeCtx;
}

static csync_vio_method_handle_t *owncloud_creat(const char *durl, mode_t mode) {

    csync_vio_method_handle_t *handle = owncloud_open(durl, O_CREAT|O_WRONLY|O_TRUNC, mode);

    /* on create, the file needs to be created empty */
    owncloud_write( handle, NULL, 0 );

    return handle;
}

/*
 * Puts a file read from the open file descriptor to the ownCloud URL.
*/
static int owncloud_put(csync_vio_method_handle_t *flocal,
                        csync_vio_method_handle_t *fremote,
                        csync_vio_file_stat_t *vfs) {
  int rc = 0;
  int neon_stat;
  const ne_status *status;
  csync_stat_t sb;
  struct transfer_context *write_ctx = (struct transfer_context*) fremote;
  int fd;
  ne_request *request = NULL;

  fd = csync_vio_getfd(flocal);
  if (fd == -1) {
      errno = EINVAL;
      return -1;
  }

  if( write_ctx == NULL ) {
      errno = EINVAL;
      return -1;
  }
  request = write_ctx->req;

  if( request == NULL) {
    errno = EINVAL;
    return -1;
  }

  /* stat the source-file to get the file size. */
  if( fstat( fd, &sb ) == 0 ) {
    if( sb.st_size != vfs->size ) {
      DEBUG_WEBDAV("WRN: Stat size differs from vfs size!");
    }
    /* Attach the request to the file descriptor */
    ne_set_request_body_fd(request, fd, 0, sb.st_size);
    DEBUG_WEBDAV("Put file size: %lld, variable sizeof: %ld", (long long int) sb.st_size,
                 sizeof(sb.st_size));

    /* Start the request. */
    neon_stat = ne_request_dispatch( write_ctx->req );
    set_errno_from_neon_errcode( neon_stat );

    status = ne_get_status( request );
    if( status->klass != 2 ) {
      DEBUG_WEBDAV("sendfile request failed with http status %d!", status->code);
      set_errno_from_http_errcode( status->code );
      /* decide if soft error or hard error that stops the whole sync. */
      /* Currently all problems concerning one file are soft errors */
      if( status->klass == 4 /* Forbidden and stuff, soft error */ ) {
        rc = 1;
      } else if( status->klass == 5 /* Server errors and such */ ) {
        rc = 1; /* No Abort on individual file errors. */
      } else {
        rc = 1;
      }
    } else {
      DEBUG_WEBDAV("http request all cool, result code %d", status->code);
    }
  } else {
    DEBUG_WEBDAV("Could not stat file descriptor");
    rc = 1;
  }

  return rc;
}

/* Gets a file from the owncloud url to the open file descriptor. */
static int owncloud_get(csync_vio_method_handle_t *flocal,
                        csync_vio_method_handle_t *fremote,
                        csync_vio_file_stat_t *vfs) {
  int rc = 0;
  int neon_stat;
  const ne_status *status;
  int fd;

  struct transfer_context *write_ctx = (struct transfer_context*) fremote;
  (void) vfs; /* stat information of the source file */

  fd = csync_vio_getfd(flocal);
  if (fd == -1) {
    errno = EINVAL;
    return -1;
  }

  /* GET a file to the open file descriptor */
  if( write_ctx == NULL ) {
    errno = EINVAL;
    return -1;
  }

  if( write_ctx->req == NULL ) {
    errno = EINVAL;
    return -1;
  }

  DEBUG_WEBDAV("  -- GET on %s", write_ctx->url);

  write_ctx->fd = fd;

  /* Allow compressed content by setting the header */
  ne_add_request_header( write_ctx->req, "Accept-Encoding", "gzip,deflate" );

  /* hook called before the content is parsed to set the correct reader,
         * either the compressed- or uncompressed reader.
         */
  ne_hook_post_headers( dav_session.ctx, install_content_reader, write_ctx );

  neon_stat = ne_request_dispatch(write_ctx->req );
  /* possible return codes are:
   *  NE_OK, NE_AUTH, NE_CONNECT, NE_TIMEOUT, NE_ERROR (from ne_request.h)
   */

  if( neon_stat != NE_OK ) {
    set_errno_from_neon_errcode(neon_stat);
    DEBUG_WEBDAV("Error GET: Neon: %d, errno %d", neon_stat, errno);
    rc = -1;
  } else {
    status = ne_get_status( write_ctx->req );
    if( status->klass != 2 ) {
      DEBUG_WEBDAV("sendfile request failed with http status %d!", status->code);
      set_errno_from_http_errcode( status->code );
      /* decide if soft error or hard error that stops the whole sync. */
      /* Currently all problems concerning one file are soft errors */
      if( status->klass == 4 /* Forbidden and stuff, soft error */ ) {
        rc = 1;
      } else if( status->klass == 5 /* Server errors and such */ ) {
        rc = 1; /* No Abort on individual file errors. */
      } else {
        rc = 1;
      }
    } else {
      DEBUG_WEBDAV("http request all cool, result code %d (%s)", status->code,
                   status->reason_phrase ? status->reason_phrase : "<empty>");
    }
  }

  /* delete the hook again, otherwise they get chained as they are with the session */
  ne_unhook_post_headers( dav_session.ctx, install_content_reader, write_ctx );

  /* if the compression handle is set through the post_header hook, delete it. */
  if( write_ctx->decompress ) {
    ne_decompress_destroy( write_ctx->decompress );
  }

  return rc;
}

static int owncloud_close(csync_vio_method_handle_t *fhandle) {
    struct transfer_context *writeCtx;
    int ret = 0;
    enum csync_notify_type_e notify_tag;

    writeCtx = (struct transfer_context*) fhandle;

    if (fhandle == NULL) {
        errno = EBADF;
        ret = -1;
    }

    /* handle the PUT request */
    if( ret != -1 && strcmp( writeCtx->method, "PUT" ) == 0 ) {
      notify_tag = CSYNC_NOTIFY_FINISHED_UPLOAD;
      ne_request_destroy( writeCtx->req );
      _fs.name = NULL;
    } else  {
      /* Its a GET request. */
      notify_tag = CSYNC_NOTIFY_FINISHED_DOWNLOAD;
    }

    /* Finish callback */
    if (_file_progress_cb) {
        ne_set_notifier(dav_session.ctx, 0, 0);
        _file_progress_cb(writeCtx->url, notify_tag, 0, 0, _userdata );
    }

    /* free mem. Note that the request mem is freed by the ne_request_destroy call */
    SAFE_FREE( writeCtx->url );
    SAFE_FREE( writeCtx );

    return ret;
}

static ssize_t owncloud_read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {

  (void) fhandle;
  (void) buf;
  (void) count;
  DEBUG_WEBDAV("XXXXXXXXXXXXXXXXXXX We should not have come here - owncloud_read");

  return 0;
}

static off_t owncloud_lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
    (void) fhandle;
    (void) offset;
    (void) whence;

    return -1;
}

/*
 * directory functions
 */
static csync_vio_method_handle_t *owncloud_opendir(const char *uri) {
    int rc;
    struct listdir_context *fetchCtx = NULL;
    struct resource *reslist = NULL;
    char *curi = _cleanPath( uri );

    DEBUG_WEBDAV("opendir method called on %s", uri );

    dav_connect( uri );

    fetchCtx = c_malloc( sizeof( struct listdir_context ));

    fetchCtx->list = reslist;
    fetchCtx->target = curi;
    fetchCtx->include_target = 0;
    fetchCtx->currResource = NULL;

    rc = fetch_resource_list( curi, NE_DEPTH_ONE, fetchCtx );
    if( rc != NE_OK ) {
        set_errno_from_session();
        return NULL;
    } else {
        fetchCtx->currResource = fetchCtx->list;
        DEBUG_WEBDAV("opendir returning handle %p", (void*) fetchCtx );
        return fetchCtx;
    }
    /* no freeing of curi because its part of the fetchCtx and gets freed later */
}

static int owncloud_closedir(csync_vio_method_handle_t *dhandle) {

    struct listdir_context *fetchCtx = dhandle;
    struct resource *r = fetchCtx->list;
    struct resource *rnext = NULL;

    DEBUG_WEBDAV("closedir method called %p!", dhandle);

    while( r ) {
        rnext = r->next;
        SAFE_FREE(r->uri);
        SAFE_FREE(r->name);
        SAFE_FREE(r);
        r = rnext;
    }
    SAFE_FREE( fetchCtx->target );

    SAFE_FREE( dhandle );
    return 0;
}

static csync_vio_file_stat_t *owncloud_readdir(csync_vio_method_handle_t *dhandle) {

    struct listdir_context *fetchCtx = dhandle;
    csync_vio_file_stat_t *lfs = NULL;

    if( fetchCtx == NULL) {
        /* DEBUG_WEBDAV("An empty dir or at end); */
        return NULL;
    }

    if( fetchCtx->currResource ) {
        /* FIXME: Who frees the allocated mem for lfs, allocated in the helper func? */
        lfs = resourceToFileStat( fetchCtx->currResource );

        /* set pointer to next element */
        fetchCtx->currResource = fetchCtx->currResource->next;

        /* fill the static stat buf as input for the stat function */
        _fs.name   = lfs->name;
        _fs.mtime  = lfs->mtime;
        _fs.fields = lfs->fields;
        _fs.type   = lfs->type;
        _fs.size   = lfs->size;
    }

    // DEBUG_WEBDAV("LFS fields: %s: %d, lfs->name, lfs->type );
    return lfs;
}

static int owncloud_mkdir(const char *uri, mode_t mode) {
    int rc = NE_OK;
    char buf[PATH_MAX +1];
    int len = 0;

    char *path = _cleanPath( uri );
    (void) mode; /* unused */

    if( ! path ) {
        errno = EINVAL;
        rc = -1;
    }
    rc = dav_connect(uri);
    if (rc < 0) {
        errno = EINVAL;
    }

    /* the uri path is required to have a trailing slash */
    if( rc >= 0 ) {
      memset( buf,0, PATH_MAX+1 );
      len = strlen( path );
      strncpy( buf, path, len );
      if( buf[len-1] != '/' ) {
          buf[len] = '/';
      }

      DEBUG_WEBDAV("MKdir on %s", buf );
      rc = ne_mkcol(dav_session.ctx, buf );
      if (rc != NE_OK ) {
          set_errno_from_session();
      }
    }
    SAFE_FREE( path );

    if( rc < 0 || rc != NE_OK ) {
        return -1;
    }
    return 0;
}

static int owncloud_rmdir(const char *uri) {
    int rc = NE_OK;
    char* curi = _cleanPath( uri );

    rc = dav_connect(uri);
    if (rc < 0) {
        errno = EINVAL;
    }

    if( rc >= 0 ) {
        rc = ne_delete(dav_session.ctx, curi);
        if ( rc != NE_OK ) {
          set_errno_from_session();
        }
    }
    SAFE_FREE( curi );
    if( rc < 0 || rc != NE_OK ) {
        return -1;
    }

    return 0;
}

static int owncloud_rename(const char *olduri, const char *newuri) {
    char *src = NULL;
    char *target = NULL;
    int rc = NE_OK;


    rc = dav_connect(olduri);
    if (rc < 0) {
        errno = EINVAL;
    }

    src    = _cleanPath( olduri );
    target = _cleanPath( newuri );

    if( rc >= 0 ) {
        DEBUG_WEBDAV("MOVE: %s => %s: %d", src, target, rc );
        rc = ne_move(dav_session.ctx, 1, src, target );

        if (rc != NE_OK ) {
          set_errno_from_session();
        }
    }
    SAFE_FREE( src );
    SAFE_FREE( target );

    if( rc != NE_OK )
        return -1;
    return 0;
}

static int owncloud_unlink(const char *uri) {
    int rc = NE_OK;
    char *path = _cleanPath( uri );

    if( ! path ) {
        rc = NE_ERROR;
        errno = EINVAL;
    }
    if( rc == NE_OK ) {
        rc = dav_connect(uri);
        if (rc < 0) {
            errno = EINVAL;
        }
    }
    if( rc == NE_OK ) {
        rc = ne_delete( dav_session.ctx, path );
        if ( rc != NE_OK )
            set_errno_from_session();
    }
    SAFE_FREE( path );

    return 0;
}

static int owncloud_chmod(const char *uri, mode_t mode) {
    (void) uri;
    (void) mode;

    return 0;
}

static int owncloud_chown(const char *uri, uid_t owner, gid_t group) {
    (void) uri;
    (void) owner;
    (void) group;

    return 0;
}

static int owncloud_set_property(const char *key, void *data) {
#define READ_STRING_PROPERTY(P) \
    if (c_streq(key, #P)) { \
        SAFE_FREE(dav_session.P); \
        dav_session.P = c_strdup((const char*)data); \
        return 0; \
    }
#undef READ_STRING_PROPERTY

    if (c_streq(key, "file_progress_callback")) {
        _file_progress_cb = *(csync_file_progress_callback*)(data);
        return 0;
    }

    return -1;
}


static char *owncloud_error_string()
{
    return dav_session.error_string;
}

static int owncloud_utimes(const char *uri, const struct timeval *times) {

    ne_proppatch_operation ops[2];
    ne_propname pname;
    int rc = NE_OK;
    char val[255];
    char *curi;

    curi = _cleanPath( uri );

    if( ! uri ) {
        errno = ENOENT;
        return -1;
    }
    if( !times ) {
        errno = EACCES;
        return -1; /* FIXME: Find good errno */
    }
    pname.nspace = "DAV:";
    pname.name = "lastmodified";

    snprintf( val, sizeof(val), "%ld", times->tv_sec );
    DEBUG_WEBDAV("Setting LastModified of %s to %s", curi, val );

    ops[0].name = &pname;
    ops[0].type = ne_propset;
    ops[0].value = val;

    ops[1].name = NULL;

    rc = ne_proppatch( dav_session.ctx, curi, ops );
    SAFE_FREE(curi);

    if( rc != NE_OK ) {
        errno = EPERM;
        DEBUG_WEBDAV("Error in propatch: %d", rc);
        return -1;
    }
    return 0;
}

csync_vio_method_t _method = {
    .method_table_size = sizeof(csync_vio_method_t),
    .get_capabilities = owncloud_get_capabilities,
    .open             = owncloud_open,
    .creat            = owncloud_creat,
    .close            = owncloud_close,
    .read             = owncloud_read,
    .write            = owncloud_write,
    .lseek            = owncloud_lseek,
    .opendir          = owncloud_opendir,
    .closedir         = owncloud_closedir,
    .readdir          = owncloud_readdir,
    .mkdir            = owncloud_mkdir,
    .rmdir            = owncloud_rmdir,
    .stat             = owncloud_stat,
    .rename           = owncloud_rename,
    .unlink           = owncloud_unlink,
    .chmod            = owncloud_chmod,
    .chown            = owncloud_chown,
    .utimes           = owncloud_utimes,
    .get_error_string = owncloud_error_string,
    .set_property     = owncloud_set_property,
    .put              = owncloud_put,
    .get              = owncloud_get
};

csync_vio_method_t *vio_module_init(const char *method_name, const char *args,
                                    csync_auth_callback cb, void *userdata) {
    (void) method_name;
    (void) args;
    _authcb = cb;
    _userdata = userdata;

    return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
    (void) method;

    SAFE_FREE( dav_session.user );
    SAFE_FREE( dav_session.pwd );

    SAFE_FREE( dav_session.error_string );

    SAFE_FREE( _lastDir );

    if( dav_session.ctx )
        ne_session_destroy( dav_session.ctx );
}



/* vim: set ts=4 sw=4 et cindent: */
