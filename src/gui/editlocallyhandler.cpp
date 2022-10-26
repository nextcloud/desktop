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

#include "editlocallyhandler.h"

#include <QMessageBox>
#include <QDesktopServices>
#include <QtConcurrent>

#include "accountmanager.h"
#include "folder.h"
#include "folderman.h"
#include "syncengine.h"
#include "systray.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcEditLocallyHandler, "nextcloud.gui.editlocallyhandler", QtInfoMsg)

static QHash<QString, QMetaObject::Connection> editLocallySyncFinishedConnections;

EditLocallyHandler::EditLocallyHandler(const QString &userId,
                                       const QString &relPath,
                                       const QString &token,
                                       QObject *parent)
    : QObject{parent}
    , _relPath(relPath)
    , _token(token)
{
    _accountState = AccountManager::instance()->accountFromUserId(userId);

    if (!_accountState) {
        qCWarning(lcEditLocallyHandler) << "Could not find an account " << userId << " to edit file " << relPath << " locally.";
        showError(tr("Could not find an account for local editing"), userId);
        return;
    }

    if (!isTokenValid(token)) {
        qCWarning(lcEditLocallyHandler) << "Edit locally request is missing a valid token, will not open file. Token received was:" << token;
        showError(tr("Invalid token received."), tr("Please try again."));
        return;
    }

    if (!isRelPathValid(relPath)) {
        qCWarning(lcEditLocallyHandler) << "Provided relPath was:" << relPath << "which is not canonical.";
        showError(tr("Invalid file path was provided."), tr("Please try again."));
        return;
    }

    const auto foundFiles = FolderMan::instance()->findFileInLocalFolders(relPath, _accountState->account());

    if (foundFiles.isEmpty()) {
        if (isRelPathExcluded(relPath)) {
            showError(tr("Could not find a file for local editing. Make sure it is not excluded via selective sync."), relPath);
        } else {
            showError(tr("Could not find a file for local editing. Make sure its path is valid and it is synced locally."), relPath);
        }
        return;
    }

    _localFilePath = foundFiles.first();
    _folderForFile = FolderMan::instance()->folderForPath(_localFilePath);

    if (!_folderForFile) {
        showError(tr("Could not find a folder to sync."), relPath);
        return;
    }

    const auto relPathSplit = relPath.split(QLatin1Char('/'));
    if (relPathSplit.size() == 0) {
        showError(tr("Could not find a file for local editing. Make sure its path is valid and it is synced locally."), relPath);
        return;
    }

    _fileName = relPathSplit.last();

    _ready = true;
}

bool EditLocallyHandler::ready() const
{
    return _ready;
}

QString EditLocallyHandler::prefixSlashToPath(const QString &path)
{
    auto slashPrefixedPath = path;
    if (!slashPrefixedPath.startsWith('/')) {
        slashPrefixedPath.prepend('/');
    }

    return slashPrefixedPath;
}

bool EditLocallyHandler::isTokenValid(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    // Token is an alphanumeric string 128 chars long.
    // Ensure that is what we received and what we are sending to the server.
    const QRegularExpression tokenRegex("^[a-zA-Z0-9]{128}$");
    const auto regexMatch = tokenRegex.match(token);

    // Means invalid token type received, be cautious with bad token
    if(!regexMatch.hasMatch()) {
        return false;
    }

    return true;
}

bool EditLocallyHandler::isRelPathValid(const QString &relPath)
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

bool EditLocallyHandler::isRelPathExcluded(const QString &relPath)
{
    if (relPath.isEmpty()) {
        return true;
    }

    const auto folderMap = FolderMan::instance()->map();
    for (const auto &folder : folderMap) {
        bool result = false;
        const auto excludedThroughSelectiveSync = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &result);
        for (const auto &excludedPath : excludedThroughSelectiveSync) {
            if (relPath.startsWith(excludedPath)) {
                return false;
            }
        }
    }

    return true;
}

void EditLocallyHandler::showError(const QString &message, const QString &informativeText) const
{
    showErrorNotification(message, informativeText);
    // to make sure the error is not missed, show a message box in addition
    showErrorMessageBox(message, informativeText);
}

void EditLocallyHandler::showErrorNotification(const QString &message, const QString &informativeText) const
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

void EditLocallyHandler::showErrorMessageBox(const QString &message, const QString &informativeText) const
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

void EditLocallyHandler::startEditLocally()
{
    if (!_ready) {
        return;
    }

    Systray::instance()->createEditFileLocallyLoadingDialog(_fileName);

    const auto encodedToken = QString::fromUtf8(QUrl::toPercentEncoding(_token)); // Sanitise the token
    const auto encodedRelPath = QUrl::toPercentEncoding(_relPath); // Sanitise the relPath
    const auto checkEditLocallyToken = new SimpleApiJob(_accountState->account(), QStringLiteral("/ocs/v2.php/apps/files/api/v1/openlocaleditor/%1").arg(encodedToken));

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("path"), prefixSlashToPath(_relPath));
    checkEditLocallyToken->addQueryParams(params);
    checkEditLocallyToken->setVerb(SimpleApiJob::Verb::Post);
    connect(checkEditLocallyToken, &SimpleApiJob::resultReceived, this, &EditLocallyHandler::remoteTokenCheckFinished);

    checkEditLocallyToken->start();
}

void EditLocallyHandler::remoteTokenCheckFinished(const int statusCode)
{
    constexpr auto HTTP_OK_CODE = 200;
    if (statusCode != HTTP_OK_CODE) {
        Systray::instance()->destroyEditFileLocallyLoadingDialog();

        showError(tr("Could not validate the request to open a file from server."), _relPath);
        qCInfo(lcEditLocallyHandler) << "token check result" << statusCode;
        return;
    }

    _folderForFile->startSync();
    const auto syncFinishedConnection = connect(_folderForFile, &Folder::syncFinished,
                                                this, &EditLocallyHandler::folderSyncFinished);
    editLocallySyncFinishedConnections.insert(_localFilePath, syncFinishedConnection);
}

void EditLocallyHandler::folderSyncFinished(const OCC::SyncResult &result)
{
    Q_UNUSED(result)
    disconnectSyncFinished();
    openFile();
}

void EditLocallyHandler::disconnectSyncFinished() const
{
    if (const auto existingConnection = editLocallySyncFinishedConnections.value(_localFilePath)) {
        disconnect(existingConnection);
        editLocallySyncFinishedConnections.remove(_localFilePath);
    }
}

void EditLocallyHandler::openFile()
{
    const auto localFilePath = _localFilePath;
    // In case the VFS mode is enabled and a file is not yet hydrated, we must call QDesktopServices::openUrl
    // from a separate thread, or, there will be a freeze. To avoid searching for a specific folder and checking
    // if the VFS is enabled - we just always call it from a separate thread.
    QtConcurrent::run([localFilePath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(localFilePath));
        Systray::instance()->destroyEditFileLocallyLoadingDialog();
    });

    Q_EMIT finished();
}

}
