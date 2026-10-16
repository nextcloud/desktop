/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderxpc_mac_utils.h"
#include "fileproviderutils_mac.h"

#import "../../../shell_integration/MacOSX/NextcloudFileProviderKit/Sources/NextcloudFileProviderXPC/include/NextcloudFileProviderXPC.h"
#import "fileprovider.h"

#include <QLoggingCategory>

#include "gui/accountmanager.h"

static_assert(__has_feature(objc_arc), "File Provider XPC utilities require ARC.");

namespace {
const char *const clientCommunicationServiceName = "com.nextcloud.desktopclient.ClientCommunicationService";
NSString *const nsClientCommunicationServiceName = [NSString stringWithUTF8String:clientCommunicationServiceName];
constexpr int64_t fileProviderRequestWaitDelta = 1000000000; // 1 second
}

namespace OCC::Mac::FileProviderXPCUtils {

Q_LOGGING_CATEGORY(lcFileProviderXPCUtils, "nextcloud.gui.macos.fileprovider.xpc.utils", QtInfoMsg)

NSArray<NSFileProviderManager *> *getDomainManagers()
{
    const auto domains = FileProviderUtils::fileProviderDomains();

    NSMutableArray<NSFileProviderManager *> *const managers = NSMutableArray.array;
    for (NSFileProviderDomain *const domain in domains) {
        qCInfo(lcFileProviderXPCUtils) << "Found file provider domain" << domain.identifier;

        NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];
        if (manager) {
            [managers addObject:manager];
        } else {
            qCWarning(lcFileProviderXPCUtils) << "Could not get manager for domain" << domain.identifier;
        }
    }

    if (managers.count == 0) {
        qCWarning(lcFileProviderXPCUtils) << "No file provider domains found!";
    }

    return managers.copy;
}

// TODO: This should work for all service names, not just the communication service!
NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *getFileProviderServices(NSArray<NSFileProviderManager *> *managers)
{
    return collectSetupResults(managers.count, "resolving file provider services", ^(SetupReplyHandler complete) {
        for (NSFileProviderManager *const manager in managers) {
            [manager getServiceWithName:nsClientCommunicationServiceName
                         itemIdentifier:NSFileProviderRootContainerItemIdentifier
                      completionHandler:^(NSFileProviderService *const service, NSError *const error) {

                if (error != nil) {
                    qCWarning(lcFileProviderXPCUtils) << "Failed to resolve service for file provider domain: " << error;
                } else if (service == nil) {
                    qCWarning(lcFileProviderXPCUtils) << "Service is nil!";
                } else if (service.name == nil) {
                    qCWarning(lcFileProviderXPCUtils) << "Service has no name";
                } else {
                    complete(@[@{service.name: service}]);
                    return;
                }

                complete(nil);
            }];
        }
    });
}

NSArray<NSXPCConnection *> *connectToFileProviderServices(NSArray<NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *> *fpServices)
{
    return collectSetupResults(fpServices.count, "getting file provider connections", ^(SetupReplyHandler complete) {
        for (NSDictionary<NSFileProviderServiceName, NSFileProviderService *> *const services in fpServices) {
            NSFileProviderService *const service = services[nsClientCommunicationServiceName];
            if (service == nil) {
                complete(nil);
                continue;
            }

            qCInfo(lcFileProviderXPCUtils) << "Got service" << nsClientCommunicationServiceName;
            [service getFileProviderConnectionWithCompletionHandler:^(NSXPCConnection *const connection, NSError *const error){
                if (error != nil) {
                    qCWarning(lcFileProviderXPCUtils) << "Error getting file provider connection" << error;
                    [connection invalidate];
                    complete(nil);
                    return;
                }

                qCInfo(lcFileProviderXPCUtils) << "Got file provider connection" << connection;

                if (connection == nil) {
                    qCWarning(lcFileProviderXPCUtils) << "Connection is nil";
                    complete(nil);
                    return;
                }

                if (!complete(@[connection])) {
                    [connection invalidate];
                }
            }];
        }
    });
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
    __block QString domainIdentifier;
    __block auto gotDomainIdentifier = false;
    dispatch_group_t group = dispatch_group_create();
    dispatch_group_enter(group);

    [clientCommService getFileProviderDomainIdentifierWithCompletionHandler:^(NSString *const extensionAccountId, NSError *const error) {
        if (error != nil) {
            qCWarning(lcFileProviderXPCUtils) << "Error getting domain id from file provider service" << error;
        } else if (extensionAccountId == nil) {
            qCWarning(lcFileProviderXPCUtils) << "File provider service returned no domain id";
        } else {
            domainIdentifier = QString::fromNSString(extensionAccountId);
            gotDomainIdentifier = true;
        }

        dispatch_group_leave(group);
    }];

    const auto waitResult = dispatch_group_wait(group, dispatch_time(DISPATCH_TIME_NOW, fileProviderRequestWaitDelta));
    if (waitResult != 0) {
        qCWarning(lcFileProviderXPCUtils) << "Timed out while getting domain id from file provider service";
    }

    if (waitResult != 0 || !gotDomainIdentifier) {
        return nil;
    }

    return domainIdentifier.toNSString();
}

ClientCommunicationConnections processClientCommunicationConnections(NSArray<NSXPCConnection *> *const connections,
                                                                      OCC::Mac::FileProviderService *const service)
{
    ClientCommunicationConnections clientCommConnections;

    for (NSXPCConnection * const connection in connections) {
        const auto exportedInterfaceProtocol = @protocol(AppProtocol);
        const auto remoteObjectInterfaceProtocol = @protocol(ClientCommunicationProtocol);
        connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:exportedInterfaceProtocol];
        
        // Set the FileProviderService delegate as the exported object
        if (service) {
            connection.exportedObject = (__bridge id<AppProtocol>)service->delegate();
        } else {
            qCWarning(lcFileProviderXPCUtils) << "FileProviderService is null, cannot set exported object";
        }
        
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:remoteObjectInterfaceProtocol];
        configureFileProviderConnection(connection);

        const auto clientCommService = (NSObject<ClientCommunicationProtocol> *)getRemoteServiceObject(connection, remoteObjectInterfaceProtocol);

        if (clientCommService == nil) {
            qCWarning(lcFileProviderXPCUtils) << "Client communication service is nil";
            [connection invalidate];
            continue;
        }

        const auto domainIdentifier = getFileProviderDomainIdentifier(clientCommService);

        if (domainIdentifier == nil) {
            qCWarning(lcFileProviderXPCUtils) << "Could not retrieve domain id from file provider service";
            [connection invalidate];
            continue;
        }

        const auto domainIdentifierString = QString::fromNSString(domainIdentifier);

        qCInfo(lcFileProviderXPCUtils) << "Got domain id"
                                       << domainIdentifierString
                                       << "from file provider service";

        if (clientCommConnections.contains(domainIdentifierString)) {
            qCWarning(lcFileProviderXPCUtils) << "Duplicate domain id from file provider service"
                                              << domainIdentifierString;
            [connection invalidate];
            continue;
        }

        ClientCommunicationConnection clientCommConnection;
        clientCommConnection.clientCommunicationService = (__bridge_retained void *)clientCommService;
        clientCommConnection.xpcConnection = (__bridge_retained void *)connection;
        clientCommConnections.insert(domainIdentifierString, clientCommConnection);
    }

    return clientCommConnections;
}

std::optional<bool> deletePassword(NSObject<ClientCommunicationProtocol> *const clientCommService, const std::chrono::nanoseconds timeout)
{
    if (clientCommService == nil) {
        qCWarning(lcFileProviderXPCUtils) << "Cannot delete password without client communication service.";
        return std::nullopt;
    }

    __block auto deleted = false;
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    // The copied reply block retains the semaphore, so a reply after the timeout is safe.
    [clientCommService deletePasswordWithCompletionHandler:^(const BOOL didDelete) {
        deleted = didDelete;
        dispatch_semaphore_signal(semaphore);
    }];

    const auto waitResult = dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, timeout.count()));

    if (waitResult != 0) {
        qCWarning(lcFileProviderXPCUtils) << "Timed out while waiting for password deletion.";
        return std::nullopt;
    }

    return deleted;
}

std::optional<bool> deletePassword(NSFileProviderDomain *const domain, OCC::Mac::FileProviderService *const service)
{
    NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];

    if (manager == nil) {
        qCWarning(lcFileProviderXPCUtils) << "Cannot delete password without file provider manager for domain" << domain.identifier;
        return std::nullopt;
    }

    const auto fpServices = getFileProviderServices(@[ manager ]);
    const auto connections = connectToFileProviderServices(fpServices);
    const auto clientCommConnections = processClientCommunicationConnections(connections, service);

    std::optional<bool> deleted;

    for (const auto &clientCommConnection : clientCommConnections) {
        deleted = deletePassword((__bridge NSObject<ClientCommunicationProtocol> *)clientCommConnection.clientCommunicationService,
                                 std::chrono::nanoseconds{fileProviderRequestWaitDelta});
        [(__bridge_transfer NSXPCConnection *)clientCommConnection.xpcConnection invalidate];
        (void)CFBridgingRelease(clientCommConnection.clientCommunicationService);
    }

    return deleted;
}

} // namespace OCC::Mac::FileProviderXPCUtils
