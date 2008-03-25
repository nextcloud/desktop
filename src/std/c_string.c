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

#include <errno.h>
#include <stdlib.h>

#include "c_string.h"
#include "c_alloc.h"
#include "c_macro.h"

int c_streq(const char *a, const char *b) {
  register const char *s1 = a;
  register const char *s2 = b;

  if (s1 == NULL || s2 ==  NULL) {
    return 0;
  }

  while (*s1 == *s2++) {
    if (*s1++ == '\0') {
      return 1;
    }
  }

  return 0;
}

c_strlist_t *c_strlist_new(size_t size) {
  c_strlist_t *strlist = NULL;

  if (size == 0) {
    errno = EINVAL;
    return NULL;
  }

  strlist = c_malloc(sizeof(c_strlist_t));
  if (strlist == NULL) {
    return NULL;
  }

  strlist->vector = (char **) c_malloc(size * sizeof(char *));
  if (strlist->vector == NULL) {
    SAFE_FREE(strlist);
    return NULL;
  }
  strlist->count = 0;
  strlist->size = size;

  return strlist;
}

c_strlist_t *c_strlist_expand(c_strlist_t *strlist, size_t size) {
  if (strlist == NULL || size == 0) {
    errno = EINVAL;
    return NULL;
  }

  if (strlist->size >= size) {
    return strlist;
  }

  strlist->vector = (char **) c_realloc(strlist->vector, size * sizeof(char *));
  if (strlist->vector == NULL) {
    return NULL;
  }

  strlist->size = size;

  return strlist;
}

int c_strlist_add(c_strlist_t *strlist, const char *string) {
  if (strlist == NULL || string == NULL) {
    return -1;
  }

  if (strlist->count < strlist->size) {
    strlist->vector[strlist->count] = c_strdup(string);
    if (strlist->vector[strlist->count] == NULL) {
      return -1;
    }
    strlist->count++;
  } else {
    return -1;
  }

  return 0;
}

void c_strlist_destroy(c_strlist_t *strlist) {
  size_t i = 0;

  if (strlist == NULL) {
    return;
  }

  for (i = 0; i < strlist->count; i++) {
    SAFE_FREE(strlist->vector[i]);
  }
  SAFE_FREE(strlist->vector);
  SAFE_FREE(strlist);
}

