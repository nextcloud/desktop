/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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
#include "accountfwd.h"
#include "accountstate.h"
#include "folderman.h"

#include <theme.h>
#include <folder.h>

#include <QObject>

namespace OCC {

class SyncStatusSummary : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double syncProgress READ syncProgress NOTIFY syncProgressChanged)
    Q_PROPERTY(QUrl syncIcon READ syncIcon NOTIFY syncIconChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(QString syncStatusString READ syncStatusString NOTIFY syncStatusStringChanged)
    Q_PROPERTY(QString syncStatusDetailString READ syncStatusDetailString NOTIFY syncStatusDetailStringChanged)
    Q_PROPERTY(qint64 totalFiles READ totalFiles NOTIFY totalFilesChanged)

public:
    explicit SyncStatusSummary(QObject *parent = nullptr);

    [[nodiscard]] double syncProgress() const;
    [[nodiscard]] QUrl syncIcon() const;
    [[nodiscard]] bool syncing() const;
    [[nodiscard]] QString syncStatusString() const;
    [[nodiscard]] QString syncStatusDetailString() const;
    [[nodiscard]] qint64 totalFiles() const;

signals:
    void syncProgressChanged();
    void syncIconChanged();
    void syncingChanged();
    void syncStatusStringChanged();
    void syncStatusDetailStringChanged();
    void totalFilesChanged();

public slots:
    void load();

private:
    void connectToFoldersProgress(const Folder::Map &map);

    void onFolderListChanged(const OCC::Folder::Map &folderMap);
    void onFolderProgressInfo(const ProgressInfo &progress);
    void onFolderSyncStateChanged(const Folder *folder);
    void onIsConnectedChanged();

    void setSyncStateForFolder(const Folder *folder);
    void markFolderAsError(const Folder *folder);
    void markFolderAsSuccess(const Folder *folder);
    [[nodiscard]] bool folderErrors() const;
    bool folderError(const Folder *folder) const;
    void clearFolderErrors();
    void setSyncStateToConnectedState();
    bool reloadNeeded(AccountState *accountState) const;
    void initSyncState();

    void setSyncProgress(double value);
    void setSyncing(bool value);
    void setSyncStatusString(const QString &value);
    void setSyncStatusDetailString(const QString &value);
    void setSyncIcon(const QUrl &value);
    void setAccountState(AccountStatePtr accountState);
    void setTotalFiles(const qint64 value);

    AccountStatePtr _accountState;
    std::set<QString> _foldersWithErrors;

    QUrl _syncIcon = Theme::instance()->syncStatusOk();
    double _progress = 1.0;
    bool _isSyncing = false;
    qint64 _totalFiles = 0;
    QString _syncStatusString = tr("All synced!");
    QString _syncStatusDetailString;
};
}
