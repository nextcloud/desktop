/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "fileprovidersettingscontroller.h"

#include <QQmlApplicationEngine>

#include "gui/systray.h"
#include "gui/userinfo.h"
#include "gui/macOS/fileprovideritemmetadata.h"

// Objective-C imports
#import <Foundation/Foundation.h>

#import "fileproviderstorageuseenumerationobserver.h"
// End of Objective-C imports

namespace {
constexpr auto fpSettingsQmlPath = "qrc:/qml/src/gui/macOS/ui/FileProviderSettings.qml";

// FileProviderSettingsPage properties -- make sure they match up in QML file!
constexpr auto fpSettingsAccountUserIdAtHostProp = "accountUserIdAtHost";

// NSUserDefaults entries
constexpr auto enabledAccountsSettingsKey = "enabledAccounts";

float gbFromBytesWithOneDecimal(const unsigned long long bytes)
{
    constexpr auto bytesIn100Mb = 1ULL * 1000ULL * 1000ULL * 100ULL;
    return ((bytes * 1.0) / bytesIn100Mb) / 10.0;
}
} // namespace

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderSettingsController, "nextcloud.gui.mac.fileprovider.settingscontroller")

class FileProviderSettingsController::MacImplementation
{
public:
    enum class VfsAccountsAction {
        VfsAccountsNoAction,
        VfsAccountsEnabledChanged,
    };

    MacImplementation(FileProviderSettingsController *const parent)
    {
        q = parent;
        initialCheck();
        fetchMaterialisedFilesStorageUsage();
    };

    ~MacImplementation() = default;

    [[nodiscard]] QStringList enabledAccounts() const
    {
        QStringList qEnabledAccounts;
        NSArray<NSString *> *const enabledAccounts = nsEnabledAccounts();
        for (NSString *const userIdAtHostString in enabledAccounts) {
            qEnabledAccounts.append(QString::fromNSString(userIdAtHostString));
        }
        return qEnabledAccounts;
    }

    [[nodiscard]] bool vfsEnabledForAccount(const QString &userIdAtHost) const
    {
        NSArray<NSString *> *const vfsEnabledAccounts = nsEnabledAccounts();
        return [vfsEnabledAccounts containsObject:userIdAtHost.toNSString()];
    }

    [[nodiscard]] VfsAccountsAction setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled) const
    {
        NSArray<NSString *> *vfsEnabledAccounts = nsEnabledAccounts();

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
        [_userDefaults setObject:modifiedVfsAccounts forKey:_accountsKey];

        Q_ASSERT(vfsEnabledForAccount(userIdAtHost) == userIdAtHost);

        return VfsAccountsAction::VfsAccountsEnabledChanged;
    }

    [[nodiscard]] VfsAccountsAction enableVfsForAllAccounts() const
    {
        const auto accManager = AccountManager::instance();
        const auto accountsList = accManager->accounts();

        if (accountsList.count() == 0) {
            return VfsAccountsAction::VfsAccountsNoAction;
        }

        auto overallActResult = VfsAccountsAction::VfsAccountsNoAction;

        for (const auto &account : accountsList) {
            const auto qAccountUserIdAtHost = account->account()->userIdAtHostWithPort();
            const auto accountActResult = setVfsEnabledForAccount(qAccountUserIdAtHost, true);

            if (accountActResult == VfsAccountsAction::VfsAccountsEnabledChanged) {
                overallActResult = accountActResult;
            }
        }

        return overallActResult;
    }

    [[nodiscard]] unsigned long long localStorageUsageForAccount(const QString &userIdAtHost) const
    {
        // Return cached value as we fetch asynchronously on initialisation of this class.
        // We will then emit a signal when the new value is found.
        NSNumber *const storageUsage = [_storageUsage objectForKey:userIdAtHost.toNSString()];
        if (storageUsage == nil) {
            return 0;
        }
        return storageUsage.unsignedLongLongValue;
    }

    [[nodiscard]] QVector<FileProviderItemMetadata> materialisedItemsForAccount(const QString &userIdAtHost) const
    {
        const auto materialisedItems = [_materialisedFiles objectForKey:userIdAtHost.toNSString()];
        if (materialisedItems == nil) {
            return {};
        }

        QVector<FileProviderItemMetadata> qMaterialisedItems;
        qMaterialisedItems.reserve(materialisedItems.count);

        for (const id<NSFileProviderItem> item in materialisedItems) {
            const auto itemMetadata = FileProviderItemMetadata::fromNSFileProviderItem(item);
            qMaterialisedItems.append(itemMetadata);
        }

        return qMaterialisedItems;
    }

private:
    [[nodiscard]] NSArray<NSString *> *nsEnabledAccounts() const
    {
        return (NSArray<NSString *> *)[_userDefaults objectForKey:_accountsKey];
    }

    void fetchMaterialisedFilesStorageUsage()
    {
        qCDebug(lcFileProviderSettingsController) << "Fetching materialised files storage usage";

        [NSFileProviderManager getDomainsWithCompletionHandler: ^(NSArray<NSFileProviderDomain *> *const domains, NSError *const error) {
            if (error != nil) {
                qCWarning(lcFileProviderSettingsController) << "Could not get file provider domains:"
                                                            << error.localizedDescription
                                                            << "Will try again in 2 secs";

                // HACK: Sometimes the system is not in a state where it wants to give us access to
                //       the file provider domains. We will try again in 2 seconds and hope it works
                __block const auto thisQobject = (QObject*)this;
                dispatch_async(dispatch_get_main_queue(), ^{
                    [NSTimer scheduledTimerWithTimeInterval:2 repeats:NO block:^(NSTimer *const timer) {
                        Q_UNUSED(timer)
                        QMetaObject::invokeMethod(thisQobject, [this] { fetchMaterialisedFilesStorageUsage(); });
                    }];
                });
                return;
            }

            for (NSFileProviderDomain *const domain in domains) {
                qCDebug(lcFileProviderSettingsController) << "Checking storage use for domain:" << domain.identifier;

                NSFileProviderManager *const managerForDomain = [NSFileProviderManager managerForDomain:domain];
                if (managerForDomain == nil) {
                    qCWarning(lcFileProviderSettingsController) << "Got a nil file provider manager for domain"
                                                                << domain.identifier
                                                                << ", returning early.";
                    return;
                }

                const id<NSFileProviderEnumerator> enumerator = [managerForDomain enumeratorForMaterializedItems];
                Q_ASSERT(enumerator != nil);

                __block FileProviderStorageUseEnumerationObserver *const storageUseObserver = [[FileProviderStorageUseEnumerationObserver alloc] init];

                storageUseObserver.enumerationFinishedHandler = ^(NSError *const error) {
                    if (error != nil) {
                        qCWarning(lcFileProviderSettingsController) << "Error while enumerating storage use" << error.localizedDescription;
                        [storageUseObserver release];
                        [enumerator release];
                        return;
                    }

                    const NSUInteger usage = storageUseObserver.usage;
                    NSSet<id<NSFileProviderItem>> *const items = storageUseObserver.materialisedItems;
                    Q_ASSERT(items != nil);

                    // Remember that OCC::Account::userIdAtHost == domain.identifier for us
                    NSMutableDictionary<NSString *, NSNumber *> *const mutableStorageDictCopy = _storageUsage.mutableCopy;
                    NSMutableDictionary<NSString *, NSSet<id<NSFileProviderItem>> *> *const mutableFilesDictCopy = _materialisedFiles.mutableCopy;

                    qCDebug(lcFileProviderSettingsController) << "Local storage use for"
                                                              << domain.identifier
                                                              << usage;

                    [mutableStorageDictCopy setObject:@(usage) forKey:domain.identifier];
                    [mutableFilesDictCopy setObject:items forKey:domain.identifier];

                    _storageUsage = mutableStorageDictCopy.copy;
                    _materialisedFiles = mutableFilesDictCopy.copy;

                    const auto qDomainIdentifier = QString::fromNSString(domain.identifier);
                    emit q->localStorageUsageForAccountChanged(qDomainIdentifier);

                    [storageUseObserver release];
                    [enumerator release];
                };

                [enumerator enumerateItemsForObserver:storageUseObserver startingAtPage:NSFileProviderInitialPageSortedByName];

                [storageUseObserver retain];
                [enumerator retain];
            }
        }];
    }

    void initialCheck()
    {
        qCDebug(lcFileProviderSettingsController) << "Running initial checks for file provider settings controller.";

        NSArray<NSString *> *const vfsEnabledAccounts = nsEnabledAccounts();
        if (vfsEnabledAccounts != nil) {
            return;
        }

        qCDebug(lcFileProviderSettingsController) << "Initial check for file provider settings found nil enabled vfs accounts array."
                                                  << "Enabling all accounts on initial setup.";

        [[maybe_unused]] const auto result = enableVfsForAllAccounts();
    }

    FileProviderSettingsController *q = nullptr;
    NSUserDefaults *_userDefaults = NSUserDefaults.standardUserDefaults;
    NSString *_accountsKey = [NSString stringWithUTF8String:enabledAccountsSettingsKey];
    NSDictionary <NSString *, NSNumber *> *_storageUsage = @{};
    NSDictionary <NSString *, NSSet<id<NSFileProviderItem>> *> *_materialisedFiles = @{};
};

FileProviderSettingsController *FileProviderSettingsController::instance()
{
    static FileProviderSettingsController controller;
    return &controller;
}

FileProviderSettingsController::~FileProviderSettingsController() = default;

FileProviderSettingsController::FileProviderSettingsController(QObject *parent)
    : QObject{parent}
{
    d = std::make_unique<FileProviderSettingsController::MacImplementation>(this);

    const auto accManager = AccountManager::instance();
    const auto accountsList = accManager->accounts();

    for (const auto &accountState : accountsList) {
        const auto userInfo = new UserInfo(accountState.data(), false, false, this);
        const auto account = accountState->account();
        const auto accountUserIdAtHost = account->userIdAtHostWithPort();

        _userInfos.insert(accountUserIdAtHost, userInfo);
        connect(userInfo, &UserInfo::fetchedLastInfo, this, [this, accountUserIdAtHost] {
            emit remoteStorageUsageForAccountChanged(accountUserIdAtHost);
        });
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
    settingsViewWidget->rootObject()->setProperty(fpSettingsAccountUserIdAtHostProp, accountUserIdAtHost);
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

void FileProviderSettingsController::setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled)
{
    const auto enabledAccountsAction = d->setVfsEnabledForAccount(userIdAtHost, setEnabled);
    if (enabledAccountsAction == MacImplementation::VfsAccountsAction::VfsAccountsEnabledChanged) {
        emit vfsEnabledAccountsChanged();
    }
}

unsigned long long FileProviderSettingsController::localStorageUsageForAccount(const QString &userIdAtHost) const
{
    return d->localStorageUsageForAccount(userIdAtHost);
}

float FileProviderSettingsController::localStorageUsageGbForAccount(const QString &userIdAtHost) const
{
    return gbFromBytesWithOneDecimal(localStorageUsageForAccount(userIdAtHost));
}

unsigned long long FileProviderSettingsController::remoteStorageUsageForAccount(const QString &userIdAtHost) const
{
    const auto userInfoForAccount = _userInfos.value(userIdAtHost);
    if (!userInfoForAccount) {
        return 0;
    }

    return userInfoForAccount->lastQuotaUsedBytes();
}

float FileProviderSettingsController::remoteStorageUsageGbForAccount(const QString &userIdAtHost) const
{
    return gbFromBytesWithOneDecimal(remoteStorageUsageForAccount(userIdAtHost));
}

} // namespace Mac

} // namespace OCC
