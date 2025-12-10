/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileprovidersettingscontroller.h"

#include <QFileDialog>
#include <QQmlApplicationEngine>

#include "gui/accountmanager.h"
#include "gui/systray.h"
#include "gui/userinfo.h"
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileprovideritemmetadata.h"
#include "gui/macOS/fileproviderutils.h"
#include "libsync/configfile.h"

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

class FileProviderSettingsController::MacImplementation : public QObject
{
public:
    enum class VfsAccountsAction {
        VfsAccountsNoAction,
        VfsAccountsEnabledChanged,
    };

    explicit MacImplementation(FileProviderSettingsController *const parent)
    {
        q = parent;
    };

    ~MacImplementation() override = default;

    [[nodiscard]] QStringList enabledAccounts() const
    {
        QStringList qEnabledAccounts;
        const auto accountStates = AccountManager::instance()->accounts();

        for (const auto &accountState : accountStates) {
            const auto account = accountState->account();
            const auto domainId = account->fileProviderDomainIdentifier();

            if (domainId.isEmpty()) {
                continue;
            }

            qEnabledAccounts.append(account->userIdAtHostWithPort());
        }

        return qEnabledAccounts;
    }

    [[nodiscard]] bool vfsEnabledForAccount(const QString &userIdAtHost) const
    {
        return q->fileProviderDomainIdentifierForAccount(userIdAtHost).isEmpty() == false;
    }

    [[nodiscard]] VfsAccountsAction setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled) const
    {
        qCInfo(lcFileProviderSettingsController) << "Setting file provider enablement of account"
                                                 << userIdAtHost
                                                 << "to"
                                                 << setEnabled;

        const auto existingDomainId = q->fileProviderDomainIdentifierForAccount(userIdAtHost);

        if (setEnabled && existingDomainId.isEmpty() == false) {
            qCWarning(lcFileProviderSettingsController) << "Cancelling because account already has a domain identifier!" << userIdAtHost;
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        const auto accountManager = AccountManager::instance();
        const auto accountState = accountManager->accountFromUserId(userIdAtHost);

        if (!accountState) {
            qCWarning(lcFileProviderSettingsController) << "Unable to set file provider enablement, account not found!" << userIdAtHost;
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        if (setEnabled) {
            auto const identifier = Mac::FileProvider::instance()->domainManager()->addFileProviderDomainForAccount(accountState.data());
            accountManager->setFileProviderDomainIdentifier(userIdAtHost, identifier);
        } else {
            Mac::FileProvider::instance()->domainManager()->removeFileProviderDomainForAccount(accountState.data());
            accountManager->setFileProviderDomainIdentifier(userIdAtHost, "");
        }

        return VfsAccountsAction::VfsAccountsEnabledChanged;
    }

    void migrateEnabledAccountsFromUserDefaults()
    {
        ConfigFile cfg;

        if (cfg.fileProviderDomainsAppSandboxMigrationCompleted()) {
            return;
        }

        qCInfo(lcFileProviderSettingsController) << "Migrating VFS enabled accounts from user defaults.";

        const auto userDefaults = NSUserDefaults.standardUserDefaults;
        NSString *const accountsKey = [NSString stringWithUTF8String:enabledAccountsSettingsKey];
        NSArray<NSString *> *const legacyEnabledAccounts = (NSArray<NSString *> *)[userDefaults objectForKey:accountsKey];

        const auto accManager = AccountManager::instance();
        const auto accountsList = accManager->accounts();

        for (const auto &accountState : accountsList) {
            const auto account = accountState->account();
            const auto accountIdentifier = account->userIdAtHostWithPort();
            const bool accountEnabled = legacyEnabledAccounts && [legacyEnabledAccounts containsObject:accountIdentifier.toNSString()];

            if (accountEnabled == false) {
                continue;
            }

            auto const identifier = Mac::FileProvider::instance()->domainManager()->addFileProviderDomainForAccount(accountState.data());
            accManager->setFileProviderDomainIdentifier(accountIdentifier, identifier);
        }

        // Cleanup migrated legacy preference to avoid reprocessing
        [userDefaults removeObjectForKey:accountsKey];
    }

private:
    [[maybe_unused]] FileProviderSettingsController *q = nullptr;
};

void FileProviderSettingsController::migrateEnabledAccountsFromUserDefaults()
{
    if (d) {
        d->migrateEnabledAccountsFromUserDefaults();
    }
}

FileProviderSettingsController *FileProviderSettingsController::instance()
{
    static FileProviderSettingsController controller;
    return &controller;
}

FileProviderSettingsController::FileProviderSettingsController(QObject *parent)
    : QObject{parent}
    , d(new FileProviderSettingsController::MacImplementation(this))
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

QStringList FileProviderSettingsController::vfsEnabledAccounts() const
{
    return d->enabledAccounts();
}

bool FileProviderSettingsController::vfsEnabledForAccount(const QString &userIdAtHost) const
{
    return d->vfsEnabledForAccount(userIdAtHost);
}

void FileProviderSettingsController::setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled, const bool showInformationDialog)
{
    const auto enabledAccountsAction = d->setVfsEnabledForAccount(userIdAtHost, setEnabled);
    if (enabledAccountsAction == MacImplementation::VfsAccountsAction::VfsAccountsEnabledChanged) {
        emit vfsEnabledAccountsChanged();

        if (setEnabled && showInformationDialog) {
            QMessageBox::information(nullptr, tr("Virtual files enabled"),
                                     tr("Virtual files have been enabled for this account.\n\n"
                                        "Your files are now accessible in Finder under the \"Locations\" section."));
        }
    }
}

bool FileProviderSettingsController::trashDeletionEnabledForAccount(const QString &userIdAtHost) const
{
    if (userIdAtHost.isEmpty()) {
        return false;
    }

    const auto xpc = FileProvider::instance()->xpc();

    if (!xpc) {
        return true;
    }

    const auto domainId = fileProviderDomainIdentifierForAccount(userIdAtHost);

    if (domainId.isEmpty()) {
        return false;
    }

    if (const auto trashDeletionState = xpc->trashDeletionEnabledStateForFileProviderDomain(domainId)) {
        return trashDeletionState->first;
    }

    return true;
}

bool FileProviderSettingsController::trashDeletionSetForAccount(const QString &userIdAtHost) const
{
    const auto xpc = FileProvider::instance()->xpc();

    if (!xpc) {
        return false;
    }

    const auto domainId = fileProviderDomainIdentifierForAccount(userIdAtHost);

    if (domainId.isEmpty()) {
        return false;
    }

    if (const auto state = xpc->trashDeletionEnabledStateForFileProviderDomain(domainId)) {
        return state->second;
    }

    return false;
}

void FileProviderSettingsController::setTrashDeletionEnabledForAccount(const QString &userIdAtHost, const bool setEnabled)
{
    const auto xpc = FileProvider::instance()->xpc();

    if (!xpc) {
        // Reset state of UI elements
        emit trashDeletionEnabledForAccountChanged(userIdAtHost);
        emit trashDeletionSetForAccountChanged(userIdAtHost);
        return;
    }

    const auto domainId = fileProviderDomainIdentifierForAccount(userIdAtHost);

    if (domainId.isEmpty()) {
        return;
    }

    xpc->setTrashDeletionEnabledForFileProviderDomain(domainId, setEnabled);

    emit trashDeletionEnabledForAccountChanged(userIdAtHost);
    emit trashDeletionSetForAccountChanged(userIdAtHost);
}

QString FileProviderSettingsController::fileProviderDomainIdentifierForAccount(const QString &userIdAtHost) const
{
    const auto accountState = AccountManager::instance()->accountFromUserId(userIdAtHost);

    if (!accountState) {
        qCWarning(lcFileProviderSettingsController) << "Account not found for user" << userIdAtHost;
        return {};
    }

    const auto account = accountState->account();

    if (!account) {
        qCWarning(lcFileProviderSettingsController) << "Account missing in state for user" << userIdAtHost;
        return {};
    }

    return account->fileProviderDomainIdentifier();
}

} // namespace Mac

} // namespace OCC
