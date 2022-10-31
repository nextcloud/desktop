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
        return;
    }

    const auto encodedToken = QString::fromUtf8(QUrl::toPercentEncoding(_token)); // Sanitise the token
    const auto encodedRelPath = QUrl::toPercentEncoding(_relPath); // Sanitise the relPath

    _checkTokenJob.reset(new SimpleApiJob(_accountState->account(),
                                          QStringLiteral("/ocs/v2.php/apps/files/api/v1/openlocaleditor/%1").arg(encodedToken)));

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("path"), prefixSlashToPath(encodedRelPath));
    _checkTokenJob->addQueryParams(params);
    _checkTokenJob->setVerb(SimpleApiJob::Verb::Post);
    connect(_checkTokenJob.get(), &SimpleApiJob::resultReceived, this, &EditLocallyJob::remoteTokenCheckResultReceived);

    _checkTokenJob->start();
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

    proceedWithSetup();
}

void EditLocallyJob::proceedWithSetup()
{
    if (!_tokenVerified) {
        qCWarning(lcEditLocallyJob) << "Could not proceed with setup as token is not verified.";
        return;
    }

    const auto foundFiles = FolderMan::instance()->findFileInLocalFolders(_relPath, _accountState->account());

    if (foundFiles.isEmpty()) {
        if (isRelPathExcluded(_relPath)) {
            showError(tr("Could not find a file for local editing. Make sure it is not excluded via selective sync."), _relPath);
        } else {
            showError(tr("Could not find a file for local editing. Make sure its path is valid and it is synced locally."), _relPath);
        }
        return;
    }

    _localFilePath = foundFiles.first();
    _folderForFile = FolderMan::instance()->folderForPath(_localFilePath);

    if (!_folderForFile) {
        showError(tr("Could not find a folder to sync."), _relPath);
        return;
    }

    const auto relPathSplit = _relPath.split(QLatin1Char('/'));
    if (relPathSplit.isEmpty()) {
        showError(tr("Could not find a file for local editing. Make sure its path is valid and it is synced locally."), _relPath);
        return;
    }

    _fileName = relPathSplit.last();

    Systray::instance()->destroyEditFileLocallyLoadingDialog();
    Q_EMIT setupFinished();
}

QString EditLocallyJob::prefixSlashToPath(const QString &path)
{
    return path.startsWith('/') ? path : QChar::fromLatin1('/') + path;
}

bool EditLocallyJob::isTokenValid(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    // Token is an alphanumeric string 128 chars long.
    // Ensure that is what we received and what we are sending to the server.
    const QRegularExpression tokenRegex("^[a-zA-Z0-9]{128}$");
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

bool EditLocallyJob::isRelPathExcluded(const QString &relPath)
{
    if (relPath.isEmpty()) {
        return false;
    }

    const auto folderMap = FolderMan::instance()->map();
    for (const auto &folder : folderMap) {
        bool result = false;
        const auto excludedThroughSelectiveSync = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &result);
        for (const auto &excludedPath : excludedThroughSelectiveSync) {
            if (relPath.startsWith(excludedPath)) {
                return true;
            }
        }
    }

    return false;
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
        (*foundFolder)->syncEngine().addErrorToGui(SyncFileItem::SoftError, message, informativeText);
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
        return;
    }

    Systray::instance()->createEditFileLocallyLoadingDialog(_fileName);

    _folderForFile->startSync();
    const auto syncFinishedConnection = connect(_folderForFile, &Folder::syncFinished,
                                                this, &EditLocallyJob::folderSyncFinished);

    EditLocallyManager::instance()->folderSyncFinishedConnections.insert(_localFilePath,
                                                                         syncFinishedConnection);
}

void EditLocallyJob::folderSyncFinished(const OCC::SyncResult &result)
{
    Q_UNUSED(result)
    disconnectSyncFinished();
    openFile();
}

void EditLocallyJob::disconnectSyncFinished() const
{
    if(_localFilePath.isEmpty()) {
        return;
    }

    const auto manager = EditLocallyManager::instance();

    if (const auto existingConnection = manager->folderSyncFinishedConnections.value(_localFilePath)) {
        disconnect(existingConnection);
        manager->folderSyncFinishedConnections.remove(_localFilePath);
    }
}

void EditLocallyJob::openFile()
{
    if(_localFilePath.isEmpty()) {
        qCWarning(lcEditLocallyJob) << "Could not edit locally. Invalid local file path.";
        return;
    }

    const auto localFilePath = _localFilePath;
    // In case the VFS mode is enabled and a file is not yet hydrated, we must call QDesktopServices::openUrl
    // from a separate thread, or, there will be a freeze. To avoid searching for a specific folder and checking
    // if the VFS is enabled - we just always call it from a separate thread.
    QtConcurrent::run([localFilePath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(localFilePath));
        Systray::instance()->destroyEditFileLocallyLoadingDialog();
    });

    Q_EMIT fileOpened();
}

}
