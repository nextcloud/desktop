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

#include <QFileDialog>
#include <QQmlApplicationEngine>

#include "gui/systray.h"
#include "gui/userinfo.h"
#include "gui/macOS/fileprovider.h"
#include "gui/macOS/fileprovideritemmetadata.h"
#include "gui/macOS/fileprovidermaterialiseditemsmodel.h"
#include "gui/macOS/fileproviderutils.h"

// Objective-C imports
#import <Foundation/Foundation.h>

#import "fileproviderstorageuseenumerationobserver.h"
// End of Objective-C imports

namespace {
constexpr auto fpSettingsQmlPath = "qrc:/qml/src/gui/macOS/ui/FileProviderSettings.qml";
constexpr auto fpEvictionDialogQmlPath = "qrc:/qml/src/gui/macOS/ui/FileProviderEvictionDialog.qml";

// QML properties -- make sure they match up in QML file!
constexpr auto fpAccountUserIdAtHostProp = "accountUserIdAtHost";
constexpr auto fpMaterialisedItemsModelProp = "materialisedItemsModel";

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
        initialCheck();
        fetchMaterialisedFilesStorageUsage();
    };

    ~MacImplementation() override = default;

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

        Q_ASSERT(vfsEnabledForAccount(userIdAtHost) == setEnabled);

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
        return _storageUsage.value(userIdAtHost);
    }

    [[nodiscard]] QVector<FileProviderItemMetadata> materialisedItemsForAccount(const QString &userIdAtHost) const
    {
        return _materialisedFiles.value(userIdAtHost);
    }

    void signalFileProviderDomain(const QString &userIdAtHost) const
    {
        qCInfo(lcFileProviderSettingsController) << "Signalling file provider domain" << userIdAtHost;
        NSFileProviderDomain * const domain = FileProviderUtils::domainForIdentifier(userIdAtHost);
        NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];
        [manager signalEnumeratorForContainerItemIdentifier:NSFileProviderRootContainerItemIdentifier
                                          completionHandler:^(NSError *const error) {
            if (error != nil) {
                qCWarning(lcFileProviderSettingsController) << "Could not signal file provider domain, error"
                                                            << error.localizedDescription;
                return;
            }

            qCInfo(lcFileProviderSettingsController) << "Successfully signalled file provider domain";
            // TODO: Provide some feedback in the UI
        }];
    }

    [[nodiscard]] FileProviderDomainSyncStatus *domainSyncStatusForAccount(const QString &userIdAtHost) const
    {
        return _fileProviderDomainSyncStatuses.value(userIdAtHost);
    }

public slots:
    void enumerateMaterialisedFilesForDomainManager(NSFileProviderManager * const managerForDomain,
                                                    NSFileProviderDomain * const domain)
    {
        const id<NSFileProviderEnumerator> enumerator = [managerForDomain enumeratorForMaterializedItems];
        Q_ASSERT(enumerator != nil);
        [enumerator retain];

        FileProviderStorageUseEnumerationObserver *const storageUseObserver = [[FileProviderStorageUseEnumerationObserver alloc] init];
        [storageUseObserver retain];
        storageUseObserver.enumerationFinishedHandler = ^(NSError *const error) {
            qCInfo(lcFileProviderSettingsController) << "Enumeration finished for" << domain.identifier;
            if (error != nil) {
                qCWarning(lcFileProviderSettingsController) << "Error while enumerating storage use" << error.localizedDescription;
                [storageUseObserver release];
                [enumerator release];
                return;
            }

            const auto items = storageUseObserver.materialisedItems;
            Q_ASSERT(items != nil);

            // Remember that OCC::Account::userIdAtHost == domain.identifier for us
            const auto qDomainIdentifier = QString::fromNSString(domain.identifier);
            QVector<FileProviderItemMetadata> qMaterialisedItems;
            qMaterialisedItems.reserve(items.count);
            unsigned long long storageUsage = 0;
            for (const id<NSFileProviderItem> item in items) {
                const auto itemMetadata = FileProviderItemMetadata::fromNSFileProviderItem(item, qDomainIdentifier);
                storageUsage += itemMetadata.documentSize();
                qCDebug(lcFileProviderSettingsController) << "Adding item" << itemMetadata.identifier()
                                                          << "with size" << itemMetadata.documentSize()
                                                          << "to storage usage for account" << qDomainIdentifier
                                                          << "with total size" << storageUsage;
                qMaterialisedItems.append(itemMetadata);
            }
            _storageUsage.insert(qDomainIdentifier, storageUsage);
            _materialisedFiles.insert(qDomainIdentifier, qMaterialisedItems);

            emit q->localStorageUsageForAccountChanged(qDomainIdentifier);
            emit q->materialisedItemsForAccountChanged(qDomainIdentifier);

            [storageUseObserver release];
            [enumerator release];
        };
        [enumerator enumerateItemsForObserver:storageUseObserver startingAtPage:NSFileProviderInitialPageSortedByName];
    }

private slots:
    void updateDomainSyncStatuses()
    {
        qCInfo(lcFileProviderSettingsController) << "Updating domain sync statuses";
        _fileProviderDomainSyncStatuses.clear();
        const auto enabledAccounts = nsEnabledAccounts();
        for (NSString *const domainIdentifier in enabledAccounts) {
            const auto qDomainIdentifier = QString::fromNSString(domainIdentifier);
            const auto syncStatus = new FileProviderDomainSyncStatus(qDomainIdentifier, q);
            _fileProviderDomainSyncStatuses.insert(qDomainIdentifier, syncStatus);
        }
    }

private:
    [[nodiscard]] NSArray<NSString *> *nsEnabledAccounts() const
    {
        return (NSArray<NSString *> *)[_userDefaults objectForKey:_accountsKey];
    }

    void fetchMaterialisedFilesStorageUsage()
    {
        qCInfo(lcFileProviderSettingsController) << "Fetching materialised files storage usage";

        [NSFileProviderManager getDomainsWithCompletionHandler: ^(NSArray<NSFileProviderDomain *> *const domains, NSError *const error) {
            if (error != nil) {
                qCWarning(lcFileProviderSettingsController) << "Could not get file provider domains:"
                                                            << error.localizedDescription
                                                            << "Will try again in 2 secs";

                // HACK: Sometimes the system is not in a state where it wants to give us access to
                //       the file provider domains. We will try again in 2 seconds and hope it works
                const auto thisQobject = (QObject*)this;
                dispatch_async(dispatch_get_main_queue(), ^{
                    [NSTimer scheduledTimerWithTimeInterval:2 repeats:NO block:^(NSTimer *const timer) {
                        Q_UNUSED(timer)
                        QMetaObject::invokeMethod(thisQobject, [this] { fetchMaterialisedFilesStorageUsage(); });
                    }];
                });
                return;
            }

            for (NSFileProviderDomain *const domain in domains) {
                qCInfo(lcFileProviderSettingsController) << "Checking storage use for domain:" << domain.identifier;

                NSFileProviderManager *const managerForDomain = [NSFileProviderManager managerForDomain:domain];
                if (managerForDomain == nil) {
                    qCWarning(lcFileProviderSettingsController) << "Got a nil file provider manager for domain"
                                                                << domain.identifier
                                                                << ", returning early.";
                    return;
                }

                enumerateMaterialisedFilesForDomainManager(managerForDomain, domain);
            }
        }];
    }

    void initialCheck()
    {
        qCInfo(lcFileProviderSettingsController) << "Running initial checks for file provider settings controller.";

        NSArray<NSString *> *const vfsEnabledAccounts = nsEnabledAccounts();
        if (vfsEnabledAccounts != nil) {
            updateDomainSyncStatuses();
            connect(q, &FileProviderSettingsController::vfsEnabledAccountsChanged, this, &MacImplementation::updateDomainSyncStatuses);
            return;
        }

        qCInfo(lcFileProviderSettingsController) << "Initial check for file provider settings found nil enabled vfs accounts array."
                                                  << "Enabling all accounts on initial setup.";

        [[maybe_unused]] const auto result = enableVfsForAllAccounts();
    }

    FileProviderSettingsController *q = nullptr;
    NSUserDefaults *_userDefaults = NSUserDefaults.standardUserDefaults;
    NSString *_accountsKey = [NSString stringWithUTF8String:enabledAccountsSettingsKey];
    QHash<QString, QVector<FileProviderItemMetadata>> _materialisedFiles;
    QHash<QString, unsigned long long> _storageUsage;
    QHash<QString, FileProviderDomainSyncStatus*> _fileProviderDomainSyncStatuses;
};

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

void FileProviderSettingsController::setVfsEnabledForAccount(const QString &userIdAtHost, const bool setEnabled)
{
    const auto enabledAccountsAction = d->setVfsEnabledForAccount(userIdAtHost, setEnabled);
    if (enabledAccountsAction == MacImplementation::VfsAccountsAction::VfsAccountsEnabledChanged) {
        emit vfsEnabledAccountsChanged();
    }
}

bool FileProviderSettingsController::fastEnumerationSetForAccount(const QString &userIdAtHost) const
{
    const auto xpc = FileProvider::instance()->xpc();
    if (!xpc) {
        return false;
    }
    if (const auto state = xpc->fastEnumerationStateForExtension(userIdAtHost)) {
        return state->second;
    }
    return false;
}

bool FileProviderSettingsController::fastEnumerationEnabledForAccount(const QString &userIdAtHost) const
{
    const auto xpc = FileProvider::instance()->xpc();
    if (!xpc) {
        return true;
    }
    if (const auto fastEnumerationState = xpc->fastEnumerationStateForExtension(userIdAtHost)) {
        return fastEnumerationState->first;
    }
    return true;
}

void FileProviderSettingsController::setFastEnumerationEnabledForAccount(const QString &userIdAtHost, const bool setEnabled)
{
    const auto xpc = FileProvider::instance()->xpc();
    if (!xpc) {
        // Reset state of UI elements
        emit fastEnumerationEnabledForAccountChanged(userIdAtHost);
        emit fastEnumerationSetForAccountChanged(userIdAtHost);
        return;
    }
    xpc->setFastEnumerationEnabledForExtension(userIdAtHost, setEnabled);
    emit fastEnumerationEnabledForAccountChanged(userIdAtHost);
    emit fastEnumerationSetForAccountChanged(userIdAtHost);
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

QAbstractListModel *FileProviderSettingsController::materialisedItemsModelForAccount(const QString &userIdAtHost)
{
    const auto items = d->materialisedItemsForAccount(userIdAtHost);
    if (items.isEmpty()) {
        return nullptr;
    }

    const auto model = new FileProviderMaterialisedItemsModel(this);
    model->setItems(items);

    connect(this, &FileProviderSettingsController::materialisedItemsForAccountChanged,
            model, [this, model, userIdAtHost](const QString &accountUserIdAtHost) {
        if (accountUserIdAtHost != userIdAtHost) {
            return;
        }

        const auto items = d->materialisedItemsForAccount(userIdAtHost);
        model->setItems(items);
    });

    return model;
}

void FileProviderSettingsController::createEvictionWindowForAccount(const QString &userIdAtHost)
{
    const auto engine = Systray::instance()->trayEngine();
    QQmlComponent component(engine, QUrl(fpEvictionDialogQmlPath));
    const auto model = materialisedItemsModelForAccount(userIdAtHost);
    const auto genericDialog = component.createWithInitialProperties({
            {fpAccountUserIdAtHostProp, userIdAtHost},
            {fpMaterialisedItemsModelProp, QVariant::fromValue(model)},
    });
    const auto dialog = qobject_cast<QQuickWindow *>(genericDialog);
    QObject::connect(dialog, SIGNAL(reloadMaterialisedItems(QString)),
                     this, SLOT(refreshMaterialisedItemsForAccount(QString)));
    Q_ASSERT(dialog);
    dialog->show();
}

void FileProviderSettingsController::refreshMaterialisedItemsForAccount(const QString &userIdAtHost)
{
    d->enumerateMaterialisedFilesForDomainManager(FileProviderUtils::managerForDomainIdentifier(userIdAtHost),
                                                  FileProviderUtils::domainForIdentifier(userIdAtHost));
}

void FileProviderSettingsController::signalFileProviderDomain(const QString &userIdAtHost)
{
    d->signalFileProviderDomain(userIdAtHost);
}

void FileProviderSettingsController::createDebugArchive(const QString &userIdAtHost)
{
    const auto filename = QFileDialog::getSaveFileName(nullptr,
                                                       tr("Create Debug Archive"),
                                                       QStandardPaths::writableLocation(QStandardPaths::StandardLocation::DocumentsLocation),
                                                       tr("Text files") + " (*.txt)");
    if (filename.isEmpty()) {
        return;
    }

    const auto xpc = FileProvider::instance()->xpc();
    if (!xpc) {
        qCWarning(lcFileProviderSettingsController) << "Could not create debug archive, FileProviderXPC is not available.";
        return;
    }
    xpc->createDebugArchiveForExtension(userIdAtHost, filename);
}

FileProviderDomainSyncStatus *FileProviderSettingsController::domainSyncStatusForAccount(const QString &userIdAtHost) const
{
    return d->domainSyncStatusForAccount(userIdAtHost);
}

} // namespace Mac

} // namespace OCC
