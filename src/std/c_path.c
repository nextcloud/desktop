/*
 * cynapses libc functions
 *
 * Copyright (c) 2007-2008 by Andreas Schneider <mail@cynapses.org>
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
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include "c_alloc.h"
#include "c_path.h"

/*
 * dirname - parse directory component.
 */
char *c_dirname (const char *path) {
  char *newpath;
  register int length;

  if (path == NULL || *path == '\0') {
    newpath = c_strdup(".");
    return newpath;
  }

  length = strlen(path);
  /* Remove trailing slashes */
  while(length > 0 && path[length - 1] == '/') --length;
  /* We have only slashes */
  if (length == 0) {
    newpath = c_strdup("/");
    return newpath;
  }
  /* goto next slash */
  while(length > 0 && path[length - 1] != '/') --length;
  if (length == 0) {
    newpath = c_strdup(".");
    return newpath;
  } else if (length == 1) {
    newpath = c_strdup("/");
    return newpath;
  }

  /* Remove slashes again */
  while(length > 0 && path[length - 1] == '/') --length;

  newpath = (char *) c_malloc(length + 1);
  if (newpath == NULL) {
    return NULL;
  }
  strncpy(newpath, path, length);
  newpath[length] = '\0';

  return newpath;
}

char *c_basename (const char *path) {
  char *newpath;
  char *slash;
  register int length;

  if (path == NULL || *path == '\0') {
    newpath = c_strdup(".");
    return newpath;
  }

  length = strlen(path);
  /* Remove trailing slashes */
  while(length > 0 && path[length - 1] == '/') --length;
  /* We have only slashes */
  if (length == 0) {
    newpath = c_strdup("/");
    return newpath;
  }
  while(length > 0 && path[length - 1] != '/') --length;

  if (length > 0) {
    slash = (char *) path + length;
    length = strlen(slash);
    while(length > 0 && slash[length - 1] == '/') --length;
  } else {
    newpath = c_strdup(path);
    return newpath;
  }
  newpath = (char *) c_malloc(length + 1);
  if (newpath == NULL) {
    return NULL;
  }
  strncpy(newpath, slash, length);
  newpath[length] = '\0';

  return newpath;
}

