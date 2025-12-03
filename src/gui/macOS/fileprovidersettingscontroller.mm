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
constexpr auto fpSettingsQmlPath = "qrc:/qml/src/gui/macOS/FileProviderSettings.qml";

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
    const auto accManager = AccountManager::instance();
    const auto accountsList = accManager->accounts();

    for (const auto &accountState : accountsList) {
        const auto userInfo = new UserInfo(accountState.data(), false, false, this);
        const auto account = accountState->account();
        const auto accountUserIdAtHost = account->userIdAtHostWithPort();

        _userInfos.insert(accountUserIdAtHost, userInfo);
        userInfo->setActive(true);
    }

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

bool FileProviderSettingsController::vfsEnabledForAccount(const QString &userIdAtHost) const
{
    NSArray<NSString *> *const vfsEnabledAccounts = static_cast<NSArray<NSString *> *>(nsEnabledAccounts());
    return [vfsEnabledAccounts containsObject:userIdAtHost.toNSString()];
}

void FileProviderSettingsController::setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled, const bool showInformationDialog)
{
    const auto enabledAccountsAction = setVfsEnabledForAccountImpl(userIdAtHost, setEnabled);
    if (enabledAccountsAction == VfsAccountsAction::VfsAccountsEnabledChanged) {
        emit vfsEnabledAccountsChanged();

        if (setEnabled && showInformationDialog) {
            QMessageBox::information(nullptr, tr("Virtual files enabled"),
                                     tr("Virtual files have been enabled for this account.\n\n"
                                        "Your files are now accessible in Finder under the \"Locations\" section."));
        }
    }
}

void *FileProviderSettingsController::nsEnabledAccounts() const
{
    NSUserDefaults *userDefaults = static_cast<NSUserDefaults *>(_userDefaults);
    NSString *accountsKey = static_cast<NSString *>(_accountsKey);
    return [userDefaults objectForKey:accountsKey];
}

FileProviderSettingsController::VfsAccountsAction FileProviderSettingsController::setVfsEnabledForAccountImpl(const QString &userIdAtHost, const bool setEnabled)
{
    NSUserDefaults *userDefaults = static_cast<NSUserDefaults *>(_userDefaults);
    NSString *accountsKey = static_cast<NSString *>(_accountsKey);
    NSArray<NSString *> *vfsEnabledAccounts = static_cast<NSArray<NSString *> *>(nsEnabledAccounts());

    qCInfo(lcFileProviderSettingsController) << "Setting file provider-based vfs of account"
                                             << userIdAtHost
                                             << "to"
                                             << setEnabled;

    if (vfsEnabledAccounts == nil) {
        qCDebug(lcFileProviderSettingsController) << "Received nil array for accounts, creating new array";
        vfsEnabledAccounts = NSArray.array;
    }

    NSString *const nsUserIdAtHost = userIdAtHost.toNSString();
    const BOOL accountEnabled = [vfsEnabledAccounts containsObject:nsUserIdAtHost];

    if (accountEnabled == setEnabled) {
        qCDebug(lcFileProviderSettingsController) << "VFS enablement status for"
                                                  << userIdAtHost
                                                  << "matches config.";
        return VfsAccountsAction::VfsAccountsNoAction;
    }

    NSMutableArray<NSString *> *const mutableVfsAccounts = vfsEnabledAccounts.mutableCopy;

    if (setEnabled) {
        [mutableVfsAccounts addObject:nsUserIdAtHost];
    } else {
        [mutableVfsAccounts removeObject:nsUserIdAtHost];
    }

    NSArray<NSString *> *const modifiedVfsAccounts = mutableVfsAccounts.copy;
    [userDefaults setObject:modifiedVfsAccounts forKey:accountsKey];

    Q_ASSERT(vfsEnabledForAccount(userIdAtHost) == setEnabled);

    return VfsAccountsAction::VfsAccountsEnabledChanged;
}

void FileProviderSettingsController::setupPersistentMainAppService()
{
    // TODO: Implement FileProviderXPCUtils functions for XPC connection management
    // The following code is disabled until the XPC utility functions are implemented
    
    qCInfo(lcFileProviderSettingsController) << "XPC connection setup not yet implemented.";
    
    /* Original code that needs FileProviderXPCUtils implementation:
    
    using namespace OCC::Mac::FileProviderXPCUtils;
    qCInfo(lcFileProviderSettingsController) << "Setting up persistent MainAppService XPC connections.";
    NSArray<NSFileProviderDomainIdentifier> *domainIdentifiers = getDomainIdentifiers();

    if (domainIdentifiers.count == 0) {
        qCWarning(lcFileProviderSettingsController) << "No File Provider domain identifiers found; skipping XPC setup.";
        return;
    }

    NSDictionary<NSFileProviderDomainIdentifier, NSFileProviderService *> *services = getFileProviderServices(domainIdentifiers);

    if (services.count == 0) {
        qCWarning(lcFileProviderSettingsController) << "No File Provider services found; skipping XPC setup.";
        return;
    }

    NSDictionary<NSFileProviderDomainIdentifier, NSXPCConnection *> *connections = connectToFileProviderServices(services);
    if (connections.count == 0) {
        qCWarning(lcFileProviderSettingsController) << "No XPC connections obtained; skipping XPC setup.";
        return;
    }

    // Configure connections (sets exported interface/object and resumes), and keep them alive.
    for (NSFileProviderDomainIdentifier domainId in connections) {
        NSXPCConnection *connection = connections[domainId];
        configureFileProviderConnection(connection);
        NSXPCConnection *nsConnection = static_cast<NSXPCConnection *>(connection);
        _serviceConnections.append(nsConnection);
    }

    // Optionally initialize remote client communication proxies to keep them active.
    [[maybe_unused]] auto proxies = processClientCommunicationConnections(connections);
    Q_UNUSED(proxies);
    */
}

} // namespace Mac

} // namespace OCC
