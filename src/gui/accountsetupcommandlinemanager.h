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

#include <QStringList>

namespace OCC {

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

    void printAccountSetupFromCommandLineStatusAndExit(const QString &status, bool isFailure);

    void fetchUserName();

private:
    QString _appPassword;
    QString _userId;
    QUrl _serverUrl;
    QString _localDirPath;
    bool _nonVfsMode = false;
    QString _remoteDirPath;

    AccountPtr _account;
};


class AccountSetupCommandLineManager
{
public:
    [[nodiscard]] static bool parseCommandlineOption(const QString &option, QStringListIterator &optionsIterator, QString &errorMessage);

    [[nodiscard]] static bool isCommandLineParsed();

    static void setupAccountFromCommandLine(QObject *parent = nullptr);

private:
    explicit AccountSetupCommandLineManager() = delete;

    static QString _appPassword;
    static QString _userId;
    static QUrl _serverUrl;
    static QString _remoteDirPath;
    static QString _localDirPath;
    static bool _nonVfsMode;
};

}
