/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "account.h"
#include "updatee2eefolderusersmetadatajob.h"
#include "foldermetadata.h"
#include "common/syncjournalfilerecord.h"
#include "common/syncjournaldb.h"

#include <QSslCertificate>

namespace OCC
{
Q_LOGGING_CATEGORY(lcUpdateE2eeFolderUsersMetadataJob, "nextcloud.gui.updatee2eefolderusersmetadatajob", QtInfoMsg)

UpdateE2eeFolderUsersMetadataJob::UpdateE2eeFolderUsersMetadataJob(const AccountPtr &account,
                                                       SyncJournalDb *journalDb,
                                                       const QString &syncFolderRemotePath,
                                                       const Operation operation,
                                                       const QString &fullRemotePath,
                                                       const QString &folderUserId,
                                                       const QSslCertificate &certificate,
                                                       QObject *parent)
    : QObject(parent)
    , _account(account)
    , _journalDb(journalDb)
    , _syncFolderRemotePath(Utility::noLeadingSlashPath(Utility::noTrailingSlashPath(syncFolderRemotePath)))
    , _operation(operation)
    , _fullRemotePath(Utility::noLeadingSlashPath(fullRemotePath))
    , _folderUserId(folderUserId)
    , _folderUserCertificate(certificate)
{
    Q_ASSERT(_syncFolderRemotePath == QStringLiteral("/") || _fullRemotePath.startsWith(_syncFolderRemotePath));
    SyncJournalFileRecord rec;
    if (!_journalDb->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(_fullRemotePath, _syncFolderRemotePath), &rec) || !rec.isValid()) {
        qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Could not get root E2ee folder recort for path" << _fullRemotePath;
        return;
    }
    _encryptedFolderMetadataHandler.reset(new EncryptedFolderMetadataHandler(_account, _fullRemotePath, _syncFolderRemotePath, _journalDb, rec.path()));
}

void UpdateE2eeFolderUsersMetadataJob::start(const bool keepLock)
{
    qCWarning(lcUpdateE2eeFolderUsersMetadataJob) << "[DEBUG_LEAVE_SHARE]: UpdateE2eeFolderUsersMetadataJob::start";

    if (!_encryptedFolderMetadataHandler) {
        emit finished(-1, tr("Error updating metadata for a folder %1").arg(_fullRemotePath));
        return;
    }

    if (keepLock) {
        connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::folderUnlocked, this, &UpdateE2eeFolderUsersMetadataJob::deleteLater);
    } else {
        connect(this, &UpdateE2eeFolderUsersMetadataJob::slotFolderUnlocked, this, &UpdateE2eeFolderUsersMetadataJob::deleteLater);
    }
    _keepLock = keepLock;
    if (_operation != Operation::Add && _operation != Operation::Remove && _operation != Operation::ReEncrypt) {
        emit finished(-1, tr("Error updating metadata for a folder %1").arg(_fullRemotePath));
        return;
    }

    if (_operation == Operation::Add) {
        connect(this, &UpdateE2eeFolderUsersMetadataJob::certificateReady, this, &UpdateE2eeFolderUsersMetadataJob::slotStartE2eeMetadataJobs);
        if (!_folderUserCertificate.isNull()) {
            emit certificateReady();
            return;
        }
        connect(_account->e2e(), &ClientSideEncryption::certificateFetchedFromKeychain,
            this, &UpdateE2eeFolderUsersMetadataJob::slotCertificateFetchedFromKeychain);
        _account->e2e()->fetchCertificateFromKeyChain(_account, _folderUserId);
        return;
    }
    slotStartE2eeMetadataJobs();
}

void UpdateE2eeFolderUsersMetadataJob::slotStartE2eeMetadataJobs()
{
    if (_operation == Operation::Add && _folderUserCertificate.isNull()) {
        emit finished(404, tr("Could not fetch public key for user %1").arg(_folderUserId));
        return;
    }

    const auto folderPathRelative = Utility::fullRemotePathToRemoteSyncRootRelative(_fullRemotePath, _syncFolderRemotePath);
    SyncJournalFileRecord rec;
    if (!_journalDb->getRootE2eFolderRecord(Utility::fullRemotePathToRemoteSyncRootRelative(folderPathRelative, _syncFolderRemotePath), &rec) || !rec.isValid()) {
        emit finished(404, tr("Could not find root encrypted folder for folder %1").arg(_fullRemotePath));
        return;
    }

    const auto rootEncFolderInfo = RootEncryptedFolderInfo(RootEncryptedFolderInfo::createRootPath(folderPathRelative, rec.path()), _metadataKeyForEncryption, _metadataKeyForDecryption, _keyChecksums);
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::fetchFinished,
            this, &UpdateE2eeFolderUsersMetadataJob::slotFetchMetadataJobFinished);
    _encryptedFolderMetadataHandler->fetchMetadata(rootEncFolderInfo, EncryptedFolderMetadataHandler::FetchMode::AllowEmptyMetadata);
}

void UpdateE2eeFolderUsersMetadataJob::slotFetchMetadataJobFinished(int statusCode, const QString &message)
{
    qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Metadata Received, Preparing it for the new file." << message;

    if (statusCode != 200) {
        qCritical(lcUpdateE2eeFolderUsersMetadataJob) << "fetch metadata finished with error" << statusCode << message;
        emit finished(-1, tr("Error updating metadata for a folder %1").arg(_fullRemotePath));
        return;
    }

    if (!_encryptedFolderMetadataHandler->folderMetadata() || !_encryptedFolderMetadataHandler->folderMetadata()->isValid()) {
        emit finished(403, tr("Could not add or remove user %1 to access folder %2").arg(_folderUserId).arg(_fullRemotePath));
        return;
    }
    startUpdate();
}

void UpdateE2eeFolderUsersMetadataJob::startUpdate()
{
    if (_operation == Operation::Invalid) {
        qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Invalid operation";
        emit finished(-1, tr("Error updating metadata for a folder %1").arg(_fullRemotePath));
        return;
    }

    if (_operation == Operation::Add || _operation == Operation::Remove) {
        if (!_encryptedFolderMetadataHandler->folderMetadata()) {
            qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Metadata is null";
            emit finished(-1, tr("Error updating metadata for a folder %1").arg(_fullRemotePath));
            return;
        }

        const auto certificateType = _account->e2e()->useTokenBasedEncryption() ?
            FolderMetadata::CertificateType::HardwareCertificate : FolderMetadata::CertificateType::SoftwareNextcloudCertificate;

        const auto result = _operation == Operation::Add
            ? _encryptedFolderMetadataHandler->folderMetadata()->addUser(_folderUserId, _folderUserCertificate, certificateType)
            : _encryptedFolderMetadataHandler->folderMetadata()->removeUser(_folderUserId);

        if (!result) {
            qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Could not perform operation" << _operation << "on metadata";
            emit finished(-1, tr("Error updating metadata for a folder %1").arg(_fullRemotePath));
            return;
        }

    }
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::uploadFinished,
            this, &UpdateE2eeFolderUsersMetadataJob::slotUpdateMetadataFinished);
    _encryptedFolderMetadataHandler->setFolderToken(_folderToken);
    _encryptedFolderMetadataHandler->uploadMetadata(EncryptedFolderMetadataHandler::UploadMode::KeepLock);
}

void UpdateE2eeFolderUsersMetadataJob::slotUpdateMetadataFinished(int code, const QString &message)
{
    if (code != 200) {
        qCWarning(lcUpdateE2eeFolderUsersMetadataJob) << "Update metadata error for folder" << _encryptedFolderMetadataHandler->folderId() << "with error"
                                                    << code << message;
        
        if (_operation == Operation::Add || _operation == Operation::Remove) {
            qCDebug(lcUpdateE2eeFolderUsersMetadataJob()) << "Unlocking the folder.";
            unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Failure);
        } else {
            emit finished(code, tr("Error updating metadata for a folder %1").arg(_fullRemotePath) + QStringLiteral(":%1").arg(message));
        }
        return;
    }

    qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Uploading of the metadata success.";
    if (_operation == Operation::Add || _operation == Operation::Remove) {
        qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Trying to schedule more jobs.";
        scheduleSubJobs();
        if (_subJobs.isEmpty()) {
            if (_keepLock) {
                emit finished(200);
            } else {
                unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Success);
            }
        } else {
            _subJobs.values().last()->start();
        }
    } else {
        emit finished(200);
    }
}

void UpdateE2eeFolderUsersMetadataJob::scheduleSubJobs()
{
    const auto isMetadataValid = _encryptedFolderMetadataHandler->folderMetadata() && _encryptedFolderMetadataHandler->folderMetadata()->isValid();
    if (!isMetadataValid) {
        if (_operation == Operation::Add || _operation == Operation::Remove) {
            qCWarning(lcUpdateE2eeFolderUsersMetadataJob()) << "Metadata is invalid. Unlocking the folder.";
            unlockFolder(EncryptedFolderMetadataHandler::UnlockFolderWithResult::Failure);
        } else {
            qCWarning(lcUpdateE2eeFolderUsersMetadataJob()) << "Metadata is invalid.";
            emit finished(-1, tr("Error updating metadata for a folder %1").arg(_fullRemotePath));
        }
        return;
    }

    const auto pathInDb = Utility::fullRemotePathToRemoteSyncRootRelative(_fullRemotePath, _syncFolderRemotePath);
    [[maybe_unused]] const auto result = _journalDb->getFilesBelowPath(pathInDb.toUtf8(), [this](const SyncJournalFileRecord &record) {
        if (record.isDirectory()) {
            const auto folderMetadata = _encryptedFolderMetadataHandler->folderMetadata();
            const auto subJob = new UpdateE2eeFolderUsersMetadataJob(_account, _journalDb, _syncFolderRemotePath, UpdateE2eeFolderUsersMetadataJob::ReEncrypt, Utility::trailingSlashPath(_syncFolderRemotePath) + QString::fromUtf8(record._e2eMangledName));
            subJob->setMetadataKeyForEncryption(folderMetadata->metadataKeyForEncryption());
            subJob->setMetadataKeyForDecryption(folderMetadata->metadataKeyForDecryption());
            subJob->setKeyChecksums(folderMetadata->keyChecksums());
            subJob->setParent(this);
            subJob->setFolderToken(_encryptedFolderMetadataHandler->folderToken());
            _subJobs.insert(subJob);
            connect(subJob, &UpdateE2eeFolderUsersMetadataJob::finished, this, &UpdateE2eeFolderUsersMetadataJob::slotSubJobFinished);
        }
    });
}

void UpdateE2eeFolderUsersMetadataJob::unlockFolder(const EncryptedFolderMetadataHandler::UnlockFolderWithResult result)
{
    qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Calling Unlock";
    connect(_encryptedFolderMetadataHandler.data(), &EncryptedFolderMetadataHandler::folderUnlocked, this, &UpdateE2eeFolderUsersMetadataJob::slotFolderUnlocked);
    _encryptedFolderMetadataHandler->unlockFolder(result);
}

void UpdateE2eeFolderUsersMetadataJob::slotFolderUnlocked(const QByteArray &folderId, int httpStatus)
{
    emit folderUnlocked();
    if (_keepLock) {
        return;
    }
    if (httpStatus != 200) {
        qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "Failed to unlock a folder" << folderId << httpStatus;
    }
    const auto message = httpStatus != 200 ? tr("Failed to unlock a folder.") : QString{};
    emit finished(httpStatus, message);
}

void UpdateE2eeFolderUsersMetadataJob::subJobsFinished(bool success)
{
    unlockFolder(success
        ? EncryptedFolderMetadataHandler::UnlockFolderWithResult::Success
        : EncryptedFolderMetadataHandler::UnlockFolderWithResult::Failure);
}

void UpdateE2eeFolderUsersMetadataJob::slotSubJobFinished(int code, const QString &message)
{
    if (code != 200) {
        qCDebug(lcUpdateE2eeFolderUsersMetadataJob) << "sub job finished with error" << message;
        subJobsFinished(false);
        return;
    }
    const auto job = qobject_cast<UpdateE2eeFolderUsersMetadataJob *>(sender());
    Q_ASSERT(job);
    if (!job) {
        qCWarning(lcUpdateE2eeFolderUsersMetadataJob) << "slotSubJobFinished must be invoked by signal";
        emit finished(-1, tr("Error updating metadata for a folder %1").arg(_fullRemotePath) + QStringLiteral(":%1").arg(message));
        subJobsFinished(false);
        return;
    }

    {
        QMutexLocker locker(&_subJobSyncItemsMutex);
        const auto foundInHash = _subJobSyncItems.constFind(job->path());
        if (foundInHash != _subJobSyncItems.constEnd() && foundInHash.value()) {
            foundInHash.value()->_e2eEncryptionStatus = job->encryptionStatus();
            foundInHash.value()->_e2eEncryptionStatusRemote = job->encryptionStatus();
            foundInHash.value()->_e2eEncryptionServerCapability = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_account->capabilities().clientSideEncryptionVersion());
            _subJobSyncItems.erase(foundInHash);
        }
    }

    _subJobs.remove(job);
    job->deleteLater();

    if (_subJobs.isEmpty()) {
        subJobsFinished(true);
    } else {
        _subJobs.values().last()->start();
    }
}

void UpdateE2eeFolderUsersMetadataJob::slotCertificateFetchedFromKeychain(const QSslCertificate &certificate)
{
    disconnect(_account->e2e(),
               &ClientSideEncryption::certificateFetchedFromKeychain,
               this,
               &UpdateE2eeFolderUsersMetadataJob::slotCertificateFetchedFromKeychain);
    if (certificate.isNull()) {
        // get folder user's public key
        _account->e2e()->getUsersPublicKeyFromServer(_account, {_folderUserId});
        connect(_account->e2e(),
                &ClientSideEncryption::certificatesFetchedFromServer,
                this,
                &UpdateE2eeFolderUsersMetadataJob::slotCertificatesFetchedFromServer);
        return;
    }
    _folderUserCertificate = certificate;
    emit certificateReady();
}

void UpdateE2eeFolderUsersMetadataJob::slotCertificatesFetchedFromServer(const QHash<QString, NextcloudSslCertificate> &results)
{
    const auto certificate = results.isEmpty() ? NextcloudSslCertificate{} : results.value(_folderUserId);
    _folderUserCertificate = certificate;
    if (certificate.get().isNull()) {
        emit certificateReady();
        return;
    }
    _account->e2e()->writeCertificate(_account, _folderUserId, certificate);
    connect(_account->e2e(), &ClientSideEncryption::certificateWriteComplete, this, &UpdateE2eeFolderUsersMetadataJob::certificateReady);
}

void UpdateE2eeFolderUsersMetadataJob::setUserData(const UserData &userData)
{
    _userData = userData;
}

void UpdateE2eeFolderUsersMetadataJob::setFolderToken(const QByteArray &folderToken)
{
    _folderToken = folderToken;
}

void UpdateE2eeFolderUsersMetadataJob::setMetadataKeyForEncryption(const QByteArray &metadataKey)
{
    _metadataKeyForEncryption = metadataKey;
}

void UpdateE2eeFolderUsersMetadataJob::setMetadataKeyForDecryption(const QByteArray &metadataKey)
{
    _metadataKeyForDecryption = metadataKey;
}

void UpdateE2eeFolderUsersMetadataJob::setKeyChecksums(const QSet<QByteArray> &keyChecksums)
{
    _keyChecksums = keyChecksums;
}

void UpdateE2eeFolderUsersMetadataJob::setSubJobSyncItems(const QHash<QString, SyncFileItemPtr> &subJobSyncItems)
{
    _subJobSyncItems = subJobSyncItems;
}

const QString &UpdateE2eeFolderUsersMetadataJob::path() const
{
    return _fullRemotePath;
}

const UpdateE2eeFolderUsersMetadataJob::UserData &UpdateE2eeFolderUsersMetadataJob::userData() const
{
    return _userData;
}

SyncFileItem::EncryptionStatus UpdateE2eeFolderUsersMetadataJob::encryptionStatus() const
{
    const auto folderMetadata = _encryptedFolderMetadataHandler->folderMetadata();
    const auto isMetadataValid = folderMetadata && folderMetadata->isValid();
    if (!isMetadataValid) {
        qCWarning(lcUpdateE2eeFolderUsersMetadataJob) << "_encryptedFolderMetadataHandler->folderMetadata() is invalid";
    }
    return !isMetadataValid
        ? EncryptionStatusEnums::ItemEncryptionStatus::NotEncrypted
        : folderMetadata->encryptedMetadataEncryptionStatus();
}

const QByteArray UpdateE2eeFolderUsersMetadataJob::folderToken() const
{
    return _encryptedFolderMetadataHandler->folderToken();
}

}
