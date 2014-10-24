/*
 * cynapses libc functions
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>
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

#include "config_csync.h"

#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <limits.h>
#include <sys/types.h>
#include <wchar.h>

#include "c_string.h"
#include "c_alloc.h"
#include "c_macro.h"

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(HAVE_ICONV) && defined(WITH_ICONV)
# ifdef HAVE_ICONV_H
#  include <iconv.h>
# endif
# ifdef HAVE_SYS_ICONV_H
#  include <sys/iconv.h>
# endif

typedef struct {
  iconv_t to;
  iconv_t from;
} iconv_conversions;

CSYNC_THREAD iconv_conversions _iconvs = { NULL, NULL };

int c_setup_iconv(const char* to) {
  _iconvs.to = iconv_open(to, "UTF-8");
  _iconvs.from = iconv_open("UTF-8", to);

  if (_iconvs.to == (iconv_t)-1 || _iconvs.from == (iconv_t)-1)
    return -1;

  return 0;
}

int c_close_iconv() {
    int ret_to = 0;
    int ret_from = 0;
    if( _iconvs.to != (iconv_t) NULL ) {
        ret_to = iconv_close(_iconvs.to);
    }
    if( _iconvs.from != (iconv_t) NULL ) {
        ret_from = iconv_close(_iconvs.from);
    }

  if (ret_to == -1 || ret_from == -1)
    return -1;

  _iconvs.to = (iconv_t) 0;
  _iconvs.from = (iconv_t) 0;

  return 0;
}

enum iconv_direction { iconv_from_native, iconv_to_native };

static char *c_iconv(const char* str, enum iconv_direction dir)
{
#ifdef HAVE_ICONV_CONST
    const char *in = str;
#else
    char *in = discard_const(str);
#endif
  size_t size;
  size_t outsize;
  char *out;
  char *out_in;
  size_t ret;

  if (str == NULL) {
    return NULL;
  }

  if(_iconvs.from == NULL && _iconvs.to == NULL) {
#ifdef __APPLE__
    c_setup_iconv("UTF-8-MAC");
#else
    return c_strdup(str);
#endif
  }

  size = strlen(in);
  outsize = size*2;
  out = c_malloc(outsize);
  out_in = out;

  if (out == NULL) {
      return NULL;
  }

  if (dir == iconv_to_native) {
      ret = iconv(_iconvs.to, &in, &size, &out, &outsize);
  } else {
      ret = iconv(_iconvs.from, &in, &size, &out, &outsize);
  }

  if (ret == (size_t)-1) {
      SAFE_FREE(out_in);
      return NULL;
  }

  return out_in;
}
#endif /* defined(HAVE_ICONV) && defined(WITH_ICONV) */

int c_strncasecmp(const char *a, const char *b, size_t n) {
#ifdef _WIN32
    return _strnicmp(a, b, n);
#else
    return strncasecmp(a, b, n);
#endif
}

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

void c_strlist_clear(c_strlist_t *strlist) {
  size_t i = 0;

  if (strlist == NULL) {
    return;
  }

  for (i = 0; i < strlist->count; i++) {
    SAFE_FREE(strlist->vector[i]);
  }

  strlist->count = 0;
}

void c_strlist_destroy(c_strlist_t *strlist) {

  if (strlist == NULL) {
    return;
  }

  c_strlist_clear(strlist);
  SAFE_FREE(strlist->vector);
  SAFE_FREE(strlist);
}

/* Convert a wide multibyte String to UTF8 */
char* c_utf8_from_locale(const mbchar_t *wstr)
{
  char *dst = NULL;
#ifdef _WIN32
  char *mdst = NULL;
  int size_needed;
  size_t len;
#endif

  if (wstr == NULL) {
    return NULL;
  }

#ifdef _WIN32
  len = wcslen(wstr);
  /* Call once to get the required size. */
  size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, len, NULL, 0, NULL, NULL);
  if (size_needed > 0) {
    mdst = c_malloc(size_needed + 1);
    if (mdst == NULL) {
      errno = ENOMEM;
      return NULL;
    }

    memset(mdst, 0, size_needed + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, mdst, size_needed, NULL, NULL);
    dst = mdst;
  }
#else
#ifdef WITH_ICONV
  dst = c_iconv(wstr, iconv_from_native);
#else
  dst = (char*) wstr;
#endif
#endif
  return dst;
}

/* Convert a an UTF8 string to multibyte */
mbchar_t* c_utf8_to_locale(const char *str)
{
  mbchar_t *dst = NULL;
#ifdef _WIN32
  size_t len;
  int size_needed;
#endif

  if (str == NULL ) {
    return NULL;
  }

#ifdef _WIN32
  len = strlen(str);
  size_needed = MultiByteToWideChar(CP_UTF8, 0, str, len, NULL, 0);
  if (size_needed > 0) {
    int size_char = (size_needed + 1) * sizeof(mbchar_t);
    dst = c_malloc(size_char);
    if (dst == NULL) {
      errno = ENOMEM;
      return NULL;
    }

    memset((void*)dst, 0, size_char);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, dst, size_needed);
  }
#else
#ifdef WITH_ICONV
  dst = c_iconv(str, iconv_to_native);
#else
  dst = (_TCHAR*) str;
#endif
#endif
  return dst;
}
