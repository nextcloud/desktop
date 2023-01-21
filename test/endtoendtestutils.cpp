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

#include "endtoendtestutils.h"

#include <QNetworkProxy>
#include <QJsonObject>
#include <QSignalSpy>

#include "cmd/simplesslerrorhandler.h"
#include "creds/httpcredentials.h"
#include "gui/accountmanager.h"
#include "libsync/theme.h"
#include "accessmanager.h"
#include "httplogger.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

constexpr auto serverUrl = "https://server";

Q_LOGGING_CATEGORY(lcEndToEndTestUtils, "nextcloud.gui.endtoendtestutils", QtInfoMsg)

/** End to end test credentials access manager class **/

class EndToEndTestCredentialsAccessManager : public OCC::AccessManager
{
public:
    EndToEndTestCredentialsAccessManager(const EndToEndTestCredentials *cred, QObject *parent = nullptr)
        : OCC::AccessManager(parent)
        , _cred(cred)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        if(!_cred) {
            qCWarning(lcEndToEndTestUtils) << "Could not create request -- null creds!";
            return {};
        }

        QNetworkRequest req(request);
        QByteArray credHash = QByteArray(_cred->user().toUtf8() + ":" + _cred->password().toUtf8()).toBase64();
        req.setRawHeader("Authorization", "Basic " + credHash);

        return OCC::AccessManager::createRequest(op, req, outgoingData);
    }

private:
    // The credentials object dies along with the account, while the QNAM might
    // outlive both.
    QPointer<const EndToEndTestCredentials> _cred;
};

/** End to end test credentials class **/

QNetworkAccessManager *EndToEndTestCredentials::createQNAM() const
{
    return new EndToEndTestCredentialsAccessManager(this);
}

/** End to end test helper class **/

EndToEndTestHelper::~EndToEndTestHelper()
{
    removeConfiguredSyncFolder();
    removeConfiguredAccount();

    OCC::AccountManager::instance()->shutdown();
}

void EndToEndTestHelper::startAccountConfig()
{
    const auto accountManager = OCC::AccountManager::instance();
    _account = accountManager->createAccount();

    _account->setCredentials(new EndToEndTestCredentials);
    _account->setUrl(OCC::Theme::instance()->overrideServerUrl());

    const auto serverUrlString = QString(serverUrl);
    _account->setUrl(serverUrlString);

    _account->networkAccessManager()->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
    _account->setSslConfiguration(QSslConfiguration::defaultConfiguration());
    _account->setSslErrorHandler(new OCC::SimpleSslErrorHandler);
    _account->setTrustCertificates(true);

    slotConnectToNCUrl(serverUrlString);
}

void EndToEndTestHelper::slotConnectToNCUrl(const QString &url)
{
    qCDebug(lcEndToEndTestUtils) << "Connect to url: " << url;

    const auto fetchUserNameJob = new OCC::JsonApiJob(_account->sharedFromThis(), QStringLiteral("/ocs/v1.php/cloud/user"));
    connect(fetchUserNameJob, &OCC::JsonApiJob::jsonReceived, this, [this, url](const QJsonDocument &json, const int statusCode) {
        if (statusCode != 100) {
            qCDebug(lcEndToEndTestUtils) << "Could not fetch username.";
        }

        const auto objData = json.object().value("ocs").toObject().value("data").toObject();
        const auto userId = objData.value("id").toString("");
        const auto displayName = objData.value("display-name").toString("");
        _account->setDavUser(userId);
        _account->setDavDisplayName(displayName);

        _accountState = new OCC::AccountState(_account);

        emit accountReady(_account);
    });
    fetchUserNameJob->start();
}

void EndToEndTestHelper::removeConfiguredAccount()
{
    OCC::AccountManager::instance()->deleteAccount(_accountState.data());
}

OCC::Folder *EndToEndTestHelper::configureSyncFolder(const QString &targetPath)
{
    if(_syncFolder) {
        removeConfiguredSyncFolder();
    }

    qCDebug(lcEndToEndTestUtils) << "Creating temp end-to-end test folder.";
    Q_ASSERT(_tempDir.isValid());
    OCC::FileSystem::setFolderMinimumPermissions(_tempDir.path());
    qCDebug(lcEndToEndTestUtils) << "Created temp end-to-end test folder at:" << _tempDir.path();

    setupFolderMan();

    OCC::FolderDefinition definition;
    definition.localPath = _tempDir.path();
    definition.targetPath = targetPath;
    _syncFolder = _folderMan->addFolder(_accountState.data(), definition);

    return _syncFolder;
}

void EndToEndTestHelper::removeConfiguredSyncFolder()
{
    if(!_syncFolder || !_folderMan) {
        return;
    }

    QSignalSpy folderSyncFinished(_syncFolder, &OCC::Folder::syncFinished);
    _folderMan->forceSyncForFolder(_syncFolder);
    Q_ASSERT(folderSyncFinished.wait(3000));
    _folderMan->unloadAndDeleteAllFolders();
    _syncFolder = nullptr;
}

void EndToEndTestHelper::setupFolderMan()
{
    if(_folderMan) {
        return;
    }

    auto folderMan = new OCC::FolderMan;
    Q_ASSERT(folderMan);
    folderMan->setSyncEnabled(true);
    _folderMan.reset(folderMan);
}
