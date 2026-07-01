#ifndef REPAIRFOLDERENCRYPTIONMETADATAJOB_H
#define REPAIRFOLDERENCRYPTIONMETADATAJOB_H

#include <QObject>

#include "accountfwd.h"
#include "syncfileitem.h"
#include "encryptedfoldermetadatahandler.h"

namespace OCC
{

class SyncJournalDb;
class OwncloudPropagator;

class OWNCLOUDSYNC_EXPORT RepairFolderEncryptionMetadataJob : public QObject
{
    Q_OBJECT
public:
    enum class Status {
        Success = 1,
        Error = -1,
    };
    Q_ENUM(Status)

    explicit RepairFolderEncryptionMetadataJob(const AccountPtr &account,
                                               SyncJournalDb *journal,
                                               const QString &path,
                                               const QString &pathNonEncrypted,
                                               const QString &remoteSyncRootPath,
                                               const QByteArray &fileId,
                                               OwncloudPropagator *propagator = nullptr,
                                               SyncFileItemPtr item = {},
                                               QObject *parent = nullptr);

    void start();

    [[nodiscard]] QString errorString() const;

Q_SIGNALS:
    void finished(int status);

private Q_SLOTS:
    void metadataUploadFinished();

private:
    AccountPtr _account;
    SyncJournalDb *_journal = nullptr;
    QString _path;
    QString _pathNonEncrypted;
    QString _remoteSyncRootPath;
    QByteArray _fileId;
    QString _errorString;
    OwncloudPropagator *_propagator = nullptr;
    SyncFileItemPtr _item;
    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
};

} // namespace OCC

#endif // REPAIRFOLDERENCRYPTIONMETADATAJOB_H
