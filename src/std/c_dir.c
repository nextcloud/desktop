#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_macro.h"
#include "c_alloc.h"
#include "c_dir.h"

int c_mkdirs(const char *path, mode_t mode) {
  int tmp;
  struct stat sb;

  if (path == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (lstat(path, &sb) == 0) {
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

    if (lstat(subpath, &sb) == 0) {
      if (! S_ISDIR(sb.st_mode)) {
        errno = ENOTDIR;
        return -1;
      }
    } else if (errno != ENOENT) {
      return -1;
    } else if (c_mkdirs(subpath, mode) < 0) {
      return -1;
    }
  }

#ifndef _WIN32
  tmp = mkdir(path, mode);
#else
  tmp = mkdir(path);
#endif
  if ((tmp < 0) && (errno == EEXIST)) {
    return 0;
  }

  return tmp;
}

int c_rmdirs(const char *path) {
  DIR *d;
  struct dirent *dp;
  struct stat sb;
  char *fname;

  if ((d = opendir(path)) != NULL) {
    while(stat(path, &sb) == 0) {
      /* if we can remove the directory we're done */
      if (rmdir(path) == 0) {
        break;
      }
      switch (errno) {
        case ENOTEMPTY:
        case EEXIST:
        case EBADF:
          break; /* continue */
        default:
          closedir(d);
          return 0;
      }

      while ((dp = readdir(d)) != NULL) {
        size_t len;
        /* skip '.' and '..' */
        if (dp->d_name[0] == '.' &&
            (dp->d_name[1] == '\0' ||
             (dp->d_name[1] == '.' && dp->d_name[2] == '\0'))) {
          continue;
        }

        len = strlen(path) + strlen(dp->d_name) + 2;
        fname = c_malloc(len);
        if (fname == NULL) {
          return -1;
        }
        snprintf(fname, len, "%s/%s", path, dp->d_name);

        /* stat the file */
        if (lstat(fname, &sb) != -1) {
          if (S_ISDIR(sb.st_mode) && !S_ISLNK(sb.st_mode)) {
            if (rmdir(fname) < 0) { /* can't be deleted */
              if (errno == EACCES) {
                closedir(d);
                SAFE_FREE(fname);
                return -1;
              }
              c_rmdirs(fname);
            }
          } else {
            unlink(fname);
          }
        } /* lstat */
        SAFE_FREE(fname);
      } /* readdir */

      rewinddir(d);
    }
  } else {
    return -1;
  }

  closedir(d);
  return 0;
}

int c_isdir(const char *path) {
  struct stat sb;

  if (lstat (path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    return 1;
  }

  return 0;
}

