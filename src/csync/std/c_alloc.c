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

#include <string.h>

#include "c_macro.h"
#include "c_alloc.h"

void *c_calloc(size_t count, size_t size) {
  if (size == 0 || count == 0) {
    return NULL;
  }

#ifdef CSYNC_MEM_NULL_TESTS
  if (getenv("CSYNC_NOMEMORY")) {
    return NULL;
  }
#endif /* CSYNC_MEM_NULL_TESTS */

#undef calloc
  return calloc(count, size);
#define calloc(x,y) DO_NOT_CALL_CALLOC__USE_XCALLOC_INSTEAD
}

void *c_malloc(size_t size) {
  if (size == 0) {
    return NULL;
  }
#undef malloc
  return c_calloc(1, size);
#define malloc(x) DO_NOT_CALL_MALLOC__USE_XMALLOC_INSTEAD
}

void *c_realloc(void *ptr, size_t size) {

#ifdef CSYNC_MEM_NULL_TESTS
  if (getenv("CSYNC_NOMEMORY")) {
    return NULL;
  }
#endif /* CSYNC_MEM_NULL_TESTS */

#undef realloc
  return realloc(ptr, size);
#define realloc(x,y) DO_NOT_CALL_REALLOC__USE_XREALLOC_INSTEAD
}

char *c_strdup(const char *str) {
  char *ret;
  ret = (char *) c_malloc(strlen(str) + 1);
  if (ret == NULL) {
    return NULL;
  }
  strcpy(ret, str);
  return ret;
}

char *c_strndup(const char *str, size_t size) {
  char *ret;
  size_t len;
  len = strlen(str);
  if (len > size) {
    len = size;
  }
  ret = (char *) c_malloc(len + 1);
  if (ret == NULL) {
    return NULL;
  }
  strncpy(ret, str, len);
  ret[size] = '\0';
  return ret;
}

