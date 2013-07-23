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

#include "c_lib.h"
#include "vio/csync_vio_file_stat.h"

csync_vio_file_stat_t *csync_vio_file_stat_new(void) {
  csync_vio_file_stat_t *file_stat = NULL;

  file_stat = (csync_vio_file_stat_t *) c_malloc(sizeof(csync_vio_file_stat_t));
  if (file_stat == NULL) {
    return NULL;
  }

  return file_stat;
}

void csync_vio_file_stat_destroy(csync_vio_file_stat_t *file_stat) {
  /* FIXME: free rest */
  if (file_stat == NULL) {
    return;
  }

  if (file_stat->fields == CSYNC_VIO_FILE_STAT_FIELDS_SYMLINK_NAME) {
    SAFE_FREE(file_stat->u.symlink_name);
  }
  if (file_stat->fields == CSYNC_VIO_FILE_STAT_FIELDS_CHECKSUM) {
    SAFE_FREE(file_stat->u.checksum);
  }

  SAFE_FREE(file_stat->name);
  SAFE_FREE(file_stat);
}

