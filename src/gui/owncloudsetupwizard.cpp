/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include <QAbstractButton>
#include <QtCore>
#include <QProcess>
#include <QMessageBox>
#include <QDesktopServices>
#include <QApplication>

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"
#include "owncloudsetupwizard.h"
#include "configfile.h"
#include "folderman.h"
#include "accessmanager.h"
#include "account.h"
#include "networkjobs.h"
#include "sslerrordialog.h"
#include "accountmanager.h"
#include "clientproxy.h"
#include "filesystem.h"
#include "owncloudgui.h"
#include "settingsdialog.h"

#include "creds/dummycredentials.h"

namespace OCC {

OwncloudSetupWizard::OwncloudSetupWizard(QWidget *parent)
    : QObject(parent)
    , _ocWizard(new OwncloudWizard(parent))
{
    connect(_ocWizard, &OwncloudWizard::determineAuthType,
        this, &OwncloudSetupWizard::slotCheckServer);
    connect(_ocWizard, &OwncloudWizard::connectToOCUrl,
        this, &OwncloudSetupWizard::slotConnectToOCUrl);
    connect(_ocWizard, &OwncloudWizard::createLocalAndRemoteFolders,
        this, &OwncloudSetupWizard::slotCreateLocalAndRemoteFolders);
    /* basicSetupFinished might be called from a reply from the network.
       slotAssistantFinished might destroy the temporary QNetworkAccessManager.
       Therefore Qt::QueuedConnection is required */
    connect(_ocWizard, &OwncloudWizard::basicSetupFinished,
        this, &OwncloudSetupWizard::slotAssistantFinished, Qt::QueuedConnection);
    connect(_ocWizard, &OwncloudWizard::finished, this, &QObject::deleteLater);
}

OwncloudSetupWizard::~OwncloudSetupWizard()
{
    _ocWizard->deleteLater();
}

void OwncloudSetupWizard::startWizard()
{
    AccountPtr account = AccountManager::createAccount();
    account->setCredentials(new DummyCredentials);
    account->setUrl(Theme::instance()->overrideServerUrlV2());
    _ocWizard->setAccount(account);
    _ocWizard->setOCUrl(account->url().toString());

    _ocWizard->setStartId(WizardCommon::Page_ServerSetup);

    _ocWizard->restart();

    _ocWizard->open();
    ownCloudGui::raiseDialog(_ocWizard);
}

// also checks if an installation is valid and determines auth type in a second step
void OwncloudSetupWizard::slotCheckServer(const QString &urlString)
{
    QString fixedUrl = urlString;
    QUrl url = QUrl::fromUserInput(fixedUrl);
    // fromUserInput defaults to http, not http if no scheme is specified
    if (!fixedUrl.startsWith("http://") && !fixedUrl.startsWith("https://")) {
        url.setScheme("https");
    }
    AccountPtr account = _ocWizard->account();
    account->setUrl(url);

    // Reset the proxy which might had been determined previously in ConnectionValidator::checkServerAndAuth()
    // when there was a previous account.
    account->networkAccessManager()->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));

    // And also reset the QSslConfiguration, for the same reason (#6832)
    // Here the client certificate is added, if any. Later it'll be in HttpCredentials
    account->setSslConfiguration(QSslConfiguration());
    auto sslConfiguration = account->getOrCreateSslConfig(); // let Account set defaults
    account->setSslConfiguration(sslConfiguration);

    // Make sure TCP connections get re-established
    account->networkAccessManager()->clearAccessCache();

    // Lookup system proxy in a thread https://github.com/owncloud/client/issues/2993
    if (ClientProxy::isUsingSystemDefault()) {
        qCDebug(lcWizard) << "Trying to look up system proxy";
        ClientProxy::lookupSystemProxyAsync(account->url(),
            this, SLOT(slotSystemProxyLookupDone(QNetworkProxy)));
    } else {
        // We want to reset the QNAM proxy so that the global proxy settings are used (via ClientProxy settings)
        account->networkAccessManager()->setProxy(QNetworkProxy(QNetworkProxy::DefaultProxy));
        // use a queued invocation so we're as asynchronous as with the other code path
        QMetaObject::invokeMethod(this, &OwncloudSetupWizard::slotFindServer, Qt::QueuedConnection);
    }
}

void OwncloudSetupWizard::slotSystemProxyLookupDone(const QNetworkProxy &proxy)
{
    if (proxy.type() != QNetworkProxy::NoProxy) {
        qCInfo(lcWizard) << "Setting QNAM proxy to be system proxy" << printQNetworkProxy(proxy);
    } else {
        qCInfo(lcWizard) << "No system proxy set by OS";
    }
    AccountPtr account = _ocWizard->account();
    account->networkAccessManager()->setProxy(proxy);

    slotFindServer();
}

void OwncloudSetupWizard::slotFindServer()
{
    AccountPtr account = _ocWizard->account();

    // Set fake credentials before we check what credential it actually is.
    account->setCredentials(new DummyCredentials);

    // Determining the actual server URL can be a multi-stage process
    // 1. Check url/status.php with CheckServerJob
    //    If that works we're done. In that case we don't check the
    //    url directly for redirects, see #5954.
    // 2. Check the url for permanent redirects (like url shorteners)
    // 3. Check redirected-url/status.php with CheckServerJob

    // Step 1: Check url/status.php
    CheckServerJob *job = new CheckServerJob(account, this);
    connect(job, &CheckServerJob::instanceFound, this, &OwncloudSetupWizard::slotFoundServer);
    connect(job, &CheckServerJob::timeout, this, &OwncloudSetupWizard::slotNoServerFoundTimeout);    
    connect(job, &CheckServerJob::instanceNotFound, this, &OwncloudSetupWizard::slotNoServerFound);
    job->setTimeout((account->url().scheme() == "https") ? 30 * 1000 : 10 * 1000);
    job->start();
}

void OwncloudSetupWizard::slotFoundServer(const QUrl &url, const QJsonObject &info)
{
    auto serverVersion = CheckServerJob::version(info);

    qCDebug(lcWizard) << "Successfully connected to" << url.toString() << "version" << CheckServerJob::versionString(info) << "(" << serverVersion << ")";

    // Note with newer servers we get the version actually only later in capabilities
    // https://github.com/owncloud/core/pull/27473/files
    _ocWizard->account()->setServerVersion(serverVersion);
    const auto oldUrl = _ocWizard->account()->url();
    if (oldUrl != url) {
        qCInfo(lcWizard) << oldUrl << "was redirected to" << url;
        if (url.scheme() == QLatin1String("https") && oldUrl.host() == url.host()) {
            _ocWizard->account()->setUrl(url);
        } else {
            auto accountState = new AccountState(_ocWizard->account());
            connect(accountState, &AccountState::urlUpdated, this, [accountState, this] {
                accountState->deleteLater();
                slotDetermineAuthType();
            });
            accountState->updateUrlDialog(url);
            return;
        }
    }
    slotDetermineAuthType();
}

void OwncloudSetupWizard::slotNoServerFound(QNetworkReply *)
{
    auto job = qobject_cast<CheckServerJob *>(sender());

    // Do this early because reply might be deleted in message box event loop
    QString msg;
    if (!_ocWizard->account()->url().isValid()) {
        msg = tr("Invalid URL");
    } else {
        msg = tr("Failed to connect to %1 at %2:<br/>%3")
                  .arg(Utility::escape(Theme::instance()->appNameGUI()),
                      Utility::escape(_ocWizard->account()->url().toString()),
                      Utility::escape(job->errorString()));
    }

    // Displays message inside wizard and possibly also another message box
    _ocWizard->displayError(msg);

    // Allow the credentials dialog to pop up again for the same URL.
    // Maybe the user just clicked 'Cancel' by accident or changed his mind.
    _ocWizard->account()->resetRejectedCertificates();
}

void OwncloudSetupWizard::slotNoServerFoundTimeout(const QUrl &url)
{
    _ocWizard->displayError(
        tr("Timeout while trying to connect to %1 at %2.")
            .arg(Utility::escape(Theme::instance()->appNameGUI()), Utility::escape(url.toString())));
}

void OwncloudSetupWizard::slotDetermineAuthType()
{
    DetermineAuthTypeJob *job = new DetermineAuthTypeJob(_ocWizard->account(), this);
    connect(job, &DetermineAuthTypeJob::authType,
        _ocWizard, &OwncloudWizard::setAuthType);
    job->start();
}

void OwncloudSetupWizard::slotConnectToOCUrl(const QString &url)
{
    qCInfo(lcWizard) << "Connect to url: " << url;
    AbstractCredentials *creds = _ocWizard->getCredentials();
    _ocWizard->account()->setCredentials(creds);
    _ocWizard->setField(QLatin1String("OCUrl"), url);

    qCDebug(lcWizard) << "Trying to connect to" << url;
    testOwnCloudConnect();
}

void OwncloudSetupWizard::testOwnCloudConnect()
{
    auto job = new ConnectionValidator(_ocWizard->account(), this);
    connect(job, &ConnectionValidator::connectionResult, this, [this](ConnectionValidator::Status status, const QStringList &errors){
        qDebug() << status;
        if (status == ConnectionValidator::Connected) {
            _ocWizard->successfulStep();
            return;
        }
        _ocWizard->show();
        if (_ocWizard->currentId() == WizardCommon::Page_OAuthCreds) {
            _ocWizard->back();
        }
        _ocWizard->displayError(errors.join(QLatin1Char('\n')));
    });
    job->checkServerAndUpdate();
}

void OwncloudSetupWizard::slotCreateLocalAndRemoteFolders()
{
    qCInfo(lcWizard) << "Setup local sync folder for new oC connection " << _ocWizard->localFolder();
    const QDir fi(_ocWizard->localFolder());

    bool nextStep = true;
    if (fi.exists()) {
        FileSystem::setFolderMinimumPermissions(_ocWizard->localFolder());
        Utility::setupFavLink(_ocWizard->localFolder());
        // there is an existing local folder. If its non empty, it can only be synced if the
        // ownCloud is newly created.
        qCDebug(lcWizard) << "Local sync folder" << _ocWizard->localFolder() << "already exists, setting it up for sync.";
    } else {
        bool ok = true;
        if (fi.mkpath(_ocWizard->localFolder())) {
            FileSystem::setFolderMinimumPermissions(_ocWizard->localFolder());
            Utility::setupFavLink(_ocWizard->localFolder());
        } else {
            ok = false;
            qCWarning(lcWizard) << "Failed to create " << fi.path();
            _ocWizard->displayError(tr("Could not create local folder %1").arg(Utility::escape(_ocWizard->localFolder())));
            nextStep = false;
        }
        qCDebug(lcWizard) << "Creating local sync folder" << _ocWizard->localFolder() << "success:" << ok;
    }
    if (nextStep) {
        EntityExistsJob *job = new EntityExistsJob(_ocWizard->account(), Utility::concatUrlPath(_ocWizard->account()->davPath(), _ocWizard->remoteFolder()).path(), this);
        connect(job, &EntityExistsJob::exists, this, &OwncloudSetupWizard::slotRemoteFolderExists);
        job->start();
    } else {
        finalizeSetup(false);
    }
}

// ### TODO move into EntityExistsJob once we decide if/how to return gui strings from jobs
void OwncloudSetupWizard::slotRemoteFolderExists(QNetworkReply *reply)
{
    auto job = qobject_cast<EntityExistsJob *>(sender());
    bool ok = true;
    QString error;
    QNetworkReply::NetworkError errId = reply->error();

    if (errId == QNetworkReply::NoError) {
        qCInfo(lcWizard) << "Remote folder found, all cool!";
    } else if (errId == QNetworkReply::ContentNotFoundError) {
        createRemoteFolder();
    } else {
        error = tr("Error: %1").arg(job->errorString());
        ok = false;
    }

    if (!ok) {
        _ocWizard->displayError(Utility::escape(error));
    }

    finalizeSetup(ok);
}

void OwncloudSetupWizard::createRemoteFolder()
{
    qCDebug(lcWizard) << "creating folder on ownCloud:" << _ocWizard->remoteFolder();

    MkColJob *job = new MkColJob(_ocWizard->account(), _ocWizard->remoteFolder(), this);
    connect(job, &MkColJob::finishedWithError, this, &OwncloudSetupWizard::slotCreateRemoteFolderFinished);
    connect(job, &MkColJob::finishedWithoutError, this, [this] {
        qCDebug(lcWizard) << "Remote folder" << _ocWizard->remoteFolder() << "created successfully.";
        finalizeSetup(true);
    });
    job->start();
}

void OwncloudSetupWizard::slotCreateRemoteFolderFinished(QNetworkReply *reply)
{
    auto error = reply->error();
    qCDebug(lcWizard) << "** webdav mkdir request finished " << error;

    bool success = true;
    if (error == 202) {
        qCDebug(lcWizard) << "The remote folder" << _ocWizard->remoteFolder() << "already exists. Connecting it for syncing.";
    } else if (error > 202 && error < 300) {
        _ocWizard->displayError(tr("The folder creation resulted in HTTP error code %1").arg((int)error));

        qCDebug(lcWizard) << "The folder creation resulted in HTTP error code" << error;
    } else if (error == QNetworkReply::OperationCanceledError) {
        _ocWizard->displayError(tr("The remote folder creation failed because the provided credentials "
                                   "are wrong!"
                                   "<br/>Please go back and check your credentials.</p>"));
        qCDebug(lcWizard) << "Remote folder creation failed probably because the provided credentials are wrong. Please go back and check your credentials.";
        _ocWizard->resetRemoteFolder();
        success = false;
    } else {
        qCDebug(lcWizard) << "Remote folder" << _ocWizard->remoteFolder() << "creation failed with error" << error;
        _ocWizard->displayError(tr("Remote folder %1 creation failed with error <tt>%2</tt>.").arg(Utility::escape(_ocWizard->remoteFolder())).arg(error));
        _ocWizard->resetRemoteFolder();
        success = false;
    }

    finalizeSetup(success);
}

void OwncloudSetupWizard::finalizeSetup(bool success)
{
    const QString localFolder = _ocWizard->localFolder();
    if (success) {
        qCDebug(lcWizard) << "A sync connection from" << localFolder << "to remote directory" << _ocWizard->remoteFolder() << "was set up.";
        qCDebug(lcWizard) << "Successfully connected";
        _ocWizard->successfulStep();
    } else {
        // ### this is not quite true, pass in the real problem as optional parameter
        qCDebug(lcWizard) << "Connection could not be established. Please check again.";
    }
}

bool OwncloudSetupWizard::ensureStartFromScratch(const QString &localFolder)
{
    // first try to rename (backup) the current local dir.
    bool renameOk = false;
    while (!renameOk) {
        renameOk = FolderMan::instance()->startFromScratch(localFolder);
        if (!renameOk) {
            QMessageBox::StandardButton but;
            but = QMessageBox::question(nullptr, tr("Folder rename failed"),
                tr("Can't remove and back up the folder because the folder or a file in it is open in another program."
                   " Please close the folder or file and hit retry or cancel the setup."),
                QMessageBox::Retry | QMessageBox::Abort, QMessageBox::Retry);
            if (but == QMessageBox::Abort) {
                break;
            }
        }
    }
    return renameOk;
}

// Method executed when the user end has finished the basic setup.
void OwncloudSetupWizard::slotAssistantFinished(int result)
{
    if (result == QDialog::Rejected) {
        // Wizard was cancelled
    } else if (_ocWizard->manualFolderConfig()) {
        applyAccountChanges();
    } else {
        FolderMan *folderMan = FolderMan::instance();
        auto account = applyAccountChanges();

        QString localFolder = FolderDefinition::prepareLocalPath(_ocWizard->localFolder());

        bool startFromScratch = _ocWizard->field("OCSyncFromScratch").toBool();
        if (!startFromScratch || ensureStartFromScratch(localFolder)) {
            qCInfo(lcWizard) << "Adding folder definition for" << localFolder << _ocWizard->remoteFolder();
            FolderDefinition folderDefinition;
            folderDefinition.localPath = localFolder;
            folderDefinition.targetPath = FolderDefinition::prepareTargetPath(_ocWizard->remoteFolder());
            folderDefinition.ignoreHiddenFiles = folderMan->ignoreHiddenFiles();
            if (_ocWizard->useVirtualFileSync()) {
                folderDefinition.virtualFilesMode = bestAvailableVfsMode();
            }
#ifdef Q_OS_WIN
            if (folderMan->navigationPaneHelper().showInExplorerNavigationPane())
                folderDefinition.navigationPaneClsid = QUuid::createUuid();
#endif

            auto f = folderMan->addFolder(account, folderDefinition);
            if (f) {
                f->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
                    _ocWizard->selectiveSyncBlacklist());
                if (!_ocWizard->isConfirmBigFolderChecked()) {
                    // The user already accepted the selective sync dialog. everything is in the white list
                    f->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList,
                        QStringList() << QLatin1String("/"));
                }
            }
            qCDebug(lcWizard) << "Local sync folder" << localFolder << "successfully created!";
        }
    }

    // notify others.
    emit ownCloudWizardDone(result);
}

AccountState *OwncloudSetupWizard::applyAccountChanges()
{
    AccountPtr newAccount = _ocWizard->account();

    // Detach the account that is going to be saved from the
    // wizard to ensure it doesn't accidentally get modified
    // later (such as from running cleanup such as
    // AbstractCredentialsWizardPage::cleanupPage())
    _ocWizard->setAccount(AccountManager::createAccount());

    auto manager = AccountManager::instance();

    auto newState = manager->addAccount(newAccount);
    manager->save();
    return newState;
}

} // namespace OCC
