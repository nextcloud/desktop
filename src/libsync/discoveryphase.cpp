/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include <QDir>
#include <QFileInfo>
#include <QTextCodec>
#include <cstring>
#include <QDateTime>

using namespace Qt::StringLiterals;

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
    connect(propfindJob, &PropfindJob::result, this, [=, this](const QVariantMap &values) {
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
            if (!(instruction == CSYNC_INSTRUCTION_REMOVE ||
                  instruction == CSYNC_INSTRUCTION_IGNORE ||
                  ((*it)->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW) ||// re-creation of virtual files count as a delete
                  ((*it)->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW))) {
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

bool DiscoveryPhase::recursiveCheckForDeletedParents(const QString &itemPath) const
{
    const auto &allKeys = _deletedItem.keys();
    qCDebug(lcDiscovery()) << allKeys.join(", ");

    auto result = false;
    const auto &pathElements = itemPath.split('/');
    auto currentParentFolder = QString{};
    for (const auto &onePathComponent : pathElements) {
        if (!currentParentFolder.isEmpty()) {
            currentParentFolder += '/';
        }
        currentParentFolder += onePathComponent;

        qCDebug(lcDiscovery()) << "checks" << currentParentFolder << "for" << allKeys.join(", ");
        if (_deletedItem.find(currentParentFolder) == _deletedItem.end()) {
            continue;
        }

        qCDebug(lcDiscovery()) << "deleted parent found";
        result = true;
        break;
    }

    return result;
}

void DiscoveryPhase::markPermanentDeletionRequests()
{
    // since we don't know in advance which files/directories need to be permanently deleted,
    // we have to look through all of them at the end of the run
    for (const auto &originalPath : std::as_const(_permanentDeletionRequests)) {
        const auto it = _deletedItem.find(originalPath);
        if (it == _deletedItem.end()) {
            qCWarning(lcDiscovery) << "didn't find an item for" << originalPath << "(yet)";
            continue;
        }

        auto item = *it;
        if (!(item->_instruction == CSYNC_INSTRUCTION_REMOVE || item->_direction == SyncFileItem::Up)) {
            qCInfo(lcDiscovery) << "will not request permanent deletion for" << originalPath << "as the instruction is not CSYNC_INSTRUCTION_REMOVE, or the direction is not Up";
            continue;
        }

        qCDebug(lcDiscovery) << "requested permanent server-side deletion for" << originalPath;
        item->_wantsSpecificActions = SyncFileItem::SynchronizationOptions::WantsPermanentDeletion;
    }
}

void DiscoveryPhase::startJob(ProcessDirectoryJob *job)
{
    Q_ASSERT(!_currentRootJob);
    connect(this, &DiscoveryPhase::itemDiscovered, this, &DiscoveryPhase::slotItemDiscovered, Qt::UniqueConnection);
    connect(job, &ProcessDirectoryJob::finished, this, [this, job] {
        Q_ASSERT(_currentRootJob == sender());
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
            markPermanentDeletionRequests();
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

DiscoverySingleLocalDirectoryJob::DiscoverySingleLocalDirectoryJob(const AccountPtr &account,
                                                                   const QString &localPath,
                                                                   OCC::Vfs *vfs,
                                                                   bool fileSystemReliablePermissions,
                                                                   QObject *parent)
    : QObject{parent}
    , QRunnable{}
    , _localPath{localPath}
    , _account{account}
    , _vfs{vfs}
    , _fileSystemReliablePermissions{fileSystemReliablePermissions}
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
        auto dirent = csync_vio_local_readdir(dh, _vfs, _fileSystemReliablePermissions);
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
        i.isDirectory = dirent->type == ItemTypeDirectory || dirent->type == ItemTypeVirtualDirectory;
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
                                                         SyncFileItem::EncryptionStatus parentEncryptionStatus,
                                                         QObject *parent)
    : QObject(parent)
    , _subPath(remoteRootFolderPath + path)
    , _remoteRootFolderPath(remoteRootFolderPath)
    , _account(account)
    , _encryptionStatusCurrent{parentEncryptionStatus}
    , _topLevelE2eeFolderPaths(topLevelE2eeFolderPaths)
{
    Q_ASSERT(!_remoteRootFolderPath.isEmpty());
}

void DiscoverySingleDirectoryJob::start()
{
    // Start the actual HTTP job
    auto *lsColJob = new LsColJob(_account, _subPath);

    const auto props = LsColJob::defaultProperties(_isRootPath ? LsColJob::FolderType::RootFolder : LsColJob::FolderType::ChildFolder,
                                                   _account);
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

void DiscoverySingleDirectoryJob::directoryListingIteratedSlot(const QString &file, const QMap<QString, QString> &map)
{
    if (!_ignoredFirst) {
        // The first entry is for the folder itself, we should process it differently.
        _ignoredFirst = true;
        if (map.contains("permissions")) {
            const auto perm = RemotePermissions::fromServerString(map.value("permissions"),
                                                            _account->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                                            map);
            emit firstDirectoryPermissions(perm);
            _isExternalStorage = perm.hasPermission(RemotePermissions::IsMounted);
        }
        if (map.contains("data-fingerprint"_L1)) {
            _dataFingerprint = map.value("data-fingerprint"_L1).toUtf8();
            if (_dataFingerprint.isEmpty()) {
                // Placeholder that means that the server supports the feature even if it did not set one.
                _dataFingerprint = "[empty]";
            }
        }
        if (map.contains("fileid"_L1)) {
            // this is from the "oc:fileid" property, this is the plain ID without any special format (e.g. "2")
            _localFileId = map.value("fileid"_L1).toUtf8();

            bool ok = false;
            if (qint64 numericFileId = _localFileId.toLongLong(&ok, 10); ok) {
                qCDebug(lcDiscovery).nospace() << "received numericFileId=" << numericFileId;
                emit firstDirectoryFileId(numericFileId);
            } else {
                qCWarning(lcDiscovery).nospace() << "conversion to qint64 failed _localFileId=" << _localFileId;
            }
        }
        if (map.contains("id"_L1)) {
            // this is from the "oc:id" property, the format is e.g. "00000002oc123xyz987e"
            _fileId = map.value("id"_L1).toUtf8();
        }
        if (map.contains("is-encrypted"_L1) && map.value("is-encrypted"_L1) == "1"_L1) {
            _encryptionStatusCurrent = SyncFileItem::EncryptionStatus::EncryptedMigratedV2_0;
            Q_ASSERT(!_fileId.isEmpty());
        }
        if (map.contains("size"_L1)) {
            _size = map.value("size"_L1).toInt();
        }

        // all folders will contain both
        if (map.contains(FolderQuota::usedBytesC) && map.contains(FolderQuota::availableBytesC)) {          
            // The server can respond with e.g. "2.58440798353E+12" for the quota
            // therefore: parse the string as a double and cast it to i64
            auto ok = false;
            auto quotaValue = static_cast<int64_t>(map.value(FolderQuota::usedBytesC).toDouble(&ok));
            _folderQuota.bytesUsed = ok ? quotaValue : -1;
            quotaValue = static_cast<int64_t>(map.value(FolderQuota::availableBytesC).toDouble(&ok));
            _folderQuota.bytesAvailable = ok ? quotaValue : -1;

            qCDebug(lcDiscovery) << "Setting quota for" << file
                                 << "bytesUsed:" << _folderQuota.bytesUsed
                                 << "bytesAvailable:" << _folderQuota.bytesAvailable
                                 << "ok:" << ok;
            emit setfolderQuota(_folderQuota);
        }
    } else {
        RemoteInfo result;
        int slash = file.lastIndexOf(u'/');
        result.name = file.mid(slash + 1);
        result.size = -1;
        LsColJob::propertyMapToRemoteInfo(map,
                                          _account->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                          result);
        if (result.isDirectory)
            result.size = 0;

        _results.push_back(std::move(result));
    }

    //This works in concerto with the RequestEtagJob and the Folder object to check if the remote folder changed.
    if (map.contains("getetag"_L1)) {
        if (_firstEtag.isEmpty()) {
            _firstEtag = parseEtag(map.value("getetag"_L1).toUtf8()); // for directory itself
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
    }
    emit etag(_firstEtag, QDateTime::fromString(QString::fromUtf8(_lsColJob->responseTimestamp()), Qt::RFC2822Date));
    emit finished(_results);
    deleteLater();
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot(QNetworkReply *reply)
{
    const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const auto invalidContentType = !contentType.contains("application/xml; charset=utf-8") &&
                                    !contentType.contains("application/xml; charset=\"utf-8\"") &&
                                    !contentType.contains("text/xml; charset=utf-8") &&
                                    !contentType.contains("text/xml; charset=\"utf-8\"");
    const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    auto errorString = _lsColJob->errorString();

    qCWarning(lcDiscovery) << "LSCOL job error" << reply->errorString() << httpCode << reply->error();

    if (reply->error() == QNetworkReply::NoError && invalidContentType) {
        errorString = tr("The server returned an unexpected response that couldn’t be read. Please reach out to your server administrator.”");
        qCWarning(lcDiscovery) << "Server error: PROPFIND reply is not XML formatted!";
    }

    if (reply->error() == QNetworkReply::ContentAccessDenied) {
        emit _account->termsOfServiceNeedToBeChecked();
    }

    emit finished(HttpError{ httpCode, errorString });
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
    Q_ASSERT(_subPath.startsWith(u'/'));

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
    QString topLevelFolderPath = u"/"_s;
    for (const QString &topLevelPath : std::as_const(_topLevelE2eeFolderPaths)) {
        if (_subPath == topLevelPath) {
            topLevelFolderPath = u"/"_s;
            break;
        }
        if (_subPath.startsWith(topLevelPath + u'/')) {
            const auto topLevelPathSplit = topLevelPath.split(u'/');
            topLevelFolderPath = topLevelPathSplit.join(u'/');
            break;
        }
    }

    if (_account->capabilities().clientSideEncryptionVersion() >= 2.0 && job->signature().isEmpty()) {
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
        _encryptionStatusRequired = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_account->capabilities().clientSideEncryptionVersion());
        _encryptionStatusCurrent = e2EeFolderMetadata->existingMetadataEncryptionStatus();

        Q_ASSERT(_encryptionStatusCurrent != SyncFileItem::EncryptionStatus::Encrypted);
        Q_ASSERT(_encryptionStatusCurrent != SyncFileItem::EncryptionStatus::NotEncrypted);

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

        std::transform(std::cbegin(_results), std::cend(_results), std::begin(_results), [=, this](const RemoteInfo &info) {
            auto result = info;
            const auto encryptedFileInfo = findEncryptedFile(result.name);
            if (encryptedFileInfo) {
                result._isE2eEncrypted = true;
                result.e2eMangledName = _subPath.mid(1) + u'/' + result.name;
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
