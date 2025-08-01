/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYNCFILEITEM_H
#define SYNCFILEITEM_H

#include <QVector>
#include <QString>
#include <QDateTime>
#include <QMetaType>
#include <QSharedPointer>

#include <csync.h>

#include <owncloudlib.h>

namespace OCC {

class SyncFileItem;
class SyncJournalFileRecord;
using SyncFileItemPtr = QSharedPointer<SyncFileItem>;

/**
 * @brief The SyncFileItem class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncFileItem
{
    Q_GADGET
public:
    enum Direction {
        None = 0,
        Up,
        Down
    };
    Q_ENUM(Direction)

    using EncryptionStatus = EncryptionStatusEnums::ItemEncryptionStatus;

    // Note: the order of these statuses is used for ordering in the SortedActivityListModel
    enum Status { // stored in 4 bits
        NoStatus,

        FatalError, ///< Error that causes the sync to stop
        NormalError, ///< Error attached to a particular file
        SoftError, ///< More like an information

        /** Marks a conflict, old or new.
         *
         * With instruction:IGNORE: detected an old unresolved old conflict
         * With instruction:CONFLICT: a new conflict this sync run
         */
        Conflict,

        FileIgnored, ///< The file is in the ignored list (or blacklisted with no retries left)
        FileLocked, ///< The file is locked
        Restoration, ///< The file was restored because what should have been done was not allowed

        /**
         * The filename is invalid on this platform and could not created.
         */
        FileNameInvalid,

        /**
         * The filename contains invalid characters and can not be uploaded to the server
         */
        FileNameInvalidOnServer,

        /**
         * There is a file name clash (e.g. attempting to download test.txt when TEST.TXT already exists
         * on a platform where the filesystem is case-insensitive
         */
        FileNameClash,

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

        Success, ///< The file was properly synced
    };
    Q_ENUM(Status)

    enum class LockStatus {
        UnlockedItem = 0,
        LockedItem = 1,
    };

    Q_ENUM(LockStatus)

    enum class LockOwnerType : int{
        UserLock = 0,
        AppLock = 1,
        TokenLock = 2,
    };

    Q_ENUM(LockOwnerType)

    [[nodiscard]] SyncJournalFileRecord toSyncJournalFileRecordWithInode(const QString &localFileName) const;

    /** Creates a basic SyncFileItem from a DB record
     *
     * This is intended in particular for read-update-write cycles that need
     * to go through a a SyncFileItem, like PollJob.
     */
    static SyncFileItemPtr fromSyncJournalFileRecord(const SyncJournalFileRecord &rec);

    /** Creates a basic SyncFileItem from remote properties
     */
    [[nodiscard]] static SyncFileItemPtr fromProperties(const QString &filePath, const QMap<QString, QString> &properties, RemotePermissions::MountedPermissionAlgorithm algorithm);


    SyncFileItem()
        : _type(ItemTypeSkip)
        , _direction(None)
        , _serverHasIgnoredFiles(false)
        , _hasBlacklistEntry(false)
        , _errorMayBeBlacklisted(false)
        , _status(NoStatus)
        , _isRestoration(false)
        , _isSelectiveSync(false)
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

    [[nodiscard]] QString destination() const
    {
        if (!_renameTarget.isEmpty()) {
            return _renameTarget;
        }
        return _file;
    }

    [[nodiscard]] bool isEmpty() const
    {
        return _file.isEmpty();
    }

    [[nodiscard]] bool isDirectory() const
    {
        return _type == ItemTypeDirectory;
    }

    /**
     * True if the item had any kind of error.
     */
    [[nodiscard]] bool hasErrorStatus() const
    {
        return _status == SyncFileItem::SoftError
            || _status == SyncFileItem::NormalError
            || _status == SyncFileItem::FatalError
            || !_errorString.isEmpty();
    }

    /**
     * Whether this item should appear on the issues tab.
     */
    [[nodiscard]] bool showInIssuesTab() const
    {
        return hasErrorStatus() || _status == SyncFileItem::Conflict;
    }

    /**
     * Whether this item should appear on the protocol tab.
     */
    [[nodiscard]] bool showInProtocolTab() const
    {
        return (!showInIssuesTab() || _status == SyncFileItem::Restoration)
            // Don't show conflicts that were resolved as "not a conflict after all"
            && !(_instruction == CSYNC_INSTRUCTION_CONFLICT && _status == SyncFileItem::Success);
    }

    [[nodiscard]] bool isEncrypted() const { return _e2eEncryptionStatus != EncryptionStatus::NotEncrypted; }

    void updateLockStateFromDbRecord(const SyncJournalFileRecord &dbRecord);

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

    /// Whether there's end to end encryption on this file.
    /// If the file is encrypted, the _encryptedFilename is
    /// the encrypted name on the server.
    QString _encryptedFileName;

    ItemType _type BITFIELD(3);
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
    bool _isSelectiveSync BITFIELD(1); // The file is removed or ignored because it is in the selective sync list
    EncryptionStatus _e2eEncryptionStatus = EncryptionStatus::NotEncrypted; // The file is E2EE or the content of the directory should be E2EE
    EncryptionStatus _e2eEncryptionServerCapability = EncryptionStatus::NotEncrypted;
    EncryptionStatus _e2eEncryptionStatusRemote = EncryptionStatus::NotEncrypted;
    quint16 _httpErrorCode = 0;
    RemotePermissions _remotePerm;
    QString _errorString; // Contains a string only in case of error
    QString _errorExceptionName; // Contains a server exception string only in case of error
    QString _errorExceptionMessage; // Contains a server exception message string only in case of error
    QByteArray _responseTimeStamp;
    QByteArray _requestId; // X-Request-Id of the failed request
    quint32 _affectedItems = 1; // the number of affected items by the operation on this item.
    // usually this value is 1, but for removes on dirs, it might be much higher.

    // Variables used by the propagator
    SyncInstructions _instruction = CSYNC_INSTRUCTION_NONE;
    time_t _modtime = 0;
    QByteArray _etag;
    qint64 _size = 0;
    quint64 _inode = 0;
    QByteArray _fileId;

    // This is the value for the 'new' side, matching with _size and _modtime.
    //
    // When is this set, and is it the local or the remote checksum?
    // - if mtime or size changed locally for *.eml files (local checksum)
    // - for potential renames of local files (local checksum)
    // - for conflicts (remote checksum)
    QByteArray _checksumHeader;

    // The size and modtime of the file getting overwritten (on the disk for downloads, on the server for uploads).
    qint64 _previousSize = 0;
    time_t _previousModtime = 0;

    QString _directDownloadUrl;
    QString _directDownloadCookies;

    LockStatus _locked = LockStatus::UnlockedItem;
    QString _lockOwnerId;
    QString _lockOwnerDisplayName;
    LockOwnerType _lockOwnerType = LockOwnerType::UserLock;
    QString _lockEditorApp;
    qint64 _lockTime = 0;
    qint64 _lockTimeout = 0;
    QString _lockToken;

    bool _isShared = false;
    time_t _lastShareStateFetchedTimestamp = 0;

    bool _sharedByMe = false;

    bool _isFileDropDetected = false;

    bool _isEncryptedMetadataNeedUpdate = false;

    bool _isAnyInvalidCharChild = false;
    bool _isAnyCaseClashChild = false;

    bool _isLivePhoto = false;
    QString _livePhotoFile;

    bool isPermissionsInvalid = false;

    QString _discoveryResult;

    /// if true, requests the file to be permanently deleted instead of moved to the trashbin
    /// only relevant for when `_instruction` is set to `CSYNC_INSTRUCTION_REMOVE`
    bool _wantsPermanentDeletion = false;

    struct FolderQuota {
        int64_t bytesUsed = -1;
        int64_t bytesAvailable = -1;
    };

    FolderQuota _folderQuota;
};

inline bool operator<(const SyncFileItemPtr &item1, const SyncFileItemPtr &item2)
{
    return *item1 < *item2;
}

using SyncFileItemVector = QVector<SyncFileItemPtr>;
}

Q_DECLARE_METATYPE(OCC::SyncFileItem)
Q_DECLARE_METATYPE(OCC::SyncFileItemPtr)

#endif // SYNCFILEITEM_H
