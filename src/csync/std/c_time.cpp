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
#include "c_time.h"

#include "common/filesystembase.h"
#include "common/utility.h"

#include <QFile>

#ifdef HAVE_UTIMES
int c_utimes(const QString &uri, const struct timeval *times) {
    int ret = utimes(QFile::encodeName(uri).constData(), times);
    return ret;
}
#else // HAVE_UTIMES

#ifdef _WIN32
#include "common/utility_win.h"
// implementation for utimes taken from KDE mingw headers

#include <errno.h>
#include <wtypes.h>
#include <winsock2.h>

Q_LOGGING_CATEGORY(lcCSyncCtime, "sync.csync.c_time", QtInfoMsg)


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

int c_utimes(const QString &uri, const struct timeval *times) {
    FILETIME LastAccessTime;
    FILETIME LastModificationTime;
    HANDLE hFile;

    const QString wuri = OCC::FileSystem::longWinPath(uri);

    if(times) {
        UnixTimevalToFileTime(times[0], &LastAccessTime);
        UnixTimevalToFileTime(times[1], &LastModificationTime);
    }
    else {
        GetSystemTimeAsFileTime(&LastAccessTime);
        GetSystemTimeAsFileTime(&LastModificationTime);
    }

    hFile = CreateFileW(reinterpret_cast<const wchar_t *>(wuri.utf16()), FILE_WRITE_ATTRIBUTES, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if(hFile==INVALID_HANDLE_VALUE) {
        const auto error = GetLastError();
        qCWarning(lcCSyncCtime) << Q_FUNC_INFO << "for" << wuri << "failed with error:" << OCC::Utility::formatWinError(error);
        switch (error) {
        case ERROR_FILE_NOT_FOUND:
            errno = ENOENT;
            break;
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
            errno = ENOTDIR;
            break;
        /*case ERROR_WRITE_PROTECT:   //CreateFile sets ERROR_ACCESS_DENIED on read-only devices
         *                errno=EROFS;
         *                break;*/
        case ERROR_ACCESS_DENIED:
            errno = EACCES;
            break;
        default:
            errno = ENOENT; // what other errors can occur?
        }

        return -1;
    }

    if(!SetFileTime(hFile, NULL, &LastAccessTime, &LastModificationTime)) {
        // can this happen?
        const auto error = GetLastError();
        qCWarning(lcCSyncCtime) << Q_FUNC_INFO << "for" << wuri << "failed with error:" << OCC::Utility::formatWinError(error);
        errno=ENOENT;
        CloseHandle(hFile);
        return -1;
    }

    CloseHandle(hFile);

    return 0;
}

#endif // _WIN32
#endif // HAVE_UTIMES
