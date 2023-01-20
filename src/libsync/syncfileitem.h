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

#include <set>

#include "common/syncjournaldb.h"
#include "common/utility.h"
#include "csync.h"

#include "owncloudlib.h"

namespace OCC {

class SyncFileItem;
class SyncJournalFileRecord;
typedef QSharedPointer<SyncFileItem> SyncFileItemPtr;

/**
 * @brief The SyncFileItem class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncFileItem
{
    Q_GADGET
public:
    enum Direction : uint8_t {
        None = 0,
        Up,
        Down
    };
    Q_ENUM(Direction)

    enum Status : uint8_t { // stored in 4 bits
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

        FileIgnored, ///< The file has an invalid name or is in the blacklisted with no retries left
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
        BlacklistedError,

        /**
         * The file is excluded by the ignore list
         */
        Excluded,

        /**
         *  Similar to SoftError but will not cause any error handling
         */
        Message,

        /** For use in an array or vector for the number of items in this enum.
         */
        StatusCount
    };
    Q_ENUM(Status)

    SyncJournalFileRecord toSyncJournalFileRecordWithInode(const QString &localFileName) const;

    /** Creates a basic SyncFileItem from a DB record
     *
     * This is intended in particular for read-update-write cycles that need
     * to go through a a SyncFileItem.
     */
    static SyncFileItemPtr fromSyncJournalFileRecord(const SyncJournalFileRecord &rec);


    SyncFileItem()
        : _type(ItemTypeSkip)
        , _direction(None)
        , _serverHasIgnoredFiles(false)
        , _hasBlacklistEntry(false)
        , _status(NoStatus)
        , _isRestoration(false)
        , _isSelectiveSync(false)
        , _httpErrorCode(0)
        , _affectedItems(1)
        , _instruction(CSYNC_INSTRUCTION_NONE)
        , _modtime(0)
        , _size(0)
        , _inode(0)
        , _previousSize(0)
        , _previousModtime(0)
        , _relevantDirectoyInstruction(false)
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

        if (data1[prefixL] == QLatin1Char('/'))
            return true;
        if (data2[prefixL] == QLatin1Char('/'))
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
        return _type == ItemTypeDirectory;
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
        return (!showInIssuesTab() || _status == SyncFileItem::Restoration)
            // Don't show conflicts that were resolved as "not a conflict after all"
            && !(_instruction == CSYNC_INSTRUCTION_CONFLICT && _status == SyncFileItem::Success);
    }

    // Variables useful for everybody

    /** The syncfolder-relative filesystem path that the operation is about
     *
     * For rename operation this is the rename source and the target is in _renameTarget.
     */
    QString _file;

    /** for renames: the name _file should be renamed to
     * for dehydrations: the name _file should become after dehydration (like adding a suffix)
     * otherwise empty. Use destination() to find the sync target.
     */
    QString _renameTarget;

    /** The db-path of this item.
     *
     * This can easily differ from _file and _renameTarget if parts of the path were renamed.
     */
    QString _originalFile;

    ItemType _type;
    Direction _direction;
    bool _serverHasIgnoredFiles;

    /// Whether there's an entry in the blacklist table.
    /// Note: that entry may have retries left, so this can be true
    /// without the status being FileIgnored.
    bool _hasBlacklistEntry;

    // Variables useful to report to the user
    Status _status;
    bool _isRestoration; // The original operation was forbidden, and this is a restoration
    bool _isSelectiveSync; // The file is removed or ignored because it is in the selective sync list
    quint16 _httpErrorCode;
    RemotePermissions _remotePerm;
    QString _errorString; // Contains a string only in case of error
    QString _messageString; // Contains a string only in case of hand crafted events
    QByteArray _responseTimeStamp;
    QByteArray _requestId; // X-Request-Id of the failed request
    quint32 _affectedItems; // the number of affected items by the operation on this item.
    // usually this value is 1, but for removes on dirs, it might be much higher.

    // Variables used by the propagator
    SyncInstructions _instruction;
    time_t _modtime;
    QString _etag;
    qint64 _size;
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
    qint64 _previousSize;
    time_t _previousModtime;

    QString _directDownloadUrl;
    QString _directDownloadCookies;

    bool _relevantDirectoyInstruction = false;
    bool _finished = false;

    auto toUploadInfo() const
    {
        SyncJournalDb::UploadInfo out;
        out._valid = true;
        out._modtime = _modtime;
        out._contentChecksum = _checksumHeader;
        out._size = _size;
        return out;
    }
};


template <>
OWNCLOUDSYNC_EXPORT QString Utility::enumToDisplayName(SyncFileItem::Status s);


inline bool operator<(const SyncFileItemPtr &item1, const SyncFileItemPtr &item2)
{
    return *item1 < *item2;
}

using SyncFileItemSet = std::set<SyncFileItemPtr>;
}

Q_DECLARE_METATYPE(OCC::SyncFileItemSet)
Q_DECLARE_METATYPE(OCC::SyncFileItem)
Q_DECLARE_METATYPE(OCC::SyncFileItemPtr)

OWNCLOUDSYNC_EXPORT QDebug operator<<(QDebug debug, const OCC::SyncFileItem *item);
inline QDebug operator<<(QDebug debug, const OCC::SyncFileItemPtr &item)
{
    return debug << item.data();
}


#endif // SYNCFILEITEM_H
