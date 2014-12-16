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
#include <QStandardItem>

#include "folder.h"
#include "progressdispatcher.h"

class QStandardItemModel;
class QModelIndex;
class QStandardItem;
class QNetworkReply;
class QListWidgetItem;

namespace OCC {

namespace Ui {
class AccountSettings;
}

class FolderMan;
class IgnoreListEditor;
class Account;

class AccountSettings : public QWidget
{
    Q_OBJECT

public:
    explicit AccountSettings(QWidget *parent = 0);
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
    void slotSetProgress(const QString& folder, const Progress::Info& progress);
    void slotButtonsSetEnabled();

    void slotUpdateQuota( qint64,qint64 );
    void slotIgnoreFilesEditor();
    void slotAccountStateChanged(int state);

    void setGeneralErrors( const QStringList& errors );
    void setFolderList( const Folder::Map& );

protected slots:
    void slotAddFolder();
    void slotAddFolder( Folder* );
    void slotEnableCurrentFolder();
    void slotSyncCurrentFolderNow();
    void slotRemoveCurrentFolder();
    void slotResetCurrentFolder();
    void slotFolderWizardAccepted();
    void slotFolderWizardRejected();
    void slotOpenAccountWizard();
    void slotHideProgress();
    void slotSelectiveSync();

private:
    QString shortenFilename( const QString& folder, const QString& file ) const;
    void folderToModelItem(QStandardItem *, Folder * , bool accountConnected);
    QStandardItem* itemForFolder(const QString& );
    void showConnectionLabel( const QString& message, const QString& tooltip = QString() );

    Ui::AccountSettings *ui;
    QPointer<IgnoreListEditor> _ignoreEditor;
    QStandardItemModel *_model;
    QUrl   _OCUrl;
    QHash<QStandardItem*, QTimer*> _hideProgressTimers;
    QStringList _generalErrors;
    bool _wasDisabledBefore;
    Account *_account;
private slots:
    void slotFolderSyncStateChange();
    void slotAccountChanged(Account*,Account*);
};

} // namespace OCC

#endif // ACCOUNTSETTINGS_H
