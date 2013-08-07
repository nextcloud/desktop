/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef FILEITEMDIALOG_H
#define FILEITEMDIALOG_H

#include <QDialog>
#include <QDateTime>
#include <QTimer>

#include "mirall/syncfileitem.h"

#include "ui_fileitemdialog.h"

namespace Mirall {
class SyncResult;

namespace Ui {
  class FileItemDialog;
}

class FileItemDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FileItemDialog(QWidget *parent = 0);
    ~FileItemDialog();
    void setSyncResult( const SyncResult& );

signals:

public slots:
    void accept();

protected slots:
    void slotSetFolderMessage();
    void copyToClipboard();

private:
    void setSyncFileItems( const SyncFileItemVector& list );
    void formatHeaderItem( QTreeWidgetItem *, const QList<QTreeWidgetItem*>& );

    QTreeWidgetItem *_newFileItem;
    QTreeWidgetItem *_syncedFileItem;
    QTreeWidgetItem *_deletedFileItem;
    QTreeWidgetItem *_renamedFileItem;
    QTreeWidgetItem *_errorFileItem;
    QTreeWidgetItem *_conflictFileItem;
    QTreeWidgetItem *_ignoredFileItem;

    QString   _folderMessage;
    QDateTime _lastSyncTime;
    QTimer    _timer;
    Ui::FileItemDialog *_ui;
};

}
#endif // FILEITEMDIALOG_H
