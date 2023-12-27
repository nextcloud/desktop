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

#import <FileProvider/FileProvider.h>

#import "ClientCommunicationProtocol.h"

namespace {
    static const auto clientCommunicationServiceName = "com.nextcloud.desktopclient.ClientCommunicationService";
    static NSString *const nsClientCommunicationServiceName = [NSString stringWithUTF8String:clientCommunicationServiceName];
}

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderXPC, "nextcloud.gui.macos.fileprovider.xpc", QtInfoMsg)

namespace XPCUtils
{

NSArray<NSFileProviderManager *> *getDomainManagers()
{
    dispatch_group_t group = dispatch_group_create();
    __block NSMutableArray<NSFileProviderManager *> *managers = NSMutableArray.array;

    dispatch_group_enter(group);

    // Set up connections for each domain
    [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *const domains, NSError *const error){
        if (error != nil) {
            qCWarning(lcFileProviderXPC) << "Error getting domains" << error;
            dispatch_group_leave(group);
            return;
        }

        for (NSFileProviderDomain *const domain in domains) {
            qCInfo(lcFileProviderXPC) << "Got domain" << domain.identifier;
            NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
            [managers addObject:manager];
        }

        dispatch_group_leave(group);
    }];

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    if (managers.count == 0) {
        qCWarning(lcFileProviderXPC) << "No domains found";
    }

    return managers.copy;
}

NSArray<NSURL *> *getDomainUrlsForManagers(NSArray<NSFileProviderManager *> *managers)
{
    dispatch_group_t group = dispatch_group_create();
    __block NSMutableArray<NSURL *> *urls = NSMutableArray.array;

    for (NSFileProviderManager *const manager in managers) {

        dispatch_group_enter(group);

        [manager getUserVisibleURLForItemIdentifier:NSFileProviderRootContainerItemIdentifier
                                  completionHandler:^(NSURL *const url, NSError *const error){
            if (error != nil) {
                qCWarning(lcFileProviderXPC) << "Error getting user visible url" << error;
                dispatch_group_leave(group);
                return;
            }

            qCDebug(lcFileProviderXPC) << "Got user visible url" << url;
            [urls addObject:url];
            dispatch_group_leave(group);
        }];
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    if (urls.count == 0) {
        qCWarning(lcFileProviderXPC) << "No urls found";
    }

    return urls.copy;
}

} // namespace XPCUtils

FileProviderXPC::FileProviderXPC(QObject *parent)
    : QObject{parent}
{
}

void FileProviderXPC::start()
{
    qCInfo(lcFileProviderXPC) << "Starting file provider XPC";

    const auto managers = XPCUtils::getDomainManagers();
    const auto domainUrls = XPCUtils::getDomainUrlsForManagers(managers);
    dispatch_group_t group = dispatch_group_create();
    NSMutableArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *const fpServices = NSMutableArray.array;

    for (NSURL *const url in domainUrls) {
        dispatch_group_enter(group);

        [NSFileManager.defaultManager getFileProviderServicesForItemAtURL:url
                                                        completionHandler:^(NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *const services, NSError *const error){
            if (error != nil) {
                qCWarning(lcFileProviderXPC) << "Error getting file provider services" << error;
                dispatch_group_leave(group);
                return;
            }

            qCInfo(lcFileProviderXPC) << "Got file provider services for"
                                      << url.absoluteString
                                      << "has number of services:"
                                      << services.count;
            [fpServices addObject:services];
            dispatch_group_leave(group);
        }];
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    if (fpServices.count == 0) {
        qCWarning(lcFileProviderXPC) << "No file provider services found";
        return;
    }

    NSMutableArray<NSXPCConnection *> *const connections = NSMutableArray.array;

    for (NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *const services in fpServices) {
        NSArray<NSFileProviderServiceName> *const serviceNamesArray = services.allKeys;

        for (NSFileProviderServiceName serviceName in serviceNamesArray) {
            qCInfo(lcFileProviderXPC) << "Got service" << serviceName;

            if (![serviceName isEqualToString:nsClientCommunicationServiceName]) {
                continue;
            }

            NSFileProviderService *const service = services[serviceName];
            dispatch_group_enter(group);

            [service getFileProviderConnectionWithCompletionHandler:^(NSXPCConnection *const connection, NSError *const error){
                if (error != nil) {
                    qCWarning(lcFileProviderXPC) << "Error getting file provider connection" << error;
                    dispatch_group_leave(group);
                    return;
                }

                qCInfo(lcFileProviderXPC) << "Got file provider connection" << connection;

                if (connection == nil) {
                    qCWarning(lcFileProviderXPC) << "Connection is nil";
                    dispatch_group_leave(group);
                    return;
                }

                [connection retain];
                [connections addObject:connection];
                dispatch_group_leave(group);
            }];
        }
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    NSMutableDictionary<NSString *, NSObject<ClientCommunicationProtocol>*> *const clientCommServices = NSMutableDictionary.dictionary;

    for (NSXPCConnection * const connection in connections) {
        Q_ASSERT(connection != nil);
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(ClientCommunicationProtocol)];
        connection.interruptionHandler = ^{
            qCInfo(lcFileProviderXPC) << "File provider connection interrupted";
        };
        connection.invalidationHandler = ^{
            qCInfo(lcFileProviderXPC) << "File provider connection invalidated";
        };
        [connection resume];

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

    for (NSString *const extensionNcAccount in clientCommServices) {
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

        NSObject<ClientCommunicationProtocol> *const clientCommService = [clientCommServices objectForKey:extensionNcAccount];
        [clientCommService configureAccountWithUser:user
                                          serverUrl:serverUrl
                                           password:password];
    }
}

} // namespace OCC

} // namespace Mac
