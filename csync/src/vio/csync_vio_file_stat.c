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
  file_stat->etag = NULL;
  memset(file_stat->file_id, 0, FILE_ID_BUF_SIZE+1);
  return file_stat;
}

void csync_vio_file_stat_destroy(csync_vio_file_stat_t *file_stat) {
  /* FIXME: free rest */
  if (file_stat == NULL) {
    return;
  }

  if (file_stat->fields & CSYNC_VIO_FILE_STAT_FIELDS_SYMLINK_NAME) {
    SAFE_FREE(file_stat->u.symlink_name);
  }
  if (file_stat->fields & CSYNC_VIO_FILE_STAT_FIELDS_CHECKSUM) {
    SAFE_FREE(file_stat->u.checksum);
  }
  if (file_stat->fields & CSYNC_VIO_FILE_STAT_FIELDS_ETAG) {
    SAFE_FREE(file_stat->etag);
  }
  SAFE_FREE(file_stat->name);
  SAFE_FREE(file_stat);
}

void csync_vio_file_stat_set_file_id( csync_vio_file_stat_t *dst, const char* src ) {

    csync_vio_set_file_id( dst->file_id, src );
    if( c_streq( dst->file_id, "" )) {
        return;
    }
    dst->fields |= CSYNC_VIO_FILE_STAT_FIELDS_FILE_ID;
}

void csync_vio_set_file_id( char* dst, const char *src ) {
    if( src && dst ) {
        if( strlen(src) > FILE_ID_BUF_SIZE ) {
            strcpy(dst, "");
        } else {
            strcpy(dst, src);
        }
    }
}
