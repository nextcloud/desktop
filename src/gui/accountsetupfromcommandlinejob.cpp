/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "accountsetupfromcommandlinejob.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "common/filesystembase.h"
#include "configfile.h"
#include "creds/abstractcredentials.h"
#include "creds/webflowcredentials.h"
#include "folder.h"
#include "folderman.h"
#include "networkjobs.h"
#include "theme.h"

#include <chrono>
#include <iostream>

#include <QDir>
#include <QGuiApplication>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace OCC
{
Q_LOGGING_CATEGORY(lcAccountSetupCommandLineJob, "nextcloud.gui.accountsetupcommandlinejob", QtInfoMsg)

// How long to wait for the keychain to confirm that the credentials were written before
// finishing the setup without them.
constexpr auto credentialsPersistTimeout = std::chrono::seconds(30);

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

bool AccountSetupFromCommandLineJob::localSyncFolderRequired() const
{
#ifdef BUILD_FILE_PROVIDER_MODULE
    // Mirrors the guard in FolderMan::addFolder(): while the app-level File Provider mode
    // is enabled, a classic sync folder cannot be created at all. The account is synced
    // through the File Provider domain that is set up for it instead.
    return !ConfigFile().macFileProviderModeEnabled();
#else
    return true;
#endif
}

QString AccountSetupFromCommandLineJob::defaultLocalDirPath() const
{
    const auto overrideLocalDir = ConfigFile().overrideLocalDir();
    auto localDirPath = overrideLocalDir;

    if (localDirPath.isEmpty()) {
        localDirPath = Theme::instance()->defaultClientFolder();

        if (localDirPath.isEmpty()) {
            return {};
        }

        if (!QDir(localDirPath).isAbsolute()) {
            localDirPath = QDir::homePath() + QLatin1Char('/') + localDirPath;
        }
    }

    auto serverUrlForFolder = _serverUrl;
    serverUrlForFolder.setUserName(_userId);

    return FolderMan::instance()->findGoodPathForNewSyncFolder(localDirPath,
                                                               serverUrlForFolder,
                                                               overrideLocalDir.isEmpty() ? FolderMan::GoodPathStrategy::AllowOnlyNewPath
                                                                                          : FolderMan::GoodPathStrategy::AllowOverrideExistingPath);
}

bool AccountSetupFromCommandLineJob::handleAccountSetupFromCommandLine()
{
    if (AccountManager::instance()->accountFromUserId(QStringLiteral("%1@%2").arg(_userId, _serverUrl.host()))) {
        printAccountSetupFromCommandLineStatusAndExit(QStringLiteral("Account %1 already exists!").arg(QDir::toNativeSeparators(_userId)), true);
        return false;
    }

    if (!localSyncFolderRequired()) {
        if (!_localDirPath.isEmpty()) {
            qCInfo(lcAccountSetupCommandLineJob) << "Ignoring the given local folder, File Provider mode is enabled";
            _localDirPath.clear();
        }
    } else {
        if (_localDirPath.isEmpty()) {
            // The local folder is documented as optional, so fall back to the folder the
            // account wizard would suggest rather than refusing to set the account up.
            _localDirPath = defaultLocalDirPath();
        }

        if (_localDirPath.isEmpty()) {
            printAccountSetupFromCommandLineStatusAndExit(
                QStringLiteral("Could not determine a local folder to sync into. Please pass one with --localdirpath."),
                true);
            return false;
        }

        const QDir localDir(_localDirPath);
        if (localDir.exists() && !localDir.isEmpty()) {
            printAccountSetupFromCommandLineStatusAndExit(
                QStringLiteral("Local folder %1 already exists and is non-empty!").arg(QDir::toNativeSeparators(_localDirPath)),
                true);
            return false;
        }
    }

    const auto credentials = new WebFlowCredentials(_userId, _appPassword);
    _account = AccountManager::createAccount();

    _account->setCredentials(credentials);
    _account->setCredentialSetting(u"user"_s, _userId);
    _account->setUrl(_serverUrl);

    // The account is only added, saved and given a sync folder once the credentials have
    // been checked against the server, so that a failed setup leaves nothing behind. Both
    // that check and the keychain write are asynchronous: the job keeps running until
    // printAccountSetupFromCommandLineStatusAndExit() ends the event loop.
    if (_appPassword.isEmpty()) {
        // Nothing to authenticate with, so the server cannot be asked for the dav user
        // either. Store what was given and let the user log in from the client later on.
        _account->setDavUser(_userId);
        accountSetupFromCommandLinePropfindHandleSuccess();
        return true;
    }

    fetchUserName();
    return true;
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

    const auto finishAccountSetup = [this, accountState]() {
        if (!_localDirPath.isEmpty()) {
            setupLocalSyncFolder(accountState);
        } else {
            qCInfo(lcAccountSetupCommandLineJob) << QStringLiteral("Set up a new account without a folder.");
            printAccountSetupFromCommandLineStatusAndExit(QStringLiteral("Account %1 setup from command line success.").arg(_account->displayName()), false);
        }
    };

    // credentials->persist() (called by save()) is asynchronous — it chains
    // multiple keychain write jobs before the password actually lands in the
    // keychain.  Wait for the final write to complete before exiting so that
    // the credentials are not lost when the process quits, but give up eventually:
    // a keychain that never answers must not turn the setup into a hang.
    const auto credentialsPersistTimer = new QTimer(this);
    credentialsPersistTimer->setSingleShot(true);

    connect(_account->credentials(), &AbstractCredentials::credentialsPersisted, this, [credentialsPersistTimer, finishAccountSetup]() {
        if (!credentialsPersistTimer->isActive()) {
            return;
        }

        credentialsPersistTimer->stop();
        finishAccountSetup();
    });

    connect(credentialsPersistTimer, &QTimer::timeout, this, [finishAccountSetup]() {
        qCWarning(lcAccountSetupCommandLineJob) << "Timed out waiting for the credentials to be written to the keychain,"
                                                << "the account may have to be authenticated again";
        finishAccountSetup();
    });

    credentialsPersistTimer->start(credentialsPersistTimeout);

    accountManager->save();
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
    QDir localDir(_localDirPath);
    if (!localDir.exists()) {
        qCInfo(lcAccountSetupCommandLineJob) << "Creating folder" << _localDirPath;

        if (!localDir.mkpath(QStringLiteral("."))) {
            AccountManager::instance()->removeAccountState(accountState);
            printAccountSetupFromCommandLineStatusAndExit(
                QStringLiteral("Folder creation failed. Could not create local folder %1").arg(QDir::toNativeSeparators(_localDirPath)),
                true);
            return;
        }
    }

    FileSystem::setFolderMinimumPermissions(_localDirPath);
    Utility::setupFavLink(_localDirPath);

    FolderDefinition definition;
    definition.localPath = _localDirPath;
    definition.targetPath = FolderDefinition::prepareTargetPath(!_remoteDirPath.isEmpty() ? _remoteDirPath : QStringLiteral("/"));
    definition.virtualFilesMode = _isVfsEnabled ? bestAvailableVfsMode() : Vfs::Off;

    const auto folderMan = FolderMan::instance();

    definition.ignoreHiddenFiles = folderMan->ignoreHiddenFiles();
    definition.alias = folderMan->map().size() > 0 ? QString::number(folderMan->map().size()) : QString::number(0);

#ifdef Q_OS_WIN
    if (folderMan->navigationPaneHelper().showInExplorerNavigationPane()) {
        definition.navigationPaneClsid = QUuid::createUuid();
    }
#endif

    folderMan->setSyncEnabled(false);

    if (const auto folder = folderMan->addFolder(accountState, definition)) {
        if (definition.virtualFilesMode != Vfs::Off) {
            folder->setRootPinState(PinState::OnlineOnly);
        }
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, QStringList() << QLatin1String("/"));
        qCInfo(lcAccountSetupCommandLineJob) << QStringLiteral("Folder %1 setup from command line success.").arg(definition.localPath);
        printAccountSetupFromCommandLineStatusAndExit(QStringLiteral("Account %1 setup from command line success.").arg(_account->displayName()), false);
    } else {
        // Drop the account again, but keep the app password valid: it was passed in on the
        // command line and revoking it would make a retry impossible.
        AccountManager::instance()->removeAccountState(accountState);
        printAccountSetupFromCommandLineStatusAndExit(
            QStringLiteral("Account %1 setup from command line failed, due to folder creation failure.").arg(_account->displayName()),
            true);
    }
}

void AccountSetupFromCommandLineJob::printAccountSetupFromCommandLineStatusAndExit(const QString &status, bool isFailure)
{
    if (isFailure) {
        qCWarning(lcAccountSetupCommandLineJob) << status;
        std::cerr << qUtf8Printable(status) << std::endl;
    } else {
        qCInfo(lcAccountSetupCommandLineJob) << status;
        std::cout << qUtf8Printable(status) << std::endl;
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
