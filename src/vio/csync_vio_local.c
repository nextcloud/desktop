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
#define _UNICODE
#endif

#include "c_private.h"
#include "c_lib.h"
#include "c_string.h"
#include "csync_util.h"
#include "csync_log.h"
#include "csync_vio.h"

#include "vio/csync_vio_handle.h"
#include "vio/csync_vio_local.h"
#include "vio/csync_vio_handle_private.h"


int csync_vio_local_getfd(csync_vio_handle_t *hnd)
{
    fhandle_t *fh;

    if (hnd == NULL) {
        return -1;
    }

    fh = (struct fhandle_s*)(hnd);

    return fh->fd;
}

/* the url comes in as utf-8 and in windows, it needs to be multibyte. */
csync_vio_method_handle_t *csync_vio_local_open(const char *durl, int flags, mode_t mode) {
  fhandle_t *handle = NULL;
  int fd = -1;
  mbchar_t *url = c_utf8_to_locale(durl);

  if ((fd = _topen(url, flags, mode)) < 0) {
    c_free_locale_string(url);
    return NULL;
  }

  handle = c_malloc(sizeof(fhandle_t));
  if (handle == NULL) {
    c_free_locale_string(url);
    close(fd);
    return NULL;
  }

  handle->fd = fd;

  c_free_locale_string(url);

  return (csync_vio_method_handle_t *) handle;
}

csync_vio_method_handle_t *csync_vio_local_creat(const char *durl, mode_t mode) {
  fhandle_t *handle = NULL;
  int fd = -1;
  mbchar_t *url = c_utf8_to_locale(durl);

  if(( fd = _tcreat( url, mode)) < 0) {
      c_free_locale_string(url);
      return NULL;
  }

  handle = c_malloc(sizeof(fhandle_t));
  if (handle == NULL) {
    c_free_locale_string(url);
    close(fd);
    return NULL;
  }

  handle->fd = fd;
  c_free_locale_string(url);
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

int64_t csync_vio_local_lseek(csync_vio_method_handle_t *fhandle, int64_t offset, int whence) {
  fhandle_t *handle = NULL;

  if (fhandle == NULL) {
    return (int64_t) -1;
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
  mbchar_t *dirname = c_utf8_to_locale(name);

  handle = c_malloc(sizeof(dhandle_t));
  if (handle == NULL) {
    c_free_locale_string(dirname);
    return NULL;
  }

  handle->dh = _topendir( dirname );
  if (handle->dh == NULL) {
    c_free_locale_string(dirname);
    SAFE_FREE(handle);
    return NULL;
  }

  handle->path = c_strdup(name);
  c_free_locale_string(dirname);

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

  file_stat->name = c_utf8_from_locale(dirent->d_name);
  file_stat->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

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

  return file_stat;

err:
  SAFE_FREE(file_stat);

  return NULL;
}

int csync_vio_local_mkdir(const char *uri, mode_t mode) {
  return c_mkdirs(uri, mode);
}

int csync_vio_local_rmdir(const char *uri) {
  mbchar_t *dirname = c_utf8_to_locale(uri);
  int re = -1;

  re = _trmdir(dirname);
  c_free_locale_string(dirname);
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
#ifdef _WIN32
  HANDLE h;
#endif
  mbchar_t *wuri = c_utf8_to_locale( uri );

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
  h = CreateFileW( wuri, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL+FILE_FLAG_BACKUP_SEMANTICS, NULL );
  if( h == INVALID_HANDLE_VALUE ) {
     errno = GetLastError();
     c_free_locale_string(wuri);
     return -1;

  } else {
     FILETIME ftCreate, ftAccess, ftWrite;
//     SYSTEMTIME stUTC;

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

  /* check if it is a symlink on win32 */
  if (c_islink(uri)) {
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
  }
#else /* non windows platforms: */

  /* Both values are only initialized to zero as they are not used in csync */
  /* They are deprecated and will be rmemoved later. */
  buf->blksize  = 0;
  buf->blkcount = 0;

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

  c_free_locale_string(wuri);
  return 0;
}

int csync_vio_local_rename(const char *olduri, const char *newuri) {
  return c_rename(olduri, newuri);
}

int csync_vio_local_unlink(const char *uri) {
  mbchar_t *nuri = c_utf8_to_locale(uri);
  int re = _tunlink( nuri );
  c_free_locale_string(nuri);
  return re;
}

int csync_vio_local_chmod(const char *uri, mode_t mode) {
  mbchar_t *nuri = c_utf8_to_locale(uri);
  int re = -1;

  re = _tchmod(nuri, mode);
  c_free_locale_string(nuri);
  return re;
}

int csync_vio_local_chown(const char *uri, uid_t owner, gid_t group) {
#if defined _WIN32
    (void)uri;
    (void)owner;
    (void)group;
#endif
  return _tchown(uri, owner, group);
}

int csync_vio_local_utimes(const char *uri, const struct timeval *times) {
    return c_utimes(uri, times);
}
