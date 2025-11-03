/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "editlocallyjob.h"

#include <QMessageBox>
#include <QDesktopServices>
#include <QtConcurrent>

#include "editlocallymanager.h"
#include "folder.h"
#include "folderman.h"
#include "syncengine.h"
#include "systray.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcEditLocallyJob, "nextcloud.gui.editlocallyjob", QtInfoMsg)

EditLocallyJob::EditLocallyJob(const AccountStatePtr &accountState,
                               const QString &relPath,
                               QObject *parent)
    : QObject{parent}
    , _accountState(accountState)
    , _relPath(relPath)
{
    connect(this, &EditLocallyJob::callShowError, this, &EditLocallyJob::showError, Qt::QueuedConnection);
}

void EditLocallyJob::startSetup()
{
    if (_relPath.isEmpty() || !_accountState) {
        qCWarning(lcEditLocallyJob) << "Could not start setup."
                                    << "relPath:" << _relPath
                                    << "accountState:" << _accountState;
        showError(tr("Could not start editing locally."), tr("An error occurred during setup."));
        return;
    }

    const auto relPathSplit = _relPath.split(QLatin1Char('/'));
    if (relPathSplit.isEmpty()) {
        showError(tr("Could not find a file for local editing. "
                     "Make sure its path is valid and it is synced locally."), _relPath);
        return;
    }

    _fileName = relPathSplit.last();
    Systray::instance()->createEditFileLocallyLoadingDialog(_fileName);
    findAfolderAndConstructPaths();
}

void EditLocallyJob::findAfolderAndConstructPaths()
{
    _folderForFile = findFolderForFile(_relPath, _accountState->account()->userIdAtHostWithPort());

    if (!_folderForFile) {
        showError(tr("Could not find a file for local editing. Make sure it is not excluded via selective sync."), _relPath);
        return;
    }

    _relativePathToRemoteRoot = getRelativePathToRemoteRootForFile();

    if (_relativePathToRemoteRoot.isEmpty()) {
        qCWarning(lcEditLocallyJob) << "_relativePathToRemoteRoot is empty for" << _relPath;
        showError(tr("Could not find a file for local editing. Make sure it is not excluded via selective sync."), _relPath);
        return;
    }

    _relPathParent = getRelativePathParent();

    if (_relPathParent.isEmpty()) {
        showError(tr("Could not find a file for local editing. Make sure it is not excluded via selective sync."), _relPath);
        return;
    }

    if (_relPathParent == QStringLiteral("/")) {
        proceedWithSetup();
        return;
    }

    fetchRemoteFileParentInfo();
}

QString EditLocallyJob::prefixSlashToPath(const QString &path)
{
    return path.startsWith('/') ? path : QChar::fromLatin1('/') + path;
}

void EditLocallyJob::fetchRemoteFileParentInfo()
{
    Q_ASSERT(_relPathParent != QStringLiteral("/"));

    if (_relPathParent == QStringLiteral("/")) {
        qCWarning(lcEditLocallyJob) << "LsColJob must only be used for nested folders.";
        showError(tr("Could not start editing locally."),
                  tr("An error occurred during data retrieval."));
        return;
    }

    const auto job = new LsColJob(_accountState->account(), QDir::cleanPath(_folderForFile->remotePathTrailingSlash() + _relPathParent));
    const QList<QByteArray> props{QByteArrayLiteral("resourcetype"),
                                  QByteArrayLiteral("getlastmodified"),
                                  QByteArrayLiteral("getetag"),
                                  QByteArrayLiteral("http://owncloud.org/ns:size"),
                                  QByteArrayLiteral("http://owncloud.org/ns:id"),
                                  QByteArrayLiteral("http://owncloud.org/ns:permissions"),
                                  QByteArrayLiteral("http://owncloud.org/ns:checksums"),
                                  QByteArrayLiteral("http://nextcloud.org/ns:is-mount-root")};

    job->setProperties(props);
    connect(job, &LsColJob::directoryListingIterated, this, &EditLocallyJob::slotDirectoryListingIterated);
    connect(job, &LsColJob::finishedWithoutError, this, &EditLocallyJob::proceedWithSetup);
    connect(job, &LsColJob::finishedWithError, this, &EditLocallyJob::slotLsColJobFinishedWithError);
    job->start();
}

void EditLocallyJob::proceedWithSetup()
{
    _folderForFile = findFolderForFile(_relPath, _accountState->account()->userIdAtHostWithPort());
    
    if (!_folderForFile) {
        showError(tr("Could not find a file for local editing. Make sure it is not excluded via selective sync."), _relPath);
        return;
    }

    if (!isFileParentItemValid()) {
        showError(tr("Could not find a file for local editing. Make sure its path is valid and it is synced locally."), _relPath);
        return;
    }

    _localFilePath = _folderForFile->path() + _relativePathToRemoteRoot;

    startEditLocally();
}

bool EditLocallyJob::checkIfFileParentSyncIsNeeded()
{
    if (_relPathParent == QLatin1String("/")) {
        return true;
    }

    Q_ASSERT(_fileParentItem && !_fileParentItem->isEmpty());

    if (!_fileParentItem || _fileParentItem->isEmpty()) {
        return true;
    }

    SyncJournalFileRecord rec;
    if (!_folderForFile->journalDb()->getFileRecord(_fileParentItem->_file, &rec) || !rec.isValid()) {
        // we don't have this folder locally, so let's sync it
        _fileParentItem->_direction = SyncFileItem::Down;
        _fileParentItem->_instruction = CSYNC_INSTRUCTION_NEW;
    } else if (rec._etag != _fileParentItem->_etag && rec._modtime != _fileParentItem->_modtime) {
        // we just need to update metadata as the folder is already present locally
        _fileParentItem->_direction = rec._modtime < _fileParentItem->_modtime ? SyncFileItem::Down : SyncFileItem::Up;
        _fileParentItem->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
    } else {
        _fileParentItem->_direction = SyncFileItem::Down;
        _fileParentItem->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
        SyncJournalFileRecord recFile;
        if (_folderForFile->journalDb()->getFileRecord(_relativePathToRemoteRoot, &recFile) && recFile.isValid()) {
            return false;
        }
    }
    return true;
}

void EditLocallyJob::startSyncBeforeOpening()
{
    if (!eraseBlacklistRecordForItem()) {
        showError(tr("Could not start editing locally."),
                  tr("An error occurred trying to synchronise the file to edit locally."));
        return;
    }

    if (!checkIfFileParentSyncIsNeeded()) {
        processLocalItem();
        return;
    }

    // connect to a SyncEngine::itemDiscovered so we can complete the job as soon as the file in question is discovered
    QObject::connect(&_folderForFile->syncEngine(), &SyncEngine::itemDiscovered, this, &EditLocallyJob::slotItemDiscovered);
    _folderForFile->syncEngine().setSingleItemDiscoveryOptions({_relPathParent, _relativePathToRemoteRoot, _fileParentItem});
    FolderMan::instance()->forceSyncForFolder(_folderForFile);
}

bool EditLocallyJob::eraseBlacklistRecordForItem()
{
    if (!_folderForFile || !isFileParentItemValid()) {
        qCWarning(lcEditLocallyJob) << "_folderForFile or _fileParentItem is invalid!";
        return false;
    }

    if (!_fileParentItem && _relPathParent == QStringLiteral("/")) {
        return true;
    }

    Q_ASSERT(_fileParentItem);
    if (!_fileParentItem) {
        qCWarning(lcEditLocallyJob) << "_fileParentItem is invalid!";
        return false;
    }

    Q_ASSERT(!_folderForFile->isSyncRunning());
    if (_folderForFile->isSyncRunning()) {
        qCWarning(lcEditLocallyJob) << "_folderForFile is syncing";
        return false;
    }

    if (_folderForFile->journalDb()->errorBlacklistEntry(_fileParentItem->_file).isValid()) {
        _folderForFile->journalDb()->wipeErrorBlacklistEntry(_fileParentItem->_file);
    }

    return true;
}

const QString EditLocallyJob::getRelativePathToRemoteRootForFile() const
{
    Q_ASSERT(_folderForFile);
    if (!_folderForFile) {
        return {};
    }

    if (_folderForFile->remotePathTrailingSlash().size() == 1) {
        return _relPath;
    } else {
        const auto remoteFolderPathWithTrailingSlash = _folderForFile->remotePathTrailingSlash();
        const auto remoteFolderPathWithoutLeadingSlash =
            remoteFolderPathWithTrailingSlash.startsWith(QLatin1Char('/')) ? remoteFolderPathWithTrailingSlash.mid(1) : remoteFolderPathWithTrailingSlash;

        return _relPath.startsWith(remoteFolderPathWithoutLeadingSlash) ? _relPath.mid(remoteFolderPathWithoutLeadingSlash.size()) : _relPath;
    }
}

const QString EditLocallyJob::getRelativePathParent() const
{
    Q_ASSERT(!_relativePathToRemoteRoot.isEmpty());
    if (_relativePathToRemoteRoot.isEmpty()) {
        return {};
    }
    auto relativePathToRemoteRootSplit = _relativePathToRemoteRoot.split(QLatin1Char('/'));
    if (relativePathToRemoteRootSplit.size() > 1) {
        relativePathToRemoteRootSplit.removeLast();
        return relativePathToRemoteRootSplit.join(QLatin1Char('/'));
    }
    return QStringLiteral("/");
}

OCC::Folder *EditLocallyJob::findFolderForFile(const QString &relPath, const QString &userId)
{
    if (relPath.isEmpty()) {
        return nullptr;
    }

    const auto folderMap = FolderMan::instance()->map();
    const auto relPathSplit = relPath.split(QLatin1Char('/'));

    // a file is on the first level of remote root, so, we just need a proper folder that points to a remote root
    if (relPathSplit.size() == 1) {
        const auto foundIt = std::find_if(std::begin(folderMap), 
                                          std::end(folderMap), 
                                          [&userId](const OCC::Folder *folder) {
            const auto folderUserId = folder->accountState()->account()->userIdAtHostWithPort();
            return folder->remotePath() == QStringLiteral("/") && folderUserId == userId;
        });

        return foundIt != std::end(folderMap) ? foundIt.value() : nullptr;
    }

    const auto relPathWithSlash = 
        relPath.startsWith(QStringLiteral("/")) ? relPath : QStringLiteral("/") + relPath;

    for (const auto &folder : folderMap) {
        // make sure we properly handle folders with non-root(nested) remote paths
        if ((folder->remotePath() != QStringLiteral("/") && !relPathWithSlash.startsWith(folder->remotePath()))
            || folder->accountState()->account()->userIdAtHostWithPort() != userId) {
            continue;
        }
        auto result = false;
        const auto excludedThroughSelectiveSync = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &result);
        auto isExcluded = false;
        for (const auto &excludedPath : excludedThroughSelectiveSync) {
            if (relPath.startsWith(excludedPath)) {
                isExcluded = true;
                break;
            }
        }
        if (isExcluded) {
            continue;
        }
        return folder;
    }

    return nullptr;
}

void EditLocallyJob::showError(const QString &message, const QString &informativeText)
{
    Systray::instance()->destroyEditFileLocallyLoadingDialog();  
    EditLocallyManager::showError(message, informativeText);
    Q_EMIT error(message, informativeText);
}

void EditLocallyJob::startEditLocally()
{
    if (_fileName.isEmpty() || _localFilePath.isEmpty() || !_folderForFile) {
        qCWarning(lcEditLocallyJob) << "Could not start to edit locally."
                                        << "fileName:" << _fileName
                                        << "localFilePath:" << _localFilePath
                                        << "folderForFile:" << _folderForFile;

        showError(tr("Could not start editing locally."), tr("An error occurred during setup."));
        return;
    }

    if (_folderForFile->isSyncRunning()) {
        // in case sync is already running - terminate it and start a new one
        _syncTerminatedConnection = connect(_folderForFile, &Folder::syncFinished, this, [this]() {
            disconnect(_syncTerminatedConnection);
            _syncTerminatedConnection = {};
            startSyncBeforeOpening();
        });
        _folderForFile->setSilenceErrorsUntilNextSync(true);
        _folderForFile->slotTerminateSync();
        _shouldScheduleFolderSyncAfterFileIsOpened = true;

        return;
    }
    startSyncBeforeOpening();
}

void EditLocallyJob::slotItemCompleted(const OCC::SyncFileItemPtr &item)
{
    Q_ASSERT(item && !item->isEmpty());
    if (!item || item->isEmpty()) {
        qCWarning(lcEditLocallyJob) << "invalid item";
    }
    if (item->_file == _relativePathToRemoteRoot) {
        disconnect(&_folderForFile->syncEngine(), &SyncEngine::itemCompleted, this, &EditLocallyJob::slotItemCompleted);
        disconnect(&_folderForFile->syncEngine(), &SyncEngine::itemDiscovered, this, &EditLocallyJob::slotItemDiscovered);
        processLocalItem();
    }
}

void EditLocallyJob::slotLsColJobFinishedWithError(QNetworkReply *reply)
{
    const auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const auto invalidContentType = !contentType.contains(QStringLiteral("application/xml; charset=utf-8"))
        && !contentType.contains(QStringLiteral("application/xml; charset=\"utf-8\"")) && !contentType.contains(QStringLiteral("text/xml; charset=utf-8"))
        && !contentType.contains(QStringLiteral("text/xml; charset=\"utf-8\""));
    const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    qCWarning(lcEditLocallyJob) << "LSCOL job error" << reply->errorString() << httpCode << reply->error();

    const auto message = reply->error() == QNetworkReply::NoError && invalidContentType
        ? tr("Server error: PROPFIND reply is not XML formatted!") : reply->errorString();
    qCWarning(lcEditLocallyJob) << "Could not proceed with setup as file PROPFIND job has failed." << httpCode << message;
    showError(tr("Could not find a remote file info for local editing. Make sure its path is valid."), _relPath);
}

void EditLocallyJob::slotDirectoryListingIterated(const QString &name, const QMap<QString, QString> &properties)
{
    Q_ASSERT(_relPathParent != QStringLiteral("/"));

    if (_relPathParent == QStringLiteral("/")) {
        qCWarning(lcEditLocallyJob) << "LsColJob must only be used for nested folders.";
        showError(tr("Could not start editing locally."),
                  tr("An error occurred during data retrieval."));
        return;
    }

    const auto job = qobject_cast<LsColJob*>(sender());
    Q_ASSERT(job);
    if (!job) {
        qCWarning(lcEditLocallyJob) << "Must call slotDirectoryListingIterated from a signal.";
        showError(tr("Could not start editing locally."),
                  tr("An error occurred during data retrieval."));
        return;
    }

    if (name.endsWith(_relPathParent)) {
        // let's remove remote dav path and remote root from the beginning of the name
        const auto startIndex = name.indexOf(_accountState->account()->davPath());
        const auto nameWithoutDavPath = name.mid(startIndex + _accountState->account()->davPath().size());

        const auto remoteFolderPathWithTrailingSlash = _folderForFile->remotePathTrailingSlash();
        const auto remoteFolderPathWithoutLeadingSlash = remoteFolderPathWithTrailingSlash.startsWith(QLatin1Char('/'))
            ? remoteFolderPathWithTrailingSlash.mid(1) : remoteFolderPathWithTrailingSlash;

        const auto cleanName = nameWithoutDavPath.startsWith(remoteFolderPathWithoutLeadingSlash)
            ? nameWithoutDavPath.mid(remoteFolderPathWithoutLeadingSlash.size()) : nameWithoutDavPath;
        disconnect(job, &LsColJob::directoryListingIterated, this, &EditLocallyJob::slotDirectoryListingIterated);
        _fileParentItem = SyncFileItem::fromProperties(cleanName,
                                                       properties,
                                                       _accountState->account()->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty);
    }
}

void EditLocallyJob::slotItemDiscovered(const OCC::SyncFileItemPtr &item)
{
    Q_ASSERT(item && !item->isEmpty());
    if (!item || item->isEmpty()) {
        qCWarning(lcEditLocallyJob) << "invalid item";
        showError(tr("Could not start editing locally."),
                  tr("An error occurred trying to synchronise the file to edit locally."));
    } else if (item->_file == _relativePathToRemoteRoot) {
        disconnect(&_folderForFile->syncEngine(), &SyncEngine::itemDiscovered, this, &EditLocallyJob::slotItemDiscovered);
        if (item->_instruction == CSYNC_INSTRUCTION_NONE) {
            // return early if the file is already in sync
            slotItemCompleted(item);
            return;
        }
        // or connect to the SyncEngine::itemCompleted and wait till the file gets sycned
        QObject::connect(&_folderForFile->syncEngine(), &SyncEngine::itemCompleted, this, &EditLocallyJob::slotItemCompleted);
    }
}

void EditLocallyJob::openFile()
{
    Q_ASSERT(_folderForFile);

    if(_localFilePath.isEmpty()) {
        qCWarning(lcEditLocallyJob) << "Could not edit locally. Invalid local file path.";
        showError(tr("Could not start editing locally."),
                  tr("Invalid local file path."));
        return;
    }

    const auto localFilePathUrl = QUrl::fromLocalFile(_localFilePath);
    // In case the VFS mode is enabled and a file is not yet hydrated, we must call QDesktopServices::openUrl
    // from a separate thread, or, there will be a freeze. To avoid searching for a specific folder and checking
    // if the VFS is enabled - we just always call it from a separate thread.
    auto futureResult = QtConcurrent::run([localFilePathUrl, this]() {
        if (!QDesktopServices::openUrl(localFilePathUrl)) {
            emit callShowError(tr("Could not open %1").arg(_fileName), tr("Please try again."));
        }

        Systray::instance()->destroyEditFileLocallyLoadingDialog();

        if (_shouldScheduleFolderSyncAfterFileIsOpened) {
            _folderForFile->startSync();
        }

        emit finished();
    });
}

void EditLocallyJob::processLocalItem()
{
    Q_ASSERT(_folderForFile);

    SyncJournalFileRecord rec;
    const auto ok = _folderForFile->journalDb()->getFileRecord(_relativePathToRemoteRoot, &rec);
    Q_ASSERT(ok);

    // Do not lock if it is a directory or lock is not available on the server
    if (rec.isDirectory() || !_accountState->account()->capabilities().filesLockAvailable()) {
        openFile();
    } else {
        lockFile(rec._etag);
    }
}

void EditLocallyJob::lockFile(const QString &etag)
{
    Q_ASSERT(_accountState);
    Q_ASSERT(_accountState->account());
    Q_ASSERT(_folderForFile);

    if (_accountState->account()->fileLockStatus(_folderForFile->journalDb(), _relativePathToRemoteRoot) == SyncFileItem::LockStatus::LockedItem) {
        fileAlreadyLocked();
        return;
    }

    const auto syncEngineFileSlot = [this](const SyncFileItemPtr &item) {
        if (item->_file == _relativePathToRemoteRoot && item->_locked == SyncFileItem::LockStatus::LockedItem) {
            fileLockSuccess(item);
        }
    };

    const auto runSingleFileDiscovery = [this] {
        const SyncEngine::SingleItemDiscoveryOptions singleItemDiscoveryOptions = {_relPathParent, _relativePathToRemoteRoot, _fileParentItem};
        _folderForFile->syncEngine().setSingleItemDiscoveryOptions(singleItemDiscoveryOptions);
        FolderMan::instance()->forceSyncForFolder(_folderForFile);
    };

    _folderConnections.append(connect(&_folderForFile->syncEngine(), &SyncEngine::itemCompleted,
                                      this, syncEngineFileSlot));
    _folderConnections.append(connect(&_folderForFile->syncEngine(), &SyncEngine::itemDiscovered,
                                      this, syncEngineFileSlot));
    _folderConnections.append(connect(_accountState->account().data(), &Account::lockFileSuccess,
                                      this, runSingleFileDiscovery));
    _folderConnections.append(connect(_accountState->account().data(), &Account::lockFileError,
                                      this, &EditLocallyJob::fileLockError));

    _folderForFile->accountState()->account()->setLockFileState(_relPath,
                                                                _folderForFile->remotePathTrailingSlash(),
                                                                _folderForFile->path(),
                                                                etag,
                                                                _folderForFile->journalDb(),
                                                                SyncFileItem::LockStatus::LockedItem,
                                                                SyncFileItem::LockOwnerType::TokenLock);
}

void EditLocallyJob::disconnectFolderSignals()
{
    for (const auto &connection : std::as_const(_folderConnections)) {
        disconnect(connection);
    }
}

void EditLocallyJob::fileAlreadyLocked()
{
    SyncJournalFileRecord rec;
    Q_ASSERT(_folderForFile->journalDb()->getFileRecord(_relativePathToRemoteRoot, &rec));
    Q_ASSERT(rec.isValid());
    Q_ASSERT(rec._lockstate._locked);

    const auto remainingTimeInMinutes = fileLockTimeRemainingMinutes(rec._lockstate._lockTime, rec._lockstate._lockTimeout);
    fileLockProcedureComplete(tr("File %1 already locked.").arg(_fileName),
                              tr("Lock will last for %1 minutes. "
                                 "You can also unlock this file manually once you are finished editing.").arg(remainingTimeInMinutes),
                              true);
}

void EditLocallyJob::fileLockSuccess(const SyncFileItemPtr &item)
{
    qCDebug(lcEditLocallyJob()) << "File lock succeeded, showing notification" << _relPath;

    const auto remainingTimeInMinutes = fileLockTimeRemainingMinutes(item->_lockTime, item->_lockTimeout);
    fileLockProcedureComplete(tr("File %1 now locked.").arg(_fileName),
                              tr("Lock will last for %1 minutes. "
                                 "You can also unlock this file manually once you are finished editing.").arg(remainingTimeInMinutes),
                              true);
}

void EditLocallyJob::fileLockError(const QString &errorMessage)
{
    qCWarning(lcEditLocallyJob()) << "File lock failed, showing notification" << _relPath << errorMessage;
    fileLockProcedureComplete(tr("File %1 could not be locked."), errorMessage, false);
}

void EditLocallyJob::fileLockProcedureComplete(const QString &notificationTitle,
                                               const QString &notificationMessage,
                                               const bool success)
{
    Systray::instance()->showMessage(notificationTitle,
                                     notificationMessage,
                                     success ? QSystemTrayIcon::Information : QSystemTrayIcon::Warning);

    disconnectFolderSignals();
    openFile();
}

int EditLocallyJob::fileLockTimeRemainingMinutes(const qint64 lockTime, const qint64 lockTimeOut)
{
    const auto lockExpirationTime = lockTime + lockTimeOut;
    const auto remainingTime = QDateTime::currentDateTime().secsTo(QDateTime::fromSecsSinceEpoch(lockExpirationTime));

    static constexpr auto SECONDS_PER_MINUTE = 60;
    const auto remainingTimeInMinutes = static_cast<int>(remainingTime > 0 ? remainingTime / SECONDS_PER_MINUTE : 0);

    return remainingTimeInMinutes;
}

bool EditLocallyJob::isFileParentItemValid() const
{
    return (_fileParentItem && !_fileParentItem->isEmpty()) || _relPathParent == QStringLiteral("/");
}

}
