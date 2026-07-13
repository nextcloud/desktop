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

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "account.h"
#include "accountstate.h"
#include "folder.h"
#include "folderman.h"
#include "macOS/fileprovider.h"
#include "macOS/fileprovidersettingscontroller.h"
#endif

#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>
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

#if defined(BUILD_FILE_PROVIDER_MODULE)
    if (Mac::FileProvider::available() && !Theme::instance()->disableVirtualFilesSyncFolder()) {
        const auto fpSettingsController = Mac::FileProviderSettingsController::instance();

        // "clicked" rather than "toggled": only direct user interaction may open the
        // confirmation flow, never the programmatic setChecked in loadMiscSettings().
        connect(_ui->fileProviderCheckBox, &QAbstractButton::clicked,
                this, &GeneralSettings::slotFileProviderSwitchClicked);
        connect(fpSettingsController, &Mac::FileProviderSettingsController::fileProviderModeEnabledChanged,
                this, &GeneralSettings::loadMiscSettings);
        connect(fpSettingsController, &Mac::FileProviderSettingsController::operationInProgressChanged,
                this, &GeneralSettings::loadMiscSettings);
    } else {
        // macOS 13 Ventura (feature unsupported) or branding that bans virtual files.
        _ui->fileProviderGroupBox->setVisible(false);
    }
#else
    _ui->fileProviderGroupBox->setVisible(false);
#endif

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

#if defined(BUILD_FILE_PROVIDER_MODULE)
    if (Mac::FileProvider::available()) {
        const auto fpSettingsController = Mac::FileProviderSettingsController::instance();
        const auto fpOperationInProgress = fpSettingsController->isOperationInProgress();
        _ui->fileProviderCheckBox->setChecked(fpSettingsController->fileProviderModeEnabled());
        _ui->fileProviderCheckBox->setEnabled(!fpOperationInProgress);
        _ui->fileProviderLabel->setEnabled(!fpOperationInProgress);
    }
#endif
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

void GeneralSettings::slotFileProviderSwitchClicked(bool checked)
{
#if defined(BUILD_FILE_PROVIDER_MODULE)
    const auto fpSettingsController = Mac::FileProviderSettingsController::instance();

    // The switch must not lead the actual state: snap it back immediately and only let
    // it move once the controller confirms the change via fileProviderModeEnabledChanged.
    {
        const QSignalBlocker blocker(_ui->fileProviderCheckBox);
        _ui->fileProviderCheckBox->setChecked(fpSettingsController->fileProviderModeEnabled());
    }

    if (checked == fpSettingsController->fileProviderModeEnabled()) {
        return;
    }

    if (checked) {
        confirmEnableFileProviderMode();
    } else {
        confirmDisableFileProviderMode();
    }
#else
    Q_UNUSED(checked)
#endif
}

#if defined(BUILD_FILE_PROVIDER_MODULE)
void GeneralSettings::confirmEnableFileProviderMode()
{
    // Gather the accounts whose classic sync folder connections enabling would discard.
    QStringList accountsWithClassicFolders;

    if (const auto folderMan = FolderMan::instance()) {
        const auto folderMap = folderMan->map();
        for (const auto folder : folderMap) {
            const auto accountState = folder->accountState();
            const auto account = accountState ? accountState->account() : nullptr;
            const auto accountName = account ? account->userIdAtHostWithPort() : tr("Unknown account");
            if (!accountsWithClassicFolders.contains(accountName)) {
                accountsWithClassicFolders.append(accountName);
            }
        }
    }

    auto text = tr("File Provider will be enabled for all accounts. Your files will appear in Finder under the \"Locations\" section. Accounts added later will also be set up as File Providers.");

    if (!accountsWithClassicFolders.isEmpty()) {
        QStringList accountLines;
        for (const auto &accountName : std::as_const(accountsWithClassicFolders)) {
            accountLines.append(QStringLiteral("• ") + accountName);
        }

        text += QStringLiteral("\n\n")
            + tr("This removes classic sync folder connections from the following accounts:")
            + QStringLiteral("\n\n")
            + accountLines.join(QStringLiteral("\n"))
            + QStringLiteral("\n\n")
            + tr("Synced files stay on your computer, but they will no longer be kept up to date and settings such as selective sync are discarded.");
    }

    const auto messageBox = new QMessageBox(QMessageBox::Question,
                                            tr("Enable File Provider?"),
                                            text,
                                            QMessageBox::NoButton,
                                            this);
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    const auto enableButton = messageBox->addButton(tr("Enable File Provider"), QMessageBox::AcceptRole);
    messageBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(messageBox, &QMessageBox::finished, this, [messageBox, enableButton] {
        if (messageBox->clickedButton() == enableButton) {
            Mac::FileProviderSettingsController::instance()->setFileProviderModeEnabled(true);
        }
    });
    messageBox->open();
}

void GeneralSettings::confirmDisableFileProviderMode()
{
    const auto text = tr("File Provider will be turned off for all accounts, and your files will no longer be available in Finder under the \"Locations\" section.")
        + QStringLiteral("\n\n")
        + tr("Items that were not uploaded yet will be preserved and shown to you. Classic sync folders are not set up again automatically — you can add folder sync connections afterwards in each account's settings.");

    const auto messageBox = new QMessageBox(QMessageBox::Question,
                                            tr("Disable File Provider?"),
                                            text,
                                            QMessageBox::NoButton,
                                            this);
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    const auto disableButton = messageBox->addButton(tr("Disable File Provider"), QMessageBox::AcceptRole);
    messageBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(messageBox, &QMessageBox::finished, this, [messageBox, disableButton] {
        if (messageBox->clickedButton() == disableButton) {
            Mac::FileProviderSettingsController::instance()->setFileProviderModeEnabled(false);
        }
    });
    messageBox->open();
}
#endif

void GeneralSettings::slotStyleChanged()
{
    customizeStyle();
}

void GeneralSettings::customizeStyle()
{
    SettingsPanelStyle::apply(this);

    // SettingsPanelStyle gives every row equal margins on all sides. For the File
    // Provider description we only want a left indent that matches the switch label
    // above it, and no padding on the other sides — so copy the row's left margin and
    // zero the rest. Done after apply() so it is not overwritten.
    const auto rowLeftMargin = _ui->fileProviderRow->contentsMargins().left();
    _ui->fileProviderDescriptionRow->setContentsMargins(rowLeftMargin, 0, 0, 0);
}

} // namespace OCC
