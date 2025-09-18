/*
 * libcsync -- a library to sync a directory with another
 *
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-FileCopyrightText: 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstdio>

#include <memory>

#include "c_private.h"
#include "c_lib.h"
#include "csync.h"

#include "vio/csync_vio_local.h"
#include "common/vfs.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QFile>

Q_LOGGING_CATEGORY(lcCSyncVIOLocal, "nextcloud.sync.csync.vio_local", QtInfoMsg)

/*
 * directory functions
 */

struct csync_vio_handle_t {
  DIR *dh;
  QByteArray path;
};

static int _csync_vio_local_stat_mb(const mbchar_t *wuri, csync_file_stat_t *buf);

csync_vio_handle_t *csync_vio_local_opendir(const QString &name) {
    auto handle = std::make_unique<csync_vio_handle_t>();
    auto dirname = QFile::encodeName(name);

    handle->dh = _topendir(dirname.constData());
    if (!handle->dh) {
        return nullptr;
    }

    handle->path = dirname;
    return handle.release();
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
      if (!dirent)
          return {};
  } while (qstrcmp(dirent->d_name, ".") == 0 || qstrcmp(dirent->d_name, "..") == 0);

  file_stat = std::make_unique<csync_file_stat_t>();
  file_stat->path = QFile::decodeName(dirent->d_name).toUtf8();
  QByteArray fullPath = handle->path % '/' % QByteArray() % const_cast<const char *>(dirent->d_name);
  if (file_stat->path.isNull()) {
      file_stat->original_path = fullPath;
      qCWarning(lcCSyncVIOLocal) << "Invalid characters in file/directory name, please rename:" << dirent->d_name << handle->path;
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

  // Override type for virtual files if desired
  if (vfs) {
      // Directly modifies file_stat->type.
      // We can ignore the return value since we're done here anyway.
      const auto result = vfs->statTypeVirtualFile(file_stat.get(), &handle->path);
      Q_UNUSED(result)
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
  buf->isPermissionsInvalid = (sb.st_mode & S_IWOTH) == S_IWOTH;

  return 0;
}
