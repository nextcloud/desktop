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
#include "userinfo.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "folderstatusmodel.h"

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
    Q_PROPERTY(AccountState* accountState MEMBER _accountState)

public:
    explicit AccountSettings(AccountState *accountState, QWidget *parent = nullptr);
    ~AccountSettings();
    QSize sizeHint() const override { return ownCloudGui::settingsDialogSize(); }
    bool canEncryptOrDecrypt(const FolderStatusModel::SubFolderInfo* folderInfo);

signals:
    void folderChanged();
    void openFolderAlias(const QString &);
    void showIssuesList(AccountState *account);
    void requestMnemonic();
    void removeAccountFolders(AccountState *account);
    void styleChanged();

public slots:
    void slotOpenOC();
    void slotUpdateQuota(qint64 total, qint64 used);
    void slotAccountStateChanged();
    void slotStyleChanged();
    AccountState *accountsState() { return _accountState; }
    void slotHideSelectiveSyncWidget();

protected slots:
    void slotAddFolder();
    void slotEnableCurrentFolder(bool terminate = false);
    void slotScheduleCurrentFolder();
    void slotScheduleCurrentFolderForceRemoteDiscovery();
    void slotForceSyncCurrentFolder();
    void slotRemoveCurrentFolder();
    void slotOpenCurrentFolder(); // sync folder
    void slotOpenCurrentLocalSubFolder(); // selected subfolder in sync folder
    void slotEditCurrentIgnoredFiles();
    void slotOpenMakeFolderDialog();
    void slotEditCurrentLocalIgnoredFiles();
    void slotEnableVfsCurrentFolder();
    void slotDisableVfsCurrentFolder();
    void slotSetCurrentFolderAvailability(PinState state);
    void slotSetSubFolderAvailability(Folder *folder, const QString &path, PinState state);
    void slotFolderWizardAccepted();
    void slotFolderWizardRejected();
    void slotDeleteAccount();
    void slotToggleSignInState();
    void refreshSelectiveSyncStatus();
    void slotMarkSubfolderEncrypted(FolderStatusModel::SubFolderInfo *folderInfo);
    void slotSubfolderContextMenuRequested(const QModelIndex& idx, const QPoint& point);
    void slotCustomContextMenuRequested(const QPoint &);
    void slotFolderListClicked(const QModelIndex &indx);
    void doExpand();
    void slotLinkActivated(const QString &link);

    // Encryption Related Stuff.
    void slotShowMnemonic(const QString &mnemonic);
    void slotNewMnemonicGenerated();
    void slotEncryptFolderFinished(int status);

    void slotSelectiveSyncChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                                  const QVector<int> &roles);

private:
    void showConnectionLabel(const QString &message,
        QStringList errors = QStringList());
    bool event(QEvent *) override;
    void createAccountToolbox();
    void openIgnoredFilesDialog(const QString & absFolderPath);
    void customizeStyle();

    /// Returns the alias of the selected folder, empty string if none
    QString selectedFolderAlias() const;

    Ui::AccountSettings *_ui;

    FolderStatusModel *_model;
    QUrl _OCUrl;
    bool _wasDisabledBefore;
    AccountState *_accountState;
    UserInfo _userInfo;
    QAction *_toggleSignInOutAction;
    QAction *_addAccountAction;

    bool _menuShown;
};

} // namespace OCC

#endif // ACCOUNTSETTINGS_H
