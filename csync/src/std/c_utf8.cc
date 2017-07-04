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

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
#include "c_alloc.h"
#include "c_string.h"

/* Convert a locale String to UTF8 */
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
    mdst = (char*)c_malloc(size_needed + 1);

    memset(mdst, 0, size_needed + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, mdst, size_needed, NULL, NULL);
    dst = mdst;
  }
#else
  dst = c_strdup(wstr);
#endif
  return dst;
}

/* Convert a an UTF8 string to locale */
mbchar_t* c_utf8_string_to_locale(const char *str)
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
        dst = (mbchar_t*)c_malloc(size_char);
        memset((void*)dst, 0, size_char);
        MultiByteToWideChar(CP_UTF8, 0, str, -1, dst, size_needed);
    }
#else
    dst = c_strdup(str);
#endif
    return dst;
}

}
