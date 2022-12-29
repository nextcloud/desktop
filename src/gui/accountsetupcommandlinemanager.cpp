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

#include "accountsetupcommandlinemanager.h"

#include "accountmanager.h"
#include "creds/webflowcredentials.h"
#include "filesystem.h"
#include "folder.h"
#include "folderman.h"
#include "networkjobs.h"

#include <QDir>
#include <QGuiApplication>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QObject>
#include <QUrl>

namespace OCC
{
Q_LOGGING_CATEGORY(lcAccountSetupCommandLineManager, "nextcloud.gui.accountsetupcommandlinemanager", QtInfoMsg)

AccountSetupFromCommandLineJob::AccountSetupFromCommandLineJob(QString appPassword,
                                                               QString userId,
                                                               QUrl serverUrl,
                                                               QString localDirPath,
                                                               bool nonVfsMode,
                                                               QString remoteDirPath,
                                                               QObject *parent)
    : QObject(parent)
    , _appPassword(appPassword)
    , _userId(userId)
    , _serverUrl(serverUrl)
    , _localDirPath(localDirPath)
    , _nonVfsMode(nonVfsMode)
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

        qCInfo(lcAccountSetupCommandLineManager) << "Creating folder" << _localDirPath;
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
        FolderDefinition definition;
        definition.localPath = _localDirPath;
        definition.targetPath = FolderDefinition::prepareTargetPath(!_remoteDirPath.isEmpty() ? _remoteDirPath : QStringLiteral("/"));
        definition.virtualFilesMode = _nonVfsMode ? Vfs::Off : bestAvailableVfsMode();

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
            qCInfo(lcAccountSetupCommandLineManager) << QStringLiteral("Folder %1 setup from command line success.").arg(definition.localPath);
            printAccountSetupFromCommandLineStatusAndExit(QStringLiteral("Account %1 setup from command line success.").arg(_account->displayName()), false);
        } else {
            accountManager->deleteAccount(accountState);
            printAccountSetupFromCommandLineStatusAndExit(
                QStringLiteral("Account %1 setup from command line failed, due to folder creation failure.").arg(_account->displayName()),
                false);
        }
    } else {
        qCInfo(lcAccountSetupCommandLineManager) << QStringLiteral("Set up a new account without a folder.");
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
        qCInfo(lcAccountSetupCommandLineManager) << "Authed request was redirected to" << redirectUrl.toString();

        // strip the expected path
        auto path = redirectUrl.path();
        static QString expectedPath = "/" + _account->davPath();
        if (path.endsWith(expectedPath)) {
            path.chop(expectedPath.size());
            redirectUrl.setPath(path);

            qCInfo(lcAccountSetupCommandLineManager) << "Setting account url to" << redirectUrl.toString();
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

void AccountSetupFromCommandLineJob::printAccountSetupFromCommandLineStatusAndExit(const QString &status, bool isFailure)
{
    if (isFailure) {
        qCWarning(lcAccountSetupCommandLineManager) << status;
    } else {
        qCInfo(lcAccountSetupCommandLineManager) << status;
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

bool AccountSetupCommandLineManager::parseCommandlineOption(const QString &option, QStringListIterator &optionsIterator, QString &errorMessage)
{
    if (option == QStringLiteral("--apppassword")) {
        if (optionsIterator.hasNext() && !optionsIterator.peekNext().startsWith(QLatin1String("--"))) {
            _appPassword = optionsIterator.next();
            return true;
        } else {
            errorMessage = QStringLiteral("apppassword not specified");
        }
    } else if (option == QStringLiteral("--localdirpath")) {
        if (optionsIterator.hasNext() && !optionsIterator.peekNext().startsWith(QLatin1String("--"))) {
            _localDirPath = optionsIterator.next();
            return true;
        } else {
            errorMessage = QStringLiteral("basedir not specified");
        }
    } else if (option == QStringLiteral("--remotedirpath")) {
        if (optionsIterator.hasNext() && !optionsIterator.peekNext().startsWith(QLatin1String("--"))) {
            _remoteDirPath = optionsIterator.next();
            return true;
        } else {
            errorMessage = QStringLiteral("remotedir not specified");
        }
    } else if (option == QStringLiteral("--serverurl")) {
        if (optionsIterator.hasNext() && !optionsIterator.peekNext().startsWith(QLatin1String("--"))) {
            _serverUrl = optionsIterator.next();
            return true;
        } else {
            errorMessage = QStringLiteral("serverurl not specified");
        }
    } else if (option == QStringLiteral("--userid")) {
        if (optionsIterator.hasNext() && !optionsIterator.peekNext().startsWith(QLatin1String("--"))) {
            _userId = optionsIterator.next();
            return true;
        } else {
            errorMessage = QStringLiteral("userid not specified");
        }
    } else if (option == QLatin1String("--nonvfsmode")) {
        if (optionsIterator.hasNext() && !optionsIterator.peekNext().startsWith(QLatin1String("--"))) {
            _nonVfsMode = optionsIterator.next().toInt() != 0;
            return true;
        } else {
            errorMessage = QStringLiteral("nonVfsMode not specified");
        }
    } 
    return false;
}

bool AccountSetupCommandLineManager::isCommandLineParsed()
{
    return !_appPassword.isEmpty() && !_userId.isEmpty() && _serverUrl.isValid();
}

void AccountSetupCommandLineManager::setupAccountFromCommandLine(QObject *parent)
{
    if (isCommandLineParsed()) {
        qCInfo(lcAccountSetupCommandLineManager) << QStringLiteral("Command line has been parsed and account setup parameters have been found. Attempting setup a new account %1...").arg(_userId);
        const auto accountSetupJob = new AccountSetupFromCommandLineJob(_appPassword, _userId, _serverUrl, _localDirPath, _nonVfsMode, _remoteDirPath, parent);
        accountSetupJob->handleAccountSetupFromCommandLine();
    } else {
        qCInfo(lcAccountSetupCommandLineManager) << QStringLiteral("No account setup parameters have been found, or they are invalid. Proceed with normal startup...");
    }
    _appPassword.clear();
    _userId.clear();
    _serverUrl.clear();
    _remoteDirPath.clear();
    _localDirPath.clear();
    _nonVfsMode = false;
}

QString AccountSetupCommandLineManager::_appPassword;
QString AccountSetupCommandLineManager::_userId;
QUrl AccountSetupCommandLineManager::_serverUrl;
QString AccountSetupCommandLineManager::_remoteDirPath;
QString AccountSetupCommandLineManager::_localDirPath;
bool AccountSetupCommandLineManager::_nonVfsMode = false;
}
