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

AccountSetupCommandLineManager *AccountSetupCommandLineManager::_instance = nullptr;

AccountSetupCommandLineManager::AccountSetupCommandLineManager(QObject *parent)
    : QObject{parent}
{
}

AccountSetupCommandLineManager *AccountSetupCommandLineManager::instance()
{
    if (!_instance) {
        _instance = new AccountSetupCommandLineManager();
    }
    return _instance;
}

void AccountSetupCommandLineManager::destroy()
{
    if (_instance) {
        _instance->deleteLater();
        _instance = nullptr;
    }
}

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

bool AccountSetupCommandLineManager::isCommandLineParsed() const
{
    return !_appPassword.isEmpty() && !_userId.isEmpty() && _serverUrl.isValid();
}

bool AccountSetupCommandLineManager::isVfsEnabled() const
{
    return _isVfsEnabled;
}

void AccountSetupCommandLineManager::setupAccountFromCommandLine()
{
    if (isCommandLineParsed()) {
        qCInfo(lcAccountSetupCommandLineManager) << QStringLiteral("Command line has been parsed and account setup parameters have been found. Attempting setup a new account %1...").arg(_userId);
        const auto accountSetupJob = new AccountSetupFromCommandLineJob(_appPassword, _userId, _serverUrl, _localDirPath, _isVfsEnabled, _remoteDirPath, parent());
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
}
