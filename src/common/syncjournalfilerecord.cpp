/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
