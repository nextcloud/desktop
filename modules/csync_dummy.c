/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008      by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software = NULL, you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation = NULL, either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY = NULL, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program = NULL, if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * vim: ts=2 sw=2 et cindent
 */

#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "c_lib.h"
#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#ifdef NDEBUG
#define DEBUG_DUMMY(x)
#else
#define DEBUG_DUMMY(x) printf x
#endif

csync_vio_method_handle_t *mh = NULL;
csync_vio_file_stat_t fs;

/*
 * file functions
 */

static csync_vio_method_handle_t *_open(const char *durl, int flags, mode_t mode) {
  (void) durl;
  (void) flags;
  (void) mode;

  return &mh;
}

static csync_vio_method_handle_t *_creat(const char *durl, mode_t mode) {
  (void) durl;
  (void) mode;

  return &mh;
}

static int _close(csync_vio_method_handle_t *fhandle) {
  (void) fhandle;

  return 0;
}

static ssize_t _read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  (void) fhandle;
  (void) buf;
  (void) count;

  return 0;
}

static ssize_t _write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  (void) fhandle;
  (void) buf;
  (void) count;

  return 0;
}

static off_t _lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
  (void) fhandle;
  (void) offset;
  (void) whence;

  return 0;
}

/*
 * directory functions
 */

static csync_vio_method_handle_t *_opendir(const char *name) {
  (void) name;

  return &mh;
}

static int _closedir(csync_vio_method_handle_t *dhandle) {
  (void) dhandle;

  return 0;
}

static csync_vio_file_stat_t *_readdir(csync_vio_method_handle_t *dhandle) {
  (void) dhandle;

  return &fs;
}

static int _mkdir(const char *uri, mode_t mode) {
  (void) uri;
  (void) mode;

  return 0;
}

static int _rmdir(const char *uri) {
  (void) uri;

  return 0;
}

static int _stat(const char *uri, csync_vio_file_stat_t *buf) {
  time_t now;

  buf->name = c_basename(uri);
  if (buf->name == NULL) {
    csync_vio_file_stat_destroy(buf);
  }
  buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  time(&now);
  buf->mtime = now;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  return 0;
}

static int _rename(const char *olduri, const char *newuri) {
  (void) olduri;
  (void) newuri;

  return 0;
}

static int _unlink(const char *uri) {
  (void) uri;

  return 0;
}

static int _chmod(const char *uri, mode_t mode) {
  (void) uri;
  (void) mode;

  return 0;
}

static int _chown(const char *uri, uid_t owner, gid_t group) {
  (void) uri;
  (void) owner;
  (void) group;

  return 0;
}

static int _utimes(const char *uri, const struct timeval *times) {
  (void) uri;
  (void) times;

  return 0;
}

csync_vio_method_t _method = {
  .method_table_size = sizeof(csync_vio_method_t),
  .open = _open,
  .creat = _creat,
  .close = _close,
  .read = _read,
  .write = _write,
  .lseek = _lseek,
  .opendir = _opendir,
  .closedir = _closedir,
  .readdir = _readdir,
  .mkdir = _mkdir,
  .rmdir = _rmdir,
  .stat = _stat,
  .rename = _rename,
  .unlink = _unlink,
  .chmod = _chmod,
  .chown = _chown,
  .utimes = _utimes
};

csync_vio_method_t *vio_module_init(const char *method_name, const char *args, csync_auth_callback cb) {
  DEBUG_DUMMY(("csync_dummy - method_name: %s\n", method_name));
  DEBUG_DUMMY(("csync_dummy - args: %s\n", args));

  (void) method_name;
  (void) args;
  (void) cb;

  mh = (void *) method_name;
  fs.mtime = 42;

  return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
  (void) method;
}

