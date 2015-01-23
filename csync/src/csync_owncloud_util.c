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
