/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef ACCOUNTSETTINGS_H
#define ACCOUNTSETTINGS_H

#include <QWidget>
#include <QUrl>
#include <QPointer>
#include <QHash>
#include <QTimer>

#include "folder.h"
#include "loginrequireddialog.h"
#include "owncloudgui.h"
#include "progressdispatcher.h"
#include "quotainfo.h"

class QModelIndex;
class QNetworkReply;
class QListWidgetItem;
class QLabel;
class QSortFilterProxyModel;

namespace OCC {

namespace Ui {
    class AccountSettings;
}

class FolderMan;

class Account;
class AccountState;
class FolderStatusModel;

/**
 * @brief The AccountSettings class
 * @ingroup gui
 */
class AccountSettings : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(AccountStatePtr accountState MEMBER _accountState)

public:
    explicit AccountSettings(const AccountStatePtr &accountState, QWidget *parent = nullptr);
    ~AccountSettings() override;

    AccountStatePtr accountsState() const { return _accountState; }

signals:
    void folderChanged();
    void showIssuesList();

public slots:
    void slotOpenOC();
    void slotUpdateQuota(qint64, qint64);
    void slotAccountStateChanged();

protected slots:
    void slotAddFolder();
    void slotEnableCurrentFolder(bool terminate = false);
    void slotScheduleCurrentFolder();
    void slotScheduleCurrentFolderForceFullDiscovery();
    void slotForceSyncCurrentFolder();
    void slotRemoveCurrentFolder();
    void slotEnableVfsCurrentFolder();
    void slotDisableVfsCurrentFolder();
    void slotFolderWizardAccepted();
    void slotDeleteAccount();
    void slotToggleSignInState();
    [[deprecated]] void refreshSelectiveSyncStatus();
    void slotCustomContextMenuRequested(const QPoint &);
    void slotFolderListClicked(const QModelIndex &indx);
    void doExpand();
    void slotLinkActivated(const QString &link);

private:
    void showConnectionLabel(const QString &message,
        QStringList errors = QStringList());
    bool event(QEvent *) override;
    void createAccountToolbox();

    /// Returns the alias of the selected folder, empty string if none
    Folder *selectedFolder() const;

    Ui::AccountSettings *ui;

    FolderStatusModel *_model;
    QSortFilterProxyModel *_sortModel;
    QUrl _OCUrl;
    bool _wasDisabledBefore;
    AccountStatePtr _accountState;
    QuotaInfo *_quotaInfo = nullptr;
    QAction *_toggleSignInOutAction;
    QAction *_toggleReconnect;

    // needed to make sure we show only one dialog at a time
    QPointer<LoginRequiredDialog> _askForOAuthLoginDialog = nullptr;
};

} // namespace OCC

#endif // ACCOUNTSETTINGS_H
