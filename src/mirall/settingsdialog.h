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

#include "mirall/progressdispatcher.h"

class QStandardItemModel;
class QListWidgetItem;

namespace Mirall {

namespace Ui {
class SettingsDialog;
}
class AccountSettings;
class Application;
class FolderMan;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(Application *app, QWidget *parent = 0);
    ~SettingsDialog();

    void addAccount(const QString &title, QWidget *widget);

protected:
    void closeEvent(QCloseEvent *event);

protected slots:
    void slotUpdateAccountState();

private:
    Ui::SettingsDialog *_ui;
    AccountSettings *_accountSettings;
    QListWidgetItem *_accountItem;

};

}

#endif // SETTINGSDIALOG_H
