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

static csync_vio_method_handle_t *dummy_open(const char *durl, int flags, mode_t mode) {
  (void) durl;
  (void) flags;
  (void) mode;

  return &mh;
}

static csync_vio_method_handle_t *dummy_creat(const char *durl, mode_t mode) {
  (void) durl;
  (void) mode;

  return &mh;
}

static int dummy_close(csync_vio_method_handle_t *fhandle) {
  (void) fhandle;

  return 0;
}

static ssize_t dummy_read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  (void) fhandle;
  (void) buf;
  (void) count;

  return 0;
}

static ssize_t dummy_write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  (void) fhandle;
  (void) buf;
  (void) count;

  return 0;
}

static off_t dummy_lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
  (void) fhandle;
  (void) offset;
  (void) whence;

  return 0;
}

/*
 * directory functions
 */

static csync_vio_method_handle_t *dummy_opendir(const char *name) {
  (void) name;

  return &mh;
}

static int dummy_closedir(csync_vio_method_handle_t *dhandle) {
  (void) dhandle;

  return 0;
}

static csync_vio_file_stat_t *dummy_readdir(csync_vio_method_handle_t *dhandle) {
  (void) dhandle;

  return &fs;
}

static int dummy_mkdir(const char *uri, mode_t mode) {
  (void) uri;
  (void) mode;

  return 0;
}

static int dummy_rmdir(const char *uri) {
  (void) uri;

  return 0;
}

static int dummy_stat(const char *uri, csync_vio_file_stat_t *buf) {
  time_t now;

  buf->name = c_basename(uri);
  if (buf->name == NULL) {
    csync_vio_file_stat_destroy(buf);
    return -1;
  }
  buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  time(&now);
  buf->mtime = now;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  return 0;
}

static int dummy_rename(const char *olduri, const char *newuri) {
  (void) olduri;
  (void) newuri;

  return 0;
}

static int dummy_unlink(const char *uri) {
  (void) uri;

  return 0;
}

static int dummy_chmod(const char *uri, mode_t mode) {
  (void) uri;
  (void) mode;

  return 0;
}

static int dummy_chown(const char *uri, uid_t owner, gid_t group) {
  (void) uri;
  (void) owner;
  (void) group;

  return 0;
}

static int dummy_utimes(const char *uri, const struct timeval *times) {
  (void) uri;
  (void) times;

  return 0;
}

static int dummy_commit() {
  return 0;
}

csync_vio_method_t dummy_method = {
  .method_table_size = sizeof(csync_vio_method_t),
  .open = dummy_open,
  .creat = dummy_creat,
  .close = dummy_close,
  .read = dummy_read,
  .write = dummy_write,
  .lseek = dummy_lseek,
  .opendir = dummy_opendir,
  .closedir = dummy_closedir,
  .readdir = dummy_readdir,
  .mkdir = dummy_mkdir,
  .rmdir = dummy_rmdir,
  .stat = dummy_stat,
  .rename = dummy_rename,
  .unlink = dummy_unlink,
  .chmod = dummy_chmod,
  .chown = dummy_chown,
  .utimes = dummy_utimes,
  .commit = dummy_commit
};

csync_vio_method_t *vio_module_init(const char *method_name, const char *args,
    csync_auth_callback cb, void *userdata) {
  DEBUG_DUMMY(("csync_dummy - method_name: %s\n", method_name));
  DEBUG_DUMMY(("csync_dummy - args: %s\n", args));

  (void) method_name;
  (void) args;
  (void) cb;
  (void) userdata;

  mh = (void *) method_name;
  fs.mtime = 42;

  return &dummy_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
  (void) method;
}

/* vim: set ts=8 sw=2 et cindent: */
