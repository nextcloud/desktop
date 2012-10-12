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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <limits.h>
#include <sys/types.h>
#include <wchar.h>

#include "config.h"

#include "c_string.h"
#include "c_alloc.h"
#include "c_macro.h"

#ifdef _WIN32
#include <windows.h>
#endif

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
    errno = ENOBUFS;
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

char *c_strreplace(char *src, const char *pattern, const char *repl) {
  char *p = NULL;

  while ((p = strstr(src, pattern)) != NULL) {
     size_t of = p - src;
     size_t l  = strlen(src);
     size_t pl = strlen(pattern);
     size_t rl = strlen(repl);

     if (rl > pl) {
        src = (char *) c_realloc(src, strlen(src) + rl - pl + 1);
     }

     if (rl != pl) {
        memmove(src + of + rl, src + of + pl, l - of - pl + 1);
     }

     strncpy(src + of, repl, rl);
  }

  return src;
}

char *c_uppercase(const char* str) {
  char *new;
  char *p;

  if (str == NULL) {
    return NULL;
  }

  new = c_strdup(str);
  if (new == NULL) {
    return NULL;
  }

  for (p = new; *p; p++) {
    *p = toupper(*p);
  }

  return new;
}

char *c_lowercase(const char* str) {
  char *new;
  char *p;

  if (str == NULL) {
    return NULL;
  }

  new = c_strdup(str);
  if (new == NULL) {
    return NULL;
  }

  for (p = new; *p; p++) {
    *p = tolower(*p);
  }

  return new;
}

/* Convert a wide multibyte String to UTF8 */
const char* c_utf8(const mbchar_t *wstr)
{
  const char *dst = NULL;

#ifdef _WIN32
  char *mdst = NULL;

  if(!wstr) return NULL;
  size_t len = _tcslen( wstr );
  /* Call once to get the required size. */
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, len, NULL, 0, NULL, NULL);
  if( size_needed > 0 ) {
    mdst = c_malloc(1+size_needed);
    memset( mdst, 0, 1+size_needed);
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, mdst, size_needed, NULL, NULL);
    dst = mdst;
  }
#else
  dst = wstr;
#endif
  return dst;
}

/* Convert a an UTF8 string to multibyte */
const mbchar_t* c_multibyte(const char *str)
{
#ifdef _WIN32
  mbchar_t *wstrTo = NULL;
  if(!str) return NULL;

  size_t len = strlen( str );
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, len, NULL, 0);
  if(size_needed > 0) {
    int size_char = (size_needed+1)*sizeof(mbchar_t);
    wstrTo = c_malloc(size_char);
    memset( (void*)wstrTo, 0, size_char);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wstrTo, size_needed);
  }
  return wstrTo;
#else
  return str;
#endif
}

void c_free_utf8(char* buf)
{
#ifdef _WIN32
    SAFE_FREE(buf);
#else
    (void)buf;
#endif
}

void c_free_multibyte(const mbchar_t* buf)
{
#ifdef _WIN32
    SAFE_FREE(buf);
#else
    (void)buf;
#endif
}
