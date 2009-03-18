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
 * vim: ft=c.doxygen ts=2 sw=2 et cindent
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

#include "c_alloc.h"
#include "c_path.h"

/*
 * dirname - parse directory component.
 */
char *c_dirname (const char *path) {
  char *new = NULL;
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

  new = c_malloc(len + 1);
  if (new == NULL) {
    return NULL;
  }

  strncpy(new, path, len);
  new[len] = '\0';

  return new;
}

char *c_basename (const char *path) {
  char *new = NULL;
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

  new = c_malloc(len + 1);
  if (new == NULL) {
    return NULL;
  }

  strncpy(new, s, len);
  new[len] = '\0';

  return new;
}

int c_tmpname(char *template) {
  char *tmp = template + strlen(template) - 6;
  int i = 0;

  if (tmp < template) {
    goto err;
  }

  for (i = 0; i < 6; i++) {
    if (tmp[i] != 'X') {
      goto err;
    }
  }

  srand(time(NULL));

  for (i = 0; i < 6; ++i) {
    int hexdigit = (rand() >> (i * 5)) & 0x1f;
    tmp[i] = hexdigit > 9 ? hexdigit + 'a' - 10 : hexdigit + '0';
  }

  return 0;

err:
  errno = EINVAL;
  return -1;
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
        }

        if (passwd != NULL) {
          *passwd = c_strndup(q + 1, p - (q + 1));
        }
      } else {
        /* user only */
        if (user != NULL) {
          *user = c_strndup(z, p - z);
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
      }
    }

    z = p;

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
       * z =                                     ^
       */
      z = p;
    }
  }

  if (*p == '\0') {
    return 0;
  }

  /* get the path with the leading slash */
  if (*p == '/') {
    if (path != NULL) {
      *path = c_strdup(p);
    }

    return 0;
  }

  return -1;
}

