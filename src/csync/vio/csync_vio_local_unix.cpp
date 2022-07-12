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
#include "csync.h"

#include "vio/csync_vio_local.h"
#include "common/vfs.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QFile>

Q_LOGGING_CATEGORY(lcCSyncVIOLocal, "sync.csync.vio_local", QtInfoMsg)

/*
 * directory functions
 */

struct csync_vio_handle_t {
  DIR *dh;
  QByteArray path;
};

static int _csync_vio_local_stat_mb(const mbchar_t *wuri, csync_file_stat_t *buf);

csync_vio_handle_t *csync_vio_local_opendir(const QString &name) {
    QScopedPointer<csync_vio_handle_t> handle(new csync_vio_handle_t{});

    auto dirname = QFile::encodeName(name);

    handle->dh = _topendir(dirname.constData());
    if (!handle->dh) {
        return nullptr;
    }

    handle->path = dirname;
    return handle.take();
}

int csync_vio_local_closedir(csync_vio_handle_t *dhandle) {
    Q_ASSERT(dhandle);
    auto rc = _tclosedir(dhandle->dh);
    delete dhandle;
    return rc;
}

std::unique_ptr<csync_file_stat_t> csync_vio_local_readdir(csync_vio_handle_t *handle, OCC::Vfs *vfs) {

  struct _tdirent *dirent = nullptr;
  std::unique_ptr<csync_file_stat_t> file_stat;

  do {
      dirent = _treaddir(handle->dh);
      if (dirent == nullptr)
          return {};
  } while (qstrcmp(dirent->d_name, ".") == 0 || qstrcmp(dirent->d_name, "..") == 0);

  file_stat.reset(new csync_file_stat_t);
  file_stat->path = QFile::decodeName(dirent->d_name).toUtf8();
  QByteArray fullPath = handle->path % '/' % QByteArray() % const_cast<const char *>(dirent->d_name);

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

  // Override type for virtual files if desired
  if (vfs) {
      // Directly modifies file_stat->type.
      // We can ignore the return value since we're done here anyway.
      vfs->statTypeVirtualFile(file_stat.get(), nullptr);
  }

  return file_stat;
}


int csync_vio_local_stat(const QString &uri, csync_file_stat_t *buf)
{
    return _csync_vio_local_stat_mb(QFile::encodeName(uri).constData(), buf);
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
