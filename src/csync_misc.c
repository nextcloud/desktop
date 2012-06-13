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
#include <sys/types.h>

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
    char *szPath = NULL;
    struct passwd pwd;
    struct passwd *pwdbuf;
    char buf[NSS_BUFLEN_PASSWD];
    int rc;

    szPath = getenv("HOME");
    if( szPath ) {
        return c_strdup(szPath);
    }

    /* Still nothing found, read the password file */
    rc = getpwuid_r(getuid(), &pwd, buf, NSS_BUFLEN_PASSWD, &pwdbuf);
    if (rc != 0) {
        szPath = c_strdup(pwd.pw_dir);
    }

    return szPath;
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
