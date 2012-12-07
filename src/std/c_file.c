/*
 * cynapses libc functions
 *
 * Copyright (c) 2008 by Andreas Schneider <mail@cynapses.org>
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
 * vim: ts=2 sw=2 et cindent
 */

#ifdef _WIN32
#include <windef.h>
#include <winbase.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "c_file.h"
#include "c_string.h"

#include "c_private.h"

/* check if path is a file */
int c_isfile(const char *path) {
  csync_stat_t sb;
  _TCHAR *wpath = c_multibyte(path);
  int re = _tstat(wpath, &sb);
  c_free_multibyte(wpath);
  
  if (re< 0) {
    return 0;
  }

#ifdef __unix__
  if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode)) {
#else
  if (S_ISREG(sb.st_mode)) {
#endif
    return 1;
  }

  return 0;
}

/* copy file from src to dst, overwrites dst */
int c_copy(const char* src, const char *dst, mode_t mode) {
  int srcfd = -1;
  int dstfd = -1;
  int rc = -1;
  ssize_t bread, bwritten;
  csync_stat_t sb;
  char buf[4096];

#ifdef _WIN32
  if(src && dst) {
      _TCHAR *wsrc = c_multibyte(src);
      _TCHAR *wdst = c_multibyte(dst);
      if (CopyFileW(wsrc, wdst, FALSE)) {
          rc = 0;
      }
      c_free_multibyte(wsrc);
      c_free_multibyte(wdst);

      if( rc < 0 ) {
          errno = GetLastError();
      }
  }
  return rc;
#else

  /* Win32 does not come here. */
  if (c_streq(src, dst)) {
    return -1;
  }

  if (lstat(src, &sb) < 0) {
    return -1;
  }

  if (S_ISDIR(sb.st_mode)) {
    errno = EISDIR;
    return -1;
  }

  if (mode == 0) {
    mode = sb.st_mode;
  }

  if (lstat(dst, &sb) == 0) {
    if (S_ISDIR(sb.st_mode)) {
      errno = EISDIR;
      return -1;
    }
  }

  if ((srcfd = open(src, O_RDONLY, 0)) < 0) {
    rc = -1;
    goto out;
  }

  if ((dstfd = open(dst, O_CREAT|O_WRONLY|O_TRUNC, mode)) < 0) {
    rc = -1;
    goto out;
  }

  for (;;) {
    bread = read(srcfd, buf, sizeof(buf));
    if (bread == 0) {
      /* done */
      break;
    } else if (bread < 0) {
      errno = ENODATA;
      rc = -1;
      goto out;
    }

    bwritten = write(dstfd, buf, bread);
    if (bwritten < 0) {
      errno = ENODATA;
      rc = -1;
      goto out;
    }

    if (bread != bwritten) {
      errno = EFAULT;
      rc = -1;
      goto out;
    }
  }

#ifdef __unix__
  fsync(dstfd);
#endif

  rc = 0;
out:
  if (srcfd > 0) {
    close(srcfd);
  }
  if (dstfd > 0) {
      close(dstfd);
  }
  if (rc < 0) {
    unlink(dst);
  }
  return rc;
#endif
}

