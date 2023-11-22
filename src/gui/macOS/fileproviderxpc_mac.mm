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

#import <FileProvider/FileProvider.h>

#import "ClientCommunicationProtocol.h"

namespace {
    static const auto clientCommunicationServiceName = "com.nextcloud.desktopclient.ClientCommunicationService";
    static NSString *const nsClientCommunicationServiceName = [NSString stringWithUTF8String:clientCommunicationServiceName];
}

namespace OCC {

namespace Mac {

Q_LOGGING_CATEGORY(lcFileProviderXPC, "nextcloud.gui.macos.fileprovider.xpc", QtInfoMsg)

FileProviderXPC::FileProviderXPC(QObject *parent)
    : QObject{parent}
{
}

void FileProviderXPC::start()
{
    qCInfo(lcFileProviderXPC) << "Starting file provider XPC";

    dispatch_group_t group = dispatch_group_create();
    __block NSArray<NSFileProviderDomain *> *fpDomains = nil;

    dispatch_group_enter(group);

    // Set up connections for each domain
    [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *const domains, NSError *const error){
        if (error != nil) {
            qCWarning(lcFileProviderXPC) << "Error getting domains" << error;
            dispatch_group_leave(group);
            return;
        }

        fpDomains = domains;
        dispatch_group_leave(group);
    }];

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    if (fpDomains == nil || fpDomains.count == 0) {
        qCWarning(lcFileProviderXPC) << "No domains found";
        return;
    }

    __block NSMutableArray<NSURL *> *urls = NSMutableArray.array;

    for (NSFileProviderDomain *const domain in fpDomains) {
        qCDebug(lcFileProviderXPC) << "Got domain" << domain.identifier;
        dispatch_group_enter(group);

        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];

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
        return;
    }

    NSMutableArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *const fpServices = NSMutableArray.array;

    for (NSURL *const url in urls) {
        dispatch_group_enter(group);

        [NSFileManager.defaultManager getFileProviderServicesForItemAtURL:url
                                                        completionHandler:^(NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *const services, NSError *const error){
            if (error != nil) {
                qCWarning(lcFileProviderXPC) << "Error getting file provider services" << error;
                dispatch_group_leave(group);
                return;
            }

            [fpServices addObject:services];
            dispatch_group_leave(group);
        }];
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    if (fpServices.count == 0) {
        qCWarning(lcFileProviderXPC) << "No file provider services found";
        return;
    }

    for (NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *const services in fpServices) {
        NSArray<NSFileProviderServiceName> *const serviceNamesArray = services.allKeys;

        for (NSFileProviderServiceName serviceName in serviceNamesArray) {
            qCDebug(lcFileProviderXPC) << "Got service" << serviceName;

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

                qCDebug(lcFileProviderXPC) << "Got file provider connection" << connection;

                if (connection == nil) {
                    qCWarning(lcFileProviderXPC) << "Connection is nil";
                    dispatch_group_leave(group);
                    return;
                }

                connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(ClientCommunicationProtocol)];
                [connection resume];
                const id<ClientCommunicationProtocol> clientCommService = (id<ClientCommunicationProtocol>)[connection remoteObjectProxyWithErrorHandler:^(NSError *const error){
                    qCWarning(lcFileProviderXPC) << "Error getting remote object proxy" << error;
                    dispatch_group_leave(group);
                }];

                if (clientCommService == nil) {
                    qCWarning(lcFileProviderXPC) << "Client communication service is nil";
                    dispatch_group_leave(group);
                    return;
                }

                dispatch_group_leave(group);
            }];
        }
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
}

} // namespace OCC

} // namespace Mac
