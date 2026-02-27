/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "rootencryptedfolderinfo.h"
#include "encryptedfoldermetadatahandler.h"
#include "foldermetadata.h"
#include "account.h"
#include "common/syncjournaldb.h"
#include "clientsideencryptionjobs.h"
#include "clientsideencryption.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <algorithm>

namespace OCC {

Q_LOGGING_CATEGORY(lcFetchAndUploadE2eeFolderMetadataJob, "nextcloud.sync.propagator.encryptedfoldermetadatahandler", QtInfoMsg)

}

namespace {
constexpr auto counterStoreKeyPrefix = "e2ee_metadata_counter:";
constexpr auto keyChecksumsStoreKeyPrefix = "e2ee_metadata_key_checksums:";

QString normalizeStorePath(const QString &path)
{
    const auto normalized = OCC::Utility::noLeadingSlashPath(OCC::Utility::noTrailingSlashPath(path));
    return normalized.isEmpty() ? QStringLiteral("/") : normalized;
}

QString counterStoreKey(const QString &folderPath)
{
    return QString::fromLatin1(counterStoreKeyPrefix) + normalizeStorePath(folderPath);
}

QString keyChecksumsStoreKey(const QString &rootPath)
{
    return QString::fromLatin1(keyChecksumsStoreKeyPrefix) + normalizeStorePath(rootPath);
}

QString rootKeyPath(const OCC::RootEncryptedFolderInfo &rootEncryptedFolderInfo, const QString &folderFullRemotePath)
{
    const auto normalizedRoot = normalizeStorePath(rootEncryptedFolderInfo.path);
    if (normalizedRoot == QStringLiteral("/")) {
        return normalizeStorePath(folderFullRemotePath);
    }
    return normalizedRoot;
}

QSet<QByteArray> parseKeyChecksums(const QString &value)
{
    if (value.isEmpty()) {
        return {};
    }
    const auto doc = QJsonDocument::fromJson(value.toUtf8());
    if (!doc.isArray()) {
        return {};
    }
    QSet<QByteArray> checksums;
    for (const auto &entry : doc.array()) {
        const auto checksum = entry.toString().toUtf8();
        if (!checksum.isEmpty()) {
            checksums.insert(checksum);
        }
    }
    return checksums;
}

QString serializeKeyChecksums(const QSet<QByteArray> &checksums)
{
    if (checksums.isEmpty()) {
        return {};
    }
    auto sortedChecksums = checksums.values();
    std::sort(sortedChecksums.begin(), sortedChecksums.end());
    QJsonArray array;
    for (const auto &checksum : sortedChecksums) {
        if (!checksum.isEmpty()) {
            array.push_back(QString::fromUtf8(checksum));
        }
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}
}

namespace OCC {

EncryptedFolderMetadataHandler::EncryptedFolderMetadataHandler(const AccountPtr &account,
                                                                         const QString &folderFullRemotePath,
                                                                         const QString &remoteFolderRoot,
                                                                         SyncJournalDb *const journalDb,
                                                                         const QString &pathForTopLevelFolder,
                                                                         QObject *parent)
    : QObject(parent)
    , _account(account)
    , _journalDb(journalDb)
    , _folderFullRemotePath(Utility::noLeadingSlashPath(Utility::noTrailingSlashPath(folderFullRemotePath)))
    , _remoteFolderRoot(Utility::noLeadingSlashPath(Utility::noTrailingSlashPath(remoteFolderRoot)))
{
    Q_ASSERT(!_remoteFolderRoot.isEmpty());
    Q_ASSERT(_remoteFolderRoot == QStringLiteral("/") || _folderFullRemotePath.startsWith(_remoteFolderRoot));

    const auto folderRelativePath = Utility::fullRemotePathToRemoteSyncRootRelative(_folderFullRemotePath, _remoteFolderRoot);
    _rootEncryptedFolderInfo = RootEncryptedFolderInfo(RootEncryptedFolderInfo::createRootPath(folderRelativePath, pathForTopLevelFolder));
}

void EncryptedFolderMetadataHandler::fetchMetadata(const FetchMode fetchMode)
{
    _fetchMode = fetchMode;
    if (_journalDb) {
        const auto storedCounter = _journalDb->keyValueStoreGetInt(counterStoreKey(_folderFullRemotePath), -1);
        if (storedCounter >= 0) {
            const auto storedCounterValue = static_cast<quint64>(storedCounter);
            if (_rootEncryptedFolderInfo.counter < storedCounterValue) {
                _rootEncryptedFolderInfo.counter = storedCounterValue;
            }
        }

        const auto storedChecksumsRaw =
            _journalDb->keyValueStoreGetString(keyChecksumsStoreKey(rootKeyPath(_rootEncryptedFolderInfo, _folderFullRemotePath)));
        const auto storedChecksums = parseKeyChecksums(storedChecksumsRaw);
        if (!storedChecksums.isEmpty()) {
            _rootEncryptedFolderInfo.keyChecksums.unite(storedChecksums);
        }
    }
    fetchFolderEncryptedId();
}

void EncryptedFolderMetadataHandler::fetchMetadata(const RootEncryptedFolderInfo &rootEncryptedFolderInfo, const FetchMode fetchMode)
{
    Q_ASSERT(!rootEncryptedFolderInfo.path.isEmpty());
    if (rootEncryptedFolderInfo.path.isEmpty()) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error fetching metadata for" << _folderFullRemotePath << ". Invalid rootEncryptedFolderInfo!";
        emit fetchFinished(-1, tr("Error fetching metadata."));
        return;
    }

    _rootEncryptedFolderInfo = rootEncryptedFolderInfo;
    if (_rootEncryptedFolderInfo.path.isEmpty()) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error fetching metadata for" << _folderFullRemotePath << ". Invalid _rootEncryptedFolderInfo!";
        emit fetchFinished(-1, tr("Error fetching metadata."));
        return;
    }
    if (_remoteFolderRoot != QStringLiteral("/") && !_folderFullRemotePath.startsWith(_remoteFolderRoot)) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error fetching metadata for" << _folderFullRemotePath
            << " and remote root" << _remoteFolderRoot << ". Invalid _remoteFolderRoot or _folderFullRemotePath!";
        emit fetchFinished(-1, tr("Error fetching metadata."));
        return;
    }
    fetchMetadata(fetchMode);
}

void EncryptedFolderMetadataHandler::uploadMetadata(const UploadMode uploadMode)
{
    _uploadMode = uploadMode;
    if (!_folderToken.isEmpty()) {
        // use existing token
        startUploadMetadata();
        return;
    }
    lockFolder();
}

void EncryptedFolderMetadataHandler::lockFolder()
{
    if (!validateBeforeLock()) {
        return;
    }

    const auto lockJob = new LockEncryptFolderApiJob(_account, _folderId, _account->e2e()->certificateSha256Fingerprint(), _journalDb, _account->e2e()->getPublicKey(), this);
    connect(lockJob, &LockEncryptFolderApiJob::success, this, &EncryptedFolderMetadataHandler::slotFolderLockedSuccessfully);
    connect(lockJob, &LockEncryptFolderApiJob::error, this, &EncryptedFolderMetadataHandler::slotFolderLockedError);
    if (_account->capabilities().clientSideEncryptionVersion() >= 2.0) {
        lockJob->setCounter(folderMetadata()->newCounter());
    }
    lockJob->start();
}

void EncryptedFolderMetadataHandler::startFetchMetadata()
{
    const auto job = new GetMetadataApiJob(_account, _folderId);
    connect(job, &GetMetadataApiJob::jsonReceived, this, &EncryptedFolderMetadataHandler::slotMetadataReceived);
    connect(job, &GetMetadataApiJob::error, this, &EncryptedFolderMetadataHandler::slotMetadataReceivedError);
    job->start();
}

void EncryptedFolderMetadataHandler::fetchFolderEncryptedId()
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Folder is encrypted, let's get the Id from it.";
    const auto job = new LsColJob(_account, _folderFullRemotePath);
    job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
    connect(job, &LsColJob::directoryListingSubfolders, this, &EncryptedFolderMetadataHandler::slotFolderEncryptedIdReceived);
    connect(job, &LsColJob::finishedWithError, this, &EncryptedFolderMetadataHandler::slotFolderEncryptedIdError);
    job->start();
}

bool EncryptedFolderMetadataHandler::validateBeforeLock()
{
    //Q_ASSERT(!_isFolderLocked && folderMetadata() && folderMetadata()->isValid() && folderMetadata()->isRootEncryptedFolder());
    if (_isFolderLocked) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error locking folder" << _folderId << "already locked";
        emit uploadFinished(-1, tr("Error locking folder."));
        return false;
    }

    if (!folderMetadata() || !folderMetadata()->isValid()) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error locking folder" << _folderId << "invalid or null metadata";
        emit uploadFinished(-1, tr("Error locking folder."));
        return false;
    }

    // normally, we should allow locking any nested folder to update its metadata, yet, with the new V2 architecture, this is something we might want to disallow
    /*if (!folderMetadata()->isRootEncryptedFolder()) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error locking folder" << _folderId << "as it is not a top level folder";
        emit uploadFinished(-1, tr("Error locking folder."));
        return false;
    }*/
    return true;
}

void EncryptedFolderMetadataHandler::slotFolderEncryptedIdReceived(const QStringList &list)
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Received id of folder. Fetching metadata...";
    const auto job = qobject_cast<LsColJob *>(sender());
    const auto &folderInfo = job->_folderInfos.value(list.first());
    _folderId = folderInfo.fileId;
    startFetchMetadata();
}

void EncryptedFolderMetadataHandler::slotFolderEncryptedIdError(QNetworkReply *reply)
{
    Q_ASSERT(reply);
    qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error retrieving the Id of the encrypted folder.";
    if (!reply) {
        emit fetchFinished(-1, tr("Error fetching encrypted folder ID."));
        return;
    }
    const auto errorCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    emit fetchFinished(errorCode, reply->errorString());
}

void EncryptedFolderMetadataHandler::slotMetadataReceived(const QJsonDocument &json, int statusCode)
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Metadata Received, parsing it and decrypting" << json.toVariant();

    const auto job = qobject_cast<GetMetadataApiJob *>(sender());
    Q_ASSERT(job);
    if (!job) {
        qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "slotMetadataReceived must be called from GetMetadataApiJob's signal";
        emit fetchFinished(statusCode, tr("Error fetching metadata."));
        return;
    }

    _fetchMode = FetchMode::NonEmptyMetadata;

    if (statusCode != 200 && statusCode != 404) {
        // neither successfully fetched, nor a folder without a metadata, fail further logic
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error fetching metadata for folder" << _folderFullRemotePath;
        emit fetchFinished(statusCode, tr("Error fetching metadata."));
        return;
    }

    const auto rawMetadata = statusCode == 404
        ? QByteArray{} : json.toJson(QJsonDocument::Compact);
    const auto metadata(QSharedPointer<FolderMetadata>::create(_account, _remoteFolderRoot, rawMetadata, _rootEncryptedFolderInfo, job->signature()));
    connect(metadata.data(), &FolderMetadata::setupComplete, this, [this, metadata] {
        if (!metadata->isValid()) {
            qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error parsing or decrypting metadata for folder" << _folderFullRemotePath;
            emit fetchFinished(-1, tr("Error parsing or decrypting metadata."));
            return;
        }
        if (_journalDb) {
            const auto counterValue = metadata->counter();
            if (counterValue > 0) {
                _journalDb->keyValueStoreSet(counterStoreKey(_folderFullRemotePath), static_cast<qulonglong>(counterValue));
            }
            const auto keyChecksums = metadata->keyChecksums();
            if (!keyChecksums.isEmpty()) {
                const auto rootPath = rootKeyPath(_rootEncryptedFolderInfo, _folderFullRemotePath);
                _journalDb->keyValueStoreSet(keyChecksumsStoreKey(rootPath), serializeKeyChecksums(keyChecksums));
            }
        }
        _rootEncryptedFolderInfo.counter = metadata->counter();
        if (!metadata->keyChecksums().isEmpty()) {
            _rootEncryptedFolderInfo.keyChecksums = metadata->keyChecksums();
        }
        _folderMetadata = metadata;
        emit fetchFinished(200);
    });
}

void EncryptedFolderMetadataHandler::slotMetadataReceivedError(const QByteArray &folderId, int httpReturnCode)
{
    Q_UNUSED(folderId);
    if (_fetchMode == FetchMode::AllowEmptyMetadata) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error Getting the encrypted metadata. Pretend we got empty metadata. In case when posting it for the first time.";
        _isNewMetadataCreated = true;
        slotMetadataReceived({}, httpReturnCode);
        return;
    }
    qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error Getting the encrypted metadata.";
    emit fetchFinished(httpReturnCode, tr("Error fetching metadata."));
}

void EncryptedFolderMetadataHandler::slotFolderLockedSuccessfully(const QByteArray &folderId, const QByteArray &token)
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Folder" << folderId << "Locked Successfully for Upload, Fetching Metadata";
    _folderToken = token;
    _isFolderLocked = true;
    startUploadMetadata();
}

void EncryptedFolderMetadataHandler::slotFolderLockedError(const QByteArray &folderId, int httpErrorCode)
{
    qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Error locking folder" << folderId;
    emit fetchFinished(httpErrorCode, tr("Error locking folder."));
}

void EncryptedFolderMetadataHandler::unlockFolder(const UnlockFolderWithResult result)
{
    Q_ASSERT(!_isUnlockRunning);
    Q_ASSERT(_isFolderLocked);

    if (_isUnlockRunning) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Double-call to unlockFolder.";
        return;
    }

    if (!_isFolderLocked) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Folder is not locked.";
        emit folderUnlocked(_folderId, 204);
        return;
    }

    if (_uploadMode == UploadMode::DoNotKeepLock) {
        if (result == UnlockFolderWithResult::Success) {
            connect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadSuccess);
        } else {
            connect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadError);
        }
    }

    if (_folderToken.isEmpty()) {
        emit folderUnlocked(_folderId, 200);
        return;
    }

    _isUnlockRunning = true;

    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Calling Unlock";

    const auto unlockJob = new UnlockEncryptFolderApiJob(_account, _folderId, _folderToken, _journalDb, this);
    connect(unlockJob, &UnlockEncryptFolderApiJob::success, unlockJob, [this](const QByteArray &folderId) {
        qDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Successfully Unlocked";
        _isFolderLocked = false;
        emit folderUnlocked(folderId, 200);
        _isUnlockRunning = false;
    });
    connect(unlockJob, &UnlockEncryptFolderApiJob::error, unlockJob, [this](const QByteArray &folderId, int httpStatus) {
        qDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Unlock Error";
        emit folderUnlocked(folderId, httpStatus);
        _isUnlockRunning = false;
    });
    unlockJob->start();
}

void EncryptedFolderMetadataHandler::startUploadMetadata()
{
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Metadata created, sending to the server.";

    _uploadErrorCode = 200;

    if (!folderMetadata() || !folderMetadata()->isValid()) {
        slotUploadMetadataError(_folderId, -1);
        return;
    }

    const auto encryptedMetadata = folderMetadata()->encryptedMetadata();
    if (_isNewMetadataCreated) {
        const auto job = new StoreMetaDataApiJob(_account, _folderId, _folderToken, encryptedMetadata, folderMetadata()->metadataSignature());
        connect(job, &StoreMetaDataApiJob::success, this, &EncryptedFolderMetadataHandler::slotUploadMetadataSuccess);
        connect(job, &StoreMetaDataApiJob::error, this, &EncryptedFolderMetadataHandler::slotUploadMetadataError);
        job->start();
    } else {
        const auto job = new UpdateMetadataApiJob(_account, _folderId, encryptedMetadata, _folderToken, folderMetadata()->metadataSignature());
        connect(job, &UpdateMetadataApiJob::success, this, &EncryptedFolderMetadataHandler::slotUploadMetadataSuccess);
        connect(job, &UpdateMetadataApiJob::error, this, &EncryptedFolderMetadataHandler::slotUploadMetadataError);
        job->start();
    }
}

void EncryptedFolderMetadataHandler::slotUploadMetadataSuccess(const QByteArray &folderId)
{
    Q_UNUSED(folderId);
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Uploading of the metadata success.";
    if (_uploadMode == UploadMode::KeepLock || !_isFolderLocked) {
        slotEmitUploadSuccess();
        return;
    }
    connect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadSuccess);
    unlockFolder(UnlockFolderWithResult::Success);
}

void EncryptedFolderMetadataHandler::slotUploadMetadataError(const QByteArray &folderId, int httpReturnCode)
{
    qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "Update metadata error for folder" << folderId << "with error" << httpReturnCode;
    qCDebug(lcFetchAndUploadE2eeFolderMetadataJob) << "Unlocking the folder.";
    _uploadErrorCode = httpReturnCode;
    if (_isFolderLocked && _uploadMode == UploadMode::DoNotKeepLock) {
        connect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadError);
        unlockFolder(UnlockFolderWithResult::Failure);
        return;
    }
    emit uploadFinished(_uploadErrorCode);
}

void EncryptedFolderMetadataHandler::slotEmitUploadSuccess()
{
    disconnect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadSuccess);
    emit uploadFinished(_uploadErrorCode);
}

void EncryptedFolderMetadataHandler::slotEmitUploadError()
{
    disconnect(this, &EncryptedFolderMetadataHandler::folderUnlocked, this, &EncryptedFolderMetadataHandler::slotEmitUploadError);
    emit uploadFinished(_uploadErrorCode, tr("Failed to upload metadata"));
}

QSharedPointer<FolderMetadata> EncryptedFolderMetadataHandler::folderMetadata() const
{
    return _folderMetadata;
}

void EncryptedFolderMetadataHandler::setPrefetchedMetadataAndId(const QSharedPointer<FolderMetadata> &metadata, const QByteArray &id)
{
    Q_ASSERT(metadata && metadata->isValid());
    Q_ASSERT(!id.isEmpty());

    if (!metadata || !metadata->isValid()) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "invalid metadata argument";
        return;
    }

    if (id.isEmpty()) {
        qCWarning(lcFetchAndUploadE2eeFolderMetadataJob) << "invalid id argument";
        return;
    }

    _folderId = id;
    _folderMetadata = metadata;
    _isNewMetadataCreated = metadata->initialMetadata().isEmpty();
}

const QByteArray& EncryptedFolderMetadataHandler::folderId() const
{
    return _folderId;
}

void EncryptedFolderMetadataHandler::setFolderToken(const QByteArray &token)
{
    _folderToken = token;
}

const QByteArray& EncryptedFolderMetadataHandler::folderToken() const
{
    return _folderToken;
}

bool EncryptedFolderMetadataHandler::isUnlockRunning() const
{
    return _isUnlockRunning;
}

bool EncryptedFolderMetadataHandler::isFolderLocked() const
{
    return _isFolderLocked;
}

}
