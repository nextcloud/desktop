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

#include "fileproviderxpc.h"

#include <QLoggingCategory>

#include "gui/accountmanager.h"
#include "gui/macOS/fileproviderxpc_mac_utils.h"

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFileProviderXPC, "nextcloud.gui.macos.fileprovider.xpc", QtInfoMsg)

FileProviderXPC::FileProviderXPC(QObject *parent)
    : QObject{parent}
{
}

void FileProviderXPC::connectToExtensions()
{
    qCInfo(lcFileProviderXPC) << "Starting file provider XPC";
    const auto managers = FileProviderXPCUtils::getDomainManagers();
    const auto domainUrls = FileProviderXPCUtils::getDomainUrlsForManagers(managers);
    const auto fpServices = FileProviderXPCUtils::getFileProviderServicesAtUrls(domainUrls);
    const auto connections = FileProviderXPCUtils::connectToFileProviderServices(fpServices);
    _clientCommServices = FileProviderXPCUtils::processClientCommunicationConnections(connections);
}

void FileProviderXPC::configureExtensions()
{
    for (const auto &extensionNcAccount : _clientCommServices.keys()) {
        qCInfo(lcFileProviderXPC) << "Sending message to client communication service";
        authenticateExtension(extensionNcAccount);
    }
}

void FileProviderXPC::authenticateExtension(const QString &extensionAccountId) const
{
    const auto accountManager = AccountManager::instance();
    Q_ASSERT(accountManager);
    const auto accountState = accountManager->accountFromUserId(extensionAccountId);
    if (!accountState) {
        qCWarning(lcFileProviderXPC) << "Account state is null for received account"
                                     << extensionAccountId;
        return;
    }

    connect(accountState.data(), &AccountState::stateChanged, this, &FileProviderXPC::slotAccountStateChanged, Qt::UniqueConnection);

    const auto account = accountState->account();
    const auto credentials = account->credentials();
    NSString *const user = credentials->user().toNSString();
    NSString *const serverUrl = account->url().toString().toNSString();
    NSString *const password = credentials->password().toNSString();

    const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(extensionAccountId);
    [clientCommService configureAccountWithUser:user
                                      serverUrl:serverUrl
                                       password:password];
}

void FileProviderXPC::unauthenticateExtension(const QString &extensionAccountId) const
{
    qCInfo(lcFileProviderXPC) << "Unauthenticating extension" << extensionAccountId;
    const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)_clientCommServices.value(extensionAccountId);
    [clientCommService removeAccountConfig];
}

void FileProviderXPC::slotAccountStateChanged(const AccountState::State state) const
{
    const auto slotSender = dynamic_cast<AccountState*>(sender());
    Q_ASSERT(slotSender);
    const auto extensionAccountId = slotSender->account()->userIdAtHostWithPort();

    switch(state) {
    case AccountState::Disconnected:
    case AccountState::ConfigurationError:
    case AccountState::NetworkError:
    case AccountState::ServiceUnavailable:
    case AccountState::MaintenanceMode:
        // Do nothing, File Provider will by itself figure out connection issue
        break;
    case AccountState::SignedOut:
    case AccountState::AskingCredentials:
    case AccountState::RedirectDetected:
        // Notify File Provider that it should show the not authenticated message
        unauthenticateExtension(extensionAccountId);
        break;
    case AccountState::Connected:
        // Provide credentials
        authenticateExtension(extensionAccountId);
        break;
    }
}

} // namespace OCC::Mac
