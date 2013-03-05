/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2012      by Andreas Schneider <asn@cryptomilk.org>
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"

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

#ifdef _WIN32
char *csync_get_user_home_dir(void) {
    wchar_t tmp[MAX_PATH];
    const char *szPath = NULL;

    if( SHGetFolderPathW( NULL,
                          CSIDL_PROFILE|CSIDL_FLAG_CREATE,
                          NULL,
                          0,
                          tmp) == S_OK ) {
        szPath = c_utf8( tmp );
        return szPath;
    }

    return NULL;
}

char *csync_get_local_username(void) {
    DWORD size = 0;
    char *user;

    /* get the size */
    GetUserName(NULL, &size);

    user = (char *) c_malloc(size);
    if (user == NULL) {
        return NULL;
    }

    if (GetUserName(user, &size)) {
        return user;
    }

    return NULL;
}

#else /* ************* !WIN32 ************ */

#ifndef NSS_BUFLEN_PASSWD
#define NSS_BUFLEN_PASSWD 4096
#endif /* NSS_BUFLEN_PASSWD */

char *csync_get_user_home_dir(void) {
    char home[PATH_MAX] = {0};
    const char *envp;
    struct passwd pwd;
    struct passwd *pwdbuf;
    char buf[NSS_BUFLEN_PASSWD];
    int rc;

    envp = getenv("HOME");
    if (envp != NULL) {
        snprintf(home, sizeof(home), "%s", envp);
        if (home[0] != '\0') {
            return c_strdup(home);
        }
    }

    /* Still nothing found, read the password file */
    rc = getpwuid_r(getuid(), &pwd, buf, NSS_BUFLEN_PASSWD, &pwdbuf);
    if (rc != 0) {
        return c_strdup(pwd.pw_dir);
    }

    return NULL;
}

char *csync_get_local_username(void) {
    struct passwd pwd;
    struct passwd *pwdbuf;
    char buf[NSS_BUFLEN_PASSWD];
    char *name;
    int rc;

    rc = getpwuid_r(getuid(), &pwd, buf, NSS_BUFLEN_PASSWD, &pwdbuf);
    if (rc != 0) {
        return NULL;
    }

    name = c_strdup(pwd.pw_name);

    if (name == NULL) {
        return NULL;
    }

    return name;
}

#endif /* ************* WIN32 ************ */

#ifdef HAVE_FNMATCH
#include <fnmatch.h>

int csync_fnmatch(__const char *__pattern, __const char *__name, int __flags) {
    return fnmatch(__pattern, __name, __flags);
}

#else /* HAVE_FNMATCH */

#include <shlwapi.h>
int csync_fnmatch(__const char *__pattern, __const char *__name, int __flags) {
    if(PathMatchSpec(__name, __pattern))
        return 0;
    else
        return 1;
}
#endif /* HAVE_FNMATCH */

CSYNC_ERROR_CODE csync_errno_to_csync_error(CSYNC_ERROR_CODE default_err)
{

  /*
   * Defined csync error codes:
  CSYNC_ERR_NONE          = 0,
  CSYNC_ERR_LOG,
  CSYNC_ERR_LOCK,
  CSYNC_ERR_STATEDB_LOAD,
  CSYNC_ERR_MODULE,
  CSYNC_ERR_TIMESKEW,
  CSYNC_ERR_FILESYSTEM,
  CSYNC_ERR_TREE,
  CSYNC_ERR_MEM,
  CSYNC_ERR_PARAM,
  CSYNC_ERR_UPDATE,
  CSYNC_ERR_RECONCILE,
  CSYNC_ERR_PROPAGATE,
  CSYNC_ERR_ACCESS_FAILED,
  CSYNC_ERR_REMOTE_CREATE,
  CSYNC_ERR_REMOTE_STAT,
  CSYNC_ERR_LOCAL_CREATE,
  CSYNC_ERR_LOCAL_STAT,
  CSYNC_ERR_PROXY,
  CSYNC_ERR_LOOKUP,
  CSYNC_ERR_AUTH_SERVER,
  CSYNC_ERR_AUTH_PROXY,
  CSYNC_ERR_CONNECT,
  CSYNC_ERR_TIMEOUT,
  CSYNC_ERR_HTTP,
  CSYNC_ERR_PERM,
  CSYNC_ERR_NOT_FOUND,
  CSYNC_ERR_EXISTS,
  CSYNC_ERR_NOSPC,
  CSYNC_ERR_QUOTA,
  CSYNC_ERR_SERVICE_UNAVAILABLE,
  CSYNC_ERR_FILE_TOO_BIG,

  CSYNC_ERR_UNSPEC
*/

  CSYNC_ERROR_CODE csync_err = CSYNC_ERR_NONE;

  switch( errno ) {
  case 0:
    csync_err = CSYNC_ERR_NONE;
    break;
    /* The custom errnos first. */
  case ERRNO_GENERAL_ERROR:
    csync_err = CSYNC_ERR_UNSPEC;
    break;
  case ERRNO_LOOKUP_ERROR: /* In Neon: Server or proxy hostname lookup failed */
    csync_err = CSYNC_ERR_LOOKUP;
    break;
  case ERRNO_USER_UNKNOWN_ON_SERVER: /* Neon: User authentication on server failed. */
    csync_err = CSYNC_ERR_AUTH_SERVER;
    break;
  case ERRNO_PROXY_AUTH:
    csync_err = CSYNC_ERR_AUTH_PROXY; /* Neon: User authentication on proxy failed */
    break;
  case ERRNO_CONNECT:
    csync_err = CSYNC_ERR_CONNECT; /* Network: Connection error */
    break;
  case ERRNO_TIMEOUT:
    csync_err = CSYNC_ERR_TIMEOUT; /* Network: Timeout error */
    break;
  case ERRNO_QUOTA_EXCEEDED:
    csync_err = CSYNC_ERR_QUOTA;   /* Quota exceeded */
    break;
  case ERRNO_SERVICE_UNAVAILABLE:
    csync_err = CSYNC_ERR_SERVICE_UNAVAILABLE;  /* Service temporarily down */
    break;
  case EFBIG:
    csync_err = CSYNC_ERR_FILE_TOO_BIG;          /* File larger than 2MB */
    break;
  case ERRNO_PRECONDITION:
  case ERRNO_RETRY:
  case ERRNO_REDIRECT:
  case ERRNO_WRONG_CONTENT:
    csync_err = CSYNC_ERR_HTTP;
    break;

  case ERRNO_TIMEDELTA:
    csync_err = CSYNC_ERR_TIMESKEW;
    break;
  case EPERM:                  /* Operation not permitted */
  case EACCES:                /* Permission denied */
    csync_err = CSYNC_ERR_PERM;
    break;
  case ENOENT:                 /* No such file or directory */
    csync_err = CSYNC_ERR_NOT_FOUND;
    break;
  case EAGAIN:                /* Try again */
    csync_err = CSYNC_ERR_TIMEOUT;
    break;
  case EEXIST:                /* File exists */
    csync_err = CSYNC_ERR_EXISTS;
    break;
  case EINVAL:
    csync_err = CSYNC_ERR_PARAM;
    break;
  case ENOSPC:
    csync_err = CSYNC_ERR_NOSPC;
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
    csync_err = default_err;
  }

  return csync_err;
}




