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
#include "csync.h"
#include "csync_log.h"

csync_vio_file_stat_t *csync_vio_file_stat_new(void) {
  csync_vio_file_stat_t *file_stat = (csync_vio_file_stat_t *) c_malloc(sizeof(csync_vio_file_stat_t));
  ZERO_STRUCTP(file_stat);
  return file_stat;
}

csync_vio_file_stat_t* csync_vio_file_stat_copy(csync_vio_file_stat_t *file_stat) {
    csync_vio_file_stat_t *file_stat_cpy = csync_vio_file_stat_new();
    memcpy(file_stat_cpy, file_stat, sizeof(csync_vio_file_stat_t));
    if (file_stat_cpy->fields & CSYNC_VIO_FILE_STAT_FIELDS_ETAG) {
        file_stat_cpy->etag = c_strdup(file_stat_cpy->etag);
    }
    if (file_stat_cpy->directDownloadCookies) {
        file_stat_cpy->directDownloadCookies = c_strdup(file_stat_cpy->directDownloadCookies);
    }
    if (file_stat_cpy->directDownloadUrl) {
        file_stat_cpy->directDownloadUrl = c_strdup(file_stat_cpy->directDownloadUrl);
    }
    if (file_stat_cpy->checksumHeader) {
        file_stat_cpy->checksumHeader = c_strdup(file_stat_cpy->checksumHeader);
    }
    file_stat_cpy->name = c_strdup(file_stat_cpy->name);
    return file_stat_cpy;
}

void csync_vio_file_stat_destroy(csync_vio_file_stat_t *file_stat) {
  /* FIXME: free rest */
  if (file_stat == NULL) {
    return;
  }

  if (file_stat->fields & CSYNC_VIO_FILE_STAT_FIELDS_ETAG) {
    SAFE_FREE(file_stat->etag);
  }
  SAFE_FREE(file_stat->directDownloadUrl);
  SAFE_FREE(file_stat->directDownloadCookies);
  SAFE_FREE(file_stat->name);
  SAFE_FREE(file_stat->original_name);
  SAFE_FREE(file_stat->checksumHeader);
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
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Ignoring file_id because it is too long: %s", src);
            strcpy(dst, "");
        } else {
            strcpy(dst, src);
        }
    }
}
