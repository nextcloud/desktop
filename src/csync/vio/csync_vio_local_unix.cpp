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
#include <dirent.h>
#include <stdio.h>

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
  DIR *dh;
  char *path;
} dhandle_t;

csync_vio_handle_t *csync_vio_local_opendir(const char *name) {
  dhandle_t *handle = NULL;
  mbchar_t *dirname = NULL;

  handle = (dhandle_t*)c_malloc(sizeof(dhandle_t));

  dirname = c_utf8_path_to_locale(name);

  handle->dh = _topendir( dirname );
  if (handle->dh == NULL) {
    c_free_locale_string(dirname);
    SAFE_FREE(handle);
    return NULL;
  }

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
  rc = _tclosedir(handle->dh);

  SAFE_FREE(handle->path);
  SAFE_FREE(handle);

  return rc;
}

csync_vio_file_stat_t *csync_vio_local_readdir(csync_vio_handle_t *dhandle) {

  dhandle_t *handle = NULL;
  csync_vio_file_stat_t *file_stat = NULL;

  handle = (dhandle_t *) dhandle;
  struct _tdirent *dirent = NULL;

  errno = 0;
  file_stat = csync_vio_file_stat_new();
  if (file_stat == NULL) {
    goto err;
  }
  file_stat->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  dirent = _treaddir(handle->dh);
  if (dirent == NULL) {
      goto err;
  }
  file_stat->name = c_utf8_from_locale(dirent->d_name);
  if (file_stat->name == NULL) {
      //file_stat->original_name = c_strdup(dirent->d_name);
      if (asprintf(&file_stat->original_name, "%s/%s", handle->path, dirent->d_name) < 0) {
          goto err;
      }
      CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Invalid characters in file/directory name, please rename: \"%s\" (%s)",
                dirent->d_name, handle->path);
  }

  /* Check for availability of d_type, see manpage. */
#if defined(_DIRENT_HAVE_D_TYPE) || defined(__APPLE__)
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
  csync_vio_file_stat_destroy(file_stat);

  return NULL;
}


int csync_vio_local_stat(const char *uri, csync_vio_file_stat_t *buf) {
  csync_stat_t sb;

  mbchar_t *wuri = c_utf8_path_to_locale( uri );

  if( _tstat(wuri, &sb) < 0) {
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
