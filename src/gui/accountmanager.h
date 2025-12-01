/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "account.h"
#include "accountstate.h"

namespace OCC {

/**
   @brief The AccountManager class
   @ingroup gui
*/
class AccountManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool forceLegacyImport READ forceLegacyImport WRITE setForceLegacyImport NOTIFY forceLegacyImportChanged)

public:
    enum AccountsRestoreResult {
        AccountsRestoreFailure = 0,
        AccountsNotFound,
        AccountsRestoreSuccess,
        AccountsRestoreSuccessFromLegacyVersion,
        AccountsRestoreSuccessWithSkipped
    };
    Q_ENUM (AccountsRestoreResult);

    static AccountManager *instance();
    ~AccountManager() override = default;

    /**
     * Creates account objects from a given settings file.
     *
     * Returns false if there was an error reading the settings,
     * but note that settings not existing is not an error.
     */
    AccountsRestoreResult restore(const bool alsoRestoreLegacySettings = true);

    /**
     * Add this account in the list of saved accounts.
     * Typically called from the wizard
     */
    AccountState *addAccount(const AccountPtr &newAccount);

    /**
     * Return a list of all accounts.
     * (this is a list of QSharedPointer for internal reasons, one should normally not keep a copy of them)
     */
    [[nodiscard]] QList<AccountStatePtr> accounts() const;

    /**
     * Return the account state pointer for an account identified by its display name
     */
    AccountStatePtr account(const QString &name);

    /**
     * Return the account state pointer for an account from its id
     */

    [[nodiscard]] AccountStatePtr accountFromUserId(const QString &id) const;

    /**
     * Returns whether the account setup will force an import of
     * legacy clients' accounts (true), or ask first (false)
     */
    [[nodiscard]] bool forceLegacyImport() const;

    /**
     * Creates an account and sets up some basic handlers.
     * Does *not* add the account to the account manager just yet.
     */
    static AccountPtr createAccount();

    /**
     * Returns the list of settings keys that can't be read because
     * they are from the future.
     */
    static void backwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys);

public slots:
    /// Saves account data when adding user, when updating e.g. dav user, not including the credentials
    void saveAccount(const OCC::AccountPtr &newAccountData);

    /// Saves account state data, not including the account
    void saveAccountState(OCC::AccountState *a);

    /// Saves the accounts to a given settings file
    void save(bool saveCredentials = true);

    /// Delete the AccountState
    void deleteAccount(OCC::AccountState *account);

    /// Remove all accounts
    void shutdown();

    void setForceLegacyImport(const bool forceLegacyImport);

signals:
    void accountAdded(OCC::AccountState *account);
    void accountRemoved(OCC::AccountState *account);
    void accountSyncConnectionRemoved(OCC::AccountState *account);
    void removeAccountFolders(OCC::AccountState *account);
    void forceLegacyImportChanged();
    void capabilitiesChanged();
    void accountListInitialized();

private:
    // saving and loading Account to settings
    void saveAccountHelper(const AccountPtr &account, QSettings &settings, bool saveCredentials = true);
    AccountPtr loadAccountHelper(QSettings &settings);
    void migrateNetworkSettings(const AccountPtr &account, const QSettings &settings);

    bool restoreFromLegacySettings();

    [[nodiscard]] bool isAccountIdAvailable(const QString &id) const;
    [[nodiscard]] QString generateFreeAccountId() const;

    // Adds an account to the tracked list, emitting accountAdded()
    void addAccountState(AccountState *const accountState);

    // update config serverHasValidSubscription when accounts list changes
    void updateServerHasValidSubscriptionConfig();
    void updateServerDesktopEnterpriseUpdateChannel();

    AccountManager() = default;
    QList<AccountStatePtr> _accounts;
    /// Account ids from settings that weren't read
    QSet<QString> _additionalBlockedAccountIds;
    bool _forceLegacyImport = false;
};
}
