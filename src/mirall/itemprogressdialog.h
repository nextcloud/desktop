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
#include <QLocale>

#include "mirall/progressdispatcher.h"

#include "ui_itemprogressdialog.h"

class QSignalMapper;

namespace Mirall {
class SyncResult;

namespace Ui {
  class ItemProgressDialog;
}
class Application;

class ItemProgressDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ItemProgressDialog(Application *app, QWidget *parent = 0);
    ~ItemProgressDialog();

    void setupList();
    void setSyncResult( const SyncResult& result );

signals:

public slots:
    void accept();
    void slotProgressInfo( const QString& folder, const Progress::Info& progress );
    void slotProgressErrors( const QString& folder, const Progress::SyncProblem& problem );
    void slotOpenFile( QTreeWidgetItem* item, int );

protected slots:
    void copyToClipboard();

signals:
    void guiLog(const QString&, const QString&);

private:
    void setSyncResultStatus(const SyncResult& result );
    void cleanErrors( const QString& folder );
    QString timeString(QDateTime dt, QLocale::FormatType format = QLocale::NarrowFormat) const;

    const int ErrorIndicatorRole;
    Ui::ItemProgressDialog *_ui;
    int _problemCounter;
};

}
#endif // FILEITEMDIALOG_H
