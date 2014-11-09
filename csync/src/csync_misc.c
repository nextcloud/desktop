/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>
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

#include "config_csync.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#if _WIN32
# ifndef _WIN32_IE
#  define _WIN32_IE 0x0501 // SHGetSpecialFolderPath
# endif
# include <shlobj.h>
#else /* _WIN32 */
# include <pwd.h>
# include <unistd.h>
#endif /* _WIN32 */

#include "c_lib.h"
#include "csync_misc.h"
#include "csync_macros.h"
#include "csync_log.h"

#ifdef HAVE_FNMATCH
#include <fnmatch.h>

int csync_fnmatch(__const char *__pattern, __const char *__name, int __flags) {
    return fnmatch(__pattern, __name, __flags);
}

#else /* HAVE_FNMATCH */

#include <shlwapi.h>
int csync_fnmatch(__const char *__pattern, __const char *__name, int __flags) {
    wchar_t *pat = NULL;
    wchar_t *name = NULL;
    BOOL match;

    (void) __flags;

    name = c_utf8_to_locale(__name);
    pat = c_utf8_to_locale(__pattern);

    match = PathMatchSpec(name, pat);

    c_free_locale_string(pat);
    c_free_locale_string(name);
    if(match)
        return 0;
    else
        return 1;
}
#endif /* HAVE_FNMATCH */

CSYNC_STATUS csync_errno_to_status(int error, CSYNC_STATUS default_status)
{
  CSYNC_STATUS status = CSYNC_STATUS_OK;

  switch (error) {
  case 0:
    status = CSYNC_STATUS_OK;
    break;
    /* The custom errnos first. */
  case ERRNO_GENERAL_ERROR:
    status = CSYNC_STATUS_UNSUCCESSFUL;
    break;
  case ERRNO_LOOKUP_ERROR: /* In Neon: Server or proxy hostname lookup failed */
    status = CSYNC_STATUS_LOOKUP_ERROR;
    break;
  case ERRNO_USER_UNKNOWN_ON_SERVER: /* Neon: User authentication on server failed. */
    status = CSYNC_STATUS_SERVER_AUTH_ERROR;
    break;
  case ERRNO_PROXY_AUTH:
    status = CSYNC_STATUS_PROXY_AUTH_ERROR; /* Neon: User authentication on proxy failed */
    break;
  case ERRNO_CONNECT:
    status = CSYNC_STATUS_CONNECT_ERROR; /* Network: Connection error */
    break;
  case ERRNO_TIMEOUT:
    status = CSYNC_STATUS_TIMEOUT; /* Network: Timeout error */
    break;
  case ERRNO_QUOTA_EXCEEDED:
    status = CSYNC_STATUS_QUOTA_EXCEEDED;   /* Quota exceeded */
    break;
  case ERRNO_SERVICE_UNAVAILABLE:
    status = CSYNC_STATUS_SERVICE_UNAVAILABLE;  /* Service temporarily down */
    break;
  case EFBIG:
    status = CSYNC_STATUS_FILE_SIZE_ERROR;          /* File larger than 2MB */
    break;
  case ERRNO_PRECONDITION:
  case ERRNO_RETRY:
  case ERRNO_REDIRECT:
  case ERRNO_WRONG_CONTENT:
    status = CSYNC_STATUS_HTTP_ERROR;
    break;

  case ERRNO_TIMEDELTA:
    status = CSYNC_STATUS_TIMESKEW;
    break;
  case EPERM:                  /* Operation not permitted */
  case EACCES:                /* Permission denied */
    status = CSYNC_STATUS_PERMISSION_DENIED;
    break;
  case ENOENT:                 /* No such file or directory */
    status = CSYNC_STATUS_NOT_FOUND;
    break;
  case EAGAIN:                /* Try again */
    status = CSYNC_STATUS_TIMEOUT;
    break;
  case EEXIST:                /* File exists */
    status = CSYNC_STATUS_FILE_EXISTS;
    break;
  case EINVAL:
    status = CSYNC_STATUS_PARAM_ERROR;
    break;
  case ENOSPC:
    status = CSYNC_STATUS_OUT_OF_SPACE;
    break;

    /* All the remaining basic errnos: */
  case EIO:                    /* I/O error */
  case ESRCH:                  /* No such process */
  case EINTR:                  /* Interrupted system call */
  case ENXIO:                  /* No such device or address */
  case E2BIG:                  /* Argument list too long */
  case ENOEXEC:                /* Exec format error */
  case EBADF:                  /* Bad file number */
  case ECHILD:                /* No child processes */
  case ENOMEM:                /* Out of memory */
  case EFAULT:                /* Bad address */
#ifndef _WIN32
  case ENOTBLK:               /* Block device required */
#endif
  case EBUSY:                 /* Device or resource busy */
  case EXDEV:                 /* Cross-device link */
  case ENODEV:                /* No such device */
  case ENOTDIR:               /* Not a directory */
  case EISDIR:                /* Is a directory */
  case ENFILE:                /* File table overflow */
  case EMFILE:                /* Too many open files */
  case ENOTTY:                /* Not a typewriter */
#ifndef _WIN32
  case ETXTBSY:               /* Text file busy */
#endif
  case ESPIPE:                /* Illegal seek */
  case EROFS:                 /* Read-only file system */
  case EMLINK:                /* Too many links */
  case EPIPE:                 /* Broken pipe */

  case ERRNO_ERROR_STRING:
  default:
    status = default_status;
  }

  return status;
}

/* Remove possible quotes, and also the -gzip at the end
 * Remove "-gzip" at the end (cf. https://github.comowncloud/client/issues/1195)
 * The caller must take ownership of the resulting string.
 */
char *csync_normalize_etag(const char *etag)
{
    int len = 0;
    char *buf = NULL;
    if (!etag)
        return NULL;

    len = strlen(etag);
    /* strip "XXXX-gzip" */
    if(len >= 7 && etag[0] == '"' && c_streq(etag + len - 6, "-gzip\"")) {
        etag++;
        len -= 7;
    }
    /* strip leading -gzip */
    if(len >= 5 && c_streq(etag + len - 5, "-gzip")) {
        len -= 5;
    }
    /* strip normal quotes */
    if (etag[0] == '"' && etag[len-1] == '"') {
        etag++;
        len -= 2;
    }

    buf = c_malloc( len+1 );
    strncpy( buf, etag, len );
    buf[len] = '\0';
    return buf;
}

