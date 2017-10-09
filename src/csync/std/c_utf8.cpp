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
#include "c_utf8.h"

#ifdef _WIN32
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <wchar.h>
#include <windows.h>
#else
#include <QtCore/QTextCodec>
#include <QtCore/QFile>
#endif

#include "c_alloc.h"
#include "c_string.h"
#include "common/filesystembase.h"

/* Convert a locale String to UTF8 */
QByteArray c_utf8_from_locale(const mbchar_t *wstr)
{
  if (wstr == NULL) {
    return QByteArray();
  }

#ifdef _WIN32
  QByteArray dst;
  int size_needed;
  size_t len;
  len = wcslen(wstr);
  /* Call once to get the required size. */
  size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, len, NULL, 0, NULL, NULL);
  if (size_needed > 0) {
    dst.resize(size_needed);
    WideCharToMultiByte(CP_UTF8, 0, wstr, len, dst.data(), size_needed, NULL, NULL);
  }
  return dst;
#else
    auto codec = QTextCodec::codecForLocale();
#ifndef __APPLE__
    if (codec->mibEnum() == 106) { // UTF-8
        // Optimisation for UTF-8: no need to convert to QString.
        // We still need to do it for mac because of normalization
        return QByteArray(wstr);
    }
#endif
    QTextDecoder dec(codec);
    QString s = dec.toUnicode(wstr, qstrlen(wstr));
    if (s.isEmpty() || dec.hasFailure()) {
        /* Conversion error: since we can't report error from this function, just return the original
            string.  We take care of invalid utf-8 in SyncEngine::treewalkFile */
        return QByteArray(wstr);
    }
#ifdef __APPLE__
    s = s.normalized(QString::NormalizationForm_C);
#endif
    return std::move(s).toUtf8();
#endif
}

extern "C" {

/* Convert a an UTF8 string to locale */
mbchar_t* c_utf8_string_to_locale(const char *str)
{
    if (str == NULL ) {
        return NULL;
    }
#ifdef _WIN32
    mbchar_t *dst = NULL;
    size_t len;
    int size_needed;

    len = strlen(str);
    size_needed = MultiByteToWideChar(CP_UTF8, 0, str, len, NULL, 0);
    if (size_needed > 0) {
        int size_char = (size_needed + 1) * sizeof(mbchar_t);
        dst = (mbchar_t*)c_malloc(size_char);
        memset((void*)dst, 0, size_char);
        MultiByteToWideChar(CP_UTF8, 0, str, -1, dst, size_needed);
    }
    return dst;
#else
    return c_strdup(QFile::encodeName(QString::fromUtf8(str)));
#endif
}

 mbchar_t* c_utf8_path_to_locale(const char *str)
 {
     if( str == NULL ) {
         return NULL;
     } else {
 #ifdef _WIN32
         QByteArray unc_str = OCC::FileSystem::pathtoUNC(QByteArray::fromRawData(str, strlen(str)));
         mbchar_t *dst = c_utf8_string_to_locale(unc_str);
         return dst;
 #else
         return c_utf8_string_to_locale(str);
 #endif
     }
 }

}
