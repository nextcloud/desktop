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
#include <limits.h>
#include <stdlib.h>

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
#include "csync_misc.h"
#include "c_private.h"

#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.owncloud"
#include "csync_log.h"

#ifdef NDEBUG
#define DEBUG_WEBDAV(...)
#else
#define DEBUG_WEBDAV(...) CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, __VA_ARGS__)
#endif

#define OC_TIMEDELTA_FAIL (NE_REDIRECT +1)
#define OC_PROPFIND_FAIL  (NE_REDIRECT +2)


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
    char*              md5;

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
    const char  *method;        /* the HTTP method, either PUT or GET  */
    ne_decompress *decompress;  /* the decompress context */
    int         fileWritten;    /* flag to indicate that a buffer file was written for PUTs */
    char        *clean_uri;
};

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

    long int prev_delta;
    long int time_delta;     /* The time delta to use.                  */
    long int time_delta_sum; /* What is the time delta average?         */
    long int time_delta_cnt; /* How often was the server time gathered? */
};

/* The list of properties that is fetched in PropFind on a collection */
static const ne_propname ls_props[] = {
    { "DAV:", "getlastmodified" },
    { "DAV:", "getcontentlength" },
    { "DAV:", "resourcetype" },
    { "DAV:", "getetag"},
    { NULL, NULL }
};

/*
 * local variables.
 */

struct dav_session_s dav_session; /* The DAV Session, initialised in dav_connect */
int _connected = 0;                   /* flag to indicate if a connection exists, ie.
                                     the dav_session is valid */
csync_vio_file_stat_t _fs;

csync_auth_callback _authcb;

#define PUT_BUFFER_SIZE 1024*5

char _buffer[PUT_BUFFER_SIZE];

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
    DEBUG_WEBDAV("Session error string %s", p);

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
    case 405:
        return EEXIST;  /* Method Not Allowed */
    case 400:           /* Bad Request */
    case 403:           /* Forbidden */
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
        } else {
	    DEBUG_WEBDAV("Authentication callback replied %s", buf );
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
    char buf[NE_ABUFSIZ];

    (void) userdata;
    (void) realm;

    /* DEBUG_WEBDAV( "Authentication required %s", realm ); */
    if( username && password ) {
        DEBUG_WEBDAV( "Authentication required %s", username );
        if( dav_session.user ) {
            /* allow user without password */
            if( strlen( dav_session.user ) < NE_ABUFSIZ ) {
                strcpy( username, dav_session.user );
            }
            if( dav_session.pwd && strlen( dav_session.pwd ) < NE_ABUFSIZ ) {
                strcpy( password, dav_session.pwd );
            }
        } else if( _authcb != NULL ){
            /* call the csync callback */
            DEBUG_WEBDAV("Call the csync callback for %s", realm );
            memset( buf, 0, NE_ABUFSIZ );
            (*_authcb) ("Enter your username: ", buf, NE_ABUFSIZ-1, 1, 0, userdata );
            if( strlen(buf) < NE_ABUFSIZ ) {
                strcpy( username, buf );
            }
            memset( buf, 0, NE_ABUFSIZ );
            (*_authcb) ("Enter your password: ", buf, NE_ABUFSIZ-1, 0, 0, userdata );
            if( strlen(buf) < NE_ABUFSIZ) {
                strcpy( password, buf );
            }
        } else {
            DEBUG_WEBDAV("I can not authenticate!");
        }
    }
    return attempt;
}

/*
 * Authentication callback. Is set by ne_set_proxy_auth to be called
 * from the neon lib to authenticate against a proxy. The data to authenticate
 * against comes from mirall throught vio_module_init function.
 */
static int ne_proxy_auth( void *userdata, const char *realm, int attempt,
                          char *username, char *password)
{
    (void) userdata;
    (void) realm;
    if( dav_session.proxy_user && strlen( dav_session.proxy_user ) < NE_ABUFSIZ) {
        strcpy( username, dav_session.proxy_user );
        if( dav_session.proxy_pwd && strlen( dav_session.proxy_pwd ) < NE_ABUFSIZ) {
            strcpy( password, dav_session.proxy_pwd );
        }
    }

    return attempt;
}

/* Configure the proxy depending on the variables */
static int configureProxy( ne_session *session )
{
    int port = 8080;
    int re = -1;

    if( ! session ) return -1;
    if( ! dav_session.proxy_type ) return 0; /* Go by NoProxy per default */

    if( dav_session.proxy_port > 0 ) {
        port = dav_session.proxy_port;
    }

    if( c_streq(dav_session.proxy_type, "NoProxy" )) {
        DEBUG_WEBDAV("No proxy configured.");
        re = 0;
    } else if( c_streq(dav_session.proxy_type, "DefaultProxy") ||
               c_streq(dav_session.proxy_type, "HttpProxy")    ||
               c_streq(dav_session.proxy_type, "HttpCachingProxy") ) {
        if( dav_session.proxy_host ) {
            DEBUG_WEBDAV("%s at %s:%d", dav_session.proxy_type, dav_session.proxy_host, port );
            ne_session_proxy(session, dav_session.proxy_host, port );
            re = 2;
        } else {
            DEBUG_WEBDAV("%s requested but no proxy host defined.", dav_session.proxy_type );
	    /* we used to try ne_system_session_proxy here, but we should rather err out
	       to behave exactly like the caller. */
        }
    } else if( c_streq(dav_session.proxy_type, "FtpCachingProxy") ||
               c_streq(dav_session.proxy_type, "Socks5Proxy") ) {
        DEBUG_WEBDAV( "Unsupported Proxy: %s", dav_session.proxy_type );
    }

    return re;
}

/*
 * This hook is called for with the response of a request. Here its checked
 * if a Set-Cookie header is there for the PHPSESSID. The key is stored into
 * the webdav session to be added to subsequent requests.
 */
#define PHPSESSID "PHPSESSID="
static void post_request_hook(ne_request *req, void *userdata, const ne_status *status)
{
    const char *set_cookie_header = NULL;
    const char *sc  = NULL;
    char *key = NULL;

    (void) userdata;

    if(!(status && req)) return;
    if( status->klass == 2 || status->code == 401 ) {
        /* successful request */
        set_cookie_header =  ne_get_response_header( req, "Set-Cookie" );
        if( set_cookie_header ) {
            DEBUG_WEBDAV(" Set-Cookie found: %s", set_cookie_header);
            /* try to find a ', ' sequence which is the separator of neon if multiple Set-Cookie
             * headers are there.
             * The following code parses a string like this:
             * PHPSESSID=n8feu3dsbarpvvufqae9btn5fl7m7ikgh5ml1fg37v4i2cah7k41; path=/; HttpOnly */
            sc = set_cookie_header;
            while(sc) {
                if( strlen(sc) > strlen(PHPSESSID) &&
                        strncmp( sc, PHPSESSID, strlen(PHPSESSID)) == 0 ) {
                    const char *sc_val = sc; /* + strlen(PHPSESSID); */
                    const char *sc_end = sc_val;
                    int cnt = 0;
                    int len = strlen(sc_val); /* The length of the rest of the header string. */

                    while( cnt < len && *sc_end != ';' && *sc_end != ',') {
                        cnt++;
                        sc_end++;
                    }
                    if( cnt == len ) {
                        /* exit: We are at the end. */
                        sc = NULL;
                    } else if( *sc_end == ';' ) {
                        /* We are at the end of the session key. */
                        int keylen = sc_end-sc_val;
                        if( key ) SAFE_FREE(key);
                        key = c_malloc(keylen+1);
                        strncpy( key, sc_val, keylen );
                        key[keylen] = '\0';

                        /* now search for a ',' to find a potential other header entry */
                        while(cnt < len && *sc_end != ',') {
                            cnt++;
                            sc_end++;
                        }
                        if( cnt < len )
                            sc = sc_end+2; /* mind the space after the comma */
                        else
                            sc = NULL;
                    } else if( *sc_end == ',' ) {
                        /* A new entry is to check. */
                        if( *(sc_end + 1) == ' ') {
                            sc = sc_end+2;
                        } else {
                            /* error condition */
                            sc = NULL;
                        }
                    }
                } else {
                    /* It is not a PHPSESSID-Header but another one which we're not interested in.
                     * forward to next header entry (search ',' )
                     */
                    int len = strlen(sc);
                    int cnt = 0;

                    while(cnt < len && *sc != ',') {
                        cnt++;
                        sc++;
                    }
                    if( cnt < len )
                        sc = sc+2; /* mind the space after the comma */
                    else
                        sc = NULL;
                }
            }
        }
    } else {
        DEBUG_WEBDAV("Request failed, don't take session header.");
    }
    if( key ) {
        DEBUG_WEBDAV("----> Session-key: %s", key);
        SAFE_FREE(dav_session.session_key);
        dav_session.session_key = key;
    }
}

/*
 * this hook is called just after a request has been created, before its sent.
 * Here it is used to set the session cookie if available.
 */
static void request_created_hook(ne_request *req, void *userdata,
                                 const char *method, const char *requri)
{
    (void) userdata;
    (void) method;
    (void) requri;

    if( !req ) return;

    if(dav_session.session_key) {
        /* DEBUG_WEBDAV("Setting PHPSESSID to %s", dav_session.session_key); */
        ne_add_request_header(req, "Cookie", dav_session.session_key);
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
    char protocol[6] = {'\0'};
    char uaBuf[256];
    char *path = NULL;
    char *scheme = NULL;
    char *host = NULL;
    unsigned int port = 0;
    int proxystate = -1;

    if (_connected) {
        return 0;
    }

    dav_session.time_delta_sum = 0;
    dav_session.time_delta_cnt = 0;
    dav_session.prev_delta     = 0;

    rc = c_parse_uri( base_url, &scheme, &dav_session.user, &dav_session.pwd, &host, &port, &path );
    if( rc < 0 ) {
        DEBUG_WEBDAV("Failed to parse uri %s", base_url );
        goto out;
    }

    DEBUG_WEBDAV("* scheme %s", scheme );
    DEBUG_WEBDAV("* host %s", host );
    DEBUG_WEBDAV("* port %u", port );
    DEBUG_WEBDAV("* path %s", path );

    if( strcmp( scheme, "owncloud" ) == 0 ) {
        strcpy( protocol, "http");
    } else if( strcmp( scheme, "ownclouds" ) == 0 ) {
        strcpy( protocol, "https");
        useSSL = 1;
    } else {
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
    ne_set_useragent( dav_session.ctx, uaBuf);
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
    ne_redirect_register( dav_session.ctx );

    /* Hook to get the Session ID */
    ne_hook_post_headers( dav_session.ctx, post_request_hook, NULL );
    /* Hook called when a request is built. It sets the PHPSESSID header */
    ne_hook_create_request( dav_session.ctx, request_created_hook, NULL );

    dav_session.session_key = NULL;

    /* Proxy support */
    proxystate = configureProxy( dav_session.ctx );
    if( proxystate < 0 ) {
        DEBUG_WEBDAV("Error: Proxy-Configuration failed.");
    } else if( proxystate > 0 ) {
        ne_set_proxy_auth( dav_session.ctx, ne_proxy_auth, 0 );
    }

    _connected = 1;
    rc = 0;
out:
    SAFE_FREE(path);
    SAFE_FREE(host);
    SAFE_FREE(scheme);
    return rc;
}

#ifndef HAVE_TIMEGM
#ifdef _WIN32
static int is_leap(unsigned y) {
    y += 1900;
    return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);
}

static time_t timegm(struct tm *tm) {
    static const unsigned ndays[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31} };

    time_t res = 0;
    int i;

    for (i = 70; i < tm->tm_year; ++i)
        res += is_leap(i) ? 366 : 365;

    for (i = 0; i < tm->tm_mon; ++i)
        res += ndays[is_leap(tm->tm_year)][i];
     res += tm->tm_mday - 1;
     res *= 24;
     res += tm->tm_hour;
     res *= 60;
     res += tm->tm_min;
     res *= 60;
     res += tm->tm_sec;
     return res;
}
#else
/* A hopefully portable version of timegm */
static time_t timegm(struct tm *tm ) {
     time_t ret;
     char *tz;

     tz = getenv("TZ");
     setenv("TZ", "", 1);
     tzset();
     ret = mktime(tm);
     if (tz)
         setenv("TZ", tz, 1);
     else
         unsetenv("TZ");
     tzset();
     return ret;
}
#endif /* Platform switch */
#endif /* HAVE_TIMEGM */

#define RFC1123_FORMAT "%3s, %02d %3s %4d %02d:%02d:%02d GMT"
static const char short_months[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
/*
 * This function is borrowed from libneon's ne_httpdate_parse.
 * Unfortunately that one converts to local time but here UTC is
 * needed.
 * This one uses timegm instead, which returns UTC.
 */
static time_t oc_httpdate_parse( const char *date ) {
    struct tm gmt;
    char wkday[4], mon[4];
    int n;
    time_t result;

    memset(&gmt, 0, sizeof(struct tm));

    /*  it goes: Sun, 06 Nov 1994 08:49:37 GMT */
    n = sscanf(date, RFC1123_FORMAT,
               wkday, &gmt.tm_mday, mon, &gmt.tm_year, &gmt.tm_hour,
               &gmt.tm_min, &gmt.tm_sec);
    /* Is it portable to check n==7 here? */
    gmt.tm_year -= 1900;
    for (n=0; n<12; n++)
        if (strcmp(mon, short_months[n]) == 0)
            break;
    /* tm_mon comes out as 12 if the month is corrupt, which is desired,
     * since the mktime will then fail */
    gmt.tm_mon = n;
    gmt.tm_isdst = -1;
    result = timegm(&gmt);
    return result;
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
    const char *md5sum = NULL;
    const ne_status *status = NULL;
    char *path = ne_path_unescape( uri->path );

    /* It seems strange: first uri->path is unescaped to escape it in the next step again.
     * The reason is that uri->path is not completely escaped (ie. it seems only to have
     * spaces escaped), while the fetchCtx->target is fully escaped.
     * See http://bugs.owncloud.org/thebuggenie/owncloud/issues/oc-613
     */
    char *escaped_path = ne_path_escape( path );

    (void) status;
    if( ! fetchCtx ) {
        DEBUG_WEBDAV("No valid fetchContext");
        return;
    }

    if( ! fetchCtx->target ) {
        DEBUG_WEBDAV("error: target must not be zero!" );
        return;
    }

    /* see if the target should be included in the result list. */
    if (ne_path_compare(fetchCtx->target, escaped_path) == 0 && !fetchCtx->include_target) {
        /* This is the target URI */
        DEBUG_WEBDAV( "Skipping target resource.");
        /* Free the private structure. */
        SAFE_FREE( path );
        SAFE_FREE( escaped_path );
        return;
    }
    SAFE_FREE( escaped_path );
    /* Fill the resource structure with the data about the file */
    newres = c_malloc(sizeof(struct resource));
    newres->uri =  path; /* no need to strdup because ne_path_unescape already allocates */
    newres->name = c_basename( path );

    modtime      = ne_propset_value( set, &ls_props[0] );
    clength      = ne_propset_value( set, &ls_props[1] );
    resourcetype = ne_propset_value( set, &ls_props[2] );
    md5sum       = ne_propset_value( set, &ls_props[3] );

    newres->type = resr_normal;
    if( clength == NULL && resourcetype && strncmp( resourcetype, "<DAV:collection>", 16 ) == 0) {
        newres->type = resr_collection;
    }

    if (modtime) {
        newres->modtime = oc_httpdate_parse(modtime);
    }

    /* DEBUG_WEBDAV("Parsing Modtime: %s -> %llu", modtime, (unsigned long long) newres->modtime ); */

    if (clength) {
        char *p;

        newres->size = DAV_STRTOL(clength, &p, 10);
        if (*p) {
            newres->size = 0;
        }
    }

    if( md5sum ) {
        int len = strlen(md5sum)-2;
        if( len > 0 ) {
            /* Skip the " around the string coming back from the ne_propset_value call */
            newres->md5 = c_malloc(len+1);
            strncpy( newres->md5, md5sum+1, len );
            newres->md5[len] = '\0';
        }
    }

    /* prepend the new resource to the result list */
    newres->next   = fetchCtx->list;
    fetchCtx->list = newres;
    fetchCtx->result_count = fetchCtx->result_count + 1;
    /* DEBUG_WEBDAV( "results for URI %s: %d %d", newres->name, (int)newres->size, (int)newres->type ); */
}

/*
 * fetches a resource list from the WebDAV server. This is equivalent to list dir.
 */

static int fetch_resource_list( const char *curi,
                                int depth,
                                struct listdir_context *fetchCtx )
{
    int ret = -1;
    ne_propfind_handler *hdl = NULL;
    ne_request *request = NULL;
    const char *date_header = NULL;
    time_t server_time;
    time_t now;
    time_t time_diff;
    time_t time_diff_delta;
    const char *err = NULL;

    /* do a propfind request and parse the results in the results function, set as callback */
    /* ret = ne_simple_propfind( dav_session.ctx, curi, depth, ls_props, results, fetchCtx ); */

    hdl = ne_propfind_create(dav_session.ctx, curi, depth);

    if(hdl)
        ret = ne_propfind_named(hdl, ls_props, results, fetchCtx);

    if( ret == NE_OK ) {
        DEBUG_WEBDAV("Simple propfind OK.");
        fetchCtx->currResource = fetchCtx->list;
        request = ne_propfind_get_request( hdl );

        date_header =  ne_get_response_header( request, "Date" );
        DEBUG_WEBDAV("Server Date from HTTP header value: %s", date_header);
        server_time = oc_httpdate_parse(date_header);
        now = time(NULL);
        time_diff = server_time - now;

        dav_session.time_delta_sum += time_diff;
        dav_session.time_delta_cnt++;

        /* Store the previous time delta */
        dav_session.prev_delta = dav_session.time_delta;

        /* check the changing of the time delta */
        time_diff_delta = llabs(dav_session.time_delta - time_diff);
        if( dav_session.time_delta_cnt == 1 ) {
            DEBUG_WEBDAV( "The first time_delta is %llu", (unsigned long long) time_diff );
        } else if( dav_session.time_delta_cnt > 1 ) {
            if( time_diff_delta > 5 ) {
                DEBUG_WEBDAV("WRN: The time delta changed more than 5 second");
                ret = OC_TIMEDELTA_FAIL;
            } else {
                DEBUG_WEBDAV("Ok: Time delta remained (almost) the same: %llu.", (unsigned long long) time_diff);
            }
        } else {
          DEBUG_WEBDAV("Difference to last server time delta: %llu", (unsigned long long) time_diff_delta );
        }
        dav_session.time_delta = time_diff;
    } else {
        err = ne_get_error( dav_session.ctx );
        DEBUG_WEBDAV("WRN: propfind named failed with %d, request error: %s", ret, err ? err : "<nil>");
    }

    if( hdl )
        ne_propfind_destroy(hdl);

    if( ret == -1 ) ret = NE_ERROR;
    return ret;
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

    /* Correct the mtime of the file with the server time delta */
    DEBUG_WEBDAV("  :> Subtracting %d from modtime %llu", dav_session.time_delta,
		 (unsigned long long) res->modtime);
    lfs->mtime = res->modtime - dav_session.time_delta ;
    lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
    lfs->size  = res->size;
    lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
    if( res->md5 ) {
        lfs->md5   = c_strdup(res->md5);
    }
    lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MD5;
    return lfs;
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

/* WebDAV does not deliver permissions. Set a default here. */
static int _stat_perms( int type ) {
    int ret = 0;

    if( type == CSYNC_VIO_FILE_TYPE_DIRECTORY ) {
        /* DEBUG_WEBDAV("Setting mode in stat (dir)"); */
        /* directory permissions */
        ret = S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR /* directory, rwx for user */
                | S_IRGRP | S_IXGRP                       /* rx for group */
                | S_IROTH | S_IXOTH;                      /* rx for others */
    } else {
        /* regualar file permissions */
        /* DEBUG_WEBDAV("Setting mode in stat (file)"); */
        ret = S_IFREG | S_IRUSR | S_IWUSR /* regular file, user read & write */
                | S_IRGRP                         /* group read perm */
                | S_IROTH;                        /* others read perm */
    }
    return ret;
}

/*
 * free the fetchCtx
 */
static void free_fetchCtx( struct listdir_context *ctx )
{
    struct resource *newres = ctx->list;
    struct resource *res = newres;
    if( ! ctx ) return;

    SAFE_FREE(ctx->target);

    while( res ) {
        SAFE_FREE(res->uri);
        SAFE_FREE(res->name);
        SAFE_FREE(res->md5);

        newres = res->next;
        SAFE_FREE(res);
        res = newres;
    }
    SAFE_FREE(ctx);
}

static void fill_stat_cache( csync_vio_file_stat_t *lfs ) {

    if( _fs.name ) SAFE_FREE(_fs.name);
    if( _fs.md5  ) SAFE_FREE(_fs.md5 );

    if( !lfs) return;

    _fs.name   = c_strdup(lfs->name);
    _fs.mtime  = lfs->mtime;
    _fs.fields = lfs->fields;
    _fs.type   = lfs->type;
    _fs.size   = lfs->size;
    if( lfs->md5 ) {
        _fs.md5    = c_strdup(lfs->md5);
    }
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
        buf->md5    = NULL;
        if( _fs.md5 ) {
            buf->md5    = c_strdup( _fs.md5 );
            buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MD5;
        }
        DEBUG_WEBDAV("stat results from fs cache - md5: %s - mtime: %llu", _fs.md5 ? _fs.md5 : "NULL",
		(unsigned long long) buf->mtime );
    } else {
      DEBUG_WEBDAV("stat results fetched.");
        /* fetch data via a propfind call. */
        fetchCtx = c_malloc( sizeof( struct listdir_context ));
        if( ! fetchCtx ) {
            errno = ENOMEM;
            csync_vio_file_stat_destroy(buf);
            return -1;
        }

        curi = _cleanPath( uri );

        fetchCtx->list = NULL;
        fetchCtx->target = curi;
        fetchCtx->include_target = 1;
        fetchCtx->currResource = NULL;

        rc = fetch_resource_list( curi, NE_DEPTH_ONE, fetchCtx );
        if( rc != NE_OK ) {
            if( rc == OC_TIMEDELTA_FAIL ) {
                DEBUG_WEBDAV("WRN: Time delta changed too much!");
                /* FIXME: Reasonable user warning */
            } else {
                errno = ne_session_error_errno( dav_session.ctx );

                DEBUG_WEBDAV("stat fails with errno %d", errno );
            }
            free_fetchCtx(fetchCtx);
            return -1;
        }

        if( fetchCtx ) {
            struct resource *res = fetchCtx->list;
            while( res ) {
                /* remove trailing slashes */
                len = strlen(res->uri);
                while( len > 0 && res->uri[len-1] == '/' ) --len;
                memset( strbuf, 0, PATH_MAX+1);
                strncpy( strbuf, res->uri, len < PATH_MAX ? len : PATH_MAX ); /* this removes the trailing slash */
                decodedUri = ne_path_unescape( curi ); /* allocates memory */

                if( c_streq(strbuf, decodedUri )) {
                    SAFE_FREE( decodedUri );
                    break;
                }
                res = res->next;
                SAFE_FREE( decodedUri );
            }
            if( res ) {
                DEBUG_WEBDAV("Working on file %s", res->name );
            } else {
                DEBUG_WEBDAV("ERROR: Result struct not valid!");
            }

            lfs = resourceToFileStat( res );
            if( lfs ) {
                buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;
                buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MD5;

                buf->fields = lfs->fields;
                buf->type   = lfs->type;
                buf->mtime  = lfs->mtime;
                buf->size   = lfs->size;
                buf->mode   = _stat_perms( lfs->type );
                buf->md5    = NULL;
                if( lfs->md5 ) {
                    buf->md5    = c_strdup( lfs->md5 );
                }

                /* put the stat information to cache for subsequent calls */
                fill_stat_cache( lfs );

                /* fill the static stat buf as input for the stat function */
                csync_vio_file_stat_destroy( lfs );
            }

            free_fetchCtx( fetchCtx );
        }
        DEBUG_WEBDAV("STAT result from propfind: %s, mtime: %llu", buf->name ? buf->name:"NULL",
                      (unsigned long long) buf->mtime );
    }

    return 0;
}

static ssize_t owncloud_write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
    struct transfer_context *writeCtx = NULL;
    size_t written = 0;
    size_t bufWritten = 0;

    if (fhandle == NULL) {
        return -1;
    }

    writeCtx = (struct transfer_context*) fhandle;

    /* check if there is space left in the mem buffer */
    if( writeCtx->bytes_written + count > PUT_BUFFER_SIZE ) {
        if( writeCtx->fileWritten == 0 ) {
            DEBUG_WEBDAV("Remaining Mem Buffer size to small, push to disk "
                          "(current buf size %lu)",
                          (unsigned long) writeCtx->bytes_written);
        }

        /* write contents to disk */
        if( writeCtx->fd > -1 ) {
            if(  writeCtx->bytes_written > 0 ) {
                /* there is something in the buffer already. Store to disk */

                written = write( writeCtx->fd, _buffer, writeCtx->bytes_written );
                if( written != writeCtx->bytes_written ) {
                    DEBUG_WEBDAV("WRN: Written bytes from buffer not equal to count");
                }
                /* reset the buffer counter */
                writeCtx->bytes_written = 0;
            }
            /* also write the incoming memory buffer content to file */
            if( count > 0 ) {
                bufWritten = write( writeCtx->fd, buf, count );

                if( bufWritten != count ) {
                    DEBUG_WEBDAV("WRN: Written bytes not equal to count");
                }
            }
            /* set a flag that file was used, needed in the close routine */
            writeCtx->fileWritten = 1;
        } else {
            /* problem: the file descriptor is not valid. */
            DEBUG_WEBDAV("ERR: Not a valid file descriptor in write");
        }
    } else {
        /* still space in the buffer */
        memcpy( _buffer + writeCtx->bytes_written, buf, count );
        writeCtx->bytes_written += count;
        bufWritten = count;
    }
    return bufWritten;
}

static int uncompress_reader(void *userdata, const char *buf, size_t len)
{
   struct transfer_context *writeCtx = userdata;
   size_t written = 0;

   if( buf && writeCtx->fd ) {
       /* DEBUG_WEBDAV("Writing NON compressed %d bytes", len); */
       written = write(writeCtx->fd, buf, len);
       if( len != written ) {
           DEBUG_WEBDAV("WRN: uncompress_reader wrote wrong num of bytes");
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
       /* DEBUG_WEBDAV("Writing compressed %d bytes", len); */
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

    if( !writeCtx ) {
        DEBUG_WEBDAV("Error: install_content_reader called without valid write context!");
        return;
    }

    enc = ne_get_response_header( req, "Content-Encoding" );
    DEBUG_WEBDAV("Content encoding ist <%s> with status %d", enc ? enc : "empty",
                  status ? status->code : -1 );

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
 *  bool atomar_copy_support - oC provides atomar copy
 *  bool do_post_copy_stat   - oC does not want the post copy check
 *  bool time_sync_required  - oC does not require the time sync
 *  int  unix_extensions     - oC supports unix extensions.
 */
static csync_vio_capabilities_t _owncloud_capabilities = { true, false, false, 1 };

static csync_vio_capabilities_t *owncloud_capabilities(void)
{
#ifdef _WIN32
  _owncloud_capabilities.unix_extensions = 0;
#endif
  return &_owncloud_capabilities;
}

static const char* owncloud_file_id( const char *path )
{
    ne_request *req    = NULL;
    const char *header = NULL;
    char *uri          = _cleanPath(path);
    char *buf          = NULL;
    const char *cbuf   = NULL;
    csync_vio_file_stat_t *fs = NULL;
    bool  doHeadRequest= false; /* ownCloud server doesn't have good support for HEAD yet */

    if( doHeadRequest ) {
        /* Perform an HEAD request to the resource. HEAD delivers the
         * ETag header back. */
        req = ne_request_create(dav_session.ctx, "HEAD", uri);
        ne_request_dispatch(req);

        header = ne_get_response_header(req, "etag");
    }
    /* If the request went wrong or the server did not respond correctly
     * (that can happen for collections) a stat call is done which translates
     * into a PROPFIND request.
     */
    if( ! header ) {
        /* Clear the cache */
        fill_stat_cache(NULL);

        /* ... and do a stat call. */
        fs = csync_vio_file_stat_new();
        if(fs == NULL) {
            DEBUG_WEBDAV( "owncloud_file_id: memory fault.");
            errno = ENOMEM;
            return NULL;
        }
        if( owncloud_stat( path, fs ) == 0 ) {
            header = fs->md5;
        }
    }

    /* In case the result is surrounded by "" cut them away. */
    if( header ) {
        if( header [0] == '"' && header[ strlen(header)-1] == '"') {
            int len = strlen( header )-2;
            buf = c_malloc( len+1 );
            strncpy( buf, header+1, len );
            buf[len] = '\0';
            cbuf = buf;
            /* do not free header here, as it belongs to the request */
        } else {
            cbuf = c_strdup(header);
        }
    }
    DEBUG_WEBDAV("Get file ID for %s: %s", path, cbuf ? cbuf:"<null>");
    if( fs ) csync_vio_file_stat_destroy(fs);
    if( req ) ne_request_destroy(req);
    SAFE_FREE(uri);

    return cbuf;
}

static csync_vio_method_handle_t *owncloud_open(const char *durl,
                                                int flags,
                                                mode_t mode) {
    char *uri = NULL;
    char *dir = NULL;
    const  char *err = NULL;
    char getUrl[PATH_MAX];
    int put = 0;
    int rc = NE_OK;
#ifdef _WIN32
    int gtp = 0;
    char tmpname[13];
    _TCHAR winTmp[PATH_MAX];
    const _TCHAR *winUrlMB = NULL;
    const char *winTmpUtf8 = NULL;
    csync_stat_t sb;
#endif

    struct transfer_context *writeCtx = NULL;
    csync_vio_file_stat_t statBuf;
    memset( getUrl, '\0', PATH_MAX );
    ZERO_STRUCT(statBuf);

    (void) mode; /* unused on webdav server */
    DEBUG_WEBDAV( "=> open called for %s", durl );

    uri = _cleanPath( durl );
    if( ! uri ) {
        DEBUG_WEBDAV("Failed to clean path for %s", durl );
        errno = EACCES;
        rc = NE_ERROR;
    }

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
            SAFE_FREE(uri);
	    return NULL;
	}
        DEBUG_WEBDAV("Stating directory %s", dir );
        if( c_streq( dir, _lastDir )) {
            DEBUG_WEBDAV("Dir %s is there, we know it already.", dir);
        } else {
            if( owncloud_stat( dir, &statBuf ) == 0 ) {
                SAFE_FREE(statBuf.name);
                SAFE_FREE(statBuf.md5);
                DEBUG_WEBDAV("Directory of file to open exists.");
                SAFE_FREE( _lastDir );
                _lastDir = c_strdup(dir);

            } else {
                DEBUG_WEBDAV("Directory %s of file to open does NOT exist.", dir );
                /* the directory does not exist. That is an ENOENT */
                errno = ENOENT;
                SAFE_FREE(dir);
                SAFE_FREE(uri);
                SAFE_FREE(statBuf.name);
                return NULL;
            }
        }
    }

    writeCtx = c_malloc( sizeof(struct transfer_context) );
    writeCtx->bytes_written = 0;
    writeCtx->clean_uri = c_strdup(uri);

    if( rc == NE_OK ) {
        /* open a temp file to store the incoming data */
#ifdef _WIN32
        memset( tmpname, '\0', 13 );
        gtp = GetTempPathW( PATH_MAX, winTmp );
        winTmpUtf8 = c_utf8( winTmp );
        strcpy( getUrl, winTmpUtf8 );
        DEBUG_WEBDAV("win32 tmp path: %s", getUrl);

        if ( gtp > MAX_PATH || (gtp == 0) ) {
            DEBUG_WEBDAV("Failed to compute Win32 tmp path, trying /tmp");
            strcpy( getUrl, "/tmp/");
        }
        strcpy( tmpname, "csync.XXXXXX" );
        if( c_tmpname( tmpname ) == 0 ) {
            /* Set the windows file mode to Binary. */
            _fmode = _O_BINARY;
            /* append the tmp file name to tmp path */
            strcat( getUrl, tmpname );
            writeCtx->tmpFileName = c_strdup( getUrl );

            /* Open the file finally. */
            winUrlMB = c_multibyte( getUrl );

            /* check if the file exists by chance. */
            if( _tstat( winUrlMB, &sb ) == 0 ) {
                /* the file exists. Remove it! */
                _tunlink( winUrlMB );
            }

            writeCtx->fd = _topen( winUrlMB, O_RDWR | O_CREAT | O_EXCL, 0600 );

            /* free the extra bytes */
            c_free_multibyte( winUrlMB );
            c_free_utf8( winTmpUtf8 );
	} else {
	   writeCtx->fd = -1;
	}
#else
        writeCtx->tmpFileName = c_strdup( "/tmp/csync.XXXXXX" );
        writeCtx->fd = mkstemp( writeCtx->tmpFileName );
#endif
        DEBUG_WEBDAV("opening temp directory %s: %d", writeCtx->tmpFileName, writeCtx->fd );
        if( writeCtx->fd == -1 ) {
	    DEBUG_WEBDAV("Failed to open temp file, errno = %d", errno );
            rc = NE_ERROR;
            /* errno is set by the mkstemp call above. */
        }
    }

    if( rc == NE_OK && put) {
        DEBUG_WEBDAV("PUT request on %s!", uri);
        /* reset the write buffer */
        writeCtx->bytes_written = 0;
        writeCtx->fileWritten = 0;   /* flag to indicate if contents was pushed to file */

        writeCtx->req = ne_request_create(dav_session.ctx, "PUT", uri);
        writeCtx->method = "PUT";
    }


    if( rc == NE_OK && ! put ) {
        writeCtx->req = 0;
        writeCtx->method = "GET";

        /* Download the data into a local temp file. */
        /* the download via the get function requires a full uri */
        snprintf( getUrl, PATH_MAX, "%s://%s%s", ne_get_scheme( dav_session.ctx),
                  ne_get_server_hostport( dav_session.ctx ), uri );
        DEBUG_WEBDAV("GET request on %s", getUrl );

#define WITH_HTTP_COMPRESSION
#ifdef WITH_HTTP_COMPRESSION
        writeCtx->req = ne_request_create( dav_session.ctx, "GET", getUrl );

        /* Allow compressed content by setting the header */
        ne_add_request_header( writeCtx->req, "Accept-Encoding", "gzip,deflate" );

        /* hook called before the content is parsed to set the correct reader,
         * either the compressed- or uncompressed reader.
         */
        ne_hook_post_headers( dav_session.ctx, install_content_reader, writeCtx );

        /* actually do the request */
        rc = ne_request_dispatch(writeCtx->req );
        /* possible return codes are:
         *  NE_OK, NE_AUTH, NE_CONNECT, NE_TIMEOUT, NE_ERROR (from ne_request.h)
         */

        if( rc != NE_OK || (rc == NE_OK && ne_get_status(writeCtx->req)->klass != 2) ) {
            DEBUG_WEBDAV("request_dispatch failed with rc=%d", rc );
	    err = ne_get_error( dav_session.ctx );
	    DEBUG_WEBDAV("request error: %s", err ? err : "<nil>");
            if( rc == NE_OK ) rc = NE_ERROR;
            errno = EACCES;
        }

        /* delete the hook again, otherwise they get chained as they are with the session */
        ne_unhook_post_headers( dav_session.ctx, install_content_reader, writeCtx );

        /* if the compression handle is set through the post_header hook, delete it. */
        if( writeCtx->decompress ) {
            ne_decompress_destroy( writeCtx->decompress );
        }

        /* delete the request in any case */
        ne_request_destroy(writeCtx->req);
#else
        DEBUG_WEBDAV("GET Compression not supported!");
        rc = ne_get( dav_session.ctx, getUrl, writeCtx->fd );  /* FIX_ESCAPE? */
#endif
        if( rc != NE_OK ) {
            DEBUG_WEBDAV("Download to local file failed: %d.", rc);
            errno = EACCES;
        }
        if( close( writeCtx->fd ) == -1 ) {
            DEBUG_WEBDAV("Close of local download file failed.");
            writeCtx->fd = -1;
            rc = NE_ERROR;
            errno = EACCES;
        }

        writeCtx->fd = -1;
    }

    if( rc != NE_OK ) {
        SAFE_FREE( writeCtx );
    }

    SAFE_FREE( uri );
    SAFE_FREE( dir );

    return (csync_vio_method_handle_t *) writeCtx;
}

static csync_vio_method_handle_t *owncloud_creat(const char *durl, mode_t mode) {

    csync_vio_method_handle_t *handle = owncloud_open(durl, O_CREAT|O_WRONLY|O_TRUNC, mode);

    /* on create, the file needs to be created empty */
    owncloud_write( handle, NULL, 0 );

    return handle;
}

static int owncloud_close(csync_vio_method_handle_t *fhandle) {
    struct transfer_context *writeCtx;
    csync_stat_t st;
    int rc;
    int ret = 0;
    size_t len = 0;
    const _TCHAR *tmpFileName = NULL;

    writeCtx = (struct transfer_context*) fhandle;

    if (fhandle == NULL) {
        errno = EBADF;
        ret = -1;
    }

    tmpFileName = c_multibyte( writeCtx->tmpFileName );

    /* handle the PUT request, means write to the WebDAV server */
    if( ret != -1 && strcmp( writeCtx->method, "PUT" ) == 0 ) {

        /* if there is a valid file descriptor, close it, reopen in read mode and start the PUT request */
        if( writeCtx->fd > -1 ) {
            if( writeCtx->fileWritten && writeCtx->bytes_written > 0 ) { /* was content written to file? */
                /* push the rest of the buffer to file as well. */
                DEBUG_WEBDAV("Write remaining %lu bytes to disk.",
                              (unsigned long) writeCtx->bytes_written );
                len = write( writeCtx->fd, _buffer, writeCtx->bytes_written );
                if( len != writeCtx->bytes_written ) {
                    DEBUG_WEBDAV("WRN: write wrote wrong number of remaining bytes");
                }
                writeCtx->bytes_written = 0;
            }

            if( close( writeCtx->fd ) < 0 ) {
                DEBUG_WEBDAV("Could not close file %s", writeCtx->tmpFileName );
                errno = EBADF;
                ret = -1;
            }

            /* and open it again to read from */
#ifdef _WIN32
	    _fmode = _O_BINARY;
#endif
            if( writeCtx->fileWritten ) {
                DEBUG_WEBDAV("Putting file through file cache.");
                /* we need to go the slow way and close and open the file and read from fd. */

                if (( writeCtx->fd = _topen( tmpFileName, O_RDONLY )) < 0) {
                    errno = EIO;
                    ret = -1;
                } else {
                    if (fstat( writeCtx->fd, &st ) < 0) {
                        DEBUG_WEBDAV("Could not stat file %s", writeCtx->tmpFileName );
                        errno = EIO;
                        ret = -1;
                    }

                    /* successfully opened for read. Now start the request via ne_put */
                    ne_set_request_body_fd( writeCtx->req, writeCtx->fd, 0, st.st_size );
                    rc = ne_request_dispatch( writeCtx->req );
                    if( close( writeCtx->fd ) == -1 ) {
                        errno = EBADF;
                        ret = -1;
                    }
                    if (rc == NE_OK) {
                        if ( ne_get_status( writeCtx->req )->klass != 2 ) {
                            // DEBUG_WEBDAV("Error - PUT status value no 2xx");
                            // errno = EIO;
                            // ret = -1;
                        }
                    } else {
                        DEBUG_WEBDAV("Error - put request on close failed: %d!", rc );
                        errno = EIO;
                        ret = -1;
                    }
                }
            } else {
                /* all content is in the buffer. */
                DEBUG_WEBDAV("Putting file through memory cache.");
                ne_set_request_body_buffer( writeCtx->req, _buffer, writeCtx->bytes_written );
                rc = ne_request_dispatch( writeCtx->req );
                if( rc == NE_OK ) {
                    if ( ne_get_status( writeCtx->req )->klass != 2 ) {
                        // DEBUG_WEBDAV("Error - PUT status value no 2xx");
                        // errno = EIO;
                        // ret = -1;
                    }
                } else {
                    DEBUG_WEBDAV("Error - put request from memory failed: %d!", rc );
                    errno = EIO;
                    ret = -1;
                }
            }
        }

        ne_request_destroy( writeCtx->req );
    } else  {
        /* Its a GET request, not much to do in close. */
        if( writeCtx->fd > -1) {
            if( close( writeCtx->fd ) == -1 ) {
                errno = EBADF;
                ret = -1;
            }
        }
    }
     /* Remove the local file. */
    if(_tunlink(tmpFileName) != 0) {
        DEBUG_WEBDAV("WRN: Removing of tmp file %s failed with errno %d!",
                     writeCtx->tmpFileName ? writeCtx->tmpFileName : "<empty>", errno );
    }
    /* DEBUG_WEBDAV("Removing tmp file %s: %d", writeCtx->tmpFileName, rc); */

    c_free_multibyte(tmpFileName);

    /* free mem. Note that the request mem is freed by the ne_request_destroy call */
    SAFE_FREE( writeCtx->tmpFileName );
    SAFE_FREE( writeCtx->clean_uri );
    SAFE_FREE( writeCtx );

    return ret;
}

static ssize_t owncloud_read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
    struct transfer_context *writeCtx = NULL;
    size_t len = 0;
    csync_stat_t st;
    const _TCHAR *tmpFileName;

    writeCtx = (struct transfer_context*) fhandle;

    /* DEBUG_WEBDAV( "read called on %s (fd=%d)!", writeCtx->tmpFileName, writeCtx->fd ); */
    if( ! fhandle ) {
        errno = EBADF;
        return -1;
    }

    if( writeCtx->fd == -1 ) {
        /* open the downloaded file to read from */
#ifdef _WIN32
	_fmode = _O_BINARY;
#endif
        tmpFileName = c_multibyte(writeCtx->tmpFileName);
        if (( writeCtx->fd = _topen( tmpFileName, O_RDONLY )) < 0) {
            c_free_multibyte(tmpFileName);
            DEBUG_WEBDAV("Could not open local file %s", writeCtx->tmpFileName );
            errno = EIO;
            return -1;
        } else {
            c_free_multibyte(tmpFileName);
            if (fstat( writeCtx->fd, &st ) < 0) {
                DEBUG_WEBDAV("Could not stat file %s", writeCtx->tmpFileName );
                errno = EIO;
                return -1;
            }

            DEBUG_WEBDAV("local downlaod file size=%d", (int) st.st_size );
        }
    }

    if( writeCtx->fd ) {
        len = read( writeCtx->fd, buf, count );
        writeCtx->bytes_written = writeCtx->bytes_written + len;
    }

    /* DEBUG_WEBDAV( "read len: %d %ul", len, count ); */

    return len;
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
    char *redir_uri = NULL;
    const ne_uri *redir_ne_uri = NULL;

    DEBUG_WEBDAV("opendir method called on %s", uri );

    dav_connect( uri );

    fetchCtx = c_malloc( sizeof( struct listdir_context ));

    fetchCtx->list = reslist;
    fetchCtx->target = curi;
    fetchCtx->include_target = 0;
    fetchCtx->currResource = NULL;

    rc = fetch_resource_list( curi, NE_DEPTH_ONE, fetchCtx );
    if( rc != NE_OK ) {
        if( rc == NE_CONNECT || rc == NE_LOOKUP ) {
            errno = EIO;
        } else {
            errno = ne_session_error_errno( dav_session.ctx );
            DEBUG_WEBDAV("Errno set to %d", errno);
            redir_ne_uri = ne_redirect_location(dav_session.ctx);
            if( redir_ne_uri ) {
                redir_uri = ne_uri_unparse(redir_ne_uri);
                DEBUG_WEBDAV("Permanently moved to %s", redir_uri);
            }
        }
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

    DEBUG_WEBDAV("closedir method called %p!", dhandle);

    free_fetchCtx(fetchCtx);

    return 0;
}

static csync_vio_file_stat_t *owncloud_readdir(csync_vio_method_handle_t *dhandle) {

    struct listdir_context *fetchCtx = dhandle;
    csync_vio_file_stat_t *lfs = NULL;

    if( fetchCtx->currResource ) {
        // DEBUG_WEBDAV("readdir method called for %s", fetchCtx->currResource->uri);
    } else {
        /* DEBUG_WEBDAV("An empty dir or at end"); */
        return NULL;
    }

    if( fetchCtx && fetchCtx->currResource ) {
        /* FIXME: Who frees the allocated mem for lfs, allocated in the helper func? */
        lfs = resourceToFileStat( fetchCtx->currResource );

        /* set pointer to next element */
        fetchCtx->currResource = fetchCtx->currResource->next;

        /* fill the static stat buf as input for the stat function */
        fill_stat_cache(lfs);
    }

    /* DEBUG_WEBDAV("LFS fields: %s: %d", lfs->name, lfs->type ); */
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
        len = strlen( path );
        if( len > PATH_MAX-1 ) {
            DEBUG_WEBDAV("ERR: Path is too long for OS max path length!");
        } else {
            strcpy( buf, path );
            if( buf[len-1] != '/' ) {
                strcat(buf, "/");
            }

            DEBUG_WEBDAV("MKdir on %s", buf );
            rc = ne_mkcol(dav_session.ctx, buf );
            if (rc != NE_OK ) {
                errno = ne_session_error_errno( dav_session.ctx );
            }
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
            errno = ne_session_error_errno( dav_session.ctx );
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
            errno = ne_session_error_errno(dav_session.ctx);
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
            errno = ne_session_error_errno( dav_session.ctx );
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

static int owncloud_utimes(const char *uri, const struct timeval *times) {

    ne_proppatch_operation ops[2];
    ne_propname pname;
    int rc = NE_OK;
    char val[255];
    char *curi = NULL;
    const struct timeval *modtime = times+1;
    long newmodtime;

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

    newmodtime = modtime->tv_sec;

    DEBUG_WEBDAV("Add a time delta to modtime %lu: %llu",
                 modtime->tv_sec, (unsigned long long) dav_session.time_delta);
    newmodtime += dav_session.time_delta;

    snprintf( val, sizeof(val), "%ld", newmodtime );
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
    .get_capabilities = owncloud_capabilities,
    .get_file_id = owncloud_file_id,
    .open = owncloud_open,
    .creat = owncloud_creat,
    .close = owncloud_close,
    .read = owncloud_read,
    .write = owncloud_write,
    .lseek = owncloud_lseek,
    .opendir = owncloud_opendir,
    .closedir = owncloud_closedir,
    .readdir = owncloud_readdir,
    .mkdir = owncloud_mkdir,
    .rmdir = owncloud_rmdir,
    .stat = owncloud_stat,
    .rename = owncloud_rename,
    .unlink = owncloud_unlink,
    .chmod = owncloud_chmod,
    .chown = owncloud_chown,
    .utimes = owncloud_utimes
};

csync_vio_method_t *vio_module_init(const char *method_name, const char *args,
                                    csync_auth_callback cb, void *userdata) {
    char **userdata_ptr = NULL;

    (void) method_name;
    (void) args;

    _authcb = cb;
    _connected = 0;  /* triggers dav_connect to go through the whole neon setup */

    if( userdata ) {
        userdata_ptr = userdata;
        if( *userdata_ptr && strlen( *userdata_ptr) )
            dav_session.proxy_type = c_strdup( *userdata_ptr );
        userdata_ptr++;
        DEBUG_WEBDAV("CSync Proxy Type: %s", dav_session.proxy_type);
        if( *userdata_ptr && strlen( *userdata_ptr) )
            dav_session.proxy_host = c_strdup( *userdata_ptr );
        userdata_ptr++;

        if( *userdata_ptr && strlen( *userdata_ptr) )
            dav_session.proxy_port = atoi( *userdata_ptr );
        userdata_ptr++;

        if( *userdata_ptr && strlen( *userdata_ptr) )
            dav_session.proxy_user = c_strdup( *userdata_ptr );
        userdata_ptr++;

        if( *userdata_ptr && strlen( *userdata_ptr) )
            dav_session.proxy_pwd = c_strdup( *userdata_ptr );
    }

    return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
    (void) method;

    SAFE_FREE( dav_session.user );
    SAFE_FREE( dav_session.pwd );

    SAFE_FREE( dav_session.proxy_type );
    SAFE_FREE( dav_session.proxy_host );
    SAFE_FREE( dav_session.proxy_user );
    SAFE_FREE( dav_session.proxy_pwd  );

    /* free stat memory */
    fill_stat_cache(NULL);

    if( dav_session.ctx )
        ne_session_destroy( dav_session.ctx );
    /* DEBUG_WEBDAV( "********** vio_module_shutdown" ); */

}

/* vim: set ts=4 sw=4 et cindent: */
