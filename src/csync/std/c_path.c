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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "c_private.h"
#include "c_alloc.h"
#include "c_path.h"
#include "c_string.h"
#include "c_utf8.h"

/*
 * dirname - parse directory component.
 */
char *c_dirname (const char *path) {
  char *newbuf = NULL;
  unsigned int len;

  if (path == NULL || *path == '\0') {
    return c_strdup(".");
  }

  len = strlen(path);

  /* Remove trailing slashes */
  while(len > 0 && path[len - 1] == '/') --len;

  /* We have only slashes */
  if (len == 0) {
    return c_strdup("/");
  }

  /* goto next slash */
  while(len > 0 && path[len - 1] != '/') --len;

  if (len == 0) {
    return c_strdup(".");
  } else if (len == 1) {
    return c_strdup("/");
  }

  /* Remove slashes again */
  while(len > 0 && path[len - 1] == '/') --len;

  newbuf = c_malloc(len + 1);

  strncpy(newbuf, path, len);
  newbuf[len] = '\0';

  return newbuf;
}

char *c_basename (const char *path) {
  char *newbuf = NULL;
  const char *s;
  unsigned int len;

  if (path == NULL || *path == '\0') {
    return c_strdup(".");
  }

  len = strlen(path);
  /* Remove trailing slashes */
  while(len > 0 && path[len - 1] == '/') --len;

  /* We have only slashes */
  if (len == 0) {
    return c_strdup("/");
  }

  while(len > 0 && path[len - 1] != '/') --len;

  if (len > 0) {
    s = path + len;
    len = strlen(s);

    while(len > 0 && s[len - 1] == '/') --len;
  } else {
    return c_strdup(path);
  }

  newbuf = c_malloc(len + 1);

  strncpy(newbuf, s, len);
  newbuf[len] = '\0';

  return newbuf;
}

int c_parse_uri(const char *uri,
    char **scheme,
    char **user, char **passwd,
    char **host, unsigned int *port,
    char **path) {
  const char *p, *z;

  if (uri == NULL || *uri == '\0') {
    return -1;
  }

  /*
   * uri = scheme://user:password@host:port/path
   * p   = ^
   * z   = ^
   */
  p = z = uri;

  /* check for valid scheme; git+ssh, pop3 */
  while (isalpha((int) *p) || isdigit((int) *p) ||
      *p == '+' || *p == '-') {
    /*
     * uri = scheme://user:password@host:port/path
     * p   =       ^
     * z   = ^
     */
    p++;
  }

  /* get scheme */
  if (*p == ':') {
    if (scheme != NULL) {
      *scheme = c_strndup(z, p - z);

      if (*scheme == NULL) {
        errno = ENOMEM;
        return -1;
      }
    }
    p++;
    z = p;
  }

  /*
   * uri = scheme://user:password@host:port/path
   * p =          ^
   * z =          ^
   */
  p = z;

  /* do we have a hostname */
  if (p[0] == '/' && p[1] == '/') {
    /*
     * uri = scheme://user:password@host:port/path
     * p   =          ^
     * z   =          ^
     */
    z += 2;
    p = z;

    /* check for user and passwd */
    while (*p && *p != '@' && *p != '/') {
      /*
       * uri = scheme://user:password@host:port/path
       * p   =                       ^    or   ^
       * z   =          ^
       */
      p++;
    }

    /* check for user and password */
    if (*p == '@') {
      const char *q;

      q = p;

      /* check if we have a password */
      while (q > z && *q != ':') {
        /*
         * uri = scheme://user:password@host:port/path
         * p   =                       ^
         * z   =          ^
         * q   =              ^
         */
        q--;
      }

      /* password found */
      if (*q == ':') {
        if (user != NULL) {
          *user = c_strndup(z, q - z);
          if (*user == NULL) {
            errno = ENOMEM;
            if (scheme != NULL) SAFE_FREE(*scheme);
            return -1;
          }
        }

        if (passwd != NULL) {
          *passwd = c_strndup(q + 1, p - (q + 1));
          if (*passwd == NULL) {
            if (scheme != NULL) SAFE_FREE(*scheme);
            if (user   != NULL) SAFE_FREE(*user);
            errno = ENOMEM;
            return -1;
          }
        }
      } else {
        /* user only */
        if (user != NULL) {
          *user = c_strndup(z, p - z);
          if( *user == NULL) {
            if (scheme != NULL) SAFE_FREE(*scheme);
            errno = ENOMEM;
            return -1;
          }
        }
      }

      p++;
      z = p;
    }

    /*
     * uri = scheme://user:password@host:port/path
     * p =                          ^
     * z =                          ^
     */
    p = z;

    /* check for IPv6 address */
    if (*p == '[') {
      /*
       * uri = scheme://user:password@[2001:0db8:85a3:08d3:1319:8a2e:0370:7344]:port/path
       * p   =                         ^
       * z   =                        ^
       */
      p++;

      /* check if we have a valid IPv6 address */
      while (*p && (isxdigit((int) *p) || *p == '.' || *p == ':')) {
        /*
         * uri = scheme://user:password@[2001:0db8:85a3:08d3:1319:8a2e:0370:7344]:port/path
         * p   =                                                                ^
         * z   =                        ^
         */
        p++;
      }

      /* valid IPv6 address found */
      if (*p == ']') {
        /*
         * uri = scheme://user:password@[2001:0db8:85a3:08d3:1319:8a2e:0370:7344]:port/path
         * p   =                                                                ^
         * z   =                         ^
         */
        z++;

        if (host != NULL) {
          *host = c_strndup(z, p - z);
          if (*host == NULL) {
            if (scheme != NULL) SAFE_FREE(*scheme);
            if (user   != NULL) SAFE_FREE(*user);
            if (passwd != NULL) SAFE_FREE(*passwd);
            errno = ENOMEM;
            return -1;
          }
        }

        /*
         * uri = scheme://user:password@[2001:0db8:85a3:08d3:1319:8a2e:0370:7344]:port/path
         * p   =                                                                 ^
         * z   =                         ^
         */
        p++;
      } else {
        /* invalid IPv6 address, assume a hostname */
        p = z;

        while (*p && *p != ':' && *p != '/') {
          p++;
          /*
           * uri = scheme://user:password@host:port/path
           * p   =                            ^ or ^
           * z   =                        ^
           */
        }

        if (host != NULL) {
          *host = c_strndup(z, p - z);
          if (*host == NULL) {
            if (scheme != NULL) SAFE_FREE(*scheme);
            if (user   != NULL) SAFE_FREE(*user);
            if (passwd != NULL) SAFE_FREE(*passwd);
            errno = ENOMEM;
            return -1;
          }
        }
      }
    } else {
      /* check for hostname */
      while (*p && *p != ':' && *p != '/') {
        /*
         * uri = scheme://user:password@host:port/path
         * p   =                            ^    ^
         * z   =                        ^
         */
        p++;
      }

      if (host != NULL) {
        *host = c_strndup(z, p - z);
        if (*host == NULL) {
          if (scheme != NULL) SAFE_FREE(*scheme);
          if (user   != NULL) SAFE_FREE(*user);
          if (passwd != NULL) SAFE_FREE(*passwd);
          errno = ENOMEM;
          return -1;
        }
      }
    }

    /* check for port */
    if (*p == ':') {
      char **e = NULL;
      /*
       * uri = scheme://user:password@host:port/path
       * p =                               ^
       * z =                               ^
       */
      z = ++p;

      /* get only the digits */
      while (isdigit((int) *p)) {
        /*
         * uri = scheme://user:password@host:port/path
         * p   =                                 ^
         * z   =                             ^
         */
        e = (char **) &p;
        p++;
      }

      if (port != NULL) {
        *port = strtoul(z, e, 0);
      }

      /*
       * uri   = scheme://user:password@host:port/path
       * p =                                     ^
       */
    }
  }

  if (*p == '\0') {
    return 0;
  }

  /* get the path with the leading slash */
  if (*p == '/') {
    if (path != NULL) {
      *path = c_strdup(p);
      if (*path == NULL) {
        if (scheme != NULL) SAFE_FREE(*scheme);
        if (user   != NULL) SAFE_FREE(*user);
        if (passwd != NULL) SAFE_FREE(*passwd);
        if (host   != NULL) SAFE_FREE(*host);
        errno = ENOMEM;
        return -1;
      }
    }

    return 0;
  }

  return -1;
}


/*
 * This function takes a path and converts it to a UNC representation of the
 * string. That means that it prepends a \\?\ (unless already UNC) and converts
 * all slashes to backslashes.
 *
 * Note the following:
 *  - The string must be absolute.
 *  - it needs to contain a drive character to be a valid UNC
 *  - A conversion is only done if the path len is larger than 245. Otherwise
 *    the windows API functions work with the normal "unixoid" representation too.
 *
 *  This function allocates memory that must be freed by the caller.
 */
 const char *c_path_to_UNC(const char *str)
 {
     int len = 0;
     char *longStr = NULL;

     len = strlen(str);
     longStr = c_malloc(len+5);
     *longStr = '\0';

     // prepend \\?\ and convert '/' => '\' to support long names
     if( str[0] == '/' || str[0] == '\\' ) {
         // Don't prepend if already UNC
         if( !(len > 1 && (str[1] == '/' || str[1] == '\\')) ) {
            strcpy( longStr, "\\\\?");
         }
     } else {
         strcpy( longStr, "\\\\?\\"); // prepend string by this four magic chars.
     }
     strncat( longStr, str, len );

     /* replace all occurences of / with the windows native \ */
     char *c = longStr;
     for (; *c; ++c) {
         if(*c == '/') {
             *c = '\\';
         }
     }
     return longStr;
 }

 mbchar_t* c_utf8_path_to_locale(const char *str)
 {
     if( str == NULL ) {
         return NULL;
     } else {
 #ifdef _WIN32
         const char *unc_str = c_path_to_UNC(str);
         mbchar_t *dst = c_utf8_string_to_locale(unc_str);
         SAFE_FREE(unc_str);
         return dst;
 #else
         return c_utf8_string_to_locale(str);
 #endif
     }
 }
