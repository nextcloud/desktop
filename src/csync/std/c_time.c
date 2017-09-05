/*
 * c_time - time functions
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
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
#include "c_private.h"
#include "c_string.h"

#include "c_path.h"
#include "c_time.h"
#include "c_utf8.h"

struct timespec c_tspecdiff(struct timespec time1, struct timespec time0) {
  struct timespec ret;
  int xsec = 0;
  int sign = 1;

  if (time0.tv_nsec > time1.tv_nsec) {
    xsec = (int) ((time0.tv_nsec - time1.tv_nsec) / (1E9 + 1));
    time0.tv_nsec -= (long int) (1E9 * xsec);
    time0.tv_sec += xsec;
  }

  if ((time1.tv_nsec - time0.tv_nsec) > 1E9) {
    xsec = (int) ((time1.tv_nsec - time0.tv_nsec) / 1E9);
    time0.tv_nsec += (long int) (1E9 * xsec);
    time0.tv_sec -= xsec;
  }

  ret.tv_sec = time1.tv_sec - time0.tv_sec;
  ret.tv_nsec = time1.tv_nsec - time0.tv_nsec;

  if (time1.tv_sec < time0.tv_sec) {
    sign = -1;
  }

  ret.tv_sec = ret.tv_sec * sign;

  return ret;
}

double c_secdiff(struct timespec clock1, struct timespec clock0) {
  double ret;
  struct timespec diff;

  diff = c_tspecdiff(clock1, clock0);

  ret = diff.tv_sec;
  ret += (double) diff.tv_nsec / (double) 1E9;

  return ret;
}


#ifdef HAVE_UTIMES
int c_utimes(const char *uri, const struct timeval *times) {
    mbchar_t *wuri = c_utf8_path_to_locale(uri);
    int ret = utimes(wuri, times);
    c_free_locale_string(wuri);
    return ret;
}
#else // HAVE_UTIMES

#ifdef _WIN32
// implementation for utimes taken from KDE mingw headers

#include <errno.h>
#include <wtypes.h>
#define CSYNC_SECONDS_SINCE_1601 11644473600LL
#define CSYNC_USEC_IN_SEC            1000000LL
//after Microsoft KB167296
static void UnixTimevalToFileTime(struct timeval t, LPFILETIME pft)
{
    LONGLONG ll;
    ll = Int32x32To64(t.tv_sec, CSYNC_USEC_IN_SEC*10) + t.tv_usec*10 + CSYNC_SECONDS_SINCE_1601*CSYNC_USEC_IN_SEC*10;
    pft->dwLowDateTime = (DWORD)ll;
    pft->dwHighDateTime = ll >> 32;
}

int c_utimes(const char *uri, const struct timeval *times) {
    FILETIME LastAccessTime;
    FILETIME LastModificationTime;
    HANDLE hFile;

    mbchar_t *wuri = c_utf8_path_to_locale( uri );

    if(times) {
        UnixTimevalToFileTime(times[0], &LastAccessTime);
        UnixTimevalToFileTime(times[1], &LastModificationTime);
    }
    else {
        GetSystemTimeAsFileTime(&LastAccessTime);
        GetSystemTimeAsFileTime(&LastModificationTime);
    }

    hFile=CreateFileW(wuri, FILE_WRITE_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL+FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if(hFile==INVALID_HANDLE_VALUE) {
        switch(GetLastError()) {
            case ERROR_FILE_NOT_FOUND:
                errno=ENOENT;
                break;
            case ERROR_PATH_NOT_FOUND:
            case ERROR_INVALID_DRIVE:
                errno=ENOTDIR;
                break;
                /*case ERROR_WRITE_PROTECT:   //CreateFile sets ERROR_ACCESS_DENIED on read-only devices
                 *                errno=EROFS;
                 *                break;*/
                case ERROR_ACCESS_DENIED:
                    errno=EACCES;
                    break;
                default:
                    errno=ENOENT;   //what other errors can occur?
        }

        return -1;
    }

    if(!SetFileTime(hFile, NULL, &LastAccessTime, &LastModificationTime)) {
        //can this happen?
        errno=ENOENT;
        CloseHandle(hFile);
        c_free_locale_string(wuri);
        return -1;
    }

    CloseHandle(hFile);
    c_free_locale_string(wuri);

    return 0;
}

#endif // _WIN32
#endif // HAVE_UTIMES
