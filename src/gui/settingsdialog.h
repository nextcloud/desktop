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

#pragma once

#include "accountstate.h"
#include "gui/qmlutils.h"
#include "owncloudgui.h"
#include "progressdispatcher.h"

#include <QMainWindow>
#include <QStyledItemDelegate>

namespace OCC {

namespace Ui {
    class SettingsDialog;
}
class AccountSettings;
class Application;
class FolderMan;
class ownCloudGui;
class ActivitySettings;
class GeneralSettings;

/**
 * @brief The SettingsDialog class
 * @ingroup gui
 */
class SettingsDialog : public QMainWindow
{
    Q_OBJECT
    Q_PROPERTY(SettingsPage currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
    Q_PROPERTY(Account *currentAccount READ currentAccount WRITE setCurrentAccount NOTIFY currentAccountChanged)
    QML_ELEMENT
    QML_UNCREATABLE("C++ only")
    OC_DECLARE_WIDGET_FOCUS
public:
    enum class SettingsPage { None, Activity, Settings, Account };
    Q_ENUM(SettingsPage)
    explicit SettingsDialog(ownCloudGui *gui, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    void addModalWidget(QWidget *w);

    void requestModality(Account *account);
    void ceaseModality(Account *account);

    AccountSettings *accountSettings(Account *account) const;

    SettingsPage currentPage() const;

    void setCurrentPage(SettingsPage currentPage);

    void setCurrentAccount(Account *account);

    Account *currentAccount() const;

public Q_SLOTS:
    void addAccount();


Q_SIGNALS:
    void currentPageChanged();
    void currentAccountChanged();


protected:
    void setVisible(bool visible) override;


private:
    Ui::SettingsDialog *const _ui;

    QHash<Account *, AccountSettings *> _widgetForAccount;

    ActivitySettings *_activitySettings;
    ownCloudGui *_gui;
    QList<Account *> _modalStack;

    GeneralSettings *_generalSettings;
    SettingsPage _currentPage = SettingsPage::None;
    Account *_currentAccount = nullptr;
};
}
