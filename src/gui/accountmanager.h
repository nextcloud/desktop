/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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
public:
    static AccountManager *instance();
    ~AccountManager() override {}

    /**
     * Saves the accounts to a given settings file
     */
    void save(bool saveCredentials = true);

    /**
     * Creates account objects from a given settings file.
     *
     * Returns false if there was an error reading the settings,
     * but note that settings not existing is not an error.
     */
    bool restore();

    /**
     * Add this account in the list of saved accounts.
     * Typically called from the wizard
     */
    AccountStatePtr addAccount(const AccountPtr &newAccount);

    /**
     * remove all accounts
     */
    void shutdown();

    /**
     * Return a map of all accounts.
     * (this is a map of QSharedPointer for internal reasons, one should normally not keep a copy of them)
     */
    const QMap<QUuid, AccountStatePtr> &accounts() { return _accounts; }

    /**
     * Return the account state pointer for an account identified by its display name
     */
    Q_DECL_DEPRECATED_X("Please use the uuid to specify the account") AccountStatePtr account(const QString &name);

    /**
     * Return the account state pointer for an account identified by its display name
     */
    AccountStatePtr account(const QUuid uuid);

    /**
     * Delete the AccountState
     */
    void deleteAccount(AccountStatePtr account);


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

    /**
     * Returns a sorted list of displayNames
     */
    QStringList accountNames() const;

private:
    // saving and loading Account to settings
    void saveAccountHelper(Account *account, QSettings &settings, bool saveCredentials = true);
    AccountPtr loadAccountHelper(QSettings &settings);

    bool restoreFromLegacySettings();

    bool isAccountIdAvailable(const QString &id) const;
    QString generateFreeAccountId() const;

    // Adds an account to the tracked list, emitting accountAdded()
    void addAccountState(AccountStatePtr accountState);

public slots:
    /// Saves account data, not including the credentials
    void saveAccount(Account *a);

Q_SIGNALS:
    void accountAdded(AccountStatePtr account);
    void accountRemoved(AccountStatePtr account);

private:
    AccountManager() {}
    QMap<QUuid, AccountStatePtr> _accounts;
    /// Account ids from settings that weren't read
    QSet<QString> _additionalBlockedAccountIds;
};
}
