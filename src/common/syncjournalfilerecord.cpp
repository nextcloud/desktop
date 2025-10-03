/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "common/syncjournalfilerecord.h"
#include "common/utility.h"

namespace OCC {

QByteArray SyncJournalFileRecord::numericFileId() const
{
    // Use the id up until the first non-numeric character
    for (int i = 0; i < _fileId.size(); ++i) {
        if (_fileId[i] < '0' || _fileId[i] > '9') {
            return _fileId.left(i);
        }
    }
    return _fileId;
}

bool SyncJournalErrorBlacklistRecord::isValid() const
{
    return !_file.isEmpty()
        && (!_lastTryEtag.isEmpty() || _lastTryModtime != 0)
        && _lastTryTime > 0;
}

bool operator==(const SyncJournalFileRecord &lhs,
    const SyncJournalFileRecord &rhs)
{
    return lhs._path == rhs._path
        && lhs._inode == rhs._inode
        && lhs._modtime == rhs._modtime
        && lhs._type == rhs._type
        && lhs._etag == rhs._etag
        && lhs._fileId == rhs._fileId
        && lhs._fileSize == rhs._fileSize
        && lhs._remotePerm == rhs._remotePerm
        && lhs._serverHasIgnoredFiles == rhs._serverHasIgnoredFiles
        && lhs._checksumHeader == rhs._checksumHeader;
}
}
