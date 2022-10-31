/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "accountstate.h"

namespace OCC {

class EditLocallyJob;
using EditLocallyJobPtr = QSharedPointer<EditLocallyJob>;

class Folder;
class SyncResult;

class EditLocallyJob : public QObject
{
    Q_OBJECT

public:
    explicit EditLocallyJob(const QString &userId,
                            const QString &relPath,
                            const QString &token,
                            QObject *parent = nullptr);

    static bool isTokenValid(const QString &token);
    static bool isRelPathValid(const QString &relPath);
    static bool isRelPathExcluded(const QString &relPath);
    static QString prefixSlashToPath(const QString &path);

signals:
    void setupFinished();
    void error(const QString &message, const QString &informativeText);
    void fileOpened();

public slots:
    void startSetup();
    void startEditLocally();

private slots:
    void startTokenRemoteCheck();
    void proceedWithSetup();

    void showError(const QString &message, const QString &informativeText);
    void showErrorNotification(const QString &message, const QString &informativeText) const;
    void showErrorMessageBox(const QString &message, const QString &informativeText) const;

    void remoteTokenCheckResultReceived(const int statusCode);
    void folderSyncFinished(const OCC::SyncResult &result);

    void disconnectSyncFinished() const;
    void openFile();

private:
    bool _tokenVerified = false;

    AccountStatePtr _accountState;
    QString _userId;
    QString _relPath;
    QString _token;

    QString _fileName;
    QString _localFilePath;
    Folder *_folderForFile = nullptr;
    std::unique_ptr<SimpleApiJob> _checkTokenJob;
};

}
