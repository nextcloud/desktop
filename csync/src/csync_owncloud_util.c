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

#include "csync_owncloud.h"
#include "csync_owncloud_private.h"

#include "csync_misc.h"

void set_error_message( csync_owncloud_ctx_t *ctx, const char *msg )
{
    SAFE_FREE(ctx->dav_session.error_string);
    if( msg )
        ctx->dav_session.error_string = c_strdup(msg);
}

void set_errno_from_http_errcode( int err ) {
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
        break;
    case 401:           /* Unauthorized */
    case 402:           /* Payment Required */
    case 407:           /* Proxy Authentication Required */
    case 405:
        new_errno = EPERM;
        break;
    case 301:           /* Moved Permanently */
    case 303:           /* See Other */
    case 404:           /* Not Found */
    case 410:           /* Gone */
        new_errno = ENOENT;
        break;
    case 408:           /* Request Timeout */
    case 504:           /* Gateway Timeout */
        new_errno = EAGAIN;
        break;
    case 423:           /* Locked */
        new_errno = EACCES;
        break;
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
        break;
    case 507:           /* Insufficient Storage */
        new_errno = ENOSPC;
        break;
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
    case 505:           /* HTTP Version Not Supported */
        new_errno = EIO;
        break;
    case 503:           /* Service Unavailable */
        new_errno = ERRNO_SERVICE_UNAVAILABLE;
        break;
    case 413:           /* Request Entity too Large */
        new_errno = EFBIG;
        break;
    default:
        new_errno = EIO;
    }

    errno = new_errno;
}

int http_result_code_from_session(csync_owncloud_ctx_t *ctx) {
    const char *p = ne_get_error( ctx->dav_session.ctx );
    char *q;
    int err;

    set_error_message(ctx, p); /* remember the error message */

    err = strtol(p, &q, 10);
    if (p == q) {
        err = ERRNO_ERROR_STRING;
    }
    return err;
}

void set_errno_from_session(csync_owncloud_ctx_t *ctx) {
    int err = http_result_code_from_session(ctx);

    if( err == EIO || err == ERRNO_ERROR_STRING) {
        errno = err;
    } else {
        set_errno_from_http_errcode(err);
    }
}

void set_errno_from_neon_errcode(csync_owncloud_ctx_t *ctx, int neon_code ) {

    if( neon_code != NE_OK ) {
        DEBUG_WEBDAV("Neon error code was %d", neon_code);
    }

    switch(neon_code) {
    case NE_OK:     /* Success, but still the possiblity of problems */
    case NE_ERROR:  /* Generic error; use ne_get_error(session) for message */
        set_errno_from_session(ctx); /* Something wrong with http communication */
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
char *_cleanPath( const char* uri ) {
    int rc = 0;
    char *path = NULL;
    char *re = NULL;

    rc = c_parse_uri( uri, NULL, NULL, NULL, NULL, NULL, &path );
    if( rc  < 0 ) {
        DEBUG_WEBDAV("Unable to cleanPath %s", uri ? uri: "<zero>" );
        re = NULL;
    } else {
	if(path) {
	    re = ne_path_escape( path );
	}
    }

    SAFE_FREE( path );
    return re;
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
time_t oc_httpdate_parse( const char *date ) {
    struct tm gmt;
    char wkday[4], mon[4];
    int n;
    time_t result = 0;

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
 * helper: convert a resource struct to file_stat struct.
 */
void resourceToFileStat(csync_vio_file_stat_t *lfs, struct resource *res )
{
    ZERO_STRUCTP(lfs);

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
    if( res->md5 ) {
        lfs->etag   = c_strdup(res->md5);
        lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ETAG;
    }

    csync_vio_file_stat_set_file_id(lfs, res->file_id);

    if (res->directDownloadUrl) {
        lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADURL;
        lfs->directDownloadUrl = c_strdup(res->directDownloadUrl);
    }
    if (res->directDownloadCookies) {
        lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADCOOKIES;
        lfs->directDownloadCookies = c_strdup(res->directDownloadCookies);
    }
    if (strlen(res->remotePerm) > 0) {
        lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERM;
        strncpy(lfs->remotePerm, res->remotePerm, sizeof(lfs->remotePerm));
    }
}

void fill_webdav_properties_into_resource(struct resource* newres, const ne_prop_result_set *set)
{
    const char *clength, *modtime, *file_id = NULL;
    const char *directDownloadUrl = NULL;
    const char *directDownloadCookies = NULL;
    const char *resourcetype = NULL;
    const char *etag = NULL;
    const char *perm = NULL;

    modtime      = ne_propset_value( set, &ls_props[0] );
    clength      = ne_propset_value( set, &ls_props[1] );
    resourcetype = ne_propset_value( set, &ls_props[2] );
    etag       = ne_propset_value( set, &ls_props[3] );
    file_id      = ne_propset_value( set, &ls_props[4] );
    directDownloadUrl = ne_propset_value( set, &ls_props[5] );
    directDownloadCookies = ne_propset_value( set, &ls_props[6] );

    // permission flags: Defined in https://github.com/owncloud/core/issues/8322
    perm = ne_propset_value( set, &ls_props[7] );

    if( resourcetype && strncmp( resourcetype, "<DAV:collection>", 16 ) == 0) {
        newres->type = resr_collection;
    } else {
        newres->type = resr_normal;
    }

    if (modtime) {
        newres->modtime = oc_httpdate_parse(modtime);
    }

    /* DEBUG_WEBDAV("Parsing Modtime: %s -> %llu", modtime, (unsigned long long) newres->modtime ); */
    newres->size = 0;
    if (clength) {
        newres->size = atoll(clength);
        /* DEBUG_WEBDAV("Parsed File size for %s from %s: %lld", newres->name, clength, (long long)newres->size ); */
    }

    if( etag ) {
        newres->md5 = csync_normalize_etag(etag);
    }

    csync_vio_set_file_id(newres->file_id, file_id);
    /*
    DEBUG_WEBDAV("propfind_results_recursive %s [%s] %s", newres->uri, newres->type == resr_collection ? "collection" : "file", newres->md5);
    */

    if (directDownloadUrl) {
        newres->directDownloadUrl = c_strdup(directDownloadUrl);
    }
    if (directDownloadCookies) {
        newres->directDownloadCookies = c_strdup(directDownloadCookies);
    }
    /* DEBUG_WEBDAV("fill_webdav_properties_into_resource %s >%p< ", newres->name, perm ); */
    if (perm && !perm[0]) {
        // special meaning for our code: server returned permissions but are empty
        // meaning only reading is allowed for this resource
        newres->remotePerm[0] = ' ';
        // see _csync_detect_update()
    } else if (perm && strlen(perm) < sizeof(newres->remotePerm)) {
        strncpy(newres->remotePerm, perm, sizeof(newres->remotePerm));
    } else {
        // old server, keep newres->remotePerm empty
    }
}

struct resource* resource_dup(struct resource* o) {
    struct resource *r = c_malloc (sizeof( struct resource ));
    ZERO_STRUCTP(r);

    r->uri = c_strdup(o->uri);
    r->name = c_strdup(o->name);
    r->type = o->type;
    r->size = o->size;
    r->modtime = o->modtime;
    if( o->md5 ) {
        r->md5 = c_strdup(o->md5);
    }
    if (o->directDownloadUrl) {
        r->directDownloadUrl = c_strdup(o->directDownloadUrl);
    }
    if (o->directDownloadCookies) {
        r->directDownloadCookies = c_strdup(o->directDownloadCookies);
    }
    if (o->remotePerm) {
        strncpy(r->remotePerm, o->remotePerm, sizeof(r->remotePerm));
    }
    r->next = o->next;
    csync_vio_set_file_id(r->file_id, o->file_id);

    return r;
}
void resource_free(struct resource* o) {
    struct resource* old = NULL;
    while (o)
    {
        old = o;
        o = o->next;
        SAFE_FREE(old->uri);
        SAFE_FREE(old->name);
        SAFE_FREE(old->md5);
        SAFE_FREE(old->directDownloadUrl);
        SAFE_FREE(old->directDownloadCookies);
        SAFE_FREE(old);
    }
}

void free_fetchCtx( struct listdir_context *ctx )
{
    struct resource *newres, *res;
    if( ! ctx ) return;
    newres = ctx->list;
    res = newres;

    ctx->ref--;
    if (ctx->ref > 0) return;

    SAFE_FREE(ctx->target);

    while( res ) {
        SAFE_FREE(res->uri);
        SAFE_FREE(res->name);
        SAFE_FREE(res->md5);
        memset( res->file_id, 0, FILE_ID_BUF_SIZE+1 );
        SAFE_FREE(res->directDownloadUrl);
        SAFE_FREE(res->directDownloadCookies);

        newres = res->next;
        SAFE_FREE(res);
        res = newres;
    }
    SAFE_FREE(ctx);
}


// as per http://sourceforge.net/p/predef/wiki/OperatingSystems/
// extend as required
const char* csync_owncloud_get_platform() {
#if defined (_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "Macintosh";
#elif defined(__gnu_linux__)
    return "Linux";
#elif defined(__DragonFly__)
    /* might also define __FreeBSD__ */
    return "DragonFlyBSD";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__NetBSD__)
    return "NetBSD";
#elif defined(__OpenBSD__)
    return "OpenBSD";
#elif defined(sun) || defined(__sun)
    return "Solaris";
#else
    return "Unknown OS";
#endif
}
