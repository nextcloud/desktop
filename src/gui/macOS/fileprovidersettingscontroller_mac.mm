/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileprovidersettingscontroller.h"

#include <QDesktopServices>
#include <QDir>
#include <QList>
#include <QMessageBox>
#include <QPushButton>

#include "gui/accountmanager.h"
#include "gui/folderman.h"
#include "gui/userinfo.h"
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileprovideritemmetadata.h"
#include "gui/macOS/fileproviderutils.h"
#include "libsync/configfile.h"
#include "libsync/theme.h"

// Objective-C imports
#import <Foundation/Foundation.h>
#import <FileProvider/FileProvider.h>
#import <AppKit/AppKit.h>
// End of Objective-C imports

namespace {
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
        VfsAccountsFailed,
    };

    explicit MacImplementation(FileProviderSettingsController *const parent)
    {
        q = parent;

        migrateToAppSandbox();
        ensureModeFlagInitialized();
        removeOrphanedDomains();

        if (Mac::FileProvider::available()) {
            // Domains are reconciled with the app-level mode later, in
            // performStartupReconciliation(), once the event loop runs and a possible
            // conflict with classic sync folders can be put in front of the user.
            Mac::FileProvider::instance()->domainManager()->reconcileDomainDisplayNames();
            Mac::FileProvider::instance()->domainManager()->reconnectAll();
            Mac::FileProvider::instance()->configureXPC();
        } else {
            disableFileProviderForAllEnabledAccountsOnUnsupportedOS();

            // Keep the persisted app-level mode consistent with what this OS can run, so
            // the wizard, folder guard and settings pages all read the same state.
            ConfigFile().setMacFileProviderModeEnabled(false);
        }
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

        if (!setEnabled && existingDomainId.isEmpty()) {
            qCInfo(lcFileProviderSettingsController) << "Account has no file provider domain, nothing to disable." << userIdAtHost;
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        const auto accountManager = AccountManager::instance();
        const auto accountState = accountManager->accountFromUserId(userIdAtHost);

        if (!accountState) {
            qCWarning(lcFileProviderSettingsController) << "Unable to set file provider enablement, account not found!" << userIdAtHost;
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        if (setEnabled) {
            // addDomainForAccount is idempotent: it returns the existing identifier when
            // the domain is still registered with the system, re-creates it when the
            // stored identifier is stale (the "fake" identifiers minted by
            // migrateToAppSandbox, or a domain the user removed in System Settings), or
            // creates a fresh one. Always going through it guarantees a real domain
            // exists before the caller discards the classic sync folders.
            auto const identifier = Mac::FileProvider::instance()->domainManager()->addDomainForAccount(accountState.data());

            if (identifier.isEmpty()) {
                qCWarning(lcFileProviderSettingsController) << "Failed to create file provider domain for account"
                                                            << userIdAtHost;
                return VfsAccountsAction::VfsAccountsFailed;
            }

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

    void ensureModeFlagInitialized()
    {
        ConfigFile cfg;

        const auto brandingDisablesVfs = Theme::instance()->disableVirtualFilesSyncFolder();
        const auto accounts = AccountManager::instance()->accounts();

        // With no accounts configured there is no deployment state to preserve, so always
        // (re-)default to File Provider on a supported, non-branding-disabled build. This
        // is the product default for a new setup — the first account becomes a File
        // Provider — and it also recovers a stale flag left behind after all accounts were
        // removed (removing accounts does not clear this key). Done before the "already
        // set" guard on purpose. On an unsupported OS the caller forces the flag back off
        // afterwards.
        if (accounts.isEmpty()) {
            qCInfo(lcFileProviderSettingsController) << "No accounts configured; defaulting app-level file provider mode to" << !brandingDisablesVfs;
            cfg.setMacFileProviderModeEnabled(!brandingDisablesVfs);
            return;
        }

        // Existing deployment: decide the app-level mode only once, so a later explicit
        // user choice is respected across launches. Keep per-account File Provider users
        // on File Provider; leave a classic-only deployment (accounts configured, none
        // using the File Provider) on classic so an upgrade does not disrupt them.
        if (cfg.macFileProviderModeEnabledIsSet()) {
            return;
        }

        const auto modeEnabled = !enabledAccounts().isEmpty() && !brandingDisablesVfs;
        qCInfo(lcFileProviderSettingsController) << "Initialising app-level file provider mode:" << modeEnabled;
        cfg.setMacFileProviderModeEnabled(modeEnabled);
    }

    void ensureDomainConfigurationMatchesMode()
    {
        const auto modeEnabled = ConfigFile().macFileProviderModeEnabled();
        qCInfo(lcFileProviderSettingsController) << "Reconciling file provider domains with app-level mode:" << modeEnabled;

        const auto accountStates = AccountManager::instance()->accounts();

        if (modeEnabled) {
            const auto domains = Mac::FileProvider::instance()->domainManager()->getDomains();
            QSet<QString> existingDomainIdentifiers;

            for (NSFileProviderDomain * const domain : domains) {
                existingDomainIdentifiers.insert(QString::fromNSString(domain.identifier));
            }

            for (const auto &accountState : accountStates) {
                const auto account = accountState->account();

                if (!account) {
                    continue;
                }

                const auto userIdAtHost = account->userIdAtHostWithPort();
                const auto identifier = account->fileProviderDomainIdentifier();

                if (identifier.isEmpty()) {
                    qCInfo(lcFileProviderSettingsController) << "Creating missing file provider domain for account" << userIdAtHost;
                    (void)setVfsEnabledForAccount(userIdAtHost, true);
                } else if (existingDomainIdentifiers.contains(identifier) == false) {
                    qCInfo(lcFileProviderSettingsController) << "Restoring missing domain with identifier"
                                                             << identifier
                                                             << "for account"
                                                             << userIdAtHost;

                    const auto newIdentifier = Mac::FileProvider::instance()->domainManager()->addDomainForAccount(accountState.data());

                    if (newIdentifier.isEmpty() == false) {
                        AccountManager::instance()->setFileProviderDomainIdentifier(userIdAtHost, newIdentifier);
                    }
                }
            }
        } else {
            for (const auto &accountState : accountStates) {
                const auto account = accountState->account();

                if (!account || account->fileProviderDomainIdentifier().isEmpty()) {
                    continue;
                }

                const auto userIdAtHost = account->userIdAtHostWithPort();
                qCInfo(lcFileProviderSettingsController) << "Removing file provider domain of account" << userIdAtHost;
                (void)setVfsEnabledForAccount(userIdAtHost, false);
            }
        }

        qCInfo(lcFileProviderSettingsController) << "Finished reconciling file provider domains with app-level mode.";
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

    // macOS 13 Ventura cleanup: removes any pre-existing file provider domain
    // gracefully (preserving dirty user data) for each account that still has
    // VFS enabled. Can be deleted once Ventura is no longer supported.
    void disableFileProviderForAllEnabledAccountsOnUnsupportedOS()
    {
        qCInfo(lcFileProviderSettingsController) << "macOS 13 Ventura: disabling file provider for all enabled accounts.";

        const auto accountStates = AccountManager::instance()->accounts();

        for (const auto &accountState : accountStates) {
            const auto account = accountState->account();

            if (!account) {
                continue;
            }

            if (account->fileProviderDomainIdentifier().isEmpty()) {
                continue;
            }

            const auto userIdAtHost = account->userIdAtHostWithPort();
            qCInfo(lcFileProviderSettingsController) << "Disabling file provider for account" << userIdAtHost;
            (void)setVfsEnabledForAccount(userIdAtHost, false);
        }
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

    connect(accManager, &AccountManager::accountAdded,
            this, [this](AccountState *const accountState) {
        const auto account = accountState ? accountState->account() : nullptr;

        if (!account) {
            return;
        }

        const auto accountUserIdAtHost = account->userIdAtHostWithPort();

        if (!_userInfos.contains(accountUserIdAtHost)) {
            const auto userInfo = new UserInfo(accountState, false, false, this);
            _userInfos.insert(accountUserIdAtHost, userInfo);
            userInfo->setActive(true);
        }

        if (Mac::FileProvider::available() && fileProviderModeEnabled() && !vfsEnabledForAccount(accountUserIdAtHost)) {
            qCInfo(lcFileProviderSettingsController) << "File provider mode is enabled, setting up domain for new account"
                                                     << accountUserIdAtHost;
            setVfsEnabledForAccount(accountUserIdAtHost, true);
        }
    });
}

QStringList FileProviderSettingsController::vfsEnabledAccounts() const
{
    return d->enabledAccounts();
}

bool FileProviderSettingsController::vfsEnabledForAccount(const QString &userIdAtHost) const
{
    return d->vfsEnabledForAccount(userIdAtHost);
}

void FileProviderSettingsController::setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled)
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
            }
        });
    });
}

bool FileProviderSettingsController::fileProviderModeEnabled() const
{
    return ConfigFile().macFileProviderModeEnabled();
}

void FileProviderSettingsController::setFileProviderModeEnabled(const bool enabled)
{
    if (_isOperationInProgress) {
        qCWarning(lcFileProviderSettingsController) << "Operation already in progress, ignoring file provider mode change";
        return;
    }

    if (enabled == fileProviderModeEnabled()) {
        // Nothing to do, but let listeners (e.g. the settings switch) re-sync.
        emit fileProviderModeEnabledChanged(enabled);
        return;
    }

    if (enabled && !Mac::FileProvider::available()) {
        qCWarning(lcFileProviderSettingsController) << "Cannot enable file provider mode, it is unavailable on this system.";
        return;
    }

    // The flag records the user's intent up front. Should anything below fail or the
    // client die mid-way, "mode enabled + classic folders still configured" is picked
    // up by performStartupReconciliation() on the next launch.
    ConfigFile().setMacFileProviderModeEnabled(enabled);
    emit fileProviderModeEnabledChanged(enabled);

    applyFileProviderModeToAllAccounts(enabled);
}

void FileProviderSettingsController::applyFileProviderModeToAllAccounts(const bool enabled)
{
    if (_isOperationInProgress) {
        qCWarning(lcFileProviderSettingsController) << "Operation already in progress, not applying file provider mode";
        return;
    }

    setOperationInProgress(true, enabled ? tr("Setting up…") : tr("Cleaning up…"));

    // Snapshot the account ids on the main thread; AccountManager is not to be iterated
    // from the background worker.
    QStringList accountIds;
    const auto accountStates = AccountManager::instance()->accounts();

    for (const auto &accountState : accountStates) {
        if (const auto account = accountState->account()) {
            accountIds.append(account->userIdAtHostWithPort());
        }
    }

    const QStringList capturedAccountIds = accountIds;
    auto *controller = this;

    // NOTE: the per-account worker still reaches into AccountManager (accountFromUserId /
    // setFileProviderDomainIdentifier) from this background queue — the same pre-existing
    // cross-thread access the single-account path has always used. This loop widens the
    // window; a full fix would marshal all AccountManager access back to the main thread.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        QStringList changedAccounts;
        QStringList failedAccounts;

        for (const auto &accountId : capturedAccountIds) {
            switch (controller->d->setVfsEnabledForAccount(accountId, enabled)) {
            case MacImplementation::VfsAccountsAction::VfsAccountsEnabledChanged:
                changedAccounts.append(accountId);
                break;
            case MacImplementation::VfsAccountsAction::VfsAccountsFailed:
                failedAccounts.append(accountId);
                break;
            case MacImplementation::VfsAccountsAction::VfsAccountsNoAction:
                break;
            }
        }

        const QStringList capturedChangedAccounts = changedAccounts;
        const QStringList capturedFailedAccounts = failedAccounts;

        dispatch_async(dispatch_get_main_queue(), ^{
            controller->setOperationInProgress(false, QString());

            for (const auto &accountId : capturedChangedAccounts) {
                emit controller->vfsEnabledForAccountChanged(accountId);
            }

            if (enabled && capturedFailedAccounts.isEmpty()) {
                // Only discard the classic sync folder configurations once every
                // account's domain has verifiably been created.
                controller->removeAllClassicSyncFolders();

                if (!capturedChangedAccounts.isEmpty()) {
                    QMessageBox::information(nullptr,
                                             tr("File Provider enabled"),
                                             tr("File Provider has been enabled for all accounts.\n\nYour files are now accessible in Finder under the \"Locations\" section."));
                }
            } else if (enabled) {
                QMessageBox::warning(nullptr,
                                     tr("File Provider could not be enabled for all accounts"),
                                     tr("File Provider could not be set up for the following account(s):\n%1\n\nYour classic sync folders were left unchanged. You will be asked how to proceed the next time %2 starts, or you can resolve it from the account settings.")
                                         .arg(capturedFailedAccounts.join(QStringLiteral("\n")),
                                              Theme::instance()->appNameGUI()));
            }

            emit controller->fileProviderModeApplyFinished(enabled, capturedFailedAccounts);
        });
    });
}

void FileProviderSettingsController::removeAllClassicSyncFolders()
{
    const auto folderMan = FolderMan::instance();

    if (!folderMan) {
        return;
    }

    const auto folders = folderMan->map().values();

    if (folders.isEmpty()) {
        return;
    }

    qCInfo(lcFileProviderSettingsController) << "Removing" << folders.count()
                                             << "classic sync folder connection(s); local files are kept on disk.";

    for (const auto folder : folders) {
        folderMan->removeFolder(folder);
    }
}

void FileProviderSettingsController::performStartupReconciliation()
{
    if (!Mac::FileProvider::available()) {
        return;
    }

    if (_isOperationInProgress) {
        qCWarning(lcFileProviderSettingsController) << "Operation already in progress, skipping reconciliation";
        return;
    }

    const auto folderMan = FolderMan::instance();
    const auto hasClassicFolders = folderMan && !folderMan->map().isEmpty();

    if (fileProviderModeEnabled() && hasClassicFolders) {
        // App-level conflict: the File Provider extension and the FinderSync extension
        // (classic sync folders) cannot run at the same time. Let the user pick a side.
        showReconciliationDialog();
        return;
    }

    d->ensureDomainConfigurationMatchesMode();
}

void FileProviderSettingsController::showReconciliationDialog()
{
    if (_reconciliationDialogShowing) {
        return;
    }

    _reconciliationDialogShowing = true;

    const auto messageBoxText = tr("This installation has both the File Provider integration and classic sync folders configured. macOS cannot run the File Provider integration and the classic Finder integration at the same time.")
        + QStringLiteral("\n\n")
        + tr("Keep File Provider: all accounts appear in Finder under \"Locations\". Classic sync folder connections will be removed. Already synced files will be preserved in place.")
        + QStringLiteral("\n\n")
        + tr("Keep classic sync folders: File Provider is turned off for all accounts. Items that were not uploaded yet will be preserved.");

    const auto messageBox = new QMessageBox(QMessageBox::Question,
                                            tr("Choose how to sync your files"),
                                            messageBoxText);
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    const auto keepFileProviderButton = messageBox->addButton(tr("Keep File Provider"), QMessageBox::AcceptRole);
    const auto keepClassicButton = messageBox->addButton(tr("Keep classic sync folders"), QMessageBox::AcceptRole);
    // The single RejectRole button, so Esc and closing the window mean "Decide later".
    messageBox->addButton(tr("Decide later"), QMessageBox::RejectRole);

    connect(messageBox, &QMessageBox::finished, this, [this, messageBox, keepFileProviderButton, keepClassicButton] {
        _reconciliationDialogShowing = false;

        // The dialog is non-modal, so the mode may have been changed elsewhere (e.g. the
        // General settings switch) while it was open. Re-validate against the current
        // state so a stale click cannot destroy the user's configuration.
        const auto modeEnabled = fileProviderModeEnabled();

        if (messageBox->clickedButton() == keepFileProviderButton) {
            if (!modeEnabled) {
                qCInfo(lcFileProviderSettingsController) << "Ignoring stale 'Keep File Provider' choice; mode was disabled meanwhile.";
                return;
            }
            // Create any missing domains and, on full success, discard the classic sync
            // folder configurations (local files stay on disk).
            applyFileProviderModeToAllAccounts(true);
        } else if (messageBox->clickedButton() == keepClassicButton) {
            if (!modeEnabled) {
                qCInfo(lcFileProviderSettingsController) << "Ignoring stale 'Keep classic' choice; mode already disabled.";
                return;
            }
            // The dialog is the explicit confirmation for turning File Provider off.
            setFileProviderModeEnabled(false);
        }
        // "Decide later": both configurations stay untouched; the dialog is shown
        // again on the next launch and can be reopened from the account settings.
    });
    messageBox->open();
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
