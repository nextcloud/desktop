/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008      by Andreas Schneider <mail@cynapses.org>
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
 *
 * vim: ts=2 sw=2 et cindent
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>

#ifdef _WIN32
#include "windows.h"
#define _UNICODE
#endif

#include "c_private.h"
#include "c_lib.h"
#include "c_string.h"
#include "csync_util.h"
#include "csync_log.h"
#include "csync_vio.h"

#include "vio/csync_vio_local.h"



/* the url comes in as utf-8 and in windows, it needs to be multibyte. */
csync_vio_method_handle_t *csync_vio_local_open(const char *durl, int flags, mode_t mode) {
  fhandle_t *handle = NULL;
  int fd = -1;
  _TCHAR *url = c_multibyte(durl);

  if ((fd = _topen(url, flags, mode)) < 0) {
    c_free_multibyte(url);
    return NULL;
  }

  handle = c_malloc(sizeof(fhandle_t));
  if (handle == NULL) {
    c_free_multibyte(url);
    close(fd);
    return NULL;
  }

  handle->fd = fd;

  c_free_multibyte(url);

  return (csync_vio_method_handle_t *) handle;
}

csync_vio_method_handle_t *csync_vio_local_creat(const char *durl, mode_t mode) {
  fhandle_t *handle = NULL;
  int fd = -1;
  _TCHAR *url = c_multibyte(durl);

  if(( fd = _tcreat( url, mode)) < 0) {
      c_free_multibyte(url);
      return NULL;
  }

  handle = c_malloc(sizeof(fhandle_t));
  if (handle == NULL) {
    c_free_multibyte(url);
    close(fd);
    return NULL;
  }

  handle->fd = fd;
  c_free_multibyte(url);
  return (csync_vio_method_handle_t *) handle;
}

int csync_vio_local_close(csync_vio_method_handle_t *fhandle) {
  int rc = -1;
  fhandle_t *handle = NULL;

  if (fhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  handle = (fhandle_t *) fhandle;

  rc = close(handle->fd);

  SAFE_FREE(handle);

  return rc;
}

ssize_t csync_vio_local_read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  fhandle_t *handle = NULL;

  if (fhandle == NULL) {
    errno = EBADF;
    return (ssize_t) -1;
  }

  handle = (fhandle_t *) fhandle;

  return read(handle->fd, buf, count);
}

ssize_t csync_vio_local_write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  ssize_t n = 0;
  fhandle_t *handle = NULL;

  if (fhandle == NULL) {
    errno = EBADF;
    return (ssize_t) -1;
  }

  handle = (fhandle_t *) fhandle;

  /* safe_write */
  do {
    n = write(handle->fd, buf, count);
  } while (n < 0 && errno == EINTR);

  return n;
}

off_t csync_vio_local_lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
  fhandle_t *handle = NULL;

  if (fhandle == NULL) {
    return (off_t) -1;
  }

  handle = (fhandle_t *) fhandle;

  return lseek(handle->fd, offset, whence);
}

/*
 * directory functions
 */

typedef struct dhandle_s {
  _TDIR *dh;
  char *path;
} dhandle_t;

csync_vio_method_handle_t *csync_vio_local_opendir(const char *name) {
  dhandle_t *handle = NULL;
  _TCHAR *dirname = c_multibyte(name);
  handle = c_malloc(sizeof(dhandle_t));
  if (handle == NULL) {
    c_free_multibyte(dirname);
    return NULL;
  }

  handle->dh = _topendir( dirname );
  if (handle->dh == NULL) {
    c_free_multibyte(dirname);
    SAFE_FREE(handle);
    return NULL;
  }

  handle->path = c_strdup(name);
  c_free_multibyte(dirname);

  return (csync_vio_method_handle_t *) handle;
}

int csync_vio_local_closedir(csync_vio_method_handle_t *dhandle) {
  dhandle_t *handle = NULL;
  int rc = -1;

  if (dhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  handle = (dhandle_t *) dhandle;
  rc = _tclosedir(handle->dh);

  SAFE_FREE(handle->path);
  SAFE_FREE(handle);

  return rc;
}

csync_vio_file_stat_t *csync_vio_local_readdir(csync_vio_method_handle_t *dhandle) {
  struct _tdirent *dirent = NULL;

  dhandle_t *handle = NULL;
  csync_vio_file_stat_t *file_stat = NULL;

  handle = (dhandle_t *) dhandle;

  errno = 0;
  dirent = _treaddir(handle->dh);
  if (dirent == NULL) {
    if (errno) {
      goto err;
    } else {
      return NULL;
    }
  }

  file_stat = csync_vio_file_stat_new();
  if (file_stat == NULL) {
    goto err;
  }

  file_stat->name = c_utf8(dirent->d_name);
  file_stat->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

#ifndef _WIN32
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

  return file_stat;

err:
  SAFE_FREE(file_stat);

  return NULL;
}

int csync_vio_local_mkdir(const char *uri, mode_t mode) {
  return c_mkdirs(uri, mode);
}

int csync_vio_local_rmdir(const char *uri) {
  _TCHAR *dirname = c_multibyte(uri);
  int re = -1;

  re = _trmdir(dirname);
  c_free_multibyte(dirname);
  return re;
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
#endif

int csync_vio_local_stat(const char *uri, csync_vio_file_stat_t *buf) {
  csync_stat_t sb;
  _TCHAR *wuri = c_multibyte( uri );
  if( _tstat(wuri, &sb) < 0) {
    c_free_multibyte(wuri);
    return -1;
  }

  buf->name = c_basename(uri);

  if (buf->name == NULL) {
    c_free_multibyte(wuri);
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
#ifndef _WIN32
    case S_IFLNK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
    case S_IFSOCK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
#endif
    default:
      buf->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
      break;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

  buf->mode = sb.st_mode;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;

  if (buf->type == CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK) {
    /* FIXME: handle symlink */
    buf->flags = CSYNC_VIO_FILE_FLAGS_SYMLINK;
  } else {
    buf->flags = CSYNC_VIO_FILE_FLAGS_NONE;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_FLAGS;

  buf->device = sb.st_dev;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_DEVICE;

  buf->inode = sb.st_ino;
#ifdef _WIN32
  /* Get the Windows file id as an inode replacement. */
  HANDLE h = CreateFileW( wuri, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			  FILE_ATTRIBUTE_NORMAL+FILE_FLAG_BACKUP_SEMANTICS, NULL );
  if( h == INVALID_HANDLE_VALUE ) {
     errno = GetLastError();
     c_free_multibyte(wuri);
     return -1;

  } else {
     FILETIME ftCreate, ftAccess, ftWrite;
     SYSTEMTIME stUTC;

     BY_HANDLE_FILE_INFORMATION fileInfo;

     if( GetFileInformationByHandle( h, &fileInfo ) ) {
        ULARGE_INTEGER FileIndex;
        FileIndex.HighPart = fileInfo.nFileIndexHigh;
        FileIndex.LowPart = fileInfo.nFileIndexLow;
        FileIndex.QuadPart &= 0x0000FFFFFFFFFFFF;

        /* printf("Index: %I64i\n", FileIndex.QuadPart); */

        buf->inode = FileIndex.QuadPart;
     }

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
     CloseHandle(h);
  }
#else /* non windows platforms: */
  buf->blksize = sb.st_blksize;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_SIZE;

  buf->blkcount = sb.st_blocks;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_COUNT;

  buf->atime = sb.st_atime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;

  buf->mtime = sb.st_mtime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  buf->ctime = sb.st_ctime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_CTIME;
#endif

  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_INODE;

  buf->nlink = sb.st_nlink;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_LINK_COUNT;

  buf->uid = sb.st_uid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_UID;

  buf->gid = sb.st_gid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_GID;

  buf->size = sb.st_size;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

  c_free_multibyte(wuri);

  return 0;
}

int csync_vio_local_rename(const char *olduri, const char *newuri) {
  _TCHAR *nuri = c_multibyte(newuri);
  _TCHAR *ouri = c_multibyte(olduri);
  int re = -1;

#ifdef _WIN32
  if(ouri && nuri) {
    if (MoveFileExW(ouri, nuri, MOVEFILE_COPY_ALLOWED + MOVEFILE_REPLACE_EXISTING + MOVEFILE_WRITE_THROUGH ) != 0) {
        /* Success */
         re = 0;
    } else {
        errno = GetLastError();
    }
    /* CSYNC_LOG( CSYNC_LOG_PRIORITY_DEBUG, "ERR: Could not rename, error %d", errno ); */
  } else {
    errno = ENOENT;
  }
#else
  re = rename(ouri, nuri);
#endif
  c_free_multibyte(nuri);
  c_free_multibyte(ouri);
  return re;
}

int csync_vio_local_unlink(const char *uri) {
  _TCHAR *nuri = c_multibyte(uri);
  int re = _tunlink( nuri );
  c_free_multibyte(nuri);
  return re;
}

int csync_vio_local_chmod(const char *uri, mode_t mode) {
  _TCHAR *nuri = c_multibyte(uri);
  int re = -1;

  re = _tchmod(nuri, mode);
  c_free_multibyte(nuri);
  return re;
}

int csync_vio_local_chown(const char *uri, uid_t owner, gid_t group) {
#ifdef _WIN32
  return 0;
#else
  return chown(uri, owner, group);
#endif
}

int csync_vio_local_utimes(const char *uri, const struct timeval *times) {
    return c_utimes(uri, times);
}
