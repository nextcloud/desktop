/*
 * Copyright (C) by Denis Dzyubenko
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

#include "settingsdialogmac.h"

#include "macstandardicon.h"

#include "folderman.h"
#include "theme.h"
#include "generalsettings.h"
#include "networksettings.h"
#include "accountsettings.h"
#include "accountstate.h"
#include "creds/abstractcredentials.h"
#include "configfile.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "activitywidget.h"
#include "accountmanager.h"

#include <QLabel>
#include <QStandardItemModel>
#include <QPushButton>
#include <QSettings>
#include <QPainter>
#include <QPainterPath>

namespace OCC {

#include "settingsdialogcommon.cpp"


// Duplicate in settingsdialog.cpp
static QIcon circleMask(const QImage &avatar)
{
    int dim = avatar.width();

    QPixmap fixedImage(dim, dim);
    fixedImage.fill(Qt::transparent);

    QPainter imgPainter(&fixedImage);
    QPainterPath clip;
    clip.addEllipse(0, 0, dim, dim);
    imgPainter.setClipPath(clip);
    imgPainter.drawImage(0, 0, avatar);
    imgPainter.end();

    return QIcon(fixedImage);
}


//
// Whenever you change something here check both settingsdialog.cpp and settingsdialogmac.cpp !
//
SettingsDialogMac::SettingsDialogMac(ownCloudGui *gui, QWidget *parent)
    : MacPreferencesWindow(parent)
    , _gui(gui)
{
    // do not show minimize button. There is no use, and restoring the
    // dialog from minimize is broken in MacPreferencesWindow
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint);

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

    _actionsIdx = -1;
    foreach (auto ai, AccountManager::instance()->accounts()) {
        accountAdded(ai.data());
    }

    _actionBefore = new QAction;
    addAction(_actionBefore);

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
    // Count backwards (0-based) from the last panel (multiple accounts can be on the left)
    setCurrentPanelIndex(preferencePanelCount() - 1 - 2);
}

void SettingsDialogMac::accountAdded(AccountState *s)
{
    QIcon accountIcon = MacStandardIcon::icon(MacStandardIcon::UserAccounts);
    auto accountSettings = new AccountSettings(s, this);
    QString displayName = Theme::instance()->multiAccount() ? SettingsDialogCommon::shortDisplayNameForSettings(s->account().data(), 0) : tr("Account");

    // if this is not the first account, then before we continue to add more accounts we add a separator
    if(AccountManager::instance()->accounts().first().data() != s &&
        AccountManager::instance()->accounts().size() >= 1){
        // in front of the action before
        ++_actionsIdx;
        _separators.insert(_actionsIdx, insertSeparator(_actionsIdx));
    }

    // this adds the panel - nothing to add here just to fix the order
    insertPreferencesPanel(++_actionsIdx, accountIcon, displayName, accountSettings);

    connect(accountSettings, &AccountSettings::folderChanged, _gui, &ownCloudGui::slotFoldersChanged);
    connect(accountSettings, &AccountSettings::openFolderAlias, _gui, &ownCloudGui::slotFolderOpenAction);

    connect(s->account().data(), &Account::accountChangedAvatar, this, &SettingsDialogMac::slotAccountAvatarChanged);
    connect(s->account().data(), &Account::accountChangedDisplayName, this, &SettingsDialogMac::slotAccountDisplayNameChanged);

    // Refresh immediatly when getting online
    connect(s, &AccountState::isConnectedChanged, this, &SettingsDialogMac::slotRefreshActivityAccountStateSender);

    // Add activity panel
    QIcon activityIcon(QLatin1String(":/client/resources/activity.png"));
    _activitySettings[s] = new ActivitySettings(s, this);
    insertPreferencesPanel(++_actionsIdx, activityIcon, tr("Activity"), _activitySettings[s]);

    connect(_activitySettings[s], SIGNAL(guiLog(QString, QString)), _gui,
        SLOT(slotShowOptionalTrayMessage(QString, QString)));

    ConfigFile cfg;
    _activitySettings[s]->setNotificationRefreshInterval(cfg.notificationRefreshInterval());

    slotRefreshActivity(s);
}

void SettingsDialogMac::accountRemoved(AccountState *s)
{
    auto list = findChildren<AccountSettings *>(QString());
    foreach (auto p, list) {
        if (p->accountsState() == s) {
            removePreferencesPanel(p);
            int idx = indexForPanel(_activitySettings[s]);
            removePreferencesPanel(_activitySettings[s]);
            _activitySettings[s]->slotRemoveAccount();
            _activitySettings.remove(s);

            // remove separator if there is any
            ++idx;
            if(idx < _separators.size()){
              if(_separators.at(idx) != nullptr){
                _separators.at(idx)->setVisible(false);
                removeSeparator(_separators.at(idx));
                //_separators.removeAt(idx++);
               }
            }
        }
    }
}

void SettingsDialogMac::slotRefreshActivityAccountStateSender()
{
    slotRefreshActivity(qobject_cast<AccountState*>(sender()));
}

void SettingsDialogMac::slotRefreshActivity(AccountState *accountState)
{
    if (accountState) {
        _activitySettings[accountState]->slotRefresh();
    }
}

void SettingsDialogMac::slotAccountAvatarChanged()
{
    Account *account = static_cast<Account *>(sender());
    auto list = findChildren<AccountSettings *>(QString());
    foreach (auto p, list) {
        if (p->accountsState()->account() == account) {
            int idx = indexForPanel(p);
            QImage pix = account->avatar();
            if (!pix.isNull()) {
                setPreferencesPanelIcon(idx, circleMask(pix));
            }
        }
    }
}

void SettingsDialogMac::slotAccountDisplayNameChanged()
{
    Account *account = static_cast<Account *>(sender());
    auto list = findChildren<AccountSettings *>(QString());
    foreach (auto p, list) {
        if (p->accountsState()->account() == account) {
            int idx = indexForPanel(p);
            QString displayName = account->displayName();
            if (!displayName.isNull()) {
                displayName = Theme::instance()->multiAccount()
                        ? SettingsDialogCommon::shortDisplayNameForSettings(account, 0)
                        : tr("Account");
                setPreferencesPanelTitle(idx, displayName);
            }
        }
    }
}

}

