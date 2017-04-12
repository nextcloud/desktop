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
#include "quotainfo.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"

class QModelIndex;
class QNetworkReply;
class QListWidgetItem;
class QLabel;

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

public:
    explicit AccountSettings(AccountState *accountState, QWidget *parent = 0);
    ~AccountSettings();
    QSize sizeHint() const Q_DECL_OVERRIDE { return ownCloudGui::settingsDialogSize(); }


signals:
    void folderChanged();
    void openFolderAlias( const QString& );

public slots:
    void slotOpenOC();
    void slotUpdateQuota( qint64,qint64 );
    void slotAccountStateChanged(int state);

    AccountState* accountsState() { return _accountState; }

protected slots:
    void slotAddFolder();
    void slotEnableCurrentFolder();
    void slotScheduleCurrentFolder();
    void slotScheduleCurrentFolderForceRemoteDiscovery();
    void slotForceSyncCurrentFolder();
    void slotRemoveCurrentFolder();
    void slotOpenCurrentFolder();
    void slotFolderWizardAccepted();
    void slotFolderWizardRejected();
    void slotDeleteAccount();
    void slotToggleSignInState();
    void slotOpenAccountWizard();
    void slotAccountAdded(AccountState *);
    void refreshSelectiveSyncStatus();
    void slotCustomContextMenuRequested(const QPoint&);
    void slotFolderListClicked( const QModelIndex& indx );
    void doExpand();
    void slotLinkActivated(const QString &link);

private:
    void showConnectionLabel(const QString& message,
                             QStringList errors = QStringList());
    bool event(QEvent*) Q_DECL_OVERRIDE;
    void createAccountToolbox();

    /// Returns the alias of the selected folder, empty string if none
    QString selectedFolderAlias() const;

    Ui::AccountSettings *ui;

    FolderStatusModel *_model;
    QUrl   _OCUrl;
    bool _wasDisabledBefore;
    AccountState *_accountState;
    QuotaInfo _quotaInfo;
    QAction *_toggleSignInOutAction;
    QAction *_addAccountAction;
};

} // namespace OCC

#endif // ACCOUNTSETTINGS_H
