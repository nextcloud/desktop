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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QStyledItemDelegate>

class QStandardItemModel;
class QListWidgetItem;

namespace Mirall {

namespace Ui {
class SettingsDialog;
}
class AccountSettings;
class Application;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(Application *app, QWidget *parent = 0);
    ~SettingsDialog();

    void addAccount(const QString &title, QWidget *widget);
public slots:
    // Progress, parameter is
    //  - filename
    //  - progress bytes, overall size.
    void slotFolderUploadProgress( const QString&, const QString&, long, long );

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void done();

private:
    Ui::SettingsDialog *_ui;
    QListWidgetItem *_addItem;
    AccountSettings *_accountSettings;

};

}

#endif // SETTINGSDIALOG_H
