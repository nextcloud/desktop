/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileprovidersettingscontroller.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QQmlApplicationEngine>

#include "accountmanager.h"
#include "gui/systray.h"
#include "gui/userinfo.h"

// Objective-C imports
#import <Foundation/Foundation.h>
// End of Objective-C imports

namespace {
constexpr auto fpSettingsQmlPath = "qrc:/qml/src/gui/macOS/ui/FileProviderSettings.qml";

// QML properties -- make sure they match up in QML file!
constexpr auto fpAccountUserIdAtHostProp = "accountUserIdAtHost";

// NSUserDefaults entries
constexpr auto enabledAccountsSettingsKey = "enabledAccounts";

} // namespace

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSettingsController, "nextcloud.gui.mac.fileprovider.settingscontroller")

FileProviderSettingsController *FileProviderSettingsController::instance()
{
    static FileProviderSettingsController controller;
    return &controller;
}

FileProviderSettingsController::FileProviderSettingsController(QObject *parent)
    : QObject{parent}
    , _userDefaults(NSUserDefaults.standardUserDefaults)
    , _accountsKey([NSString stringWithUTF8String:enabledAccountsSettingsKey])
{
    setupPersistentMainAppService();
}

QQuickWidget *FileProviderSettingsController::settingsViewWidget(const QString &accountUserIdAtHost,
                                                                 QWidget *const parent,
                                                                 const QQuickWidget::ResizeMode resizeMode)
{
    const auto settingsViewWidget = new QQuickWidget(Systray::instance()->trayEngine(), parent);
    settingsViewWidget->setResizeMode(resizeMode);
    settingsViewWidget->setSource(QUrl(fpSettingsQmlPath));
    settingsViewWidget->rootObject()->setProperty(fpAccountUserIdAtHostProp, accountUserIdAtHost);
    return settingsViewWidget;
}

bool FileProviderSettingsController::isFileProviderEnabledForAccount(const QString &userIdAtHost) const
{
    NSArray<NSString *> *const enabledAccounts = static_cast<NSArray<NSString *> *>(accountsInUserDefaultsEnabledForFileProviders());
    return [enabledAccounts containsObject:userIdAtHost.toNSString()];
}

void FileProviderSettingsController::setFileProviderEnabledForAccount(const QString &userIdAtHost, const bool setEnabled, const bool showInformationDialog)
{
    const auto enabledAccountsAction = setFileProviderEnabledForAccountImpl(userIdAtHost, setEnabled);

    if (enabledAccountsAction == FileProviderAction::EnabledChanged) {
        emit enabledAccountsChanged();

        if (setEnabled && showInformationDialog) {
            QMessageBox::information(nullptr, tr("Virtual files enabled"),
                                     tr("Virtual files have been enabled for this account.\n\n"
                                        "Your files are now accessible in Finder under the \"Locations\" section."));
        }
    }
}

void *FileProviderSettingsController::accountsInUserDefaultsEnabledForFileProviders() const
{
    NSUserDefaults *userDefaults = static_cast<NSUserDefaults *>(_userDefaults);
    NSString *accountsKey = static_cast<NSString *>(_accountsKey);
    return [userDefaults objectForKey:accountsKey];
}

FileProviderSettingsController::FileProviderAction FileProviderSettingsController::setFileProviderEnabledForAccountImpl(const QString &userIdAtHost, const bool setEnabled)
{
    NSUserDefaults *userDefaults = static_cast<NSUserDefaults *>(_userDefaults);
    NSString *accountsKey = static_cast<NSString *>(_accountsKey);
    NSArray<NSString *> *enabledAccounts = static_cast<NSArray<NSString *> *>(accountsInUserDefaultsEnabledForFileProviders());

    qCInfo(lcFileProviderSettingsController) << "Changing file provider enablement for"
                                             << userIdAtHost
                                             << "to"
                                             << setEnabled;

    if (enabledAccounts == nil) {
        qCDebug(lcFileProviderSettingsController) << "Received nil array for accounts, creating new array.";
        enabledAccounts = NSArray.array;
    }

    NSString *const nsUserIdAtHost = userIdAtHost.toNSString();
    const BOOL accountEnabled = [enabledAccounts containsObject:nsUserIdAtHost];

    if (accountEnabled == setEnabled) {
        qCDebug(lcFileProviderSettingsController) << "File provider enablement status for"
                                                  << userIdAtHost
                                                  << "matches statet in user defaults.";
        return FileProviderAction::NoAction;
    }

    NSMutableArray<NSString *> *const mutableAccounts = enabledAccounts.mutableCopy;

    if (setEnabled) {
        [mutableAccounts addObject:nsUserIdAtHost];
    } else {
        [mutableAccounts removeObject:nsUserIdAtHost];
    }

    NSArray<NSString *> *const modifiedAccounts = mutableAccounts.copy;
    [userDefaults setObject:modifiedAccounts forKey:accountsKey];

    Q_ASSERT(isFileProviderEnabledForAccount(userIdAtHost) == setEnabled);

    return FileProviderAction::EnabledChanged;
}

void FileProviderSettingsController::setupPersistentMainAppService()
{
    qCInfo(lcFileProviderSettingsController) << "XPC connection setup not yet implemented.";
}

} // namespace Mac

} // namespace OCC
