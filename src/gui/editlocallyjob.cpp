/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

EditLocallyJob::EditLocallyJob(const QString &userId,
                                       const QString &relPath,
                                       const QString &token,
                                       QObject *parent)
    : QObject{parent}
    , _userId(userId)
    , _relPath(relPath)
    , _token(token)
{
    connect(this, &EditLocallyJob::callShowError, this, &EditLocallyJob::showError, Qt::QueuedConnection);
}

void EditLocallyJob::startSetup()
{
    if (_token.isEmpty() || _relPath.isEmpty() || _userId.isEmpty()) {
        qCWarning(lcEditLocallyJob) << "Could not start setup."
                                        << "token:" << _token
                                        << "relPath:" << _relPath
                                        << "userId" << _userId;
        return;
    }

    // Show the loading dialog but don't show the filename until we have
    // verified the token
    Systray::instance()->createEditFileLocallyLoadingDialog({});

    // We check the input data locally first, without modifying any state or
    // showing any potentially misleading data to the user
    if (!isTokenValid(_token)) {
        qCWarning(lcEditLocallyJob) << "Edit locally request is missing a valid token, will not open file. "
                                        << "Token received was:" << _token;
        showError(tr("Invalid token received."), tr("Please try again."));
        return;
    }

    if (!isRelPathValid(_relPath)) {
        qCWarning(lcEditLocallyJob) << "Provided relPath was:" << _relPath << "which is not canonical.";
        showError(tr("Invalid file path was provided."), tr("Please try again."));
        return;
    }

    _accountState = AccountManager::instance()->accountFromUserId(_userId);

    if (!_accountState) {
        qCWarning(lcEditLocallyJob) << "Could not find an account " << _userId << " to edit file " << _relPath << " locally.";
        showError(tr("Could not find an account for local editing."), tr("Please try again."));
        return;
    }

    // We now ask the server to verify the token, before we again modify any
    // state or look at local files
    startTokenRemoteCheck();
}

void EditLocallyJob::startTokenRemoteCheck()
{
    if (!_accountState || _relPath.isEmpty() || _token.isEmpty()) {
        qCWarning(lcEditLocallyJob) << "Could not start token check."
                                        << "accountState:" << _accountState
                                        << "relPath:" << _relPath
                                        << "token:" << _token;

        showError(tr("Could not start editing locally."),
                  tr("An error occurred trying to verify the request to edit locally."));
        return;
    }

    const auto encodedToken = QString::fromUtf8(QUrl::toPercentEncoding(_token)); // Sanitise the token
    const auto encodedRelPath = QUrl::toPercentEncoding(_relPath); // Sanitise the relPath

    const auto checkTokenJob = new SimpleApiJob(_accountState->account(),
                                          QStringLiteral("/ocs/v2.php/apps/files/api/v1/openlocaleditor/%1").arg(encodedToken));

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("path"), prefixSlashToPath(encodedRelPath));
    checkTokenJob->addQueryParams(params);
    checkTokenJob->setVerb(SimpleApiJob::Verb::Post);
    connect(checkTokenJob, &SimpleApiJob::resultReceived, this, &EditLocallyJob::remoteTokenCheckResultReceived);

    checkTokenJob->start();
}

void EditLocallyJob::remoteTokenCheckResultReceived(const int statusCode)
{
    qCInfo(lcEditLocallyJob) << "token check result" << statusCode;

    constexpr auto HTTP_OK_CODE = 200;
    _tokenVerified = statusCode == HTTP_OK_CODE;

    if (!_tokenVerified) {
        showError(tr("Could not validate the request to open a file from server."), tr("Please try again."));
        return;
    }

    findAfolderAndConstructPaths();
}

void EditLocallyJob::proceedWithSetup()
{
    if (!_tokenVerified) {
        qCWarning(lcEditLocallyJob) << "Could not proceed with setup as token is not verified.";
        showError(tr("Could not validate the request to open a file from server."), tr("Please try again."));
        return;
    }

    const auto relPathSplit = _relPath.split(QLatin1Char('/'));
    if (relPathSplit.isEmpty()) {
        showError(tr("Could not find a file for local editing. Make sure its path is valid and it is synced locally."), _relPath);
        return;
    }

    _fileName = relPathSplit.last();
    _folderForFile = findFolderForFile(_relPath, _userId);

    if (!_folderForFile) {
        showError(tr("Could not find a file for local editing. Make sure it is not excluded via selective sync."), _relPath);
        return;
    }

    if (!isFileParentItemValid()) {
        showError(tr("Could not find a file for local editing. Make sure its path is valid and it is synced locally."), _relPath);
        return;
    }

    _localFilePath = _folderForFile->path() + _relativePathToRemoteRoot;

    Systray::instance()->destroyEditFileLocallyLoadingDialog();
    startEditLocally();
}

void EditLocallyJob::findAfolderAndConstructPaths()
{
    _folderForFile = findFolderForFile(_relPath, _userId);

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

bool EditLocallyJob::isTokenValid(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    // Token is an alphanumeric string 128 chars long.
    // Ensure that is what we received and what we are sending to the server.
    static const QRegularExpression tokenRegex("^[a-zA-Z0-9]{128}$");
    const auto regexMatch = tokenRegex.match(token);

    return regexMatch.hasMatch();
}

bool EditLocallyJob::isRelPathValid(const QString &relPath)
{
    if (relPath.isEmpty()) {
        return false;
    }

    // We want to check that the path is canonical and not relative
    // (i.e. that it doesn't contain ../../) but we always receive
    // a relative path, so let's make it absolute by prepending a
    // slash
    const auto slashPrefixedPath = prefixSlashToPath(relPath);

    // Let's check that the filepath is canonical, and that the request
    // contains no funny behaviour regarding paths
    const auto cleanedPath = QDir::cleanPath(slashPrefixedPath);

    if (cleanedPath != slashPrefixedPath) {
        return false;
    }

    return true;
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
        const auto foundIt = std::find_if(std::begin(folderMap), std::end(folderMap), [&userId](const OCC::Folder *folder) {
            return folder->remotePath() == QStringLiteral("/") && folder->accountState()->account()->userIdAtHostWithPort() == userId;
        });

        return foundIt != std::end(folderMap) ? foundIt.value() : nullptr;
    }

    const auto relPathWithSlash = relPath.startsWith(QStringLiteral("/")) ? relPath : QStringLiteral("/") + relPath;

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
    showErrorNotification(message, informativeText);
    // to make sure the error is not missed, show a message box in addition
    showErrorMessageBox(message, informativeText);
    Q_EMIT error(message, informativeText);
}

void EditLocallyJob::showErrorNotification(const QString &message, const QString &informativeText) const
{
    if (!_accountState || !_accountState->account()) {
        return;
    }

    const auto folderMap = FolderMan::instance()->map();
    const auto foundFolder = std::find_if(folderMap.cbegin(), folderMap.cend(), [this](const auto &folder) {
        return _accountState->account()->davUrl() == folder->remoteUrl();
    });

    if (foundFolder != folderMap.cend()) {
        emit (*foundFolder)->syncEngine().addErrorToGui(SyncFileItem::SoftError, message, informativeText, OCC::ErrorCategory::GenericError);
    }
}

void EditLocallyJob::showErrorMessageBox(const QString &message, const QString &informativeText) const
{
    const auto messageBox = new QMessageBox;
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    messageBox->setText(message);
    messageBox->setInformativeText(informativeText);
    messageBox->setIcon(QMessageBox::Warning);
    messageBox->addButton(QMessageBox::StandardButton::Ok);
    messageBox->show();
    messageBox->activateWindow();
    messageBox->raise();
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

    Systray::instance()->createEditFileLocallyLoadingDialog(_fileName);

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
        const auto nameWithoutDavPath = name.mid(_accountState->account()->davPath().size());

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
        lockFile();
    }
}

void EditLocallyJob::lockFile()
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
                                                                _folderForFile->journalDb(),
                                                                SyncFileItem::LockStatus::LockedItem,
                                                                SyncFileItem::LockOwnerType::TokenLock);
}

void EditLocallyJob::disconnectFolderSignals()
{
    for (const auto &connection : qAsConst(_folderConnections)) {
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
