/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "updatee2eesharemetadatajob.h"
#include "clientsideencryption.h"
#include "clientsideencryptionjobs.h"
#include "foldermetadata.h"
#include "folderman.h"

namespace OCC
{
Q_LOGGING_CATEGORY(lcUpdateE2eeShareMetadataJob, "nextcloud.gui.updatee2eesharemetadatajob", QtInfoMsg)

UpdateE2eeShareMetadataJob::UpdateE2eeShareMetadataJob(const AccountPtr &account,
                                const QByteArray &folderId,
                                const QString &folderAlias,
                                const ShareePtr &sharee,
                                const Operation operation,
                                const QString &sharePath,
                                const Share::Permissions desiredPermissions,
                                const QString &password,
                                QObject *parent)
    : QObject{parent}
    , _account(account)
    , _folderId(folderId)
    , _folderAlias(folderAlias)
    , _sharee(sharee)
    , _operation(operation)
    , _sharePath(sharePath)
    , _desiredPermissions(desiredPermissions)
    , _password(password)
{
    if (_operation == Operation::Add) {
        connect(this, &UpdateE2eeShareMetadataJob::certificateReady, this, &UpdateE2eeShareMetadataJob::slotCertificateReady);
    }
    connect(this, &UpdateE2eeShareMetadataJob::finished, this, &UpdateE2eeShareMetadataJob::deleteLater);
}

QString UpdateE2eeShareMetadataJob::password() const
{
    return _password;
}

QString UpdateE2eeShareMetadataJob::sharePath() const
{
    return _sharePath;
}

Share::Permissions UpdateE2eeShareMetadataJob::desiredPermissions() const
{
    return _desiredPermissions;
}

ShareePtr UpdateE2eeShareMetadataJob::sharee() const
{
    return _sharee;
}

void UpdateE2eeShareMetadataJob::start()
{
    _folder = FolderMan::instance()->folder(_folderAlias);
    if (!_folder || !_folder->journalDb()) {
        emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_folderId)));
        return;
    }

    if (_operation == Operation::Add) {
        _account->e2e()->fetchFromKeyChain(_account, _sharee->shareWith());
        connect(_account->e2e(), &ClientSideEncryption::certificateFetchedFromKeychain, this, &UpdateE2eeShareMetadataJob::slotCertificateFetchedFromKeychain);
    } else if (_operation == Operation::Remove || _operation == Operation::ReEncrypt) {
        slotFetchFolderMetadata();
    } else {
        emit finished(404, tr("Invalid share metadata operation for a folder user %1, for folder %2").arg(_sharee->shareWith()).arg(QString::fromUtf8(_folderId)));
    }
}

void UpdateE2eeShareMetadataJob::setMetadataKeyOverride(const QByteArray &metadataKeyOverride)
{
    _metadataKeyOverride = metadataKeyOverride;
}

void UpdateE2eeShareMetadataJob::slotCertificateFetchedFromKeychain(const QSslCertificate certificate)
{
    disconnect(_account->e2e(), &ClientSideEncryption::certificateFetchedFromKeychain, this, &UpdateE2eeShareMetadataJob::slotCertificateFetchedFromKeychain);
    if (certificate.isNull()) {
        // get sharee's public key
        _account->e2e()->getUsersPublicKeyFromServer(_account, {_sharee->shareWith()});
        connect(_account->e2e(), &ClientSideEncryption::certificatesFetchedFromServer, this, &UpdateE2eeShareMetadataJob::slotCertificatesFetchedFromServer);
        return;
    }
    emit certificateReady(certificate);
}

void UpdateE2eeShareMetadataJob::slotCertificatesFetchedFromServer(const QHash<QString, QSslCertificate> &results)
{
    const auto certificate = results.isEmpty() ? QSslCertificate{} : results.value(_sharee->shareWith());
    if (certificate.isNull()) {
        emit certificateReady(certificate);
        return;
    }
    _account->e2e()->writeCertificate(_account, _sharee->shareWith(), certificate);
    connect(_account->e2e(), &ClientSideEncryption::certificateWriteComplete, this, &UpdateE2eeShareMetadataJob::certificateReady);
}

void UpdateE2eeShareMetadataJob::slotCertificateReady(const QSslCertificate certificate)
{
    _shareeCertificate = certificate;

    if (certificate.isNull()) {
        emit finished(404, tr("Could not fetch publicKey for user %1").arg(_sharee->shareWith()));
    } else {
        slotFetchFolderMetadata();
    }
}

void UpdateE2eeShareMetadataJob::slotFetchFolderMetadata()
{
    const auto job = new GetMetadataApiJob(_account, _folderId);
    connect(job, &GetMetadataApiJob::jsonReceived, this, &UpdateE2eeShareMetadataJob::slotMetadataReceived);
    connect(job, &GetMetadataApiJob::error, this, &UpdateE2eeShareMetadataJob::slotMetadataError);
    job->start();
}

void UpdateE2eeShareMetadataJob::slotMetadataReceived(const QJsonDocument &json, int statusCode)
{
    qCDebug(lcUpdateE2eeShareMetadataJob) << "Metadata received, applying it to the result list";
    if (!_folder || !_folder->journalDb()) {
        emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_folderId)));
        return;
    }
    _folderMetadata.reset(new FolderMetadata(_account, statusCode == 404 ? QByteArray{} : json.toJson(QJsonDocument::Compact), _sharePath, {}));
    if (_folderMetadata->versionFromMetadata() < 2) {
        emit finished(405, tr("Could not share legacy encrypted folder %1. Migration is required.").arg(_sharePath));
        return;
    }
    connect(_folderMetadata.data(), &FolderMetadata::setupComplete, this, [this] {
        if (!_folder || !_folder->journalDb()) {
            emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_folderId)));
            return;
        }

        if (_operation == Operation::Add) {
            if (!_folderMetadata->addUser(_sharee->shareWith(), _shareeCertificate)) {
                emit finished(403, tr("Could not add a folder user %1, for folder %2").arg(_sharee->shareWith()).arg(_sharePath));
                return;
            }
        } else if (_operation == Operation::Remove) {
            if (!_folderMetadata->removeUser(_sharee->shareWith())) {
                emit finished(403, tr("Could not remove a folder user %1, for folder %2").arg(_sharee->shareWith()).arg(_sharePath));
                return;
            }
        } else if (_operation == Operation::ReEncrypt) {
            _folderMetadata->setMetadataKeyOverride(_metadataKeyOverride);
            slotLockFolder();
            return;
        } else {
            emit finished(400, tr("Invalid share metadata operation for a folder user %1, for folder %2").arg(_sharee->shareWith()).arg(_sharePath));
            return;
        }

        const auto shareDbPath = _sharePath.mid(_folder->remotePath().size());

        const auto result = _folder->journalDb()->getFilesBelowPath(shareDbPath.toUtf8(), [this](const SyncJournalFileRecord &record) -> void {
            if (record.isDirectory()) {
                const auto removeE2eeShareJob = new UpdateE2eeShareMetadataJob(_account,
                                                                               record._fileId,
                                                                               _folderAlias,
                                                                               _sharee,
                                                                               UpdateE2eeShareMetadataJob::ReEncrypt,
                                                                               QString::fromUtf8(record._path));
                removeE2eeShareJob->setMetadataKeyOverride(_folderMetadata->metadataKey());
                removeE2eeShareJob->setParent(this);
                const auto fileId = record._fileId;
                _subJobs.insert(fileId, removeE2eeShareJob);
                connect(removeE2eeShareJob, &UpdateE2eeShareMetadataJob::finished, [this, fileId](int code, const QString &message) {
                    Q_UNUSED(code);
                    Q_UNUSED(message);
                    const auto job = _subJobs.take(fileId);
                    job->deleteLater();
                    if (_subJobs.isEmpty()) {
                        slotSubJobsFinished();
                    } else {
                        _subJobs.values().last()->start();
                    }
                });
            }
        });

        if (!result || _subJobs.isEmpty()) {
            slotLockFolder();
        } else {
            _subJobs.values().last()->start();
        }
    });
}

void UpdateE2eeShareMetadataJob::slotMetadataError(const QByteArray &folderId, int httpReturnCode)
{
    qCWarning(lcUpdateE2eeShareMetadataJob) << "E2EE Metadata job error. Trying to proceed without it." << folderId << httpReturnCode;
    emit finished(404, tr("Could not fetch metadata for folder %1").arg(QString::fromUtf8(_folderId)));
}
void UpdateE2eeShareMetadataJob::slotLockFolder()
{
    if (!_folder || !_folder->journalDb()) {
        emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_folderId)));
        return;
    }
    const auto lockJob = new LockEncryptFolderApiJob(_account, _folderId, _folder->journalDb(), _account->e2e()->_publicKey, this);
    connect(lockJob, &LockEncryptFolderApiJob::success, this, &UpdateE2eeShareMetadataJob::slotFolderLockedSuccessfully);
    connect(lockJob, &LockEncryptFolderApiJob::error, this, &UpdateE2eeShareMetadataJob::slotFolderLockedError);
    lockJob->start();
}

void UpdateE2eeShareMetadataJob::slotUnlockFolder()
{
    if (!_folder || !_folder->journalDb()) {
        emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_folderId)));
        return;
    }

    qCDebug(lcUpdateE2eeShareMetadataJob) << "Calling Unlock";
    const auto unlockJob = new UnlockEncryptFolderApiJob(_account, _folderId, _folderToken, _folder->journalDb(), this);

    connect(unlockJob, &UnlockEncryptFolderApiJob::success, [this](const QByteArray &folderId) {
        qCDebug(lcUpdateE2eeShareMetadataJob) << "Successfully Unlocked";
        _folderToken = "";
        _folderId = "";

        slotFolderUnlocked(folderId, 200);
    });
    connect(unlockJob, &UnlockEncryptFolderApiJob::error, [this](const QByteArray &folderId, int httpStatus) {
        qCDebug(lcUpdateE2eeShareMetadataJob) << "Unlock Error";

        slotFolderUnlocked(folderId, httpStatus);
    });
    unlockJob->start();
}

void UpdateE2eeShareMetadataJob::slotFolderLockedSuccessfully(const QByteArray &folderId, const QByteArray &token)
{
    qCDebug(lcUpdateE2eeShareMetadataJob) << "Folder" << folderId << "Locked Successfully for Upload, Fetching Metadata";
    _folderToken = token;
    slotUpdateFolderMetadata();
}

void UpdateE2eeShareMetadataJob::slotUpdateMetadataSuccess(const QByteArray &folderId)
{
    Q_UNUSED(folderId);
    qCDebug(lcUpdateE2eeShareMetadataJob) << "Uploading of the metadata success, Encrypting the file";
    slotUnlockFolder();
}

void UpdateE2eeShareMetadataJob::slotUpdateMetadataError(const QByteArray &folderId, int httpErrorResponse)
{
    qCDebug(lcUpdateE2eeShareMetadataJob) << "Update metadata error for folder" << folderId << "with error" << httpErrorResponse;
    qCDebug(lcUpdateE2eeShareMetadataJob()) << "Unlocking the folder.";
    slotUnlockFolder();
}

void UpdateE2eeShareMetadataJob::slotFolderUnlocked(const QByteArray &folderId, int httpStatus)
{
    const QString message = httpStatus != 200 ? tr("Failed to unlock a folder.") : QString{};
    emit finished(httpStatus, message);
}

void UpdateE2eeShareMetadataJob::slotUpdateFolderMetadata()
{
    _folderMetadata->encryptMetadata();
    connect(_folderMetadata.data(), &FolderMetadata::encryptionFinished, this, [this](const QByteArray encryptedMetadata) {
        const auto job = new UpdateMetadataApiJob(_account, _folderId, encryptedMetadata, _folderToken);
        connect(job, &UpdateMetadataApiJob::success, this, &UpdateE2eeShareMetadataJob::slotUpdateMetadataSuccess);
        connect(job, &UpdateMetadataApiJob::error, this, &UpdateE2eeShareMetadataJob::slotUpdateMetadataError);
        job->start();
    });
}

void UpdateE2eeShareMetadataJob::slotSubJobsFinished()
{
    slotLockFolder();
}

void UpdateE2eeShareMetadataJob::slotFolderLockedError(const QByteArray &folderId, int httpErrorCode)
{
    Q_UNUSED(httpErrorCode);
    qCDebug(lcUpdateE2eeShareMetadataJob) << "Folder" << folderId << "Couldn't be locked.";
    emit finished(404, tr("Could not lock a folder %1").arg(QString::fromUtf8(_folderId)));
}

}
