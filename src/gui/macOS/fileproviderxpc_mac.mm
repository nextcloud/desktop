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

#import "ClientCommunicationProtocol.h"

namespace OCC {

namespace Mac {

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
    processConnections(connections);
}

void FileProviderXPC::processConnections(NSArray *const connections)
{
    NSMutableDictionary<NSString *, NSObject<ClientCommunicationProtocol>*> *const clientCommServices = NSMutableDictionary.dictionary;

    for (NSXPCConnection * const connection in connections) {
        const auto remoteObjectInterfaceProtocol = @protocol(ClientCommunicationProtocol);
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:remoteObjectInterfaceProtocol];
        FileProviderXPCUtils::configureFileProviderConnection(connection);

        const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)FileProviderXPCUtils::getRemoteServiceObject(connection, remoteObjectInterfaceProtocol);
        if (clientCommService == nil) {
            qCWarning(lcFileProviderXPC) << "Client communication service is nil";
            continue;
        }
        [clientCommService retain];

        const auto extensionNcAccount = FileProviderXPCUtils::getExtensionAccountId(clientCommService);
        if (extensionNcAccount == nil) {
            qCWarning(lcFileProviderXPC) << "Extension account id is nil";
            continue;
        }
        qCInfo(lcFileProviderXPC) << "Got extension account id" << extensionNcAccount.UTF8String;
        [clientCommServices setObject:clientCommService forKey:extensionNcAccount];
    }

    _clientCommServices = clientCommServices.copy;
}

void FileProviderXPC::configureExtensions()
{
    for (NSString *const extensionNcAccount in _clientCommServices) {
        qCInfo(lcFileProviderXPC) << "Sending message to client communication service";

        const auto qExtensionNcAccount = QString::fromNSString(extensionNcAccount);
        const auto accountManager = AccountManager::instance();

        Q_ASSERT(accountManager);

        const auto accountState = accountManager->accountFromUserId(qExtensionNcAccount);
        if (!accountState) {
            qCWarning(lcFileProviderXPC) << "Account state is null for received account"
                                         << qExtensionNcAccount;
            return;
        }

        const auto account = accountState->account();
        const auto credentials = account->credentials();
        NSString *const user = credentials->user().toNSString();
        NSString *const serverUrl = account->url().toString().toNSString();
        NSString *const password = credentials->password().toNSString();

        NSObject<ClientCommunicationProtocol> *const clientCommService = [_clientCommServices objectForKey:extensionNcAccount];
        [clientCommService configureAccountWithUser:user
                                          serverUrl:serverUrl
                                           password:password];
    }
}

void FileProviderXPC::unauthenticateExtension(const QString &extensionAccountId)
{
    qCInfo(lcFileProviderXPC) << "Unauthenticating extension" << extensionAccountId;
    NSString *const nsExtensionAccountId = extensionAccountId.toNSString();
    NSObject<ClientCommunicationProtocol> *const clientCommService = [_clientCommServices objectForKey:nsExtensionAccountId];
    [clientCommService removeAccountConfig];
}

} // namespace OCC

} // namespace Mac
