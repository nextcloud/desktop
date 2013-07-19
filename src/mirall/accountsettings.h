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

#include "mirall/folder.h"
#include "mirall/progressdispatcher.h"

class QStandardItemModel;
class QModelIndex;
class QStandardItem;
class QNetworkReply;
class QListWidgetItem;

namespace Mirall {

namespace Ui {
class AccountSettings;
}

class FolderMan;
class FileItemDialog;

class AccountSettings : public QWidget
{
    Q_OBJECT

public:
    explicit AccountSettings(FolderMan *folderMan, QWidget *parent = 0);
    ~AccountSettings();

    void setFolderList( const Folder::Map& );
    void buttonsSetEnabled();
    void setListWidgetItem(QListWidgetItem* item);

signals:
    void folderChanged();
    void openFolderAlias( const QString& );
    void infoFolderAlias( const QString& );

public slots:
    void slotFolderActivated( const QModelIndex& );
    void slotOpenOC();
    void slotUpdateFolderState( Folder* );
    void slotCheckConnection();
    void slotOCInfo( const QString&, const QString&, const QString&, const QString& );
    void slotOCInfoFail( QNetworkReply* );
    void slotDoubleClicked( const QModelIndex& );
    void slotFolderOpenAction( const QString& );
    void slotSetProgress( Progress::Kind, const QString&, const QString&, long, long );
    void slotSetOverallProgress( const QString&, const QString&, int, int, qlonglong, qlonglong );
    void slotUpdateQuota( qint64,qint64 );

protected slots:
    void slotAddFolder();
    void slotAddFolder( Folder* );
    void slotEnableCurrentFolder();
    void slotRemoveCurrentFolder();
    void slotInfoAboutCurrentFolder();
    void slotResetCurrentFolder();
    void slotFolderWizardAccepted();
    void slotFolderWizardRejected();
    void slotOpenAccountWizard();
    void slotPasswordDialog();
    void slotChangePassword(const QString&);
    void slotHideProgress();

private:
    void folderToModelItem( QStandardItem *, Folder * );
    QStandardItem* itemForFolder(const QString& );

    Ui::AccountSettings *ui;
    QPointer<FileItemDialog> _fileItemDialog;
    FolderMan *_folderMan;
    QStandardItemModel *_model;
    QListWidgetItem *_item;
    QUrl   _OCUrl;
    double _progressFactor;
    QHash<QStandardItem*, QTimer*> _hideProgressTimers;
    QTimer *_timer;
    long _lastSyncProgress;
};

} // namespace Mirall

#endif // ACCOUNTSETTINGS_H
