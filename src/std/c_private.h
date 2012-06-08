/*
 * Copyright (c) 2012 by Dominik Schmidt <dev@dominik-schmidt.de>
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
 */

#ifndef _C_PRIVATE_H
#define _C_PRIVATE_H

#include "config.h"

/* cross platform defines */
#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windef.h>
#include <winbase.h>
#endif


#ifdef _WIN32
#define EDQUOT 0
#define ENODATA 0
#define S_IRGRP 0
#define S_IROTH 0
#define S_IXGRP 0
#define S_IXOTH 0
#define O_NOFOLLOW 0
#define O_NOATIME 0
#define O_NOCTTY 0

#define uid_t int
#define gid_t int
#define nlink_t int
#define getuid() 0
#define geteuid() 0
#endif

#ifdef _WIN32
typedef struct _stat csync_stat_t;
#else
typedef struct stat csync_stat_t;
#endif

#ifndef HAVE_STRERROR_R
#define strerror_r(errnum, buf, buflen) snprintf(buf, buflen, "%s", strerror(errnum))
#endif

#ifndef HAVE_LSTAT
#define lstat _stat
#endif
#ifdef _WIN32
#define fstat  _fstat
#endif

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

/* tchar definitions for clean win32 filenames */
#define _UNICODE

#if defined _WIN32 && defined _UNICODE 
typedef  wchar_t    _TCHAR;
#define _tcslen      wcslen
#define _topen      _wopen
#define _tdirent    _wdirent
#define _TDIR       _WDIR
#define _topendir   _wopendir
#define _tclosedir  _wclosedir
#define _treaddir   _wreaddir
#define _trewinddir _wrewinddir
#define _ttelldir   _wtelldir
#define _tseekdir   _wseekdir
#define _tcreat     _wcreat
#define _tstat      _wstat
#define _tunlink    _wunlink
#define _tmkdir     _wmkdir
#define _trmdir	    _wrmdir
#define _tchmod     _wchmod
#define _trewinddir _wrewinddir
#else 
typedef char        _TCHAR;
#define _tdirent    dirent
#define _tcslen     strlen
#define _topen      open
#define _TDIR       DIR
#define _topendir   opendir
#define _tclosedir  closedir
#define _treaddir   readdir
#define _trewinddir rewinddir
#define _ttelldir   telldir
#define _tseekdir   seekdir
#define _tcreat     creat
#define _tstat      stat
#define _tunlink    unlink
#define _tmkdir     mkdir
#define _trmdir	    rmdir
#define _tchmod     chmod
#define _trewinddir rewinddir
#endif

#endif //_C_PRIVATE_H

/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
