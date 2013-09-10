/*
 * cynapses libc functions
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

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_private.h"
#include "c_macro.h"
#include "c_alloc.h"
#include "c_dir.h"
#include "c_string.h"

int c_mkdirs(const char *path, mode_t mode) {
  int tmp;
  csync_stat_t sb;
  mbchar_t *wpath = c_utf8_to_locale(path);
  mbchar_t *swpath = NULL;

  if (path == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (_tstat(wpath, &sb) == 0) {
    if (! S_ISDIR(sb.st_mode)) {
      errno = ENOTDIR;
      c_free_locale_string(wpath);
      return -1;
    }
  }
  
  tmp = strlen(path);
  while(tmp > 0 && path[tmp - 1] == '/') --tmp;
  while(tmp > 0 && path[tmp - 1] != '/') --tmp;
  while(tmp > 0 && path[tmp - 1] == '/') --tmp;

  if (tmp > 0) {
    char subpath[tmp + 1];
    memcpy(subpath, path, tmp);
    subpath[tmp] = '\0';
    swpath = c_utf8_to_locale(subpath);
    if (_tstat(swpath, &sb) == 0) {
      if (! S_ISDIR(sb.st_mode)) {
        c_free_locale_string(swpath);
        c_free_locale_string(wpath);
        errno = ENOTDIR;
        return -1;
      }
    } else if (errno != ENOENT) {
      c_free_locale_string(wpath);
      c_free_locale_string(swpath);
      return -1;
    } else if (c_mkdirs(subpath, mode) < 0) {
      c_free_locale_string(swpath);
      c_free_locale_string(wpath);
      return -1;
    }
  }
  tmp = _tmkdir(wpath, mode);

  c_free_locale_string(swpath);
  c_free_locale_string(wpath);

  if ((tmp < 0) && (errno == EEXIST)) {
    return 0;
  }
  return tmp;
}

int c_rmdirs(const char *path) {
  _TDIR *d;
  struct _tdirent *dp;
  csync_stat_t sb;
  char *fname = NULL;
  mbchar_t *wfname = NULL;
  mbchar_t *wpath = c_utf8_to_locale(path);
  char *rd_name = NULL;

  if ((d = _topendir(wpath)) != NULL) {
    while( _tstat(wpath, &sb) == 0) {
      /* if we can remove the directory we're done */
      if (_trmdir(wpath) == 0) {
        break;
      }
      switch (errno) {
        case ENOTEMPTY:
        case EEXIST:
        case EBADF:
          break; /* continue */
        default:
          _tclosedir(d);
           c_free_locale_string(wpath);
          return 0;
      }

      while ((dp = _treaddir(d)) != NULL) {
        size_t len;
        rd_name = c_utf8_from_locale(dp->d_name);
            /* skip '.' and '..' */
        if( c_streq( rd_name, "." ) || c_streq( rd_name, ".." ) ) {
            c_free_locale_string(rd_name);
            continue;
        }

        len = strlen(path) + strlen(rd_name) + 2;
        fname = c_malloc(len);
        if (fname == NULL) {
          _tclosedir(d);
	  c_free_locale_string(rd_name);
	  c_free_locale_string(wpath);
          return -1;
        }
        snprintf(fname, len, "%s/%s", path, rd_name);
	wfname = c_utf8_to_locale(fname);

        /* stat the file */
        if (_tstat(wfname, &sb) != -1) {
#ifdef __unix__
          if (S_ISDIR(sb.st_mode) && !S_ISLNK(sb.st_mode)) {
#else
          if (S_ISDIR(sb.st_mode)) {
#endif
            if (_trmdir(wfname) < 0) { /* can't be deleted */
              if (errno == EACCES) {
                _tclosedir(d);
                SAFE_FREE(fname);
		c_free_locale_string(wfname);
		c_free_locale_string(rd_name);
		c_free_locale_string(wpath);
                return -1;
              }
              c_rmdirs(fname);
            }
          } else {
            _tunlink(wfname);
          }
        } /* lstat */
        SAFE_FREE(fname);
	c_free_locale_string(wfname);
	c_free_locale_string(rd_name);
      } /* readdir */

      _trewinddir(d);
    }
  } else {
    c_free_locale_string(wpath);
    return -1;
  }
  c_free_locale_string(wpath);
  _tclosedir(d);
  return 0;
}

int c_isdir(const char *path) {
  csync_stat_t sb;
  mbchar_t *wpath = c_utf8_to_locale(path);
  int re = 0;

  if (path != NULL) {
      if (_tstat (wpath, &sb) == 0 && S_ISDIR(sb.st_mode)) {
          re = 1;
      }
  }
  c_free_locale_string(wpath);
  return re;
}

