/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#include "progressdispatcher.h"

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

class AccountSettings : public QWidget
{
    Q_OBJECT

public:
    explicit AccountSettings(AccountState *accountState, QWidget *parent = 0);
    ~AccountSettings();


signals:
    void folderChanged();
    void openProtocol();
    void openFolderAlias( const QString& );
    void infoFolderAlias( const QString& );

public slots:
    void slotFolderActivated( const QModelIndex& );
    void slotOpenOC();
    void slotUpdateFolderState( Folder* );
    void slotDoubleClicked( const QModelIndex& );
    void slotSetProgress(const QString& folder, const ProgressInfo& progress);

    void slotUpdateQuota( qint64,qint64 );
    void slotAccountStateChanged(int state);

    void setGeneralErrors( const QStringList& errors );
    AccountState* accountsState() { return _accountState; };

protected slots:
    void slotAddFolder();
    void slotEnableCurrentFolder();
    void slotSyncCurrentFolderNow();
    void slotRemoveCurrentFolder();
    void slotResetCurrentFolder();
    void slotFolderWizardAccepted();
    void slotFolderWizardRejected();
    void slotHideProgress();
    void slotDeleteAccount();
    void refreshSelectiveSyncStatus();

private:
    QString shortenFilename( const QString& folder, const QString& file ) const;
    void showConnectionLabel( const QString& message, const QString& tooltip = QString() );

    Ui::AccountSettings *ui;

    FolderStatusModel *_model;
    QUrl   _OCUrl;
    QStringList _generalErrors;
    bool _wasDisabledBefore;
    AccountState *_accountState;
    QLabel *_quotaLabel;
private slots:
    void slotFolderSyncStateChange();
    void slotCustomContextMenuRequested(const QPoint&);
};

} // namespace OCC

#endif // ACCOUNTSETTINGS_H
