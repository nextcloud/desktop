/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileprovidersettingscontroller.h"

#include <QDesktopServices>
#include <QDir>
#include <QList>
#include <QQmlApplicationEngine>
#include <QUrl>
#include <QMessageBox>

#include "gui/accountmanager.h"
#include "gui/systray.h"
#include "gui/userinfo.h"
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileprovideritemmetadata.h"
#include "gui/macOS/fileproviderutils.h"
#include "libsync/configfile.h"

// Objective-C imports
#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>
#import <AppKit/AppKit.h>
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

        migrateToAppSandbox();
        removeOrphanedDomains();
        restoreMissingDomains();
        Mac::FileProvider::instance()->domainManager()->reconnectAll();
        Mac::FileProvider::instance()->configureXPC();
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
            auto const identifier = Mac::FileProvider::instance()->domainManager()->addDomainForAccount(accountState.data());
            accountManager->setFileProviderDomainIdentifier(userIdAtHost, identifier);
        } else {
            // Check if the extension has dirty user data before removing the domain.
            const auto xpc = Mac::FileProvider::instance()->xpc();

            if (xpc && xpc->fileProviderDomainHasDirtyUserData(existingDomainId)) {
                qCWarning(lcFileProviderSettingsController) << "File provider domain" << existingDomainId << "has dirty user data.";

                // Remove the domain and get the URL where preserved user data is located
                const auto preservedDataUrl = Mac::FileProvider::instance()->domainManager()->removeDomainByAccount(accountState.data());

                if (!preservedDataUrl.isEmpty()) {
                    // UI operations must be dispatched to main queue
                    // Copy the URL to ensure it's valid in the block
                    const QString capturedUrl = preservedDataUrl;
                    dispatch_async(dispatch_get_main_queue(), ^{
                        QMessageBox::warning(nullptr,
                                           q->tr("Unsynchronized Content"),
                                           q->tr("Some of your locally changed items were not uploaded yet but will be preserved."));
                        NSURL *url = [NSURL fileURLWithPath:capturedUrl.toNSString() isDirectory:YES];
                        qCDebug(lcFileProviderSettingsController) << "Opening directory in file viewer:" << url.path;
                        [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
                    });
                } else {
                    qCWarning(lcFileProviderSettingsController) << "Could not get preserved data URL for domain" << existingDomainId;
                }
            } else {
                Mac::FileProvider::instance()->domainManager()->removeDomainByAccount(accountState.data());
            }

            accountManager->setFileProviderDomainIdentifier(userIdAtHost, "");
        }

        Mac::FileProvider::instance()->configureXPC();

        return VfsAccountsAction::VfsAccountsEnabledChanged;
    }

    void migrateToAppSandbox()
    {
        ConfigFile cfg;

        if (cfg.fileProviderDomainsAppSandboxMigrationCompleted()) {
            qCInfo(lcFileProviderSettingsController) << "Migration app sandbox was already completed earlier.";
            return;
        }

        qCInfo(lcFileProviderSettingsController) << "Starting migration to app sandbox...";
        qCInfo(lcFileProviderSettingsController) << "Removing file provider domain to account mappings from configuration file...";

        cfg.removeFileProviderDomainMapping();

        qCInfo(lcFileProviderSettingsController) << "Removing all existing file provider domains...";

        Mac::FileProvider::instance()->domainManager()->removeAllDomains();

        qCInfo(lcFileProviderSettingsController) << "Migrating user defaults...";

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
                qCInfo(lcFileProviderSettingsController) << "Skipping file provider disabled account" << accountIdentifier;
                continue;
            } else {
                qCInfo(lcFileProviderSettingsController) << "Migrating file provider enabled account" << accountIdentifier;
            }

            auto const identifier = QUuid::createUuid().toString(QUuid::WithoutBraces);
            qCInfo(lcFileProviderSettingsController) << "Assigning temporary fake file provider domain identifier" << identifier;
            accManager->setFileProviderDomainIdentifier(accountIdentifier, identifier);
        }

        // Cleanup migrated legacy preference to avoid reprocessing
        [userDefaults removeObjectForKey:accountsKey];
        cfg.setFileProviderDomainsAppSandboxMigrationCompleted(true);
        qCInfo(lcFileProviderSettingsController) << "App sandbox migration completed.";
    }

    void restoreMissingDomains()
    {
        qCInfo(lcFileProviderSettingsController) << "Restoring missing domains...";

        const auto domains = Mac::FileProvider::instance()->domainManager()->getDomains();
        const auto accountStates = AccountManager::instance()->accounts();
        QSet<QString> existingDomainIdentifiers;

        for (NSFileProviderDomain * const domain : domains) {
            existingDomainIdentifiers.insert(QString::fromNSString(domain.identifier));
        }

        for (const auto &accountState : accountStates) {
            const auto account = accountState->account();

            if (!account) {
                continue;
            }

            const auto identifier = account->fileProviderDomainIdentifier();

            if (identifier.isEmpty()) {
                continue;
            }

            if (existingDomainIdentifiers.contains(identifier) == false) {
                qCInfo(lcFileProviderSettingsController) << "Restoring missing domain with identifier"
                                                       << identifier
                                                       << "for account"
                                                       << account->userIdAtHostWithPort();

                const auto newIdentifier = Mac::FileProvider::instance()->domainManager()->addDomainForAccount(accountState.data());

                if (newIdentifier.isEmpty() == false) {
                    AccountManager::instance()->setFileProviderDomainIdentifier(account->userIdAtHostWithPort(), newIdentifier);
                }
            } else {
                qCInfo(lcFileProviderSettingsController) << "Domain"
                                                       << identifier
                                                       << "for account"
                                                       << account->userIdAtHostWithPort()
                                                       << "is still there.";
            }
        }

        qCInfo(lcFileProviderSettingsController) << "Finished restoring missing domains.";
    }

    void removeOrphanedDomains()
    {
        qCInfo(lcFileProviderSettingsController) << "Removing orphaned domains...";

        const auto domains = Mac::FileProvider::instance()->domainManager()->getDomains();
        QSet<QString> configuredDomainIdentifiers;
        const auto accountStates = AccountManager::instance()->accounts();

        for (const auto &accountState : accountStates) {
            const auto account = accountState->account();

            if (!account) {
                continue;
            }

            const auto identifier = account->fileProviderDomainIdentifier();

            if (identifier.isEmpty()) {
                continue;
            }

            configuredDomainIdentifiers.insert(identifier);
        }

        for (NSFileProviderDomain * const domain : domains) {
            const auto identifier = QString::fromNSString(domain.identifier);

            if (!configuredDomainIdentifiers.contains(identifier)) {
                qCInfo(lcFileProviderSettingsController) << "Identified orphaned domain" << domain.identifier;
                Mac::FileProvider::instance()->domainManager()->removeDomain(domain);
            } else {
                qCInfo(lcFileProviderSettingsController) << "Identified domain belonging to an account" << domain.identifier;
            }
        }

        qCInfo(lcFileProviderSettingsController) << "Finished removing orphaned domains.";
    }

private:
    [[maybe_unused]] FileProviderSettingsController *q = nullptr;
};

// MARK: -

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
    // Prevent multiple concurrent operations
    if (_isOperationInProgress) {
        qCWarning(lcFileProviderSettingsController) << "Operation already in progress, ignoring request";
        return;
    }

    setOperationInProgress(true, setEnabled ? tr("Setting up…") : tr("Cleaning up…"));

    // Capture necessary data for the background operation
    // We need to copy the QString to ensure it's valid in the block
    const QString capturedUserIdAtHost = userIdAtHost;
    auto *controller = this;

    // Dispatch to background queue
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        const auto enabledAccountsAction = controller->d->setVfsEnabledForAccount(capturedUserIdAtHost, setEnabled);

        // Dispatch back to main queue for UI updates and signal emissions
        dispatch_async(dispatch_get_main_queue(), ^{
            controller->setOperationInProgress(false, QString());

            if (enabledAccountsAction == MacImplementation::VfsAccountsAction::VfsAccountsEnabledChanged) {
                emit controller->vfsEnabledForAccountChanged(capturedUserIdAtHost);

                if (setEnabled && showInformationDialog) {
                    QMessageBox::information(nullptr,
                                             controller->tr("Virtual files enabled"),
                                             controller->tr("Virtual files have been enabled for this account.\n\n"
                                                            "Your files are now accessible in Finder under the \"Locations\" section."));
                }
            }
        });
    });
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

bool FileProviderSettingsController::isOperationInProgress() const
{
    return _isOperationInProgress;
}

QString FileProviderSettingsController::operationMessage() const
{
    return _operationMessage;
}

void FileProviderSettingsController::setOperationInProgress(bool inProgress, const QString &message)
{
    if (_isOperationInProgress != inProgress) {
        _isOperationInProgress = inProgress;
        emit operationInProgressChanged();
    }
    if (_operationMessage != message) {
        _operationMessage = message;
        emit operationMessageChanged();
    }
}

} // namespace Mac

} // namespace OCC
