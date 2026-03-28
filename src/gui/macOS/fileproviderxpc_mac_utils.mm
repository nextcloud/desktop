/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderxpc_mac_utils.h"

#import "AppProtocol.h"
#import "fileprovider.h"

#include <QLoggingCategory>

#include "gui/accountmanager.h"

namespace {
const char *const clientCommunicationServiceName = "com.nextcloud.desktopclient.ClientCommunicationService";
NSString *const nsClientCommunicationServiceName = [NSString stringWithUTF8String:clientCommunicationServiceName];
}

namespace OCC::Mac::FileProviderXPCUtils {

Q_LOGGING_CATEGORY(lcFileProviderXPCUtils, "nextcloud.gui.macos.fileprovider.xpc.utils", QtInfoMsg)

NSArray<NSFileProviderManager *> *getDomainManagers()
{
    dispatch_group_t group = dispatch_group_create();
    __block NSMutableArray<NSFileProviderManager *> *const managers = NSMutableArray.array;

    dispatch_group_enter(group);

    // Set up connections for each domain
    [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *const domains, NSError *const error) {
        if (error != nil) {
            qCWarning(lcFileProviderXPCUtils) << "Error getting file provider domains" << error;
            dispatch_group_leave(group);
            return;
        }

        for (NSFileProviderDomain *const domain in domains) {
            qCInfo(lcFileProviderXPCUtils) << "Found file provider domain" << domain.identifier;

            NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
            if (manager) {
                [managers addObject:manager];
            } else {
                qCWarning(lcFileProviderXPCUtils) << "Could not get manager for domain" << domain.identifier;
            }
        }

        dispatch_group_leave(group);
    }];

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    if (managers.count == 0) {
        qCWarning(lcFileProviderXPCUtils) << "No file provider domains found!";
    }

    return managers.copy;
}

// TODO: This should work for all service names, not just the communication service!
NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *getFileProviderServices(NSArray<NSFileProviderManager *> *managers)
{
    NSMutableArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *const fpServices = NSMutableArray.array;
    dispatch_group_t group = dispatch_group_create();

    for (NSFileProviderManager *const manager in managers) {
        dispatch_group_enter(group);

        [manager getServiceWithName:nsClientCommunicationServiceName
                     itemIdentifier:NSFileProviderRootContainerItemIdentifier
                  completionHandler:^(NSFileProviderService *const service, NSError *const error) {

            if (error != nil) {
                qCWarning(lcFileProviderXPCUtils) << "Failed to resolve service for file provider domain: " << error;
            } else if (service == nil) {
                qCWarning(lcFileProviderXPCUtils) << "Service is nil!";
            } else {
                [fpServices addObject:@{service.name: service}];
            }

            dispatch_group_leave(group);
        }];
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    return fpServices.copy;
}

NSArray<NSURL *> *getDomainUrlsForManagers(NSArray<NSFileProviderManager *> *managers)
{
    dispatch_group_t group = dispatch_group_create();
    __block NSMutableArray<NSURL *> *const urls = NSMutableArray.array;

    for (NSFileProviderManager *const manager in managers) {

        dispatch_group_enter(group);

        [manager getUserVisibleURLForItemIdentifier:NSFileProviderRootContainerItemIdentifier
                                  completionHandler:^(NSURL *const url, NSError *const error){
            if (error != nil) {
                qCWarning(lcFileProviderXPCUtils) << "Error getting user visible url" << error;
                dispatch_group_leave(group);
                return;
            }

            qCDebug(lcFileProviderXPCUtils) << "Got user visible url" << url;
            [urls addObject:url];
            dispatch_group_leave(group);
        }];
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    if (urls.count == 0) {
        qCWarning(lcFileProviderXPCUtils) << "No urls found";
    }

    return urls.copy;
}

NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *getFileProviderServicesAtUrls(NSArray<NSURL *> *urls)
{
    dispatch_group_t group = dispatch_group_create();
    NSMutableArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *const fpServices = NSMutableArray.array;

    for (NSURL *const url in urls) {
        dispatch_group_enter(group);

        [NSFileManager.defaultManager getFileProviderServicesForItemAtURL:url
                                                        completionHandler:^(NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *const services, NSError *const error){
            if (error != nil) {
                qCWarning(lcFileProviderXPCUtils) << "Error getting file provider services" << error;
                dispatch_group_leave(group);
                return;
            }

            qCInfo(lcFileProviderXPCUtils) << "Got file provider services for"
                                      << url.absoluteString
                                      << "has number of services:"
                                      << services.count;
            [fpServices addObject:services];
            dispatch_group_leave(group);
        }];
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    if (fpServices.count == 0) {
        qCWarning(lcFileProviderXPCUtils) << "No file provider services found";
    }

    return fpServices.copy;
}

NSArray<NSXPCConnection *> *connectToFileProviderServices(NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *fpServices)
{
    dispatch_group_t group = dispatch_group_create();
    NSMutableArray<NSXPCConnection *> *const connections = NSMutableArray.array;

    for (NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *const services in fpServices) {
        NSArray<NSFileProviderServiceName> *const serviceNamesArray = services.allKeys;

        for (NSFileProviderServiceName serviceName in serviceNamesArray) {
            qCInfo(lcFileProviderXPCUtils) << "Got service" << serviceName;

            if (![serviceName isEqualToString:nsClientCommunicationServiceName]) {
                continue;
            }

            NSFileProviderService *const service = services[serviceName];
            dispatch_group_enter(group);

            [service getFileProviderConnectionWithCompletionHandler:^(NSXPCConnection *const connection, NSError *const error){
                if (error != nil) {
                    qCWarning(lcFileProviderXPCUtils) << "Error getting file provider connection" << error;
                    dispatch_group_leave(group);
                    return;
                }

                qCInfo(lcFileProviderXPCUtils) << "Got file provider connection" << connection;

                if (connection == nil) {
                    qCWarning(lcFileProviderXPCUtils) << "Connection is nil";
                    dispatch_group_leave(group);
                    return;
                }

                [connections addObject:connection];
                dispatch_group_leave(group);
            }];
        }
    }

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    return connections.copy;
}

void configureFileProviderConnection(NSXPCConnection *const connection)
{
    Q_ASSERT(connection != nil);
    connection.interruptionHandler = ^{
        qCInfo(lcFileProviderXPCUtils) << "File provider connection interrupted";
    };
    connection.invalidationHandler = ^{
        qCInfo(lcFileProviderXPCUtils) << "File provider connection invalidated";
    };
    [connection resume];
}

NSObject *getRemoteServiceObject(NSXPCConnection *const connection, Protocol *const protocol)
{
    Q_ASSERT(connection != nil);
    Q_ASSERT(protocol != nil);
    const id remoteServiceObject = [connection remoteObjectProxyWithErrorHandler:^(NSError *const error){
        qCWarning(lcFileProviderXPCUtils) << "Error getting remote object proxy" << error;
    }];
    if (remoteServiceObject == nil) {
        return nil;
    }
    if (![remoteServiceObject conformsToProtocol:@protocol(ClientCommunicationProtocol)]) {
        qCWarning(lcFileProviderXPCUtils) << "Remote service object does not conform to protocol";
        return nil;
    }
    return remoteServiceObject;
}

NSString *getFileProviderDomainIdentifier(NSObject<ClientCommunicationProtocol> *const clientCommService)
{
    Q_ASSERT(clientCommService != nil);
    __block NSString *domainIdentifier;
    dispatch_group_t group = dispatch_group_create();
    dispatch_group_enter(group);

    [clientCommService getFileProviderDomainIdentifierWithCompletionHandler:^(NSString *const extensionAccountId, NSError *const error){
        if (error != nil) {
            qCWarning(lcFileProviderXPCUtils) << "Error getting domain id from file provider service" << error;
            dispatch_group_leave(group);

            return;
        }

        domainIdentifier = [[NSString alloc] initWithString:extensionAccountId];
        dispatch_group_leave(group);
    }];

    dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

    return domainIdentifier;
}

QHash<QString, void*> processClientCommunicationConnections(NSArray<NSXPCConnection *> *const connections, OCC::Mac::FileProviderService *const service)
{
    QHash<QString, void*> clientCommServices;

    for (NSXPCConnection * const connection in connections) {
        const auto exportedInterfaceProtocol = @protocol(AppProtocol);
        const auto remoteObjectInterfaceProtocol = @protocol(ClientCommunicationProtocol);
        connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:exportedInterfaceProtocol];
        
        // Set the FileProviderService delegate as the exported object
        if (service) {
            connection.exportedObject = (id<AppProtocol>)service->delegate();
        } else {
            qCWarning(lcFileProviderXPCUtils) << "FileProviderService is null, cannot set exported object";
        }
        
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:remoteObjectInterfaceProtocol];
        configureFileProviderConnection(connection);

        const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)getRemoteServiceObject(connection, remoteObjectInterfaceProtocol);

        if (clientCommService == nil) {
            qCWarning(lcFileProviderXPCUtils) << "Client communication service is nil";
            continue;
        }

        [clientCommService retain];

        const auto domainIdentifier = getFileProviderDomainIdentifier(clientCommService);

        if (domainIdentifier == nil) {
            qCWarning(lcFileProviderXPCUtils) << "Could not retrieve domain id from file provider service";
            continue;
        }

        qCInfo(lcFileProviderXPCUtils) << "Got domain id"
                                       << domainIdentifier.UTF8String
                                       << "from file provider service";

        clientCommServices.insert(QString::fromNSString(domainIdentifier), clientCommService);
    }

    return clientCommServices;
}

} // namespace OCC::Mac::FileProviderXPCUtils
