/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QList>

#include "accountstate.h"

#ifdef __OBJC__
@class NSFileProviderDomain;
#else
struct NSFileProviderDomain;
#endif

namespace OCC {

class Account;

namespace Mac {

class FileProviderDomainManager : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderDomainManager(QObject * const parent = nullptr);
    ~FileProviderDomainManager() override;

    /**
     * @brief Add a new file provider domain for the given account.
     * @return The raw identifier of the added domain as a string.
     */
    QString addDomainForAccount(const OCC::AccountState * const accountState);

    /**
     * @brief Remove all file provider domains managed by this application.
     */
    void removeAllDomains();

    /**
     * @brief Reconnect all file provider domains.
     */
    void reconnectAll();

    /**
     * @brief Remove a file provider domain independent from an account.
     */
    void removeDomain(NSFileProviderDomain *domain);

    /**
     * @brief Remove the file provider domain for the given account.
     */
    void removeDomainByAccount(const OCC::AccountState * const accountState);

    void start();

    [[nodiscard]] QList<NSFileProviderDomain *> getDomains() const;

    NSFileProviderDomain *domainForAccount(const OCC::Account *account) const;

private slots:
    void disconnectFileProviderDomainForAccount(const OCC::AccountState * const accountState, const QString &reason);
    void reconnectFileProviderDomainForAccount(const OCC::AccountState * const accountState);

    void signalEnumeratorChanged(const OCC::Account * const account);
    void slotAccountStateChanged(const OCC::AccountState * const accountState);

private:
    class MacImplementation;
    std::unique_ptr<MacImplementation> d;
};

} // namespace Mac

} // namespace OCC
