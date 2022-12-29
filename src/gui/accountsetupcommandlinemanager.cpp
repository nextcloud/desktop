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
#include "accountsetupfromcommandlinejob.h"

namespace OCC
{
Q_LOGGING_CATEGORY(lcAccountSetupCommandLineManager, "nextcloud.gui.accountsetupcommandlinemanager", QtInfoMsg)

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
    } else if (option == QLatin1String("--isvfsenabled")) {
        if (optionsIterator.hasNext() && !optionsIterator.peekNext().startsWith(QLatin1String("--"))) {
            _isVfsEnabled = optionsIterator.next().toInt() != 0;
            return true;
        } else {
            errorMessage = QStringLiteral("isvfsenabled not specified");
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
        const auto accountSetupJob = new AccountSetupFromCommandLineJob(_appPassword, _userId, _serverUrl, _localDirPath, _isVfsEnabled, _remoteDirPath, parent);
        accountSetupJob->handleAccountSetupFromCommandLine();
    } else {
        qCInfo(lcAccountSetupCommandLineManager) << QStringLiteral("No account setup parameters have been found, or they are invalid. Proceed with normal startup...");
    }
    _appPassword.clear();
    _userId.clear();
    _serverUrl.clear();
    _remoteDirPath.clear();
    _localDirPath.clear();
    _isVfsEnabled = true;
}

QString AccountSetupCommandLineManager::_appPassword;
QString AccountSetupCommandLineManager::_userId;
QUrl AccountSetupCommandLineManager::_serverUrl;
QString AccountSetupCommandLineManager::_remoteDirPath;
QString AccountSetupCommandLineManager::_localDirPath;
bool AccountSetupCommandLineManager::_isVfsEnabled = true;
}
