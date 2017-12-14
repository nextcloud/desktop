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
