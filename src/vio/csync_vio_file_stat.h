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
 * vim: ft=c.doxygen ts=2 sw=2 et cindent
 */

#ifndef _CSYNC_VIO_FILE_STAT_H
#define _CSYNC_VIO_FILE_STAT_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct csync_vio_file_stat_s csync_vio_file_stat_t;

enum csync_vio_file_flags_e {
  CSYNC_VIO_FILE_FLAGS_NONE = 0,
  CSYNC_VIO_FILE_FLAGS_SYMLINK = 1 << 0,
  CSYNC_VIO_FILE_FLAGS_LOCAL = 1 << 1
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
  CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS = 1 << 1,
  CSYNC_VIO_FILE_STAT_FIELDS_FLAGS = 1 << 2,
  CSYNC_VIO_FILE_STAT_FIELDS_DEVICE = 1 << 3,
  CSYNC_VIO_FILE_STAT_FIELDS_INODE = 1 << 4,
  CSYNC_VIO_FILE_STAT_FIELDS_LINK_COUNT = 1 << 5,
  CSYNC_VIO_FILE_STAT_FIELDS_SIZE = 1 << 6,
  CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_COUNT = 1 << 7,
  CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_SIZE = 1 << 8,
  CSYNC_VIO_FILE_STAT_FIELDS_ATIME = 1 << 9,
  CSYNC_VIO_FILE_STAT_FIELDS_MTIME = 1 << 10,
  CSYNC_VIO_FILE_STAT_FIELDS_CTIME = 1 << 11,
  CSYNC_VIO_FILE_STAT_FIELDS_SYMLINK_NAME = 1 << 12,
  CSYNC_VIO_FILE_STAT_FIELDS_CHECKSUM = 1 << 13,
  CSYNC_VIO_FILE_STAT_FIELDS_ACL = 1 << 14,
  CSYNC_VIO_FILE_STAT_FIELDS_UID = 1 << 15,
  CSYNC_VIO_FILE_STAT_FIELDS_GID = 1 << 16,
};


struct csync_vio_file_stat_s {
  union {
    char *symlink_name;
    char *checksum;
  } u;

  void *acl;
  char *name;

  uid_t uid;
  gid_t gid;

  time_t atime;
  time_t mtime;
  time_t ctime;

  off_t size;
  off_t blksize;
  unsigned long blkcount;

  mode_t mode;

  dev_t device;
  ino_t inode;
  nlink_t link_count;

  enum csync_vio_file_stat_fields_e fields;
  enum csync_vio_file_type_e type;

  enum csync_vio_file_flags_e flags;

  void *reserved1;
  void *reserved2;
  void *reserved3;
};

csync_vio_file_stat_t *csync_vio_file_stat_new(void);

void csync_vio_file_stat_destroy(csync_vio_file_stat_t *fstat);

#endif /* _CSYNC_VIO_METHOD_H */
