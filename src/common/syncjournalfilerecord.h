/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SYNCJOURNALFILERECORD_H
#define SYNCJOURNALFILERECORD_H

#include <QString>
#include <QDateTime>

#include "csync.h"
#include "ocsynclib.h"
#include "remotepermissions.h"
#include "common/utility.h"

namespace OCC {

class SyncFileItem;

struct SyncJournalFileLockInfo {
    bool _locked = false;
    QString _lockOwnerDisplayName;
    QString _lockOwnerId;
    qint64 _lockOwnerType = 0;
    QString _lockEditorApp;
    qint64 _lockTime = 0;
    qint64 _lockTimeout = 0;
    QString _lockToken;
};

/**
 * @brief The SyncJournalFileRecord class
 * @ingroup libsync
 */
class OCSYNC_EXPORT SyncJournalFileRecord
{
public:
    [[nodiscard]] bool isValid() const
    {
        return !_path.isEmpty();
    }

    using EncryptionStatus = EncryptionStatusEnums::JournalDbEncryptionStatus;

    /** Returns the numeric part of the full id in _fileId.
     *
     * On the server this is sometimes known as the internal file id.
     *
     * It is used in the construction of private links.
     */
    [[nodiscard]] QByteArray numericFileId() const;
    [[nodiscard]] QDateTime modDateTime() const { return Utility::qDateTimeFromTime_t(_modtime); }

    [[nodiscard]] bool isDirectory() const { return _type == ItemTypeDirectory; }
    [[nodiscard]] bool isFile() const { return _type == ItemTypeFile || _type == ItemTypeVirtualFileDehydration; }
    [[nodiscard]] bool isVirtualFile() const { return _type == ItemTypeVirtualFile || _type == ItemTypeVirtualFileDownload; }
    [[nodiscard]] QString path() const { return QString::fromUtf8(_path); }
    [[nodiscard]] QString e2eMangledName() const { return QString::fromUtf8(_e2eMangledName); }
    [[nodiscard]] bool isE2eEncrypted() const { return _e2eEncryptionStatus != EncryptionStatus::NotEncrypted; }

    QByteArray _path;
    quint64 _inode = 0;
    qint64 _modtime = 0;
    ItemType _type = ItemTypeSkip;
    QByteArray _etag;
    QByteArray _fileId;
    qint64 _fileSize = 0;
    RemotePermissions _remotePerm;
    bool _serverHasIgnoredFiles = false;
    QByteArray _checksumHeader;
    QByteArray _e2eMangledName;
    EncryptionStatus _e2eEncryptionStatus = EncryptionStatus::NotEncrypted;
    SyncJournalFileLockInfo _lockstate;
    bool _isShared = false;
    qint64 _lastShareStateFetchedTimestamp = 0;
    bool _sharedByMe = false;
    bool _isLivePhoto = false;
    QString _livePhotoFile;
    struct FolderQuota {
        qint64 bytesUsed = -1;
        qint64 bytesAvailable = -1;
    };
    FolderQuota _folderQuota;
};

QDebug& operator<<(QDebug &stream, const SyncJournalFileRecord::EncryptionStatus status);

bool OCSYNC_EXPORT
operator==(const SyncJournalFileRecord &lhs,
    const SyncJournalFileRecord &rhs);

class OCSYNC_EXPORT SyncJournalErrorBlacklistRecord
{
public:
    enum Category {
        /// Normal errors have no special behavior
        Normal = 0,
        /// These get a special summary message
        InsufficientRemoteStorage
    };

    /// The number of times the operation was unsuccessful so far.
    int _retryCount = 0;

    /// The last error string.
    QString _errorString;
    /// The error category. Sometimes used for special actions.
    Category _errorCategory = Category::Normal;

    qint64 _lastTryModtime = 0;
    QByteArray _lastTryEtag;

    /// The last time the operation was attempted (in s since epoch).
    qint64 _lastTryTime = 0;

    /// The number of seconds the file shall be ignored.
    qint64 _ignoreDuration = 0;

    QString _file;
    QString _renameTarget;

    /// The last X-Request-ID of the request that failed
    QByteArray _requestId;

    [[nodiscard]] bool isValid() const;
};

/** Represents a conflict in the conflicts table.
 *
 * In the following the "conflict file" is the file that has the conflict
 * tag in the filename, and the base file is the file that it's a conflict for.
 * So if "a/foo.txt" is the base file, its conflict file could be
 * "a/foo (conflicted copy 1234).txt".
 */
class OCSYNC_EXPORT ConflictRecord
{
public:
    /** Path to the file with the conflict tag in the name
     *
     * The path is sync-folder relative.
     */
    QByteArray path;

    /// File id of the base file
    QByteArray baseFileId;

    /** Modtime of the base file
     *
     * may not be available and be -1
     */
    qint64 baseModtime = -1;

    /** Etag of the base file
     *
     * may not be available and empty
     */
    QByteArray baseEtag;

    /**
     * The path of the original file at the time the conflict was created
     *
     * Note that in nearly all cases one should query the db by baseFileId and
     * thus retrieve the *current* base path instead!
     *
     * maybe be empty if not available
     */
    QByteArray initialBasePath;


    [[nodiscard]] bool isValid() const { return !path.isEmpty(); }
};
}

#endif // SYNCJOURNALFILERECORD_H
