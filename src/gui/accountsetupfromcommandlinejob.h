/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "account.h"

#include <QObject>
#include <QString>
#include <QUrl>

namespace OCC
{
class AccountState;

class AccountSetupFromCommandLineJob : public QObject
{
    Q_OBJECT

public:
    AccountSetupFromCommandLineJob(QString appPassword,
                                   QString userId,
                                   QUrl serverUrl,
                                   QString localDirPath = {},
                                   bool nonVfsMode = false,
                                   QString remoteDirPath = QStringLiteral("/"),
                                   QObject *parent = nullptr);

public slots:
    void handleAccountSetupFromCommandLine();

private slots:
    void checkLastModifiedWithPropfind();

    void accountSetupFromCommandLinePropfindHandleSuccess();

    void accountSetupFromCommandLinePropfindHandleFailure();

    void setupLocalSyncFolder(OCC::AccountState *accountState);

    void printAccountSetupFromCommandLineStatusAndExit(const QString &status, bool isFailure);

    void fetchUserName();

private:
    QString _appPassword;
    QString _userId;
    QUrl _serverUrl;
    QString _localDirPath;
    bool _isVfsEnabled = true;
    QString _remoteDirPath;

    AccountPtr _account;
};
}
