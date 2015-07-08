/*
 * libcsync -- a library to sync a directory with another
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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>

#ifdef _WIN32
#include "windows.h"
#endif

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
#if defined _WIN32
  WIN32_FIND_DATA ffd;
  HANDLE hFind;
  int firstFind;
#else
  DIR *dh;
#endif
  char *path;
} dhandle_t;

csync_vio_handle_t *csync_vio_local_opendir(const char *name) {
  dhandle_t *handle = NULL;
  mbchar_t *dirname = NULL;

  handle = c_malloc(sizeof(dhandle_t));

#ifdef _WIN32
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
      SAFE_FREE(handle);
      return NULL;
  }

  handle->firstFind = 1; // Set a flag that there first fileinfo is available.
#else
  dirname = c_utf8_path_to_locale(name);

  handle->dh = _topendir( dirname );
  if (handle->dh == NULL) {
    c_free_locale_string(dirname);
    SAFE_FREE(handle);
    return NULL;
  }
#endif
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
#ifdef _WIN32
  // FindClose returns non-zero on success
  if( FindClose(handle->hFind) != 0 ) {
      rc = 0;
  }
#else
  rc = _tclosedir(handle->dh);
#endif

  SAFE_FREE(handle->path);
  SAFE_FREE(handle);

  return rc;
}

csync_vio_file_stat_t *csync_vio_local_readdir(csync_vio_handle_t *dhandle) {

  dhandle_t *handle = NULL;
  csync_vio_file_stat_t *file_stat = NULL;

  handle = (dhandle_t *) dhandle;

  errno = 0;
  file_stat = csync_vio_file_stat_new();
  if (file_stat == NULL) {
    goto err;
  }
  file_stat->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

#ifdef _WIN32
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

  file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
  if (handle->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      file_stat->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
  } else {
      file_stat->type = CSYNC_VIO_FILE_TYPE_REGULAR;
  }
#else
  struct _tdirent *dirent = NULL;

  dirent = _treaddir(handle->dh);
  if (dirent == NULL) {
      goto err;
  }
  file_stat->name = c_utf8_from_locale(dirent->d_name);

  /* Check for availability of d_type, see manpage. */
#ifdef _DIRENT_HAVE_D_TYPE
  switch (dirent->d_type) {
    case DT_FIFO:
    case DT_SOCK:
    case DT_CHR:
    case DT_BLK:
      break;
    case DT_DIR:
    case DT_REG:
      file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      if (dirent->d_type == DT_DIR) {
        file_stat->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      } else {
        file_stat->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      }
      break;
    case DT_UNKNOWN:
      file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      file_stat->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
    default:
      break;
  }
#endif

#endif // non WIN32

  return file_stat;

err:
  SAFE_FREE(file_stat);

  return NULL;
}


#ifdef _WIN32
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

int csync_vio_local_stat(const char *uri, csync_vio_file_stat_t *buf) {
    HANDLE h, hFind;
    FILETIME ftCreate, ftAccess, ftWrite;
    BY_HANDLE_FILE_INFORMATION fileInfo;
    WIN32_FIND_DATAW FindFileData;
    ULARGE_INTEGER FileIndex;
    mbchar_t *wuri = c_utf8_path_to_locale( uri );

    h = CreateFileW( wuri, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL+FILE_FLAG_BACKUP_SEMANTICS, NULL );
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

    buf->flags = CSYNC_VIO_FILE_FLAGS_NONE;
    do {
        // Check first if it is a symlink (code from c_islink)
        if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            hFind = FindFirstFileW(wuri, &FindFileData );
            if (hFind !=  INVALID_HANDLE_VALUE) {
                if( (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                        (FindFileData.dwReserved0 & IO_REPARSE_TAG_SYMLINK) ) {
                    buf->flags = CSYNC_VIO_FILE_FLAGS_SYMLINK;
                    buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
                    break;
                }
            }
            FindClose(hFind);
        }
        if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DEVICE
                || fileInfo.dwFileAttributes & FILE_ATTRIBUTE_OFFLINE
                || fileInfo.dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) {
            buf->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
            break;
        }
        if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            buf->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
            break;
        }
        // fallthrough:
        buf->type = CSYNC_VIO_FILE_TYPE_REGULAR;
        break;
    } while (0);
    /* TODO Do we want to parse for CSYNC_VIO_FILE_FLAGS_HIDDEN ? */
    buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_FLAGS;
    buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

    buf->device = fileInfo.dwVolumeSerialNumber;
    buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_DEVICE;

    /* Get the Windows file id as an inode replacement. */
    FileIndex.HighPart = fileInfo.nFileIndexHigh;
    FileIndex.LowPart = fileInfo.nFileIndexLow;
    FileIndex.QuadPart &= 0x0000FFFFFFFFFFFF;
    /* printf("Index: %I64i\n", FileIndex.QuadPart); */
    buf->inode = FileIndex.QuadPart;

    buf->size = (fileInfo.nFileSizeHigh * ((int64_t)(MAXDWORD)+1)) + fileInfo.nFileSizeLow;
    buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

    /* Get the file time with a win32 call rather than through stat. See
      * http://www.codeproject.com/Articles/1144/Beating-the-Daylight-Savings-Time-bug-and-getting
      * for deeper explanation.
      */
    if( GetFileTime(h, &ftCreate, &ftAccess, &ftWrite) ) {
        DWORD rem;
        buf->atime = FileTimeToUnixTime(&ftAccess, &rem);
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;

        buf->mtime = FileTimeToUnixTime(&ftWrite, &rem);
        /* CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Local File MTime: %llu", (unsigned long long) buf->mtime ); */
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

        buf->ctime = FileTimeToUnixTime(&ftCreate, &rem);
        buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_CTIME;
    }

    c_free_locale_string(wuri);
    CloseHandle(h);

    return 0;
}

#else

int csync_vio_local_stat(const char *uri, csync_vio_file_stat_t *buf) {
  csync_stat_t sb;

  mbchar_t *wuri = c_utf8_path_to_locale( uri );

  if( _tstat(wuri, &sb) < 0) {
    c_free_locale_string(wuri);
    return -1;
  }

  buf->name = c_basename(uri);

  if (buf->name == NULL) {
    csync_vio_file_stat_destroy(buf);
    c_free_locale_string(wuri);
    return -1;
  }
  buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  switch(sb.st_mode & S_IFMT) {
    case S_IFBLK:
      buf->type = CSYNC_VIO_FILE_TYPE_BLOCK_DEVICE;
      break;
    case S_IFCHR:
      buf->type = CSYNC_VIO_FILE_TYPE_CHARACTER_DEVICE;
      break;
    case S_IFDIR:
      buf->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      break;
    case S_IFIFO:
      buf->type = CSYNC_VIO_FILE_TYPE_FIFO;
      break;
    case S_IFREG:
      buf->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      break;
    case S_IFLNK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
    case S_IFSOCK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
    default:
      buf->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
      break;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

  buf->mode = sb.st_mode;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MODE;

  if (buf->type == CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK) {
    /* FIXME: handle symlink */
    buf->flags = CSYNC_VIO_FILE_FLAGS_SYMLINK;
  } else {
    buf->flags = CSYNC_VIO_FILE_FLAGS_NONE;
  }
#ifdef __APPLE__
  if (sb.st_flags & UF_HIDDEN) {
      buf->flags |= CSYNC_VIO_FILE_FLAGS_HIDDEN;
  }
#endif
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_FLAGS;

  buf->device = sb.st_dev;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_DEVICE;

  buf->inode = sb.st_ino;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_INODE;

  buf->atime = sb.st_atime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;

  buf->mtime = sb.st_mtime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  buf->ctime = sb.st_ctime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_CTIME;

  buf->size = sb.st_size;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

  c_free_locale_string(wuri);
  return 0;
}
#endif




