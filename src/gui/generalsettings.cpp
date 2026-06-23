/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "generalsettings.h"
#include "ui_generalsettings.h"

#include "accountmanager.h"
#include "common/utility.h"
#include "configfile.h"
#include "owncloudgui.h"
#include "settingspanelstyle.h"
#include "theme.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QScopedValueRollback>
#include <QSignalBlocker>

namespace OCC {

GeneralSettings::GeneralSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::GeneralSettings)
{
    _ui->setupUi(this);

    connect(_ui->serverNotificationsCheckBox, &QAbstractButton::toggled,
        this, &GeneralSettings::slotToggleOptionalServerNotifications);
    _ui->serverNotificationsCheckBox->setToolTip(tr("Server notifications that require attention."));

    connect(_ui->chatNotificationsCheckBox, &QAbstractButton::toggled,
            this, &GeneralSettings::slotToggleChatNotifications);
    _ui->chatNotificationsCheckBox->setToolTip(tr("Show chat notification dialogs."));

    connect(_ui->callNotificationsCheckBox, &QAbstractButton::toggled,
        this, &GeneralSettings::slotToggleCallNotifications);
    _ui->callNotificationsCheckBox->setToolTip(tr("Show call notification dialogs."));

    connect(_ui->quotaWarningNotificationsCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotToggleQuotaWarningNotifications);
    _ui->quotaWarningNotificationsCheckBox->setToolTip(tr("Show notification when quota usage exceeds 80%."));

    if (const auto hasSystemAutoStart = Utility::hasSystemLaunchOnStartup(Theme::instance()->appName())) {
        _ui->autostartCheckBox->setChecked(hasSystemAutoStart);
        _ui->autostartCheckBox->setDisabled(hasSystemAutoStart);
        _ui->autostartCheckBox->setToolTip(tr("You cannot disable autostart because system-wide autostart is enabled."));
    } else {
        connect(_ui->autostartCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotToggleLaunchOnStartup);
        _ui->autostartCheckBox->setChecked(Utility::hasLaunchOnStartup(Theme::instance()->appName()));
    }

    loadMiscSettings();

    connect(_ui->monoIconsCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);

    // accountAdded means the wizard was finished and the wizard might change some options.
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, &GeneralSettings::loadMiscSettings);

    // OEM themes are not obliged to ship mono icons, so there is no point in offering an option.
    const auto monoIconsAvailable = Theme::instance()->monoIconsAvailable();
    _ui->monoIconsCheckBox->setVisible(monoIconsAvailable);
    _ui->monoIconsLabel->setVisible(monoIconsAvailable);
    _ui->monoIconsRowWidget->setVisible(monoIconsAvailable);
    _ui->startupSeparator->setVisible(monoIconsAvailable);

    customizeStyle();
}

GeneralSettings::~GeneralSettings()
{
    delete _ui;
}

QSize GeneralSettings::sizeHint() const
{
    return {
        ownCloudGui::settingsDialogSize().width(),
        QWidget::sizeHint().height()
    };
}

void GeneralSettings::loadMiscSettings()
{
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;

    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
    _ui->serverNotificationsCheckBox->setChecked(cfgFile.optionalServerNotifications());
    _ui->chatNotificationsLabel->setEnabled(cfgFile.optionalServerNotifications());
    _ui->chatNotificationsCheckBox->setEnabled(cfgFile.optionalServerNotifications());
    _ui->chatNotificationsCheckBox->setChecked(cfgFile.showChatNotifications());
    _ui->callNotificationsLabel->setEnabled(cfgFile.optionalServerNotifications());
    _ui->callNotificationsCheckBox->setEnabled(cfgFile.optionalServerNotifications());
    _ui->callNotificationsCheckBox->setChecked(cfgFile.showCallNotifications());
    _ui->quotaWarningNotificationsLabel->setEnabled(cfgFile.optionalServerNotifications());
    _ui->quotaWarningNotificationsCheckBox->setEnabled(cfgFile.optionalServerNotifications());
    _ui->quotaWarningNotificationsCheckBox->setChecked(cfgFile.showQuotaWarningNotifications());
}

void GeneralSettings::saveMiscSettings()
{
    if (_currentlyLoading) {
        return;
    }

    const auto useMonoIcons = _ui->monoIconsCheckBox->isChecked();
    Theme::instance()->setSystrayUseMonoIcons(useMonoIcons);
    ConfigFile().setMonoIcons(useMonoIcons);
}

void GeneralSettings::slotToggleLaunchOnStartup(bool enable)
{
    const auto theme = Theme::instance();
    if (enable == Utility::hasLaunchOnStartup(theme->appName())) {
        return;
    }

    Utility::setLaunchOnStartup(theme->appName(), theme->appNameGUI(), enable);

    const auto actualState = Utility::hasLaunchOnStartup(theme->appName());
    ConfigFile().setLaunchOnSystemStartup(actualState);

    if (actualState != enable) {
        const QSignalBlocker blocker(_ui->autostartCheckBox);
        _ui->autostartCheckBox->setChecked(actualState);
    }

#ifdef Q_OS_MACOS
    if (enable && Utility::launchOnStartupRequiresApproval()) {
        QMessageBox::information(
            this,
            tr("Login Item Requires Approval"),
            tr("The login item has been registered but needs your approval to become active. "
               "Please open System Settings → General → Login Items and enable %1 there.")
                .arg(theme->appNameGUI()));
    }
#endif
}

void GeneralSettings::slotToggleOptionalServerNotifications(bool enable)
{
    ConfigFile().setOptionalServerNotifications(enable);
    _ui->chatNotificationsLabel->setEnabled(enable);
    _ui->chatNotificationsCheckBox->setEnabled(enable);
    _ui->callNotificationsLabel->setEnabled(enable);
    _ui->callNotificationsCheckBox->setEnabled(enable);
    _ui->quotaWarningNotificationsLabel->setEnabled(enable);
    _ui->quotaWarningNotificationsCheckBox->setEnabled(enable);
}

void GeneralSettings::slotToggleChatNotifications(bool enable)
{
    ConfigFile().setShowChatNotifications(enable);
}

void GeneralSettings::slotToggleCallNotifications(bool enable)
{
    ConfigFile().setShowCallNotifications(enable);
}

void GeneralSettings::slotToggleQuotaWarningNotifications(bool enable)
{
    ConfigFile().setShowQuotaWarningNotifications(enable);
}

void GeneralSettings::slotStyleChanged()
{
    customizeStyle();
}

void GeneralSettings::customizeStyle()
{
    SettingsPanelStyle::apply(this);
}

} // namespace OCC
