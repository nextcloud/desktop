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
#include "c_time.h"

#include <QFile>

#ifdef HAVE_UTIMES
int c_utimes(const QString &uri, const time_t time)
{
    struct timeval times[2];
    times[0].tv_sec = times[1].tv_sec = time;
    times[0].tv_usec = times[1].tv_usec = 0;
    return utimes(QFile::encodeName(uri).constData(), times);
}

#else // HAVE_UTIMES

#ifdef _WIN32
// based on the implementation for utimes from KDE mingw headers

#include <errno.h>
#include <wtypes.h>

constexpr long long CSYNC_SECONDS_SINCE_1601 = 11644473600LL;
constexpr long long CSYNC_USEC_IN_SEC = 1000000LL;

// after Microsoft KB167296, except it uses a `time_t` instead of a `struct timeval`.
//
// `struct timeval` is defined in the winsock.h header of all places, and its fields are two `long`s,
// which even on x64 Windows is 4 bytes wide (i.e. int32).  `time_t` on the other hand is 8 bytes
// wide (int64) on x64 Windows as well.
static void UnixTimeToFiletime(const time_t time, LPFILETIME pft)
{
    LONGLONG ll = time * CSYNC_USEC_IN_SEC * 10 + CSYNC_SECONDS_SINCE_1601 * CSYNC_USEC_IN_SEC * 10;
    pft->dwLowDateTime = (DWORD)ll;
    pft->dwHighDateTime = ll >> 32;
}

int c_utimes(const QString &uri, const time_t time)
{
    FILETIME filetime;
    HANDLE hFile = nullptr;

    auto wuri = uri.toStdWString();

    UnixTimeToFiletime(time, &filetime);

    hFile=CreateFileW(wuri.data(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL+FILE_FLAG_BACKUP_SEMANTICS, nullptr);
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

    if (!SetFileTime(hFile, nullptr, &filetime, &filetime)) {
        //can this happen?
        errno=ENOENT;
        CloseHandle(hFile);
        return -1;
    }

    CloseHandle(hFile);

    return 0;
}

#endif // _WIN32
#endif // HAVE_UTIMES
