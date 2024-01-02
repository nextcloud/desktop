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
    dispatch_group_t group = dispatch_group_create();

    NSMutableDictionary<NSString *, NSObject<ClientCommunicationProtocol>*> *const clientCommServices = NSMutableDictionary.dictionary;

    for (NSXPCConnection * const connection in connections) {
        const auto remoteObjectInterfaceProtocol = @protocol(ClientCommunicationProtocol);
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:remoteObjectInterfaceProtocol];
        FileProviderXPCUtils::configureFileProviderConnection(connection);

        const id remoteServiceObject = [connection remoteObjectProxyWithErrorHandler:^(NSError *const error){
            qCWarning(lcFileProviderXPC) << "Error getting remote object proxy" << error;
        }];

        if (![remoteServiceObject conformsToProtocol:@protocol(ClientCommunicationProtocol)]) {
            qCWarning(lcFileProviderXPC) << "Remote service object does not conform to protocol";
            continue;
        }

        NSObject<ClientCommunicationProtocol> *const clientCommService = (NSObject<ClientCommunicationProtocol> *)remoteServiceObject;
        if (clientCommService == nil) {
            qCWarning(lcFileProviderXPC) << "Client communication service is nil";
            continue;
        }

        [clientCommService retain];
        __block NSString *extensionNcAccount = @"";
        dispatch_group_enter(group);
        [clientCommService getExtensionAccountIdWithCompletionHandler:^(NSString *const extensionAccountId, NSError *const error){
            if (error != nil) {
                qCWarning(lcFileProviderXPC) << "Error getting extension account id" << error;
                dispatch_group_leave(group);
                return;
            }

            extensionNcAccount = [NSString stringWithString:extensionAccountId];
            [extensionNcAccount retain];
            dispatch_group_leave(group);
        }];

        dispatch_group_wait(group, DISPATCH_TIME_FOREVER); // Do not edit the NSDictionary concurrently

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

} // namespace OCC

} // namespace Mac
