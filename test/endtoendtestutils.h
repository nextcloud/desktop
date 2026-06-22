/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QTemporaryDir>

#include "gui/accountstate.h"
#include "gui/folderman.h"
#include "libsync/account.h"

constexpr auto testUsername = "test";
constexpr auto testPassword = "test";

class QNetworkReply;

namespace OCC
{
class Folder;
class FolderMan;
}

class EndToEndTestCredentials : public OCC::AbstractCredentials
{
    Q_OBJECT

public:
    explicit EndToEndTestCredentials()
        : OCC::AbstractCredentials()
        , _user(testUsername)
        , _password(testPassword)
    {
        _wasFetched = true;
    };

    [[nodiscard]] QString authType() const override { return QStringLiteral("http"); }
    [[nodiscard]] QString user() const override { return _user; }
    [[nodiscard]] QString password() const override { return _password; }
    [[nodiscard]] bool ready() const override { return true; }
    bool stillValid(QNetworkReply *) override { return true; }
    void askFromUser() override {};
    void fetchFromKeychain(const QString &appName = {}) override {
        Q_UNUSED(appName)
        _wasFetched = true;
        Q_EMIT fetched();
    };
    void persist() override {};
    void invalidateToken() override {};
    void forgetSensitiveData() override {};

    [[nodiscard]] QNetworkAccessManager *createQNAM() const override;

private:
    QString _user;
    QString _password;
};

class EndToEndTestHelper : public QObject
{
    Q_OBJECT

public:
    EndToEndTestHelper() = default;
    ~EndToEndTestHelper() override;

    [[nodiscard]] OCC::AccountPtr account() const { return _account; }
    [[nodiscard]] OCC::AccountStatePtr accountState() const { return _accountState; }

    OCC::Folder *configureSyncFolder(const QString &targetPath = QStringLiteral(""));

signals:
    void accountReady(const OCC::AccountPtr &account);

public slots:
    void startAccountConfig();
    void removeConfiguredAccount();
    void removeConfiguredSyncFolder();

private slots:
    void slotConnectToNCUrl(const QString &url);
    void setupFolderMan();

private:
    OCC::AccountPtr _account;
    OCC::AccountStatePtr _accountState;
    QScopedPointer<OCC::FolderMan> _folderMan;
    QTemporaryDir _tempDir;

    OCC::Folder* _syncFolder = nullptr;
};
