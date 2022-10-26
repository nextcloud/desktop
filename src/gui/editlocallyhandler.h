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

#include "accountmanager.h"
#include "folder.h"

namespace OCC {

class EditLocallyHandler : public QObject
{
    Q_OBJECT

public:
    explicit EditLocallyHandler(const QString &userId,
                                const QString &relPath,
                                const QString &token,
                                QObject *parent = nullptr);

    [[nodiscard]] static bool isTokenValid(const QString &token);
    [[nodiscard]] static bool isRelPathValid(const QString &relPath);
    [[nodiscard]] static bool isRelPathExcluded(const QString &relPath);
    [[nodiscard]] static QString prefixSlashToPath(const QString &path);

    [[nodiscard]] bool ready() const;

signals:
    void finished();

public slots:
    void startEditLocally();
    void startTokenRemoteCheck();

private slots:
    void showError(const QString &message, const QString &informativeText) const;
    void showErrorNotification(const QString &message, const QString &informativeText) const;
    void showErrorMessageBox(const QString &message, const QString &informativeText) const;

    void remoteTokenCheckFinished(const int statusCode);
    void folderSyncFinished(const OCC::SyncResult &result);

    void disconnectSyncFinished() const;
    void openFile();

private:
    bool _ready = false;

    AccountStatePtr _accountState;
    QString _relPath;
    QString _token;

    QString _fileName;
    QString _localFilePath;
    Folder *_folderForFile = nullptr;
};

}
