/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
