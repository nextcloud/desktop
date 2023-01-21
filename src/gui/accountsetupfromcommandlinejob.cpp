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

#include "accountsetupfromcommandlinejob.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "creds/webflowcredentials.h"
#include "filesystem.h"
#include "folder.h"
#include "folderman.h"
#include "networkjobs.h"

#include <QDir>
#include <QGuiApplication>
#include <QJsonObject>
#include <QLoggingCategory>

namespace OCC
{
Q_LOGGING_CATEGORY(lcAccountSetupCommandLineJob, "nextcloud.gui.accountsetupcommandlinejob", QtInfoMsg)

AccountSetupFromCommandLineJob::AccountSetupFromCommandLineJob(QString appPassword,
                                                               QString userId,
                                                               QUrl serverUrl,
                                                               QString localDirPath,
                                                               bool isVfsEnabled,
                                                               QString remoteDirPath,
                                                               QObject *parent)
    : QObject(parent)
    , _appPassword(appPassword)
    , _userId(userId)
    , _serverUrl(serverUrl)
    , _localDirPath(localDirPath)
    , _isVfsEnabled(isVfsEnabled)
    , _remoteDirPath(remoteDirPath)
{
}

void AccountSetupFromCommandLineJob::handleAccountSetupFromCommandLine()
{
    if (AccountManager::instance()->accountFromUserId(QStringLiteral("%1@%2").arg(_userId).arg(_serverUrl.host()))) {
        printAccountSetupFromCommandLineStatusAndExit(QStringLiteral("Account %1 already exists!").arg(QDir::toNativeSeparators(_userId)), true);
        return;
    }

    if (!_localDirPath.isEmpty()) {
        QDir dir(_localDirPath);
        if (dir.exists() && !dir.isEmpty()) {
            printAccountSetupFromCommandLineStatusAndExit(
                QStringLiteral("Local folder %1 already exists and is non-empty!").arg(QDir::toNativeSeparators(_localDirPath)),
                true);
            return;
        }

        qCInfo(lcAccountSetupCommandLineJob) << "Creating folder" << _localDirPath;
        if (!dir.exists() && !dir.mkpath(".")) {
            printAccountSetupFromCommandLineStatusAndExit(
                QStringLiteral("Folder creation failed. Could not create local folder %1").arg(QDir::toNativeSeparators(_localDirPath)),
                true);
            return;
        }

        FileSystem::setFolderMinimumPermissions(_localDirPath);
        Utility::setupFavLink(_localDirPath);
    }

    const auto credentials = new WebFlowCredentials(_userId, _appPassword);
    _account = AccountManager::createAccount();
    _account->setCredentials(credentials);
    _account->setUrl(_serverUrl);

    fetchUserName();
}

void AccountSetupFromCommandLineJob::checkLastModifiedWithPropfind()
{
    const auto job = new PropfindJob(_account, "/", this);
    job->setIgnoreCredentialFailure(true);
    // There is custom redirect handling in the error handler,
    // so don't automatically follow redirects.
    job->setFollowRedirects(false);
    job->setProperties(QList<QByteArray>() << QByteArrayLiteral("getlastmodified"));
    connect(job, &PropfindJob::result, this, &AccountSetupFromCommandLineJob::accountSetupFromCommandLinePropfindHandleSuccess);
    connect(job, &PropfindJob::finishedWithError, this, &AccountSetupFromCommandLineJob::accountSetupFromCommandLinePropfindHandleFailure);
    job->start();
}

void AccountSetupFromCommandLineJob::accountSetupFromCommandLinePropfindHandleSuccess()
{
    const auto accountManager = AccountManager::instance();
    const auto accountState = accountManager->addAccount(_account);
    accountManager->save();

    if (!_localDirPath.isEmpty()) {
        setupLocalSyncFolder(accountState);
    } else {
        qCInfo(lcAccountSetupCommandLineJob) << QStringLiteral("Set up a new account without a folder.");
        printAccountSetupFromCommandLineStatusAndExit(QStringLiteral("Account %1 setup from command line success.").arg(_account->displayName()), false);
    }
}

void AccountSetupFromCommandLineJob::accountSetupFromCommandLinePropfindHandleFailure()
{
    const auto job = qobject_cast<PropfindJob *>(sender());
    if (!job) {
        printAccountSetupFromCommandLineStatusAndExit(QStringLiteral("Cannot check for authed redirects. This slot should be invoked from PropfindJob!"), true);
        return;
    }
    const auto reply = job->reply();

    QString errorMsg;

    // If there were redirects on the *authed* requests, also store
    // the updated server URL, similar to redirects on status.php.
    QUrl redirectUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (!redirectUrl.isEmpty()) {
        qCInfo(lcAccountSetupCommandLineJob) << "Authed request was redirected to" << redirectUrl.toString();

        // strip the expected path
        auto path = redirectUrl.path();
        static QString expectedPath = "/" + _account->davPath();
        if (path.endsWith(expectedPath)) {
            path.chop(expectedPath.size());
            redirectUrl.setPath(path);

            qCInfo(lcAccountSetupCommandLineJob) << "Setting account url to" << redirectUrl.toString();
            _account->setUrl(redirectUrl);
            checkLastModifiedWithPropfind();
        }
        errorMsg = tr("The authenticated request to the server was redirected to "
                      "\"%1\". The URL is bad, the server is misconfigured.")
                       .arg(Utility::escape(redirectUrl.toString()));

        // A 404 is actually a success: we were authorized to know that the folder does
        // not exist. It will be created later...
    } else if (reply->error() == QNetworkReply::ContentNotFoundError) {
        accountSetupFromCommandLinePropfindHandleSuccess();
    } else if (reply->error() != QNetworkReply::NoError) {
        if (!_account->credentials()->stillValid(reply)) {
            errorMsg = tr("Access forbidden by server. To verify that you have proper access, "
                          "<a href=\"%1\">click here</a> to access the service with your browser.")
                           .arg(Utility::escape(_account->url().toString()));
        } else {
            errorMsg = job->errorStringParsingBody();
        }
        // Something else went wrong, maybe the response was 200 but with invalid data.
    } else {
        errorMsg = tr("There was an invalid response to an authenticated WebDAV request");
    }
    printAccountSetupFromCommandLineStatusAndExit(
        QStringLiteral("Account %1 setup from command line failed with error: %2.").arg(_account->displayName()).arg(errorMsg),
        true);
}

void AccountSetupFromCommandLineJob::setupLocalSyncFolder(AccountState *accountState)
{
    FolderDefinition definition;
    definition.localPath = _localDirPath;
    definition.targetPath = FolderDefinition::prepareTargetPath(!_remoteDirPath.isEmpty() ? _remoteDirPath : QStringLiteral("/"));
    definition.virtualFilesMode = _isVfsEnabled ? bestAvailableVfsMode() : Vfs::Off;

    const auto folderMan = FolderMan::instance();

    definition.ignoreHiddenFiles = folderMan->ignoreHiddenFiles();
    definition.alias = folderMan->map().size() > 0 ? QString::number(folderMan->map().size()) : QString::number(0);

    if (folderMan->navigationPaneHelper().showInExplorerNavigationPane()) {
        definition.navigationPaneClsid = QUuid::createUuid();
    }

    folderMan->setSyncEnabled(false);

    if (const auto folder = folderMan->addFolder(accountState, definition)) {
        if (definition.virtualFilesMode != Vfs::Off) {
            folder->setRootPinState(PinState::OnlineOnly);
        }
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, QStringList() << QLatin1String("/"));
        qCInfo(lcAccountSetupCommandLineJob) << QStringLiteral("Folder %1 setup from command line success.").arg(definition.localPath);
        printAccountSetupFromCommandLineStatusAndExit(QStringLiteral("Account %1 setup from command line success.").arg(_account->displayName()), false);
    } else {
        AccountManager::instance()->deleteAccount(accountState);
        printAccountSetupFromCommandLineStatusAndExit(
            QStringLiteral("Account %1 setup from command line failed, due to folder creation failure.").arg(_account->displayName()),
            false);
    }
}

void AccountSetupFromCommandLineJob::printAccountSetupFromCommandLineStatusAndExit(const QString &status, bool isFailure)
{
    if (isFailure) {
        qCWarning(lcAccountSetupCommandLineJob) << status;
    } else {
        qCInfo(lcAccountSetupCommandLineJob) << status;
    }
    QTimer::singleShot(0, this, [this, isFailure]() {
        this->deleteLater();
        if (!isFailure) {
            qApp->quit();
        } else {
            qApp->exit(1);
        }
    });
}

void AccountSetupFromCommandLineJob::fetchUserName()
{
    const auto fetchUserNameJob = new JsonApiJob(_account, QStringLiteral("/ocs/v1.php/cloud/user"));
    connect(fetchUserNameJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        sender()->deleteLater();

        if (statusCode != 100) {
            printAccountSetupFromCommandLineStatusAndExit("Could not fetch username.", true);
            return;
        }

        const auto objData = json.object().value("ocs").toObject().value("data").toObject();
        const auto userId = objData.value("id").toString("");
        const auto displayName = objData.value("display-name").toString("");
        _account->setDavUser(userId);
        _account->setDavDisplayName(displayName);

        checkLastModifiedWithPropfind();
    });
    fetchUserNameJob->start();
}
}
