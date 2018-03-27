/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <QSharedPointer>

#include <csync.h>

namespace OCC {

class SyncFileItem;
class SyncJournalFileRecord;
typedef QSharedPointer<SyncFileItem> SyncFileItemPtr;

/**
 * @brief The SyncFileItem class
 * @ingroup libsync
 */
class SyncFileItem
{
public:
    enum Direction {
        None = 0,
        Up,
        Down
    };

    enum Type {
        UnknownType = 0,
        File = CSYNC_FTW_TYPE_FILE,
        Directory = CSYNC_FTW_TYPE_DIR,
        SoftLink = CSYNC_FTW_TYPE_SLINK
    };

    enum Status { // stored in 4 bits
        NoStatus,

        FatalError, ///< Error that causes the sync to stop
        NormalError, ///< Error attached to a particular file
        SoftError, ///< More like an information

        Success, ///< The file was properly synced

        /** Marks a conflict, old or new.
         *
         * With instruction:IGNORE: detected an old unresolved old conflict
         * With instruction:CONFLICT: a new conflict this sync run
         */
        Conflict,

        FileIgnored, ///< The file is in the ignored list (or blacklisted with no retries left)
        Restoration, ///< The file was restored because what should have been done was not allowed

        /** For errors that should only appear in the error view.
         *
         * Some errors also produce a summary message. Usually displaying that message is
         * sufficient, but the individual errors should still appear in the issues tab.
         *
         * These errors do cause the sync to fail.
         *
         * A NormalError that isn't as prominent.
         */
        DetailError,

        /** For files whose errors were blacklisted
         *
         * If an file is blacklisted due to an error it isn't even reattempted. These
         * errors should appear in the issues tab but should be silent otherwise.
         *
         * A SoftError caused by blacklisting.
         */
        BlacklistedError
    };

    SyncJournalFileRecord toSyncJournalFileRecordWithInode(const QString &localFileName);

    /** Creates a basic SyncFileItem from a DB record
     *
     * This is intended in particular for read-update-write cycles that need
     * to go through a a SyncFileItem, like PollJob.
     */
    static SyncFileItemPtr fromSyncJournalFileRecord(const SyncJournalFileRecord &rec);


    SyncFileItem()
        : _type(UnknownType)
        , _direction(None)
        , _serverHasIgnoredFiles(false)
        , _hasBlacklistEntry(false)
        , _errorMayBeBlacklisted(false)
        , _status(NoStatus)
        , _isRestoration(false)
        , _httpErrorCode(0)
        , _affectedItems(1)
        , _instruction(CSYNC_INSTRUCTION_NONE)
        , _modtime(0)
        , _size(0)
        , _inode(0)
        , _previousSize(0)
        , _previousModtime(0)
    {
    }

    friend bool operator==(const SyncFileItem &item1, const SyncFileItem &item2)
    {
        return item1._originalFile == item2._originalFile;
    }

    friend bool operator<(const SyncFileItem &item1, const SyncFileItem &item2)
    {
        // Sort by destination
        auto d1 = item1.destination();
        auto d2 = item2.destination();

        // But this we need to order it so the slash come first. It should be this order:
        //  "foo", "foo/bar", "foo-bar"
        // This is important since we assume that the contents of a folder directly follows
        // its contents

        auto data1 = d1.constData();
        auto data2 = d2.constData();

        // Find the length of the largest prefix
        int prefixL = 0;
        auto minSize = std::min(d1.size(), d2.size());
        while (prefixL < minSize && data1[prefixL] == data2[prefixL]) {
            prefixL++;
        }

        if (prefixL == d2.size())
            return false;
        if (prefixL == d1.size())
            return true;

        if (data1[prefixL] == '/')
            return true;
        if (data2[prefixL] == '/')
            return false;

        return data1[prefixL] < data2[prefixL];
    }

    QString destination() const
    {
        if (!_renameTarget.isEmpty()) {
            return _renameTarget;
        }
        return _file;
    }

    bool isEmpty() const
    {
        return _file.isEmpty();
    }

    bool isDirectory() const
    {
        return _type == SyncFileItem::Directory;
    }

    /**
     * True if the item had any kind of error.
     */
    bool hasErrorStatus() const
    {
        return _status == SyncFileItem::SoftError
            || _status == SyncFileItem::NormalError
            || _status == SyncFileItem::FatalError
            || !_errorString.isEmpty();
    }

    /**
     * Whether this item should appear on the issues tab.
     */
    bool showInIssuesTab() const
    {
        return hasErrorStatus() || _status == SyncFileItem::Conflict;
    }

    /**
     * Whether this item should appear on the protocol tab.
     */
    bool showInProtocolTab() const
    {
        return !showInIssuesTab()
            // Don't show conflicts that were resolved as "not a conflict after all"
            && !(_instruction == CSYNC_INSTRUCTION_CONFLICT && _status == SyncFileItem::Success);
    }

    // Variables useful for everybody
    QString _file;
    QString _renameTarget;
    Type _type BITFIELD(3);
    Direction _direction BITFIELD(3);
    bool _serverHasIgnoredFiles BITFIELD(1);

    /// Whether there's an entry in the blacklist table.
    /// Note: that entry may have retries left, so this can be true
    /// without the status being FileIgnored.
    bool _hasBlacklistEntry BITFIELD(1);

    /** If true and NormalError, this error may be blacklisted
     *
     * Note that non-local errors (httpErrorCode!=0) may also be
     * blacklisted independently of this flag.
     */
    bool _errorMayBeBlacklisted BITFIELD(1);

    // Variables useful to report to the user
    Status _status BITFIELD(4);
    bool _isRestoration BITFIELD(1); // The original operation was forbidden, and this is a restoration
    quint16 _httpErrorCode;
    RemotePermissions _remotePerm;
    QString _errorString; // Contains a string only in case of error
    QByteArray _responseTimeStamp;
    quint32 _affectedItems; // the number of affected items by the operation on this item.
    // usually this value is 1, but for removes on dirs, it might be much higher.

    // Variables used by the propagator
    csync_instructions_e _instruction;
    QString _originalFile; // as it is in the csync tree
    time_t _modtime;
    QByteArray _etag;
    quint64 _size;
    quint64 _inode;
    QByteArray _fileId;

    // This is the value for the 'new' side, matching with _size and _modtime.
    //
    // When is this set, and is it the local or the remote checksum?
    // - if mtime or size changed locally for *.eml files (local checksum)
    // - for potential renames of local files (local checksum)
    // - for conflicts (remote checksum)
    QByteArray _checksumHeader;

    // The size and modtime of the file getting overwritten (on the disk for downloads, on the server for uploads).
    quint64 _previousSize;
    time_t _previousModtime;

    QString _directDownloadUrl;
    QString _directDownloadCookies;
};

inline bool operator<(const SyncFileItemPtr &item1, const SyncFileItemPtr &item2)
{
    return *item1 < *item2;
}

typedef QVector<SyncFileItemPtr> SyncFileItemVector;
}

Q_DECLARE_METATYPE(OCC::SyncFileItem)
Q_DECLARE_METATYPE(OCC::SyncFileItemPtr)

#endif // SYNCFILEITEM_H
