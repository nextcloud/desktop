/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef SYNCFILEITEM_H
#define SYNCFILEITEM_H

#include <QVector>
#include <QString>
#include <QDateTime>
#include <QMetaType>

#include <csync.h>

namespace OCC {

class SyncFileItem {
public:
    enum Direction {
      None = 0,
      Up,
      Down };

    enum Type {
      UnknownType = 0,
      File      = CSYNC_FTW_TYPE_FILE,
      Directory = CSYNC_FTW_TYPE_DIR,
      SoftLink  = CSYNC_FTW_TYPE_SLINK
    };

    enum Status {
        NoStatus,

        FatalError, ///< Error that causes the sync to stop
        NormalError, ///< Error attached to a particular file
        SoftError, ///< More like an information

        Success, ///< The file was properly synced
        Conflict, ///< The file was properly synced, but a conflict was created
        FileIgnored, ///< The file is in the ignored list (or blacklisted with no retries left)
        Restoration ///< The file was restored because what should have been done was not allowed
    };

    SyncFileItem() : _type(UnknownType),  _direction(None), _isDirectory(false),
        _instruction(CSYNC_INSTRUCTION_NONE), _modtime(0),
        _size(0), _inode(0), _should_update_etag(false), _hasBlacklistEntry(false),
        _status(NoStatus), _httpErrorCode(0), _requestDuration(0), _isRestoration(false),
        _affectedItems(1)
    {
    }

    friend bool operator==(const SyncFileItem& item1, const SyncFileItem& item2) {
        return item1._file == item2._file;
    }

    friend bool operator<(const SyncFileItem& item1, const SyncFileItem& item2) {
        // Sort by destination
        return item1.destination() < item2.destination();
    }

    QString destination() const {
        if (!_renameTarget.isEmpty()) {
            return _renameTarget;
        }
        return _file;
    }

    bool isEmpty() const {
        return _file.isEmpty();
    }

    bool hasErrorStatus() const {
        return _status == SyncFileItem::SoftError
                || _status == SyncFileItem::NormalError
                || _status == SyncFileItem::FatalError
                || !_errorString.isEmpty();
    }

    // Variables usefull for everybody
    QString _file;
    QString _renameTarget;
    Type      _type;
    Direction _direction;
    bool _isDirectory;

    // Variables used by the propagator
    QString              _originalFile; // as it is in the csync tree
    csync_instructions_e _instruction;
    time_t               _modtime;
    QByteArray           _etag;
    quint64              _size;
    quint64              _inode;
    bool                 _should_update_etag;
    QByteArray           _fileId;
    QByteArray           _remotePerm;
    QString              _directDownloadUrl;
    QString              _directDownloadCookies;

    /// Whether there's an entry in the blacklist table.
    /// Note: that entry may have retries left, so this can be true
    /// without the status being FileIgnored.
    bool                 _hasBlacklistEntry;

    // Variables usefull to report to the user
    Status               _status;
    QString              _errorString; // Contains a string only in case of error
    int                  _httpErrorCode;
    QString              _responseTimeStamp;
    quint64              _requestDuration;
    bool                 _isRestoration; // The original operation was forbidden, and this is a restoration
    int                  _affectedItems; // the number of affected items by the operation on this item.
     // usually this value is 1, but for removes on dirs, it might be much higher.
    struct {
        quint64     _size;
        time_t      _modtime;
        QByteArray  _etag;
        QByteArray  _fileId;
        enum csync_instructions_e _instruction;
        quint64     _other_size;
        time_t      _other_modtime;
        QByteArray  _other_etag;
        QByteArray  _other_fileId;
        QByteArray  _other_remotePerm;
        enum csync_instructions_e _other_instruction;
    } log;
};



typedef QVector<SyncFileItem> SyncFileItemVector;

}

Q_DECLARE_METATYPE(OCC::SyncFileItem)

#endif // SYNCFILEITEM_H
