#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_macro.h"
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

  tmp = mkdir(path, mode);
  if ((tmp < 0) && (errno == EEXIST)) {
    return 0;
  }

  return tmp;
}

int c_isdir(const char *path) {
  struct stat sb;

  if (lstat (path, &sb) < 0) {
    return 0;
  }
  if (S_ISDIR (sb.st_mode)) {
    return 1;
  }

  return 0;
}

