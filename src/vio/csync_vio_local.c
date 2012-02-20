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

#include "c_lib.h"
#include "vio/csync_vio_local.h"

typedef struct fhandle_s {
  int fd;
} fhandle_t;

csync_vio_method_handle_t *csync_vio_local_open(const char *durl, int flags, mode_t mode) {
  fhandle_t *handle = NULL;
  int fd = -1;

  if ((fd = open(durl, flags, mode)) < 0) {
    return NULL;
  }

  handle = c_malloc(sizeof(fhandle_t));
  if (handle == NULL) {
    return NULL;
  }

  handle->fd = fd;
  return (csync_vio_method_handle_t *) handle;
}

csync_vio_method_handle_t *csync_vio_local_creat(const char *durl, mode_t mode) {
  fhandle_t *handle = NULL;
  int fd = -1;

  if ((fd = creat(durl, mode)) < 0) {
    return NULL;
  }

  handle = c_malloc(sizeof(fhandle_t));
  if (handle == NULL) {
    return NULL;
  }

  handle->fd = fd;
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
  DIR *dh;
  char *path;
} dhandle_t;

csync_vio_method_handle_t *csync_vio_local_opendir(const char *name) {
  dhandle_t *handle = NULL;

  handle = c_malloc(sizeof(dhandle_t));
  if (handle == NULL) {
    return NULL;
  }

  handle->dh = opendir(name);
  if (handle->dh == NULL) {
    SAFE_FREE(handle);
    return NULL;
  }
  handle->path = c_strdup(name);

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

  rc = closedir(handle->dh);

  SAFE_FREE(handle->path);
  SAFE_FREE(handle);

  return rc;
}

csync_vio_file_stat_t *csync_vio_local_readdir(csync_vio_method_handle_t *dhandle) {
  struct dirent *dirent = NULL;
  dhandle_t *handle = NULL;
  csync_vio_file_stat_t *file_stat = NULL;

  handle = (dhandle_t *) dhandle;

  errno = 0;
  dirent = readdir(handle->dh);
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

  file_stat->name = c_strdup(dirent->d_name);
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
  return rmdir(uri);
}

int csync_vio_local_stat(const char *uri, csync_vio_file_stat_t *buf) {
  struct stat sb;

  if (lstat(uri, &sb) < 0) {
    return -1;
  }

  buf->name = c_basename(uri);
  if (buf->name == NULL) {
    csync_vio_file_stat_destroy(buf);
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
#ifndef _WIN32
    case S_IFLNK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
#endif
    case S_IFREG:
      buf->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      break;
#ifndef _WIN32
    case S_IFSOCK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
#endif
      break;
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
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_INODE;

  buf->nlink = sb.st_nlink;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_LINK_COUNT;

  buf->uid = sb.st_uid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_UID;

  buf->gid = sb.st_gid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_GID;

  buf->size = sb.st_size;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

#ifndef _WIN32
  buf->blksize = sb.st_blksize;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_SIZE;

  buf->blkcount = sb.st_blocks;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_COUNT;
#endif

  buf->atime = sb.st_atime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;

  buf->mtime = sb.st_mtime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  buf->ctime = sb.st_ctime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_CTIME;

  return 0;
}

int csync_vio_local_rename(const char *olduri, const char *newuri) {
  return rename(olduri, newuri);
}

int csync_vio_local_unlink(const char *uri) {
  return unlink(uri);
}

int csync_vio_local_chmod(const char *uri, mode_t mode) {
  return chmod(uri, mode);
}

int csync_vio_local_chown(const char *uri, uid_t owner, gid_t group) {
#ifndef _WIN32
    return chown(uri, owner, group);
#else
    return 0;
#endif
}

int csync_vio_local_utimes(const char *uri, const struct timeval *times) {
    return c_utimes(uri, times);
}
