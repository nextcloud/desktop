/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#ifdef BUILD_FILE_PROVIDER_MODULE
    void onFileProviderDomainSyncStateChanged(const AccountPtr &account, const SyncResult::Status status);
#endif

    void setSyncState(const SyncResult::Status state);
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
#ifdef BUILD_FILE_PROVIDER_MODULE
    std::set<QString> _fileProviderDomainsWithErrors;
#endif

    QUrl _syncIcon = Theme::instance()->syncStatusOk();
    double _progress = 1.0;
    bool _isSyncing = false;
    qint64 _totalFiles = 0;
    QString _syncStatusString = tr("All synced!");
    QString _syncStatusDetailString;
};
}
