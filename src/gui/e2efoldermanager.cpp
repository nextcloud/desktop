// SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "e2efoldermanager.h"
#include "accountmanager.h"
#include "clientsideencryption.h"
#include "folderman.h"
#include "folder.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcE2eFolderManager, "nextcloud.gui.e2efoldermanager", QtInfoMsg)

E2EFolderManager *E2EFolderManager::_instance = nullptr;

E2EFolderManager *E2EFolderManager::instance()
{
    if (!_instance) {
        _instance = new E2EFolderManager();
    }
    return _instance;
}

E2EFolderManager::E2EFolderManager(QObject *parent)
    : QObject(parent)
{
    qCInfo(lcE2eFolderManager) << "E2EFolderManager created";
}

E2EFolderManager::~E2EFolderManager()
{
    _instance = nullptr;
}

void E2EFolderManager::initialize()
{
    qCInfo(lcE2eFolderManager) << "Initializing E2EFolderManager";

    // Connect to existing accounts
    const auto accounts = AccountManager::instance()->accounts();
    for (const auto &accountState : accounts) {
        if (accountState && accountState->account() && accountState->account()->e2e()) {
            connectE2eSignals(accountState->account());
        }
    }

    // Connect to new accounts being added
    connect(AccountManager::instance(), &AccountManager::accountAdded,
            this, &E2EFolderManager::slotAccountAdded);

    qCInfo(lcE2eFolderManager) << "E2EFolderManager initialized for" << accounts.size() << "accounts";
}

void E2EFolderManager::slotAccountAdded(AccountState *accountState)
{
    if (accountState && accountState->account() && accountState->account()->e2e()) {
        qCInfo(lcE2eFolderManager) << "New account added, connecting E2E signals:" << accountState->account()->displayName();
        connectE2eSignals(accountState->account());
    }
}

void E2EFolderManager::connectE2eSignals(const AccountPtr &account)
{
    if (!account || !account->e2e()) {
        return;
    }

    qCInfo(lcE2eFolderManager) << "Connecting E2E initialization signal for account:" << account->displayName();

    connect(account->e2e(), &ClientSideEncryption::initializationFinished,
            this, &E2EFolderManager::slotE2eInitializationFinished, Qt::UniqueConnection);

    // If E2E is already initialized, restore folders immediately
    if (account->e2e()->isInitialized()) {
        qCInfo(lcE2eFolderManager) << "E2E already initialized for account:" << account->displayName() 
                                   << ", restoring folders immediately";
        restoreE2eFoldersForAccount(account);
    }
}

void E2EFolderManager::slotE2eInitializationFinished()
{
    qCInfo(lcE2eFolderManager) << "E2E initialization finished, restoring blacklisted E2E folders";

    auto *e2e = qobject_cast<ClientSideEncryption *>(sender());
    if (!e2e) {
        qCWarning(lcE2eFolderManager) << "slotE2eInitializationFinished called but sender is not ClientSideEncryption";
        return;
    }

    // Find the account this E2E belongs to
    const auto accounts = AccountManager::instance()->accounts();
    for (const auto &accountState : accounts) {
        if (accountState->account()->e2e() == e2e) {
            restoreE2eFoldersForAccount(accountState->account());
            break;
        }
    }
}

void E2EFolderManager::restoreE2eFoldersForAccount(const AccountPtr &account)
{
    if (!account || !account->e2e() || !account->e2e()->isInitialized()) {
        qCDebug(lcE2eFolderManager) << "Cannot restore folders - account or E2E not ready";
        return;
    }

    qCInfo(lcE2eFolderManager) << "Restoring E2E folders for account:" << account->displayName();

    auto *folderMan = FolderMan::instance();
    const auto folders = folderMan->map();

    int foldersProcessed = 0;
    for (const auto &folder : folders) {
        if (folder->accountState()->account() != account) {
            continue;
        }

        bool ok = false;
        const auto foldersToRemoveFromBlacklist = folder->journalDb()->getSelectiveSyncList(
            SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, &ok);

        if (foldersToRemoveFromBlacklist.isEmpty()) {
            continue;
        }

        qCInfo(lcE2eFolderManager) << "Found E2E folders to restore for" << folder->alias() 
                                   << ":" << foldersToRemoveFromBlacklist;

        auto blackList = folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
        qCDebug(lcE2eFolderManager) << "Current blacklist:" << blackList;

        // Remove E2E folders from blacklist
        for (const auto &pathToRemoveFromBlackList : foldersToRemoveFromBlacklist) {
            blackList.removeAll(pathToRemoveFromBlackList);
        }

        qCInfo(lcE2eFolderManager) << "New blacklist after E2E folder removal:" << blackList;

        // Update database
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackList);
        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, {});

        // Schedule remote discovery for restored folders
        for (const auto &pathToRemoteDiscover : foldersToRemoveFromBlacklist) {
            folder->journalDb()->schedulePathForRemoteDiscovery(pathToRemoteDiscover);
            qCDebug(lcE2eFolderManager) << "Scheduled remote discovery for:" << pathToRemoteDiscover;
        }

        // Schedule folder sync
        folderMan->scheduleFolder(folder);
        foldersProcessed++;
    }

    if (foldersProcessed > 0) {
        qCInfo(lcE2eFolderManager) << "Restored E2E folders for" << foldersProcessed << "sync folders";
    } else {
        qCDebug(lcE2eFolderManager) << "No E2E folders needed restoration";
    }
}

} // namespace OCC
