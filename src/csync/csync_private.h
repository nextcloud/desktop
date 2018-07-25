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

/**
 * @file csync_private.h
 *
 * @brief Private interface of csync
 *
 * @defgroup csyncInternalAPI csync internal API
 *
 * @{
 */

#ifndef _CSYNC_PRIVATE_H
#define _CSYNC_PRIVATE_H

#include <unordered_map>
#include <QHash>
#include <stdint.h>
#include <stdbool.h>
#include <map>
#include <set>
#include <functional>

#include "common/syncjournaldb.h"
#include "config_csync.h"
#include "std/c_lib.h"
#include "std/c_private.h"
#include "csync.h"
#include "csync_misc.h"
#include "csync_exclude.h"
#include "csync_macros.h"

/**
 * How deep to scan directories.
 */
#define MAX_DEPTH 100

#define CSYNC_STATUS_INIT 1 << 0
#define CSYNC_STATUS_UPDATE 1 << 1
#define CSYNC_STATUS_RECONCILE 1 << 2
#define CSYNC_STATUS_PROPAGATE 1 << 3

#define CSYNC_STATUS_DONE (CSYNC_STATUS_INIT | \
                           CSYNC_STATUS_UPDATE | \
                           CSYNC_STATUS_RECONCILE | \
                           CSYNC_STATUS_PROPAGATE)

enum csync_replica_e {
  LOCAL_REPLICA,
  REMOTE_REPLICA
};

enum class LocalDiscoveryStyle {
    FilesystemOnly, //< read all local data from the filesystem
    DatabaseAndFilesystem, //< read from the db, except for listed paths
};


/*
 * This is a structurere similar to QStringRef
 * The difference is that it keeps the QByteArray by value and not by pointer
 * And it only implements a very small subset of the API that is required by csync, the API can be
 * added as we need it.
 */
class ByteArrayRef
{
    QByteArray _arr;
    int _begin = 0;
    int _size = -1;

    /* Pointer to the beginning of the data. WARNING: not null terminated */
    const char *data() const { return _arr.constData() + _begin; }
    friend struct ByteArrayRefHash;

public:
    ByteArrayRef(QByteArray arr = {}, int begin = 0, int size = -1)
        : _arr(std::move(arr))
        , _begin(begin)
        , _size(qMin(_arr.size() - begin, size < 0 ? _arr.size() - begin : size))
    {
    }
    ByteArrayRef left(int l) const { return ByteArrayRef(_arr, _begin, l); };
    char at(int x) const { return _arr.at(_begin + x); }
    int size() const { return _size; }
    int length() const { return _size; }
    bool isEmpty() const { return _size == 0; }

    friend bool operator==(const ByteArrayRef &a, const ByteArrayRef &b)
    { return a.size() == b.size() && qstrncmp(a.data(), b.data(), a.size()) == 0; }
};
struct ByteArrayRefHash { uint operator()(const ByteArrayRef &a) const { return qHashBits(a.data(), a.size()); } };

/**
 * @brief csync public structure
 */
struct OCSYNC_EXPORT csync_s {


  // For some reason MSVC references the copy constructor and/or the assignment operator
  // if a class is exported. This is a problem since unique_ptr isn't copyable.
  // Explicitly disable them to fix the issue.
  // https://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/e39ab33d-1aaf-4125-b6de-50410d9ced1d
  csync_s(const csync_s &) = delete;
  csync_s &operator=(const csync_s &) = delete;
};

void set_errno_from_http_errcode( int err );

/**
 * }@
 */
#endif /* _CSYNC_PRIVATE_H */
/* vim: set ft=c.doxygen ts=8 sw=2 et cindent: */
