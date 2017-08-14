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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

#include "windows.h"

#include "c_private.h"
#include "c_lib.h"
#include "c_string.h"
#include "csync_util.h"
#include "csync_log.h"
#include "csync_vio.h"

#include "vio/csync_vio_local.h"


/*
 * directory functions
 */

typedef struct dhandle_s {
  WIN32_FIND_DATA ffd;
  HANDLE hFind;
  int firstFind;
  char *path;
} dhandle_t;

csync_vio_handle_t *csync_vio_local_opendir(const char *name) {
  dhandle_t *handle = NULL;
  mbchar_t *dirname = NULL;

  handle = c_malloc(sizeof(dhandle_t));

  // the file wildcard has to be attached
  int len_name = strlen(name);
  if( len_name ) {
      char *h = NULL;

      // alloc an enough large buffer to take the name + '/*' + the closing zero.
      h = c_malloc(len_name+3);
      strncpy( h, name, 1+len_name);
      strncat(h, "/*", 2);

      dirname = c_utf8_path_to_locale(h);
      SAFE_FREE(h);
  }

  if( dirname ) {
      handle->hFind = FindFirstFile(dirname, &(handle->ffd));
  }

  if (!dirname || handle->hFind == INVALID_HANDLE_VALUE) {
      int retcode = GetLastError();
      if( retcode == ERROR_FILE_NOT_FOUND ) {
          errno = ENOENT;
      } else {
          errno = EACCES;
      }
      SAFE_FREE(handle);
      return NULL;
  }

  handle->firstFind = 1; // Set a flag that there first fileinfo is available.

  handle->path = c_strdup(name);
  c_free_locale_string(dirname);

  return (csync_vio_handle_t *) handle;
}

int csync_vio_local_closedir(csync_vio_handle_t *dhandle) {
  dhandle_t *handle = NULL;
  int rc = -1;

  if (dhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  handle = (dhandle_t *) dhandle;
  // FindClose returns non-zero on success
  if( FindClose(handle->hFind) != 0 ) {
      rc = 0;
  } else {
      // error case, set errno
      errno = EBADF;
  }

  SAFE_FREE(handle->path);
  SAFE_FREE(handle);

  return rc;
}


static time_t FileTimeToUnixTime(FILETIME *filetime, DWORD *remainder)
{
    long long int t = filetime->dwHighDateTime;
    t <<= 32;
    t += (UINT32)filetime->dwLowDateTime;
    t -= 116444736000000000LL;
    if (t < 0)
    {
        if (remainder) *remainder = 9999999 - (-t - 1) % 10000000;
        return -1 - ((-t - 1) / 10000000);
    }
    else
    {
        if (remainder) *remainder = t % 10000000;
        return t / 10000000;
    }
}

csync_vio_file_stat_t *csync_vio_local_readdir(csync_vio_handle_t *dhandle) {

  dhandle_t *handle = NULL;
  csync_vio_file_stat_t *file_stat = NULL;
  DWORD rem;

  handle = (dhandle_t *) dhandle;

  errno = 0;
  file_stat = csync_vio_file_stat_new();
  if (file_stat == NULL) {
      errno = ENOMEM;
      goto err;
  }
  file_stat->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  // the win32 functions get the first valid entry with the opendir
  // thus we must not jump to next entry if it was the first find.
  if( handle->firstFind ) {
      handle->firstFind = 0;
  } else {
      if( FindNextFile(handle->hFind, &(handle->ffd)) == 0 ) {
          // might be error, check!
          int dwError = GetLastError();
          if (dwError != ERROR_NO_MORE_FILES) {
              errno = EACCES; // no more files is fine. Otherwise EACCESS
          }
          goto err;
      }
  }
  file_stat->name = c_utf8_from_locale(handle->ffd.cFileName);

    file_stat->flags = CSYNC_VIO_FILE_FLAGS_NONE;
    file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
    if (handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        // Detect symlinks, and treat junctions as symlinks too.
        if (handle->ffd.dwReserved0 == IO_REPARSE_TAG_SYMLINK
                || handle->ffd.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT) {
            file_stat->flags |= CSYNC_VIO_FILE_FLAGS_SYMLINK;
            file_stat->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
        } else {
            // The SIS and DEDUP reparse points should be treated as
            // regular files. We don't know about the other ones yet,
            // but will also treat them normally for now.
            file_stat->type = CSYNC_VIO_FILE_TYPE_REGULAR;
        }
    } else if (handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE
                || handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE
                || handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) {
        file_stat->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
    } else if (handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        file_stat->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
    } else {
        file_stat->type = CSYNC_VIO_FILE_TYPE_REGULAR;
    }

    /* Check for the hidden flag */
    if( handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN ) {
        file_stat->flags |= CSYNC_VIO_FILE_FLAGS_HIDDEN;
    }

    file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_FLAGS;
    file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

    file_stat->size = (handle->ffd.nFileSizeHigh * ((int64_t)(MAXDWORD)+1)) + handle->ffd.nFileSizeLow;
    file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

    file_stat->atime = FileTimeToUnixTime(&handle->ffd.ftLastAccessTime, &rem);
    file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;

    file_stat->mtime = FileTimeToUnixTime(&handle->ffd.ftLastWriteTime, &rem);
      /* CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Local File MTime: %llu", (unsigned long long) buf->mtime ); */
    file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

    file_stat->ctime = FileTimeToUnixTime(&handle->ffd.ftCreationTime, &rem);
    file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_CTIME;

    return file_stat;

err:
  SAFE_FREE(file_stat);

  return NULL;
}



int csync_vio_local_stat(const char *uri, csync_vio_file_stat_t *buf) {
    /* Almost nothing to do since csync_vio_local_readdir already filled up most of the information
       But we still need to fetch the file ID.
       Possible optimisation: only fetch the file id when we need it (for new files)
      */

    HANDLE h;
    BY_HANDLE_FILE_INFORMATION fileInfo;
    ULARGE_INTEGER FileIndex;
    mbchar_t *wuri = c_utf8_path_to_locale( uri );

    h = CreateFileW( wuri, 0, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                     NULL );
    if( h == INVALID_HANDLE_VALUE ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_CRIT, "CreateFileW failed on %s", uri );
        errno = GetLastError();
        c_free_locale_string(wuri);
        return -1;
    }

    if(!GetFileInformationByHandle( h, &fileInfo ) ) {
        CSYNC_LOG(CSYNC_LOG_PRIORITY_CRIT, "GetFileInformationByHandle failed on %s", uri );
        errno = GetLastError();
        c_free_locale_string(wuri);
        CloseHandle(h);
        return -1;
    }

    /* Get the Windows file id as an inode replacement. */
    FileIndex.HighPart = fileInfo.nFileIndexHigh;
    FileIndex.LowPart = fileInfo.nFileIndexLow;
    FileIndex.QuadPart &= 0x0000FFFFFFFFFFFF;
    /* printf("Index: %I64i\n", FileIndex.QuadPart); */
    buf->inode = FileIndex.QuadPart;

    if (!(buf->fields & CSYNC_VIO_FILE_STAT_FIELDS_SIZE)) {
        buf->size = (fileInfo.nFileSizeHigh * ((int64_t)(MAXDWORD)+1)) + fileInfo.nFileSizeLow;
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
    }
    if (!(buf->fields & CSYNC_VIO_FILE_STAT_FIELDS_MTIME)) {
        DWORD rem;
        buf->mtime = FileTimeToUnixTime(&fileInfo.ftLastWriteTime, &rem);
        /* CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Local File MTime: %llu", (unsigned long long) buf->mtime ); */
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
    }

    c_free_locale_string(wuri);
    CloseHandle(h);
    return 0;
}
