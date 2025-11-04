/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include "accountstate.h"

namespace OCC {

class Account;

namespace Mac {

class FileProviderDomainManager : public QObject
{
    Q_OBJECT

public:
    explicit FileProviderDomainManager(QObject * const parent = nullptr);
    ~FileProviderDomainManager() override;

    static AccountStatePtr accountStateFromFileProviderDomainIdentifier(const QString &domainIdentifier);
    QString fileProviderDomainIdentifierFromAccountId(const QString &accountId);
    
    void start();
    void* domainForAccount(const OCC::AccountState * const accountState);

signals:
    void domainSetupComplete();

public slots:
    void addFileProviderDomainForAccount(const OCC::AccountState * const accountState);

private slots:
    void setupFileProviderDomains();
    void updateFileProviderDomains();

    void removeFileProviderDomainForAccount(const OCC::AccountState * const accountState);
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
