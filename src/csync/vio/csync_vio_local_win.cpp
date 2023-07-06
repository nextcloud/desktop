/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2013- by Klaas Freitag <freitag@owncloud.com>
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

#include <errno.h>

#include "common/filesystembase.h"
#include "common/utility.h"
#include "common/utility_win.h"
#include "csync.h"
#include "vio/csync_vio_local.h"

#include <QtCore/QLoggingCategory>

#include "common/vfs.h"

Q_LOGGING_CATEGORY(lcCSyncVIOLocal, "sync.csync.vio_local", QtInfoMsg)

/*
 * directory functions
 */

struct csync_vio_handle_t
{
    WIN32_FIND_DATA ffd;
    HANDLE hFind;
    int firstFind;
    QString path; // Always ends with '\'
};

csync_vio_handle_t *csync_vio_local_opendir(const QString &name)
{
    std::unique_ptr<csync_vio_handle_t> handle(new csync_vio_handle_t{});

    // the file wildcard has to be attached
    QString dirname = OCC::FileSystem::longWinPath(name + QLatin1String("/*"));

    handle->hFind = FindFirstFile(reinterpret_cast<const wchar_t *>(dirname.utf16()), &(handle->ffd));

    if (handle->hFind == INVALID_HANDLE_VALUE) {
        int retcode = GetLastError();
        if (retcode == ERROR_FILE_NOT_FOUND) {
            errno = ENOENT;
        } else {
            errno = EACCES;
        }
        return nullptr;
    }

    handle->firstFind = 1; // Set a flag that there first fileinfo is available.

    dirname.chop(1); // remove the *
    handle->path = std::move(dirname);
    return handle.release();
}

int csync_vio_local_closedir(csync_vio_handle_t *dhandle)
{
    Q_ASSERT(dhandle);
    int rc = -1;

    // FindClose returns non-zero on success
    if (FindClose(dhandle->hFind) != 0) {
        rc = 0;
    } else {
        // error case, set errno
        errno = EBADF;
    }
    delete dhandle;
    return rc;
}


static time_t FileTimeToUnixTime(FILETIME *filetime, DWORD *remainder)
{
    long long int t = filetime->dwHighDateTime;
    t <<= 32;
    t += (UINT32)filetime->dwLowDateTime;
    t -= 116444736000000000LL;
    if (t < 0) {
        if (remainder)
            *remainder = 9999999 - (-t - 1) % 10000000;
        return -1 - ((-t - 1) / 10000000);
    } else {
        if (remainder)
            *remainder = t % 10000000;
        return t / 10000000;
    }
}

std::unique_ptr<csync_file_stat_t> csync_vio_local_readdir(csync_vio_handle_t *handle, OCC::Vfs *vfs)
{
    std::unique_ptr<csync_file_stat_t> file_stat;
    DWORD rem;

    errno = 0;

    // the win32 functions get the first valid entry with the opendir
    // thus we must not jump to next entry if it was the first find.
    if (handle->firstFind) {
        handle->firstFind = 0;
    } else {
        if (FindNextFile(handle->hFind, &(handle->ffd)) == 0) {
            // might be error, check!
            int dwError = GetLastError();
            if (dwError != ERROR_NO_MORE_FILES) {
                qCWarning(lcCSyncVIOLocal, "FindNextFile error %d", dwError);
                errno = EACCES; // no more files is fine. Otherwise EACCESS
            }
            return nullptr;
        }
    }
    const QString path = QString::fromWCharArray(handle->ffd.cFileName);
    if (path == QLatin1String(".") || path == QLatin1String(".."))
        return csync_vio_local_readdir(handle, vfs);

    file_stat.reset(new csync_file_stat_t);
    file_stat->path = path;

    if (vfs && vfs->statTypeVirtualFile(file_stat.get(), &handle->ffd)) {
        // all good
    } else if (handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        // Detect symlinks, and treat junctions as symlinks too.
        if (handle->ffd.dwReserved0 == IO_REPARSE_TAG_SYMLINK
            || handle->ffd.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT) {
            file_stat->type = ItemTypeSoftLink;
        } else {
            // The SIS and DEDUP reparse points should be treated as
            // regular files. We don't know about the other ones yet,
            // but will also treat them normally for now.
            file_stat->type = ItemTypeFile;
        }
    } else if (handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE
        || handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE) {
        file_stat->type = ItemTypeSkip;
    } else if (handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        file_stat->type = ItemTypeDirectory;
    } else {
        file_stat->type = ItemTypeFile;
    }

    /* Check for the hidden flag */
    if (handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
        file_stat->is_hidden = true;
    }

    file_stat->size = (handle->ffd.nFileSizeHigh * ((int64_t)(MAXDWORD) + 1)) + handle->ffd.nFileSizeLow;
    file_stat->modtime = FileTimeToUnixTime(&handle->ffd.ftLastWriteTime, &rem);

    // path always ends with '\', by construction

    if (csync_vio_local_stat(handle->path + path, file_stat.get()) < 0) {
        // Will get excluded by _csync_detect_update.
        file_stat->type = ItemTypeSkip;
    }

    return file_stat;
}

int csync_vio_local_stat(const QString &uri, csync_file_stat_t *buf)
{
    /* Almost nothing to do since csync_vio_local_readdir already filled up most of the information
       But we still need to fetch the file ID.
       Possible optimisation: only fetch the file id when we need it (for new files)
      */

    HANDLE h;
    BY_HANDLE_FILE_INFORMATION fileInfo;
    ULARGE_INTEGER FileIndex;

    h = CreateFileW(reinterpret_cast<const wchar_t *>(OCC::FileSystem::longWinPath(uri).utf16()), 0, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
        NULL);
    if (h == INVALID_HANDLE_VALUE) {
        errno = GetLastError();
        qCCritical(lcCSyncVIOLocal) << "CreateFileW failed on" << uri << OCC::Utility::formatWinError(errno);
        return -1;
    }

    if (!GetFileInformationByHandle(h, &fileInfo)) {
        errno = GetLastError();
        qCCritical(lcCSyncVIOLocal) << "GetFileInformationByHandle failed on" << uri << OCC::Utility::formatWinError(errno);
        CloseHandle(h);
        return -1;
    }

    /* Get the Windows file id as an inode replacement. */
    FileIndex.HighPart = fileInfo.nFileIndexHigh;
    FileIndex.LowPart = fileInfo.nFileIndexLow;
    FileIndex.QuadPart &= 0x0000FFFFFFFFFFFF;
    /* printf("Index: %I64i\n", FileIndex.QuadPart); */
    buf->inode = FileIndex.QuadPart;

    buf->size = ULARGE_INTEGER { { fileInfo.nFileSizeLow, fileInfo.nFileSizeHigh } }.QuadPart;

    DWORD rem;
    buf->modtime = FileTimeToUnixTime(&fileInfo.ftLastWriteTime, &rem);

    CloseHandle(h);
    return 0;
}
