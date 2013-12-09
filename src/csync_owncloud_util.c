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
#include "csync_misc.h"

void set_error_message( const char *msg )
{
    SAFE_FREE(dav_session.error_string);
    if( msg )
        dav_session.error_string = c_strdup(msg);
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

int http_result_code_from_session() {
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

void set_errno_from_session() {
    int err = http_result_code_from_session();

    if( err == EIO || err == ERRNO_ERROR_STRING) {
        errno = err;
    } else {
        set_errno_from_http_errcode(err);
    }
}

void set_errno_from_neon_errcode( int neon_code ) {

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
csync_vio_file_stat_t *resourceToFileStat( struct resource *res )
{
    csync_vio_file_stat_t *lfs = NULL;

    if( ! res ) {
        return NULL;
    }

    lfs = c_malloc(sizeof(csync_vio_file_stat_t));
    if (lfs == NULL) {
        errno = ENOMEM;
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
    if( res->md5 ) {
        lfs->etag   = c_strdup(res->md5);
    }
    lfs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ETAG;
    csync_vio_file_stat_set_file_id(lfs, res->file_id);

    return lfs;
}

/* WebDAV does not deliver permissions. Set a default here. */
int _stat_perms( int type ) {
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

void oc_notify_progress(const char *file, enum csync_notify_type_e kind, int64_t current_size, int64_t full_size)
{
  csync_progress_callback progress_cb = csync_get_progress_callback(dav_session.csync_ctx);

  csync_overall_progress_t overall_progress;
  ZERO_STRUCT(overall_progress);

  if( dav_session.overall_progress_data) {
    overall_progress = *dav_session.overall_progress_data;
  }

  if (progress_cb) {
    CSYNC_PROGRESS progress;
    progress.kind = kind;
    progress.path = file;
    progress.curr_bytes = current_size;
    progress.file_size  = full_size;
    progress.overall_transmission_size = overall_progress.byte_sum;
    progress.current_overall_bytes     = overall_progress.byte_current+current_size;
    progress.overall_file_count        = overall_progress.file_count;
    progress.current_file_no           = overall_progress.current_file_no;

    progress_cb(&progress, csync_get_userdata(dav_session.csync_ctx));
  }
}
