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

#ifndef _CSYNC_VIO_FILE_STAT_H
#define _CSYNC_VIO_FILE_STAT_H

/*
 * cannot include csync_private here because
 * that would cause circular inclusion
 */
#include "std/c_private.h"
#include "csync.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#define FILE_ID_BUF_SIZE 21

// currently specified at https://github.com/owncloud/core/issues/8322 are 9 to 10
#define REMOTE_PERM_BUF_SIZE 15

typedef struct csync_vio_file_stat_s csync_vio_file_stat_t;

enum csync_vio_file_flags_e {
  CSYNC_VIO_FILE_FLAGS_NONE = 0,
  CSYNC_VIO_FILE_FLAGS_SYMLINK = 1 << 0,
  CSYNC_VIO_FILE_FLAGS_HIDDEN = 1 << 1
};

enum csync_vio_file_type_e {
  CSYNC_VIO_FILE_TYPE_UNKNOWN,
  CSYNC_VIO_FILE_TYPE_REGULAR,
  CSYNC_VIO_FILE_TYPE_DIRECTORY,
  CSYNC_VIO_FILE_TYPE_FIFO,
  CSYNC_VIO_FILE_TYPE_SOCKET,
  CSYNC_VIO_FILE_TYPE_CHARACTER_DEVICE,
  CSYNC_VIO_FILE_TYPE_BLOCK_DEVICE,
  CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK
};

enum csync_vio_file_stat_fields_e {
  CSYNC_VIO_FILE_STAT_FIELDS_NONE = 0,
  CSYNC_VIO_FILE_STAT_FIELDS_TYPE = 1 << 0,
  CSYNC_VIO_FILE_STAT_FIELDS_MODE = 1 << 1, // local POSIX mode
  CSYNC_VIO_FILE_STAT_FIELDS_FLAGS = 1 << 2,
  CSYNC_VIO_FILE_STAT_FIELDS_DEVICE = 1 << 3,
  CSYNC_VIO_FILE_STAT_FIELDS_INODE = 1 << 4,
  CSYNC_VIO_FILE_STAT_FIELDS_LINK_COUNT = 1 << 5,
  CSYNC_VIO_FILE_STAT_FIELDS_SIZE = 1 << 6,
//  CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_COUNT = 1 << 7, /* will be removed */
//  CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_SIZE = 1 << 8,  /* will be removed */
  CSYNC_VIO_FILE_STAT_FIELDS_ATIME = 1 << 9,
  CSYNC_VIO_FILE_STAT_FIELDS_MTIME = 1 << 10,
  CSYNC_VIO_FILE_STAT_FIELDS_CTIME = 1 << 11,
//  CSYNC_VIO_FILE_STAT_FIELDS_SYMLINK_NAME = 1 << 12,
//  CSYNC_VIO_FILE_STAT_FIELDS_CHECKSUM = 1 << 13,
//  CSYNC_VIO_FILE_STAT_FIELDS_ACL = 1 << 14,
//  CSYNC_VIO_FILE_STAT_FIELDS_UID = 1 << 15,
//  CSYNC_VIO_FILE_STAT_FIELDS_GID = 1 << 16,
  CSYNC_VIO_FILE_STAT_FIELDS_ETAG = 1 << 17,
  CSYNC_VIO_FILE_STAT_FIELDS_FILE_ID = 1 << 18,
  CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADURL = 1 << 19,
  CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADCOOKIES = 1 << 20,
  CSYNC_VIO_FILE_STAT_FIELDS_PERM = 1 << 21 // remote oC perm

};


struct csync_vio_file_stat_s {
  char *name;
  char *etag; // FIXME: Should this be inlined like file_id and perm?
  char file_id[FILE_ID_BUF_SIZE+1];
  char *directDownloadUrl;
  char *directDownloadCookies;
  char remotePerm[REMOTE_PERM_BUF_SIZE+1];

  time_t atime;
  time_t mtime;
  time_t ctime;

  int64_t size;

  mode_t mode;

  dev_t device;
  uint64_t inode;
  nlink_t nlink;

  enum csync_vio_file_stat_fields_e fields;
  enum csync_vio_file_type_e type;

  enum csync_vio_file_flags_e flags;
};

csync_vio_file_stat_t *csync_vio_file_stat_new(void);

void csync_vio_file_stat_destroy(csync_vio_file_stat_t *fstat);

void csync_vio_file_stat_set_file_id( csync_vio_file_stat_t* dst, const char* src );

void csync_vio_set_file_id(char* dst, const char *src );

#endif /* _CSYNC_VIO_METHOD_H */
