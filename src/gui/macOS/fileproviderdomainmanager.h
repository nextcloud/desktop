/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
    static QString fileProviderDomainIdentifierFromAccountState(const AccountStatePtr &accountState);
    
    void start();

signals:
    void domainSetupComplete();

private slots:
    void setupFileProviderDomains();
    void updateFileProviderDomains();

    void addFileProviderDomainForAccount(const OCC::AccountState * const accountState);
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
