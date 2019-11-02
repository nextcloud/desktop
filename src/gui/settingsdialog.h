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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QLoggingCategory>
#include <QDialog>
#include <QStyledItemDelegate>

#include "progressdispatcher.h"
#include "owncloudgui.h"

class QAction;
class QActionGroup;
class QToolBar;
class QStandardItemModel;

namespace OCC {

 Q_DECLARE_LOGGING_CATEGORY(lcSettings)

class AccountState;

namespace Ui {
    class SettingsDialog;
}
class AccountSettings;
class Application;
class FolderMan;
class ownCloudGui;
class ActivitySettings;

/**
 * @brief The SettingsDialog class
 * @ingroup gui
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ownCloudGui *gui, QWidget *parent = nullptr);
    ~SettingsDialog();

    void addAccount(const QString &title, QWidget *widget);

public slots:
    void showFirstPage();
    void showActivityPage();
    void showIssuesList(AccountState *account);
    void slotSwitchPage(QAction *action);
    void slotRefreshActivity(AccountState *accountState);
    void slotRefreshActivityAccountStateSender();
    void slotAccountAvatarChanged();
    void slotAccountDisplayNameChanged();

protected:
    void reject() override;
    void accept() override;
    void changeEvent(QEvent *) override;

    // FIX - White Window issue on Windows after Qt 5.12.4 upgrade ///////////////////////////////
    #if (defined(Q_OS_WIN) && (QT_VERSION >= QT_VERSION_CHECK(5, 12, 4)))
        void showEvent(QShowEvent *) override;
    #endif
    // FIX - White Window issue on Windows after Qt 5.12.4 upgrade ///////////////////////////////

private slots:
    void accountAdded(AccountState *);
    void accountRemoved(AccountState *);

    // FIX - White Window issue on Windows after Qt 5.12.4 upgrade ///////////////////////////////
    #if (defined(Q_OS_WIN) && (QT_VERSION >= QT_VERSION_CHECK(5, 12, 4)))
        void timerShowEvent();
    #endif
    // FIX - White Window issue on Windows after Qt 5.12.4 upgrade ///////////////////////////////

private:
    void customizeStyle();
    void activityAdded(AccountState *);

    QIcon createColorAwareIcon(const QString &name);
    QAction *createColorAwareAction(const QString &iconName, const QString &fileName);
    QAction *createActionWithIcon(const QIcon &icon, const QString &text, const QString &iconPath = QString());
    void checkSchedule();
    void setPauseOnAllFoldersHelper(bool pause);
  
    Ui::SettingsDialog *const _ui;

    QActionGroup *_actionGroup;
    QAction *_actionBefore;
    // Maps the actions from the action group to the corresponding widgets
    QHash<QAction *, QWidget *> _actionGroupWidgets;

    // Maps the action in the dialog to their according account. Needed in
    // case the account avatar changes
    QHash<Account *, QAction *> _actionForAccount;

    QToolBar *_toolBar;
    QMap<AccountState *, ActivitySettings *> _activitySettings;

    // Timer for schedule syncing
    QTimer *_scheduleTimer;
  
    ownCloudGui *_gui;
};
}

#endif // SETTINGSDIALOG_H
