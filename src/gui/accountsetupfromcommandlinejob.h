/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "account.h"

#include <QList>
#include <QObject>
#include <QPair>
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
                                   QList<QPair<QString, QString>> folderPaths = {},
                                   bool isVfsEnabled = false,
                                   QObject *parent = nullptr);

public slots:
    void handleAccountSetupFromCommandLine();

private slots:
    void checkLastModifiedWithPropfind();

    void accountSetupFromCommandLinePropfindHandleSuccess();

    void accountSetupFromCommandLinePropfindHandleFailure();

    void setupLocalSyncFolders(OCC::AccountState *accountState);

    void printAccountSetupFromCommandLineStatusAndExit(const QString &status, bool isFailure);

    void fetchUserName();

private:
    QString _appPassword;
    QString _userId;
    QUrl _serverUrl;
    QList<QPair<QString, QString>> _folderPaths;
    bool _isVfsEnabled = false;
    bool _accountIsNew = false;

    AccountPtr _account;
};
}
