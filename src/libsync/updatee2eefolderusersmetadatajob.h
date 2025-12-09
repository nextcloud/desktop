/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"
#include "encryptedfoldermetadatahandler.h" //NOTE: Forward declarion is not gonna work because of OWNCLOUDSYNC_EXPORT for UpdateE2eeFolderUsersMetadataJob
#include "gui/sharemanager.h"
#include "syncfileitem.h"
#include "gui/sharee.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QScopedPointer>
#include <QString>

class QSslCertificate;
namespace OCC
{
class SyncJournalDb;
class OWNCLOUDSYNC_EXPORT UpdateE2eeFolderUsersMetadataJob : public QObject
{
    Q_OBJECT
public:
    enum Operation { Invalid = -1, Add = 0, Remove, ReEncrypt };

    struct UserData {
        ShareePtr sharee;
        Share::Permissions desiredPermissions;
        QString password;
    };
    
    explicit UpdateE2eeFolderUsersMetadataJob(const AccountPtr &account, SyncJournalDb *journalDb,const QString &syncFolderRemotePath, const Operation operation, const QString &fullRemotePath = {}, const QString &folderUserId = {}, const QSslCertificate &certificate = QSslCertificate{}, QObject *parent = nullptr);
    ~UpdateE2eeFolderUsersMetadataJob() override = default;

public:
    [[nodiscard]] const QString &path() const;
    [[nodiscard]] const UserData &userData() const;
    [[nodiscard]] SyncFileItem::EncryptionStatus encryptionStatus() const;
    [[nodiscard]] const QByteArray folderToken() const;
    
    void unlockFolder(const EncryptedFolderMetadataHandler::UnlockFolderWithResult result);

public slots:
    void start(const bool keepLock = false);
    void setUserData(const OCC::UpdateE2eeFolderUsersMetadataJob::UserData &userData);

    void setFolderToken(const QByteArray &folderToken);
    void setMetadataKeyForEncryption(const QByteArray &metadataKey);
    void setMetadataKeyForDecryption(const QByteArray &metadataKey);
    void setKeyChecksums(const QSet<QByteArray> &keyChecksums);

    void setSubJobSyncItems(const QHash<QString, OCC::SyncFileItemPtr> &subJobSyncItems);

private:
    void scheduleSubJobs();
    void startUpdate();
    void subJobsFinished(bool success);

private slots:
    void slotStartE2eeMetadataJobs();
    void slotFetchMetadataJobFinished(int statusCode, const QString &message);

    void slotSubJobFinished(int code, const QString &message = {});

    void slotFolderUnlocked(const QByteArray &folderId, int httpStatus);

    void slotUpdateMetadataFinished(int code, const QString &message = {});
    void slotCertificatesFetchedFromServer(const QHash<QString, OCC::NextcloudSslCertificate> &results);
    void slotCertificateFetchedFromKeychain(const QSslCertificate &certificate);

private: signals:
    void certificateReady();
    void finished(int code, const QString &message = {});
    void folderUnlocked();

private:
    AccountPtr _account;
    QPointer<SyncJournalDb> _journalDb;
    QString _syncFolderRemotePath;
    Operation _operation = Invalid;
    QString _fullRemotePath;
    QString _folderUserId;
    QSslCertificate _folderUserCertificate;
    QByteArray _folderToken;
    // needed when re-encrypting nested folders' metadata after top-level folder's metadata has changed
    QByteArray _metadataKeyForEncryption;
    QByteArray _metadataKeyForDecryption;
    QSet<QByteArray> _keyChecksums;
    //-------------------------------------------------------------------------------------------------
    QSet<UpdateE2eeFolderUsersMetadataJob *> _subJobs;
    UserData _userData; // share info, etc.
    QHash<QString, SyncFileItemPtr> _subJobSyncItems; //used when migrating to update corresponding SyncFileItem(s) for nested folders, such that records in db will get updated when propagate item job is finalized
    QMutex _subJobSyncItemsMutex;
    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
    bool _keepLock = false;
};

}
