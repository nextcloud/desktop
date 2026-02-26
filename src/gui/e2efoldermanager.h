// SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include "account.h"
#include "accountmanager.h"

namespace OCC {

/**
 * @brief Stateless bridge between E2E encryption and folder management
 * 
 * This class acts as a mediator that:
 * - Listens to E2E initialization signals from all accounts
 * - Coordinates folder restoration when E2E becomes ready
 * - Keeps E2E concerns separate from FolderMan's core responsibilities
 * 
 * @ingroup gui
 */
class E2EFolderManager : public QObject
{
    Q_OBJECT

public:
    static E2EFolderManager *instance();
    ~E2EFolderManager() override;

    /**
     * Initialize the manager and connect to existing accounts
     * Should be called once during application startup
     */
    void initialize();

private slots:
    /**
     * Called when E2E initialization completes for any account
     * Triggers restoration of blacklisted E2E folders for that account
     */
    void slotE2eInitializationFinished();

    /**
     * Called when a new account is added
     * Connects E2E signals for the new account
     */
    void slotAccountAdded(AccountState *accountState);

private:
    E2EFolderManager(QObject *parent = nullptr);

    /**
     * Connect E2E initialization signals for an account
     * @param account The account to connect signals for
     */
    void connectE2eSignals(const AccountPtr &account);

    /**
     * Restore E2E folders for a specific account
     * Removes E2E folders from blacklist and schedules sync
     * @param account The account to restore folders for
     */
    void restoreE2eFoldersForAccount(const AccountPtr &account);

    static E2EFolderManager *_instance;
};

} // namespace OCC
