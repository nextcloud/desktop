/*
 * Copyright (C) by Denis Dzyubenko
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

#include "settingsdialogmac.h"

#include "macstandardicon.h"

#include "folderman.h"
#include "theme.h"
#include "generalsettings.h"
#include "networksettings.h"
#include "accountsettings.h"
#include "configfile.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "protocolwidget.h"
#include "accountmanager.h"

#include <QLabel>
#include <QStandardItemModel>
#include <QPushButton>
#include <QDebug>
#include <QSettings>

namespace OCC {

//
// Whenever you change something here check both settingsdialog.cpp and settingsdialogmac.cpp !
//
SettingsDialogMac::SettingsDialogMac(ownCloudGui *gui, QWidget *parent)
    : MacPreferencesWindow(parent), _gui(gui)
{
    // do not show minimize button. There is no use, and retoring the
    // dialog from minimize is broken in MacPreferencesWindow
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint |
                   Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint);


    // Emulate dialog behavior: Escape means close
    QAction *closeDialogAction = new QAction(this);
    closeDialogAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(closeDialogAction, &QAction::triggered, this, &SettingsDialogMac::close);
    addAction(closeDialogAction);
    // People perceive this as a Window, so also make Ctrl+W work
    QAction *closeWindowAction = new QAction(this);
    closeWindowAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeWindowAction, &QAction::triggered, this, &SettingsDialogMac::close);
    addAction(closeWindowAction);
    // People perceive this as a Window, so also make Ctrl+H work
    QAction *hideWindowAction = new QAction(this);
    hideWindowAction->setShortcut(QKeySequence("Ctrl+H"));
    connect(hideWindowAction, &QAction::triggered, this, &SettingsDialogMac::hide);
    addAction(hideWindowAction);

    setObjectName("SettingsMac"); // required as group for saveGeometry call

    setWindowTitle(tr("%1").arg(Theme::instance()->appNameGUI()));

    connect(AccountManager::instance(), &AccountManager::accountAdded,
            this, &SettingsDialogMac::accountAdded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
            this, &SettingsDialogMac::accountRemoved);
    foreach (auto ai , AccountManager::instance()->accounts()) {
        accountAdded(ai.data());
    }

    QIcon protocolIcon(QLatin1String(":/client/resources/activity.png"));
    _protocolWidget = new ProtocolWidget;
    _protocolIdx = addPreferencesPanel(protocolIcon, tr("Activity"), _protocolWidget);

    QIcon generalIcon = MacStandardIcon::icon(MacStandardIcon::PreferencesGeneral);
    GeneralSettings *generalSettings = new GeneralSettings;
    addPreferencesPanel(generalIcon, tr("General"), generalSettings);

    QIcon networkIcon = MacStandardIcon::icon(MacStandardIcon::Network);
    NetworkSettings *networkSettings = new NetworkSettings;
    addPreferencesPanel(networkIcon, tr("Network"), networkSettings);

    QAction *showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, &QAction::triggered, gui, &ownCloudGui::slotToggleLogBrowser);
    addAction(showLogWindow);

    ConfigFile cfg;
    cfg.restoreGeometry(this);
}

void SettingsDialogMac::closeEvent(QCloseEvent *event)
{
    ConfigFile cfg;
    cfg.saveGeometry(this);
    MacPreferencesWindow::closeEvent(event);
}

void SettingsDialogMac::showActivityPage()
{
    setCurrentPanelIndex(_protocolIdx);
}

void SettingsDialogMac::accountAdded(AccountState *s)
{
    QIcon accountIcon = MacStandardIcon::icon(MacStandardIcon::UserAccounts);
    auto accountSettings = new AccountSettings(s, this);
    //FIXME: add at the begining: (and don(t foget to adjust for _protocolIdx)
    addPreferencesPanel(accountIcon, s->account()->displayName(), accountSettings);

    connect( accountSettings, &AccountSettings::folderChanged, _gui,  &ownCloudGui::slotFoldersChanged);
    connect( accountSettings, &AccountSettings::openFolderAlias, _gui, &ownCloudGui::slotFolderOpenAction);
}

void SettingsDialogMac::accountRemoved(AccountState *s)
{
    // FIXME: is it the correct way to remove a panel?
    auto list = findChildren<AccountSettings*>(QString(), Qt::FindDirectChildrenOnly);
    foreach(auto p, list) {
        if (p->accountsState() == s) {
            p->deleteLater();
        }
    }
}


}
