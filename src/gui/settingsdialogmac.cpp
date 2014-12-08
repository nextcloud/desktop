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

#include <QLabel>
#include <QStandardItemModel>
#include <QPushButton>
#include <QDebug>
#include <QSettings>

namespace OCC {

SettingsDialogMac::SettingsDialogMac(ownCloudGui *gui, QWidget *parent)
    : MacPreferencesWindow(parent)
{
    // do not show minimize button. There is no use, and retoring the
    // dialog from minimize is broken in MacPreferencesWindow
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint |
                   Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint);


    // Emulate dialog behavior: Escape means close
    QAction *closeDialogAction = new QAction(this);
    closeDialogAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(closeDialogAction, SIGNAL(triggered()), SLOT(close()));
    addAction(closeDialogAction);
    // People perceive this as a Window, so also make Ctrl+W work
    QAction *closeWindowAction = new QAction(this);
    closeWindowAction->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeWindowAction, SIGNAL(triggered()), SLOT(close()));
    addAction(closeWindowAction);
    // People perceive this as a Window, so also make Ctrl+H work
    QAction *hideWindowAction = new QAction(this);
    hideWindowAction->setShortcut(QKeySequence("Ctrl+H"));
    connect(hideWindowAction, SIGNAL(triggered()), SLOT(hide()));
    addAction(hideWindowAction);

    setObjectName("SettingsMac"); // required as group for saveGeometry call

    setWindowTitle(tr("%1").arg(Theme::instance()->appNameGUI()));

    _accountSettings = new AccountSettings;
    QIcon accountIcon(QLatin1String(":/client/resources/accounts.png"));
    addPreferencesPanel(accountIcon, tr("Account"), _accountSettings);

    QIcon protocolIcon(QLatin1String(":/client/resources/activity.png"));
    _protocolWidget = new ProtocolWidget;
    _protocolIdx = addPreferencesPanel(protocolIcon, tr("Activity"), _protocolWidget);

    QIcon generalIcon = MacStandardIcon::icon(MacStandardIcon::PreferencesGeneral);
    GeneralSettings *generalSettings = new GeneralSettings;
    addPreferencesPanel(generalIcon, tr("General"), generalSettings);

    QIcon networkIcon = MacStandardIcon::icon(MacStandardIcon::Network);
    NetworkSettings *networkSettings = new NetworkSettings;
    addPreferencesPanel(networkIcon, tr("Network"), networkSettings);

    FolderMan *folderMan = FolderMan::instance();
    connect( folderMan, SIGNAL(folderSyncStateChange(QString)),
             this, SLOT(slotSyncStateChange(QString)));

    connect( ProgressDispatcher::instance(), SIGNAL(progressInfo(QString, Progress::Info)),
             _accountSettings, SLOT(slotSetProgress(QString, Progress::Info)) );

    QAction *showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, SIGNAL(triggered()), gui, SLOT(slotToggleLogBrowser()));
    addAction(showLogWindow);

    ConfigFile cfg;
    cfg.restoreGeometry(this);
}

void SettingsDialogMac::slotSyncStateChange(const QString& alias)
{
    FolderMan *folderMan = FolderMan::instance();

    Folder *folder = folderMan->folder(alias);
    if( folder ) {
        _accountSettings->slotUpdateFolderState(folder);
    }
}

void SettingsDialogMac::setGeneralErrors(const QStringList &errors)
{
    _accountSettings->setGeneralErrors(errors);
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

}
