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

#pragma once

#include "account.h"
#include "accountstate.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
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

    void accountCheckConnectivityFinished(OCC::AccountState::State state);

    void accountCredentialsWriteJoobDone();

    void accountSetupFromCommandLinePropfindHandleSuccess();

    void accountSetupFromCommandLinePropfindHandleFailure();

    void setupLocalSyncFolder(AccountState *accountState);

    void printAccountSetupFromCommandLineStatusAndExit(const QString &status, bool isFailure);

    void fetchUserName();

private:
    QString _appPassword;
    QString _userId;
    QUrl _serverUrl;
    QString _localDirPath;
    bool _isVfsEnabled = true;
    QString _remoteDirPath;
    AccountState *_accountState = nullptr;
    AccountPtr _account;
    QTimer _checkConnectivityTimeout;
};
}
