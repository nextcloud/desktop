/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include "accountstate.h"

#ifdef __OBJC__
@class NSFileProviderDomain;
#else
class NSFileProviderDomain;
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

    void start();

    NSFileProviderDomain *domainForAccount(const OCC::Account *account) const;

signals:
    void domainSetupComplete();

public slots:
    /**
     * @brief Add a new file provider domain for the given account.
     * @return The raw identifier of the added domain as a string.
     */
    QString addFileProviderDomainForAccount(const OCC::AccountState * const accountState);

    /**
     * @brief Remove the file provider domain for the given account.
     */
    void removeFileProviderDomainForAccount(const OCC::AccountState * const accountState);

private slots:
    void updateFileProviderDomains();

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
