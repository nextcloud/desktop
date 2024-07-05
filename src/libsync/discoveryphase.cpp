/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include "discoveryphase.h"
#include "common/utility.h"
#include "configfile.h"
#include "discovery.h"
#include "helpers.h"
#include "progressdispatcher.h"

#include "account.h"
#include "clientsideencryptionjobs.h"
#include "foldermetadata.h"

#include "common/asserts.h"
#include "common/checksums.h"

#include <csync_exclude.h>
#include "vio/csync_vio_local.h"

#include <QLoggingCategory>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <cstring>
#include <QDateTime>


namespace OCC {

Q_LOGGING_CATEGORY(lcDiscovery, "nextcloud.sync.discovery", QtInfoMsg)

bool DiscoveryPhase::isInSelectiveSyncBlackList(const QString &path) const
{
    if (_selectiveSyncBlackList.isEmpty()) {
        // If there is no black list, everything is allowed
        return false;
    }

    // Block if it is in the black list
    if (SyncJournalDb::findPathInSelectiveSyncList(_selectiveSyncBlackList, path)) {
        return true;
    }

    return false;
}

bool DiscoveryPhase::activeFolderSizeLimit() const
{
    return _syncOptions._newBigFolderSizeLimit > 0 && _syncOptions._vfs->mode() == Vfs::Off;
}

bool DiscoveryPhase::notifyExistingFolderOverLimit() const
{
    return activeFolderSizeLimit() && ConfigFile().notifyExistingFoldersOverLimit();
}

void DiscoveryPhase::checkFolderSizeLimit(const QString &path, const std::function<void(bool)> completionCallback)
{
    if (!activeFolderSizeLimit()) {
        // no limit, everything is allowed;
        return completionCallback(false);
    }

    // do a PROPFIND to know the size of this folder
    const auto propfindJob = new PropfindJob(_account, _remoteFolder + path, this);
    propfindJob->setProperties(QList<QByteArray>() << "resourcetype"
                                                   << "http://owncloud.org/ns:size");

    connect(propfindJob, &PropfindJob::finishedWithError, this, [=] {
        return completionCallback(false);
    });
    connect(propfindJob, &PropfindJob::result, this, [=](const QVariantMap &values) {
        const auto result = values.value(QLatin1String("size")).toLongLong();
        const auto limit = _syncOptions._newBigFolderSizeLimit;
        qCDebug(lcDiscovery) << "Folder size check complete for" << path << "result:" << result << "limit:" << limit;
        return completionCallback(result >= limit);
    });
    propfindJob->start();
}

void DiscoveryPhase::checkSelectiveSyncNewFolder(const QString &path,
                                                 const RemotePermissions remotePerm,
                                                 const std::function<void(bool)> callback)
{
    if (_syncOptions._confirmExternalStorage && _syncOptions._vfs->mode() == Vfs::Off
        && remotePerm.hasPermission(RemotePermissions::IsMounted)) {
        // external storage.

        /* Note: DiscoverySingleDirectoryJob::directoryListingIteratedSlot make sure that only the
         * root of a mounted storage has 'M', all sub entries have 'm' */

        // Only allow it if the white list contains exactly this path (not parents)
        // We want to ask confirmation for external storage even if the parents where selected
        if (_selectiveSyncWhiteList.contains(path + QLatin1Char('/'))) {
            return callback(false);
        }

        emit newBigFolder(path, true);
        return callback(true);
    }

    // If this path or the parent is in the white list, then we do not block this file
    if (SyncJournalDb::findPathInSelectiveSyncList(_selectiveSyncWhiteList, path)) {
        return callback(false);
    }

    checkFolderSizeLimit(path, [this, path, callback](const bool bigFolder) {
        if (bigFolder) {
            // we tell the UI there is a new folder
            emit newBigFolder(path, false);
            return callback(true);
        }

        // it is not too big, put it in the white list (so we will not do more query for the children) and and do not block.
        const auto sanitisedPath = Utility::trailingSlashPath(path);
        _selectiveSyncWhiteList.insert(std::upper_bound(_selectiveSyncWhiteList.begin(), _selectiveSyncWhiteList.end(), sanitisedPath), sanitisedPath);
        return callback(false);
    });
}

void DiscoveryPhase::checkSelectiveSyncExistingFolder(const QString &path)
{
    // If no size limit is enforced, or if is in whitelist (explicitly allowed) or in blacklist (explicitly disallowed), do nothing.
    if (!notifyExistingFolderOverLimit() || SyncJournalDb::findPathInSelectiveSyncList(_selectiveSyncWhiteList, path)
        || SyncJournalDb::findPathInSelectiveSyncList(_selectiveSyncBlackList, path)) {
        return;
    }

    checkFolderSizeLimit(path, [this, path](const bool bigFolder) {
        if (bigFolder) {
            // Notify the user and prompt for response.
            emit existingFolderNowBig(path);
        }
    });
}

/* Given a path on the remote, give the path as it is when the rename is done */
QString DiscoveryPhase::adjustRenamedPath(const QString &original, SyncFileItem::Direction d) const
{
    return OCC::adjustRenamedPath(d == SyncFileItem::Down ? _renamedItemsRemote : _renamedItemsLocal, original);
}

QString adjustRenamedPath(const QMap<QString, QString> &renamedItems, const QString &original)
{
    int slashPos = original.size();
    while ((slashPos = original.lastIndexOf('/', slashPos - 1)) > 0) {
        auto it = renamedItems.constFind(original.left(slashPos));
        if (it != renamedItems.constEnd()) {
            return *it + original.mid(slashPos);
        }
    }
    return original;
}

QPair<bool, QByteArray> DiscoveryPhase::findAndCancelDeletedJob(const QString &originalPath)
{
    bool result = false;
    QByteArray oldEtag;
    auto it = _deletedItem.find(originalPath);
    if (it != _deletedItem.end()) {
        const SyncInstructions instruction = (*it)->_instruction;
        if (instruction == CSYNC_INSTRUCTION_IGNORE && (*it)->_type == ItemTypeVirtualFile) {
            // re-creation of virtual files count as a delete
            // a file might be in an error state and thus gets marked as CSYNC_INSTRUCTION_IGNORE
            // after it was initially marked as CSYNC_INSTRUCTION_REMOVE
            // return true, to not trigger any additional actions on that file that could elad to dataloss
            result = true;
            oldEtag = (*it)->_etag;
        } else {
            if (!(instruction == CSYNC_INSTRUCTION_REMOVE
                    // re-creation of virtual files count as a delete
                    || ((*it)->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW)
                    || ((*it)->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW)))
            {
                qCWarning(lcDiscovery) << "ENFORCE(FAILING)" << originalPath;
                qCWarning(lcDiscovery) << "instruction == CSYNC_INSTRUCTION_REMOVE" << (instruction == CSYNC_INSTRUCTION_REMOVE);
                qCWarning(lcDiscovery) << "((*it)->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW)"
                                       << ((*it)->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW);
                qCWarning(lcDiscovery) << "((*it)->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW))"
                                       << ((*it)->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW);
                qCWarning(lcDiscovery) << "instruction" << instruction;
                qCWarning(lcDiscovery) << "(*it)->_type" << (*it)->_type;
                qCWarning(lcDiscovery) << "(*it)->_isRestoration " << (*it)->_isRestoration;
                Q_ASSERT(false);
                emit addErrorToGui(SyncFileItem::Status::FatalError, tr("Error while canceling deletion of a file"), originalPath, ErrorCategory::GenericError);
                emit fatalError(tr("Error while canceling deletion of %1").arg(originalPath), ErrorCategory::GenericError);
            }
            (*it)->_instruction = CSYNC_INSTRUCTION_NONE;
            result = true;
            oldEtag = (*it)->_etag;
        }
        _deletedItem.erase(it);
    }
    if (auto *otherJob = _queuedDeletedDirectories.take(originalPath)) {
        oldEtag = otherJob->_dirItem->_etag;
        delete otherJob;
        result = true;
    }
    return { result, oldEtag };
}

void DiscoveryPhase::enqueueDirectoryToDelete(const QString &path, ProcessDirectoryJob* const directoryJob)
{
    _queuedDeletedDirectories[path] = directoryJob;

    if (directoryJob->_dirItem &&
        directoryJob->_dirItem->_isRestoration &&
        directoryJob->_dirItem->_direction == SyncFileItem::Down &&
        directoryJob->_dirItem->_instruction == CSYNC_INSTRUCTION_NEW) {

        _directoryNamesToRestoreOnPropagation.push_back(path);
    }
}

void DiscoveryPhase::startJob(ProcessDirectoryJob *job)
{
    ENFORCE(!_currentRootJob);
    connect(this, &DiscoveryPhase::itemDiscovered, this, &DiscoveryPhase::slotItemDiscovered, Qt::UniqueConnection);
    connect(job, &ProcessDirectoryJob::finished, this, [this, job] {
        ENFORCE(_currentRootJob == sender());
        _currentRootJob = nullptr;
        if (job->_dirItem)
            emit itemDiscovered(job->_dirItem);
        job->deleteLater();

        // Once the main job has finished recurse here to execute the remaining
        // jobs for queued deleted directories.
        if (!_queuedDeletedDirectories.isEmpty()) {
            auto nextJob = _queuedDeletedDirectories.take(_queuedDeletedDirectories.firstKey());
            startJob(nextJob);
        } else {
            emit finished();
        }
    });
    _currentRootJob = job;
    job->start();
}

void DiscoveryPhase::setSelectiveSyncBlackList(const QStringList &list)
{
    _selectiveSyncBlackList = list;
    _selectiveSyncBlackList.sort();
}

void DiscoveryPhase::setSelectiveSyncWhiteList(const QStringList &list)
{
    _selectiveSyncWhiteList = list;
    _selectiveSyncWhiteList.sort();
}

bool DiscoveryPhase::isRenamed(const QString &p) const
{
    return _renamedItemsLocal.contains(p) || _renamedItemsRemote.contains(p);
}

void DiscoveryPhase::scheduleMoreJobs()
{
    auto limit = qMax(1, _syncOptions._parallelNetworkJobs);
    if (_currentRootJob && _currentlyActiveJobs < limit) {
        _currentRootJob->processSubJobs(limit - _currentlyActiveJobs);
    }
}

void DiscoveryPhase::slotItemDiscovered(const OCC::SyncFileItemPtr &item)
{
    if (item->_instruction == CSYNC_INSTRUCTION_ERROR && item->_direction == SyncFileItem::Up) {
        _hasUploadErrorItems = true;
    }
    if (item->_instruction == CSYNC_INSTRUCTION_REMOVE && item->_direction == SyncFileItem::Down) {
        _hasDownloadRemovedItems = true;
    }
}

DiscoverySingleLocalDirectoryJob::DiscoverySingleLocalDirectoryJob(const AccountPtr &account, const QString &localPath, OCC::Vfs *vfs, QObject *parent)
 : QObject(parent), QRunnable(), _localPath(localPath), _account(account), _vfs(vfs)
{
    qRegisterMetaType<QVector<OCC::LocalInfo> >("QVector<OCC::LocalInfo>");
}

// Use as QRunnable
void DiscoverySingleLocalDirectoryJob::run() {
    QString localPath = _localPath;
    if (localPath.endsWith('/')) // Happens if _currentFolder._local.isEmpty()
        localPath.chop(1);

    auto dh = csync_vio_local_opendir(localPath);
    if (!dh) {
        qCInfo(lcDiscovery) << "Error while opening directory" << (localPath) << errno;
        QString errorString = tr("Error while opening directory %1").arg(localPath);
        if (errno == EACCES) {
            errorString = tr("Directory not accessible on client, permission denied");
            emit finishedNonFatalError(errorString);
            return;
        } else if (errno == ENOENT) {
            errorString = tr("Directory not found: %1").arg(localPath);
        } else if (errno == ENOTDIR) {
            // Not a directory..
            // Just consider it is empty
            return;
        }
        emit finishedFatalError(errorString);
        return;
    }

    QVector<LocalInfo> results;
    while (true) {
        errno = 0;
        auto dirent = csync_vio_local_readdir(dh, _vfs);
        if (!dirent)
            break;
        if (dirent->type == ItemTypeSkip)
            continue;
        LocalInfo i;
        static QTextCodec *codec = QTextCodec::codecForName("UTF-8");
        ASSERT(codec);
        QTextCodec::ConverterState state;
        i.name = codec->toUnicode(dirent->path, dirent->path.size(), &state);
        if (state.invalidChars > 0 || state.remainingChars > 0) {
            emit childIgnored(true);
            auto item = SyncFileItemPtr::create();
            //item->_file = _currentFolder._target + i.name;
            // FIXME ^^ do we really need to use _target or is local fine?
            item->_file = _localPath + i.name;
            item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            item->_status = SyncFileItem::NormalError;
            item->_errorString = tr("Filename encoding is not valid");
            emit itemDiscovered(item);
            continue;
        }
        i.modtime = dirent->modtime;
        i.size = dirent->size;
        i.inode = dirent->inode;
        i.isDirectory = dirent->type == ItemTypeDirectory;
        i.isHidden = dirent->is_hidden;
        i.isSymLink = dirent->type == ItemTypeSoftLink;
        i.isVirtualFile = dirent->type == ItemTypeVirtualFile || dirent->type == ItemTypeVirtualFileDownload;
        i.isMetadataMissing = dirent->is_metadata_missing;
        i.isPermissionsInvalid = dirent->isPermissionsInvalid;
        i.type = dirent->type;
        results.push_back(i);
    }
    if (errno != 0) {
        csync_vio_local_closedir(dh);

        // Note: Windows vio converts any error into EACCES
        qCWarning(lcDiscovery) << "readdir failed for file in " << localPath << " - errno: " << errno;
        emit finishedFatalError(tr("Error while reading directory %1").arg(localPath));
        return;
    }

    errno = 0;
    csync_vio_local_closedir(dh);
    if (errno != 0) {
        qCWarning(lcDiscovery) << "closedir failed for file in " << localPath << " - errno: " << errno;
    }

    emit finished(results);
}

DiscoverySingleDirectoryJob::DiscoverySingleDirectoryJob(const AccountPtr &account,
                                                         const QString &path,
                                                         const QString &remoteRootFolderPath,
                                                         const QSet<QString> &topLevelE2eeFolderPaths,
                                                         QObject *parent)
    : QObject(parent)
    , _subPath(remoteRootFolderPath + path)
    , _remoteRootFolderPath(remoteRootFolderPath)
    , _account(account)
    , _topLevelE2eeFolderPaths(topLevelE2eeFolderPaths)
{
    Q_ASSERT(!_remoteRootFolderPath.isEmpty());
}

void DiscoverySingleDirectoryJob::start()
{
    // Start the actual HTTP job
    auto *lsColJob = new LsColJob(_account, _subPath);

    QList<QByteArray> props;
    props << "resourcetype"
          << "getlastmodified"
          << "getcontentlength"
          << "getetag"
          << "http://owncloud.org/ns:size"
          << "http://owncloud.org/ns:id"
          << "http://owncloud.org/ns:fileid"
          << "http://owncloud.org/ns:downloadURL"
          << "http://owncloud.org/ns:dDC"
          << "http://owncloud.org/ns:permissions"
          << "http://owncloud.org/ns:checksums"
          << "http://nextcloud.org/ns:is-encrypted"
          << "http://nextcloud.org/ns:metadata-files-live-photo";

    if (_isRootPath)
        props << "http://owncloud.org/ns:data-fingerprint";
    if (_account->serverVersionInt() >= Account::makeServerVersion(10, 0, 0)) {
        // Server older than 10.0 have performances issue if we ask for the share-types on every PROPFIND
        props << "http://owncloud.org/ns:share-types";
    }
    if (_account->capabilities().filesLockAvailable()) {
        props << "http://nextcloud.org/ns:lock"
              << "http://nextcloud.org/ns:lock-owner-displayname"
              << "http://nextcloud.org/ns:lock-owner"
              << "http://nextcloud.org/ns:lock-owner-type"
              << "http://nextcloud.org/ns:lock-owner-editor"
              << "http://nextcloud.org/ns:lock-time"
              << "http://nextcloud.org/ns:lock-timeout"
              << "http://nextcloud.org/ns:lock-token";
    }
    props << "http://nextcloud.org/ns:is-mount-root";

    lsColJob->setProperties(props);

    QObject::connect(lsColJob, &LsColJob::directoryListingIterated,
        this, &DiscoverySingleDirectoryJob::directoryListingIteratedSlot);
    QObject::connect(lsColJob, &LsColJob::finishedWithError, this, &DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot);
    QObject::connect(lsColJob, &LsColJob::finishedWithoutError, this, &DiscoverySingleDirectoryJob::lsJobFinishedWithoutErrorSlot);
    lsColJob->start();

    _lsColJob = lsColJob;
}

void DiscoverySingleDirectoryJob::abort()
{
    if (_lsColJob && _lsColJob->reply()) {
        _lsColJob->reply()->abort();
    }
}

bool DiscoverySingleDirectoryJob::isFileDropDetected() const
{
    return _isFileDropDetected;
}

bool DiscoverySingleDirectoryJob::encryptedMetadataNeedUpdate() const
{
    return _encryptedMetadataNeedUpdate;
}

SyncFileItem::EncryptionStatus DiscoverySingleDirectoryJob::currentEncryptionStatus() const
{
    return _encryptionStatusCurrent;
}

SyncFileItem::EncryptionStatus DiscoverySingleDirectoryJob::requiredEncryptionStatus() const
{
    return _encryptionStatusRequired;
}

QByteArray DiscoverySingleDirectoryJob::certificateSha256Fingerprint() const
{
    return _e2eCertificateFingerprint;
}

static void propertyMapToRemoteInfo(const QMap<QString, QString> &map, RemotePermissions::MountedPermissionAlgorithm algorithm, RemoteInfo &result)
{
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        QString property = it.key();
        QString value = it.value();
        if (property == QLatin1String("resourcetype")) {
            result.isDirectory = value.contains(QLatin1String("collection"));
        } else if (property == QLatin1String("getlastmodified")) {
            value.replace("GMT", "+0000");
            const auto date = QDateTime::fromString(value, Qt::RFC2822Date);
            Q_ASSERT(date.isValid());
            result.modtime = 0;
            if (date.toSecsSinceEpoch() > 0) {
                result.modtime = date.toSecsSinceEpoch();
            }
        } else if (property == QLatin1String("getcontentlength")) {
            // See #4573, sometimes negative size values are returned
            bool ok = false;
            qlonglong ll = value.toLongLong(&ok);
            if (ok && ll >= 0) {
                result.size = ll;
            } else {
                result.size = 0;
            }
        } else if (property == "getetag") {
            result.etag = Utility::normalizeEtag(value.toUtf8());
        } else if (property == "id") {
            result.fileId = value.toUtf8();
        } else if (property == "downloadURL") {
            result.directDownloadUrl = value;
        } else if (property == "dDC") {
            result.directDownloadCookies = value;
        } else if (property == "permissions") {
            result.remotePerm = RemotePermissions::fromServerString(value, algorithm, map);
        } else if (property == "checksums") {
            result.checksumHeader = findBestChecksum(value.toUtf8());
        } else if (property == "share-types" && !value.isEmpty()) {
            // Since QMap is sorted, "share-types" is always after "permissions".
            if (result.remotePerm.isNull()) {
                qWarning() << "Server returned a share type, but no permissions?";
            } else {
                // S means shared with me.
                // But for our purpose, we want to know if the file is shared. It does not matter
                // if we are the owner or not.
                // Piggy back on the permission field
                result.remotePerm.setPermission(RemotePermissions::IsShared);
                result.sharedByMe = true;
            }
        } else if (property == "is-encrypted" && value == QStringLiteral("1")) {
            result._isE2eEncrypted = true;
        } else if (property == "lock") {
            result.locked = (value == QStringLiteral("1") ? SyncFileItem::LockStatus::LockedItem : SyncFileItem::LockStatus::UnlockedItem);
        }
        if (property == "lock-owner-displayname") {
            result.lockOwnerDisplayName = value;
        }
        if (property == "lock-owner") {
            result.lockOwnerId = value;
        }
        if (property == "lock-owner-type") {
            auto ok = false;
            const auto intConvertedValue = value.toULongLong(&ok);
            if (ok) {
                result.lockOwnerType = static_cast<SyncFileItem::LockOwnerType>(intConvertedValue);
            } else {
                result.lockOwnerType = SyncFileItem::LockOwnerType::UserLock;
            }
        }
        if (property == "lock-owner-editor") {
            result.lockEditorApp = value;
        }
        if (property == "lock-time") {
            auto ok = false;
            const auto intConvertedValue = value.toULongLong(&ok);
            if (ok) {
                result.lockTime = intConvertedValue;
            } else {
                result.lockTime = 0;
            }
        }
        if (property == "lock-timeout") {
            auto ok = false;
            const auto intConvertedValue = value.toULongLong(&ok);
            if (ok) {
                result.lockTimeout = intConvertedValue;
            } else {
                result.lockTimeout = 0;
            }
        }
        if (property == "lock-token") {
            result.lockToken = value;
        }
        if (property == "metadata-files-live-photo") {
            result.livePhotoFile = value;
            result.isLivePhoto = true;
        }
    }

    if (result.isDirectory && map.contains("size")) {
        result.sizeOfFolder = map.value("size").toInt();
    }
}

void DiscoverySingleDirectoryJob::directoryListingIteratedSlot(const QString &file, const QMap<QString, QString> &map)
{
    if (!_ignoredFirst) {
        // The first entry is for the folder itself, we should process it differently.
        _ignoredFirst = true;
        if (map.contains("permissions")) {
            auto perm = RemotePermissions::fromServerString(map.value("permissions"),
                                                            _account->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                                            map);
            emit firstDirectoryPermissions(perm);
            _isExternalStorage = perm.hasPermission(RemotePermissions::IsMounted);
        }
        if (map.contains("data-fingerprint")) {
            _dataFingerprint = map.value("data-fingerprint").toUtf8();
            if (_dataFingerprint.isEmpty()) {
                // Placeholder that means that the server supports the feature even if it did not set one.
                _dataFingerprint = "[empty]";
            }
        }
        if (map.contains(QStringLiteral("fileid"))) {
            _localFileId = map.value(QStringLiteral("fileid")).toUtf8();
        }
        if (map.contains("id")) {
            _fileId = map.value("id").toUtf8();
        }
        if (map.contains("is-encrypted") && map.value("is-encrypted") == QStringLiteral("1")) {
            _encryptionStatusCurrent = SyncFileItem::EncryptionStatus::Encrypted;
            Q_ASSERT(!_fileId.isEmpty());
        }
        if (map.contains("size")) {
            _size = map.value("size").toInt();
        }
    } else {
        RemoteInfo result;
        int slash = file.lastIndexOf('/');
        result.name = file.mid(slash + 1);
        result.size = -1;
        propertyMapToRemoteInfo(map,
                                _account->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                result);
        if (result.isDirectory)
            result.size = 0;

        _results.push_back(std::move(result));
    }

    //This works in concerto with the RequestEtagJob and the Folder object to check if the remote folder changed.
    if (map.contains("getetag")) {
        if (_firstEtag.isEmpty()) {
            _firstEtag = parseEtag(map.value(QStringLiteral("getetag")).toUtf8()); // for directory itself
        }
    }
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithoutErrorSlot()
{
    if (!_ignoredFirst) {
        // This is a sanity check, if we haven't _ignoredFirst then it means we never received any directoryListingIteratedSlot
        // which means somehow the server XML was bogus
        emit finished(HttpError{ 0, tr("Server error: PROPFIND reply is not XML formatted!") });
        deleteLater();
        return;
    } else if (!_error.isEmpty()) {
        emit finished(HttpError{ 0, _error });
        deleteLater();
        return;
    } else if (isE2eEncrypted() && _account->capabilities().clientSideEncryptionAvailable()) {
        emit etag(_firstEtag, QDateTime::fromString(QString::fromUtf8(_lsColJob->responseTimestamp()), Qt::RFC2822Date));
        fetchE2eMetadata();
        return;
    } else if (isE2eEncrypted() && !_account->capabilities().clientSideEncryptionAvailable()) {
        emit etag(_firstEtag, QDateTime::fromString(QString::fromUtf8(_lsColJob->responseTimestamp()), Qt::RFC2822Date));
        emit finished(_results);
    }
    emit etag(_firstEtag, QDateTime::fromString(QString::fromUtf8(_lsColJob->responseTimestamp()), Qt::RFC2822Date));
    emit finished(_results);
    deleteLater();
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot(QNetworkReply *r)
{
    const auto contentType = r->header(QNetworkRequest::ContentTypeHeader).toString();
    const auto invalidContentType = !contentType.contains("application/xml; charset=utf-8") &&
                                    !contentType.contains("application/xml; charset=\"utf-8\"") &&
                                    !contentType.contains("text/xml; charset=utf-8") &&
                                    !contentType.contains("text/xml; charset=\"utf-8\"");
    const auto httpCode = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    auto msg = r->errorString();

    qCWarning(lcDiscovery) << "LSCOL job error" << r->errorString() << httpCode << r->error();

    if (r->error() == QNetworkReply::NoError && invalidContentType) {
        msg = tr("Server error: PROPFIND reply is not XML formatted!");
    }

    if (r->error() == QNetworkReply::ContentAccessDenied) {
        emit _account->termsOfServiceNeedToBeChecked();
    }

    emit finished(HttpError{ httpCode, msg });
    deleteLater();
}

void DiscoverySingleDirectoryJob::fetchE2eMetadata()
{
    const auto job = new GetMetadataApiJob(_account, _localFileId);
    connect(job, &GetMetadataApiJob::jsonReceived,
            this, &DiscoverySingleDirectoryJob::metadataReceived);
    connect(job, &GetMetadataApiJob::error,
            this, &DiscoverySingleDirectoryJob::metadataError);
    job->start();
}

void DiscoverySingleDirectoryJob::metadataReceived(const QJsonDocument &json, int statusCode)
{
    qCDebug(lcDiscovery) << "Metadata received, applying it to the result list";
    Q_ASSERT(_subPath.startsWith('/'));

    const auto job = qobject_cast<GetMetadataApiJob *>(sender());
    Q_ASSERT(job);
    if (!job) {
        qCDebug(lcDiscovery) << "metadataReceived must be called from GetMetadataApiJob's signal";
        emit finished(HttpError{0, tr("Encrypted metadata setup error!")});
        deleteLater();
        return;
    }

    // as per E2EE V2, top level folder is the only source of encryption keys and users that have access to it
    // hence, we need to find its path and pass to any subfolder's metadata, so it will fetch the top level metadata when needed
    // see https://github.com/nextcloud/end_to_end_encryption_rfc/blob/v2.1/RFC.md
    auto topLevelFolderPath = QStringLiteral("/");
    for (const QString &topLevelPath : _topLevelE2eeFolderPaths) {
        if (_subPath == topLevelPath) {
            topLevelFolderPath = QStringLiteral("/");
            break;
        }
        if (_subPath.startsWith(topLevelPath + QLatin1Char('/'))) {
            const auto topLevelPathSplit = topLevelPath.split(QLatin1Char('/'));
            topLevelFolderPath = topLevelPathSplit.join(QLatin1Char('/'));
            break;
        }
    }

    if (job->signature().isEmpty()) {
        qCDebug(lcDiscovery) << "Initial signature is empty.";
        _account->reportClientStatus(OCC::ClientStatusReportingStatus::E2EeError_GeneralError);
        emit finished(HttpError{0, tr("Encrypted metadata setup error: initial signature from server is empty.")});
        deleteLater();
        return;
    }

    const auto e2EeFolderMetadata = new FolderMetadata(_account,
                                                 _remoteRootFolderPath,
                                                 statusCode == 404 ? QByteArray{} : json.toJson(QJsonDocument::Compact),
                                                 RootEncryptedFolderInfo(Utility::fullRemotePathToRemoteSyncRootRelative(topLevelFolderPath, _remoteRootFolderPath)),
                                                 job->signature());
    connect(e2EeFolderMetadata, &FolderMetadata::setupComplete, this, [this, e2EeFolderMetadata] {
        e2EeFolderMetadata->deleteLater();
        if (!e2EeFolderMetadata->isValid()) {
            emit finished(HttpError{0, tr("Encrypted metadata setup error!")});
            deleteLater();
            return;
        }
        _isFileDropDetected = e2EeFolderMetadata->isFileDropPresent();
        _encryptedMetadataNeedUpdate = e2EeFolderMetadata->encryptedMetadataNeedUpdate();
        _e2eCertificateFingerprint = e2EeFolderMetadata->certificateSha256Fingerprint();
        _encryptionStatusRequired = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_account->capabilities().clientSideEncryptionVersion());
        _encryptionStatusCurrent = e2EeFolderMetadata->existingMetadataEncryptionStatus();

        const auto encryptedFiles = e2EeFolderMetadata->files();

        const auto findEncryptedFile = [=](const QString &name) {
            const auto it = std::find_if(std::cbegin(encryptedFiles), std::cend(encryptedFiles), [=](const FolderMetadata::EncryptedFile &file) {
                return file.encryptedFilename == name;
            });
            if (it == std::cend(encryptedFiles)) {
                return Optional<FolderMetadata::EncryptedFile>();
            } else {
                return Optional<FolderMetadata::EncryptedFile>(*it);
            }
        };

        std::transform(std::cbegin(_results), std::cend(_results), std::begin(_results), [=](const RemoteInfo &info) {
            auto result = info;
            const auto encryptedFileInfo = findEncryptedFile(result.name);
            if (encryptedFileInfo) {
                result._isE2eEncrypted = true;
                result.e2eMangledName = _subPath.mid(1) + QLatin1Char('/') + result.name;
                result.name = encryptedFileInfo->originalFilename;
            }
            return result;
        });

        emit finished(_results);
        deleteLater();
    });
}

void DiscoverySingleDirectoryJob::metadataError(const QByteArray &fileId, int httpReturnCode)
{
    qCWarning(lcDiscovery) << "E2EE Metadata job error. Trying to proceed without it." << fileId << httpReturnCode;
    emit finished(_results);
    deleteLater();
}
}
