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
  _TCHAR *wpath = c_multibyte(path);
  _TCHAR *swpath = NULL;
  
  if (path == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (_tstat(wpath, &sb) == 0) {
    if (! S_ISDIR(sb.st_mode)) {
      errno = ENOTDIR;
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
    swpath = c_multibyte(subpath);
    if (_tstat(swpath, &sb) == 0) {
      if (! S_ISDIR(sb.st_mode)) {
        errno = ENOTDIR;
        return -1;
      }
    } else if (errno != ENOENT) {
      c_free_multibyte(swpath);
      return -1;
    } else if (c_mkdirs(subpath, mode) < 0) {
      c_free_multibyte(swpath);
      return -1;
    }
  }
#ifdef _WIN32
  tmp = _tmkdir(wpath);
#else
  tmp = _tmkdir(wpath, mode);
#endif
  c_free_multibyte(swpath);
  c_free_multibyte(wpath);

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
  _TCHAR *wfname = NULL;
  _TCHAR *wpath = c_multibyte(path);
  
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
          return 0;
      }

      while ((dp = _treaddir(d)) != NULL) {
        size_t len;
        /* skip '.' and '..' */
        if (dp->d_name[0] == '.' &&
            (dp->d_name[1] == '\0' ||
             (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))) {
          continue;
        }

        len = strlen(path) + _tcslen(dp->d_name) + 2;
        fname = c_malloc(len);
        if (fname == NULL) {
          _tclosedir(d);
          return -1;
        }
        snprintf(fname, len, "%s/%s", path, dp->d_name);
	wfname = c_multibyte(fname);
	
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
		c_free_multibyte(wfname);
                return -1;
              }
              c_rmdirs(fname);
            }
          } else {
            _tunlink(wfname);
          }
        } /* lstat */
        SAFE_FREE(fname);
	c_free_multibyte(wfname);
      } /* readdir */

      _trewinddir(d);
    }
  } else {
    return -1;
  }

  _tclosedir(d);
  return 0;
}

int c_isdir(const char *path) {
  csync_stat_t sb;
  _TCHAR *wpath = c_multibyte(path);

  if (_tstat (wpath, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    c_free_multibyte(wpath);
    return 1;
  }
  c_free_multibyte(wpath);
  return 0;
}

