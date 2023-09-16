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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace OCC {
class AccountSetupCommandLineManager : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] static AccountSetupCommandLineManager *instance();
    static void destroy();

    [[nodiscard]] bool parseCommandlineOption(const QString &option, QStringListIterator &optionsIterator, QString &errorMessage);

    [[nodiscard]] bool isCommandLineParsed() const;

    [[nodiscard]] bool isVfsEnabled() const;

public slots:
    void setupAccountFromCommandLine();

private:
    explicit AccountSetupCommandLineManager(QObject *parent = nullptr);

    static AccountSetupCommandLineManager *_instance;

    QString _appPassword;
    QString _userId;
    QUrl _serverUrl;
    QString _remoteDirPath;
    QString _localDirPath;
    bool _isVfsEnabled = false;
};

}
