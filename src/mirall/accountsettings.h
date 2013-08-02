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
#include "mirall/itemprogressdialog.h"

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
class ItemProgressDialog;
class IgnoreListEditor;

class AccountSettings : public QWidget
{
    Q_OBJECT

public:
    explicit AccountSettings(QWidget *parent = 0);
    ~AccountSettings();

    void setFolderList( const Folder::Map& );
    void buttonsSetEnabled();
    void setListWidgetItem(QListWidgetItem* item);

signals:
    void folderChanged();
    void openProgressDialog();
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
    void slotSetProgress(const QString&, const Progress::Info& progress);
    void slotProgressProblem(const QString& folder, const Progress::SyncProblem& problem);

    void slotUpdateQuota( qint64,qint64 );
    void slotIgnoreFilesEditor();

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
    void slotHideProgress();

private:
    QString shortenFilename( const QString& folder, const QString& file ) const;
    void folderToModelItem( QStandardItem *, Folder * );
    QStandardItem* itemForFolder(const QString& );

    Ui::AccountSettings *ui;
    QPointer<ItemProgressDialog> _fileItemDialog;
    QPointer<IgnoreListEditor> _ignoreEditor;
    QStandardItemModel *_model;
    QListWidgetItem *_item;
    QUrl   _OCUrl;
    QHash<QStandardItem*, QTimer*> _hideProgressTimers;
    QString _kindContext;

};

} // namespace Mirall

#endif // ACCOUNTSETTINGS_H
