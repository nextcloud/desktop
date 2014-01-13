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
  if (newbuf == NULL) {
    return NULL;
  }

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
  if (newbuf == NULL) {
    return NULL;
  }

  strncpy(newbuf, s, len);
  newbuf[len] = '\0';

  return newbuf;
}


char *c_tmpname(const char *templ) {
  char *tmp = NULL;
  char *target = NULL;
  int rc;
  int i = 0;

  if (!templ) {
    goto err;
  }

  /* If the template does not contain the XXXXXX it will be appended. */
  if( !strstr( templ, "XXXXXX" )) {
      /* split up the path */
      char *path = c_dirname(templ);
      char *base = c_basename(templ);

      if (!base) {
        if (path) {
          SAFE_FREE(path);
        }
        goto err;
      }
      /* Create real hidden files for unixoide. */
      if( path ) {
#ifdef _WIN32
	rc = asprintf(&target, "%s/%s.~XXXXXX", path, base);
#else
        rc = asprintf(&target, "%s/.%s.~XXXXXX", path, base);
#endif
      } else {
#ifdef _WIN32
	rc = asprintf(&target, "%s.~XXXXXX", base);
#else
        rc = asprintf(&target, ".%s.~XXXXXX", base);
#endif

      }
      SAFE_FREE(path);
      SAFE_FREE(base);

      if (rc < 0) {
        goto err;
      }
  } else {
    target = c_strdup(templ);
  }

  if (!target) {
    goto err;
  }
  tmp = strstr( target, "XXXXXX" );
  if (!tmp) {
    goto err;
  }

  for (i = 0; i < 6; ++i) {
#ifdef _WIN32
    /* in win32 MAX_RAND is 32767, thus we can not shift that far,
     * otherwise the last three chars are 0
     */
    int hexdigit = (rand() >> (i * 2)) & 0x1f;
#else
    int hexdigit = (rand() >> (i * 5)) & 0x1f;
#endif
    tmp[i] = hexdigit > 9 ? hexdigit + 'a' - 10 : hexdigit + '0';
  }

  return target;

err:
  errno = EINVAL;
  return NULL;
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
 * http://refactormycode.com/codes/1345-extracting-directory-filename-and-extension-from-a-path
 * Allocate a block of memory that holds the PATHINFO at the beginning
 * followed by the actual path. Two extra bytes are allocated (+3 instead
 * of just +1) to deal with shifting the filename and extension to protect the trailing '/'
 * and the leading '.'. These extra bytes also double as the empty string, as
 * well as a pad to keep from reading past the memory block.
 * 
 */
C_PATHINFO * c_split_path(const char* pathSrc)
{
    size_t length = strlen(pathSrc);
	size_t len=0;
    C_PATHINFO * pathinfo = (C_PATHINFO *) c_malloc(sizeof(C_PATHINFO) + length + 3);

    if (pathinfo)
    {
        char * path = (char *) &pathinfo[1];    // copy of the path
        char * theEnd = &path[length + 1];      // second null terminator
        char * extension;
        char * lastSep;

        // Copy the original string and double null terminate it.
        strcpy(path, pathSrc);
        *theEnd = '\0';
        pathinfo->directory = theEnd;   // Assume no path
        pathinfo->extension = theEnd;   // Assume no extension
        pathinfo->filename = path;      // Assume filename only

        lastSep = strrchr(path, '/');
        if (lastSep)
        {
            pathinfo->directory = path;     // Pick up the path

            memmove(lastSep + 1, lastSep, strlen(lastSep));
            *lastSep++ ='/';
            *lastSep++ ='\0';  // Truncate directory

            pathinfo->filename = lastSep;  // Pick up name after path 
        }
    
        // Start at the second character of the filename to handle
        // filenames that start with '.' like ".login".
        // We don't overrun the buffer in the cases of an empty path
        // or a path that looks like "/usr/bin/" because of the extra
        // byte.
        
        
        extension = strrchr(&pathinfo->filename[1], '.');
        if (extension)
        {
            // Shift the extension over to protect the leading '.' since
            // we need to truncate the filename.
            memmove(extension + 1, extension, strlen(extension));
            pathinfo->extension = extension + 1;

            *extension = '\0';          // Truncate filename
        }
        else
        {
            len=strlen(pathinfo->filename);
            if(len>1)
            {
                //tmp files from kate/kwrite "somefile~": '~' should be the extension
                if(pathinfo->filename[len-1]=='~')
                {
                    extension = &pathinfo->filename[len-1];
                    memmove(extension + 1, extension, strlen(extension));
                    pathinfo->extension = extension + 1;
                    *extension = '\0';          // Truncate filename
                }
            }
        }
        
    }

    return pathinfo;
}


