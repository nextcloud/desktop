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
                                   bool isVfsEnabled = false,
                                   QString remoteDirPath = QStringLiteral("/"),
                                   QObject *parent = nullptr);

public Q_SLOTS:
    [[nodiscard]] bool handleAccountSetupFromCommandLine();

private Q_SLOTS:
    void checkLastModifiedWithPropfind();

    void accountSetupFromCommandLinePropfindHandleSuccess();

    void accountSetupFromCommandLinePropfindHandleFailure();

    void setupLocalSyncFolder(OCC::AccountState *accountState);

    void printAccountSetupFromCommandLineStatusAndExit(const QString &status, bool isFailure);

    void fetchUserName();

private:
    /** Whether a classic sync folder has to be set up for the new account.
     *
     * With the app-level File Provider mode enabled the account is synced through its
     * File Provider domain and FolderMan refuses to add a classic sync folder.
     */
    [[nodiscard]] bool localSyncFolderRequired() const;

    /** The local folder to use when none was given on the command line.
     *
     * Follows what the account wizard suggests: the configured override, otherwise the
     * theme's default client folder, made unique against the existing sync folders.
     */
    [[nodiscard]] QString defaultLocalDirPath() const;

    QString _appPassword;
    QString _userId;
    QUrl _serverUrl;
    QString _localDirPath;
    bool _isVfsEnabled = true;
    QString _remoteDirPath;

    AccountPtr _account;
};
}
