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
int c_utimes(const QString &uri, const struct timespec *times) {
    int ret = utimensat(AT_FDCWD, QFile::encodeName(uri).constData(), times, AT_SYMLINK_NOFOLLOW);
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
static void UnixTimevalToFileTime(struct timespec t, LPFILETIME pft)
{
    LONGLONG ll = 0;
    ll = Int32x32To64(t.tv_sec, CSYNC_USEC_IN_SEC*10) + t.tv_nsec/100 + CSYNC_SECONDS_SINCE_1601*CSYNC_USEC_IN_SEC*10;
    pft->dwLowDateTime = (DWORD)ll;
    pft->dwHighDateTime = ll >> 32;
}

int c_utimes(const QString &uri, const struct timespec *times) {
    FILETIME LastAccessTime;
    FILETIME LastModificationTime;
    HANDLE hFile = nullptr;

    auto wuri = uri.toStdWString();

    if(times) {
        UnixTimevalToFileTime(times[0], &LastAccessTime);
        UnixTimevalToFileTime(times[1], &LastModificationTime);
    }
    else {
        GetSystemTimeAsFileTime(&LastAccessTime);
        GetSystemTimeAsFileTime(&LastModificationTime);
    }

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

    if(!SetFileTime(hFile, nullptr, &LastAccessTime, &LastModificationTime)) {
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
