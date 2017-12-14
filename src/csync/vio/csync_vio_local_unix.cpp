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
#include "c_utf8.h"
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

static int _csync_vio_local_stat_mb(const mbchar_t *wuri, csync_file_stat_t *buf);

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

std::unique_ptr<csync_file_stat_t> csync_vio_local_readdir(csync_vio_handle_t *dhandle) {

  dhandle_t *handle = NULL;

  handle = (dhandle_t *) dhandle;
  struct _tdirent *dirent = NULL;
  std::unique_ptr<csync_file_stat_t> file_stat;

  do {
      dirent = _treaddir(handle->dh);
      if (dirent == NULL)
          return {};
  } while (qstrcmp(dirent->d_name, ".") == 0 || qstrcmp(dirent->d_name, "..") == 0);

  file_stat.reset(new csync_file_stat_t);
  file_stat->path = c_utf8_from_locale(dirent->d_name);
  QByteArray fullPath = QByteArray() % const_cast<const char *>(handle->path) % '/' % QByteArray() % const_cast<const char *>(dirent->d_name);
  if (file_stat->path.isNull()) {
      file_stat->original_path = fullPath;
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
      if (dirent->d_type == DT_DIR) {
        file_stat->type = ItemTypeDirectory;
      } else {
        file_stat->type = ItemTypeFile;
      }
      break;
    default:
      break;
  }
#endif

  if (file_stat->path.isNull())
      return file_stat;

  if (_csync_vio_local_stat_mb(fullPath.constData(), file_stat.get()) < 0) {
      // Will get excluded by _csync_detect_update.
      file_stat->type = ItemTypeSkip;
  }
  return file_stat;
}


int csync_vio_local_stat(const char *uri, csync_file_stat_t *buf)
{
    mbchar_t *wuri = c_utf8_path_to_locale(uri);
    *buf = csync_file_stat_t();
    int rc = _csync_vio_local_stat_mb(wuri, buf);
    c_free_locale_string(wuri);
    return rc;
}

static int _csync_vio_local_stat_mb(const mbchar_t *wuri, csync_file_stat_t *buf)
{
    csync_stat_t sb;

    if (_tstat(wuri, &sb) < 0) {
        return -1;
    }

    switch (sb.st_mode & S_IFMT) {
    case S_IFDIR:
      buf->type = ItemTypeDirectory;
      break;
    case S_IFREG:
      buf->type = ItemTypeFile;
      break;
    case S_IFLNK:
    case S_IFSOCK:
      buf->type = ItemTypeSoftLink;
      break;
    default:
      buf->type = ItemTypeSkip;
      break;
  }

#ifdef __APPLE__
  if (sb.st_flags & UF_HIDDEN) {
      buf->is_hidden = true;
  }
#endif

  buf->inode = sb.st_ino;
  buf->modtime = sb.st_mtime;
  buf->size = sb.st_size;
  return 0;
}
