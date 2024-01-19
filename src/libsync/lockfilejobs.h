#ifndef LOCKFILEJOBS_H
#define LOCKFILEJOBS_H

#include "abstractnetworkjob.h"

#include "syncfileitem.h"

class QXmlStreamReader;

namespace OCC {

class SyncJournalDb;

class OWNCLOUDSYNC_EXPORT LockFileJob : public AbstractNetworkJob
{
    Q_OBJECT

public:
    static constexpr auto LOCKED_HTTP_ERROR_CODE = 423;
    static constexpr auto PRECONDITION_FAILED_ERROR_CODE = 412;

    explicit LockFileJob(const AccountPtr account,
                         SyncJournalDb* const journal,
                         const QString &path,
                         const QString &remoteSyncPathWithTrailingSlash,
                         const QString &localSyncPath,
                         const SyncFileItem::LockStatus requestedLockState,
                         const SyncFileItem::LockOwnerType lockOwnerType,
                         QObject *parent = nullptr);
    void start() override;

signals:
    void finishedWithError(int httpErrorCode,
                           const QString &errorString,
                           const QString &lockOwnerName);
    void finishedWithoutError();

private:
    bool finished() override;

    void setFileRecordLocked(SyncJournalFileRecord &record) const;

    SyncJournalFileRecord handleReply();

    void resetState();

    void decodeStartElement(const QString &name,
                            QXmlStreamReader &reader);

    SyncJournalDb* _journal = nullptr;
    SyncFileItem::LockStatus _requestedLockState = SyncFileItem::LockStatus::LockedItem;
    SyncFileItem::LockOwnerType _requestedLockOwnerType = SyncFileItem::LockOwnerType::UserLock;

    SyncFileItem::LockStatus _lockStatus = SyncFileItem::LockStatus::UnlockedItem;
    SyncFileItem::LockOwnerType _lockOwnerType = SyncFileItem::LockOwnerType::UserLock;
    QString _userDisplayName;
    QString _editorName;
    QString _userId;
    QByteArray _etag;
    qint64 _lockTime = 0;
    qint64 _lockTimeout = 0;
    QString _remoteSyncPathWithTrailingSlash;
    QString _localSyncPath;
};

}

#endif // LOCKFILEJOBS_H
