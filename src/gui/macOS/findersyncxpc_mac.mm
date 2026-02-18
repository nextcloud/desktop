/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "findersyncxpc.h"

#import <Foundation/Foundation.h>
#import "FinderSyncProtocol.h"
#import "FinderSyncAppProtocol.h"

#include <QLoggingCategory>

#include "findersyncservice.h"

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFinderSyncXPC, "nextcloud.gui.macos.findersync.xpc", QtInfoMsg)

} // namespace OCC::Mac

/**
 * @brief NSXPCListener delegate that accepts connections from FinderSync extensions.
 */
@interface FinderSyncXPCListenerDelegate : NSObject<NSXPCListenerDelegate>
@property (nonatomic, assign) OCC::Mac::FinderSyncXPC *finderSyncXPC;
@property (nonatomic, assign) OCC::Mac::FinderSyncService *service;
@property (nonatomic, assign) NSUInteger connectionCounter;
@end

@implementation FinderSyncXPCListenerDelegate

- (BOOL)listener:(NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection
{
    qCInfo(OCC::Mac::lcFinderSyncXPC) << "FinderSync extension attempting to connect via XPC";

    if (!_service) {
        qCWarning(OCC::Mac::lcFinderSyncXPC) << "No FinderSyncService available, rejecting connection";
        return NO;
    }

    // Configure the connection
    newConnection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncAppProtocol)];
    newConnection.exportedObject = (__bridge id)_service->delegate();

    newConnection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncProtocol)];

    // Set up interruption and invalidation handlers
    // Use __block for MRC - the connection won't be deallocated while blocks are alive
    __block NSXPCConnection *blockConnection = newConnection;
    newConnection.interruptionHandler = ^{
        qCWarning(OCC::Mac::lcFinderSyncXPC) << "FinderSync XPC connection interrupted";
        if (blockConnection) {
            [blockConnection invalidate];
        }
    };

    newConnection.invalidationHandler = ^{
        qCInfo(OCC::Mac::lcFinderSyncXPC) << "FinderSync XPC connection invalidated";
        if (_finderSyncXPC && blockConnection) {
            // Clean up the connection from our tracking
            _finderSyncXPC->removeExtensionProxy((void *)CFBridgingRetain(blockConnection));
        }
    };

    // Resume the connection
    [newConnection resume];

    // Store the connection proxy
    id<FinderSyncProtocol> proxy = [newConnection remoteObjectProxyWithErrorHandler:^(NSError *error) {
        qCWarning(OCC::Mac::lcFinderSyncXPC) << "Error getting remote FinderSync proxy:"
                                             << QString::fromNSString(error.localizedDescription);
    }];

    if (proxy && _finderSyncXPC) {
        // Generate a unique connection ID
        self.connectionCounter++;
        QString connectionId = QStringLiteral("FinderSyncConnection_%1").arg(self.connectionCounter);

        qCInfo(OCC::Mac::lcFinderSyncXPC) << "Storing FinderSync extension proxy with ID:" << connectionId;

        // Store the proxy in the C++ object via public method (retain for manual memory management)
        // CFBridgingRetain works both with and without ARC
        // Also pass the connection for reverse mapping
        _finderSyncXPC->storeExtensionProxy(connectionId, (void *)CFBridgingRetain(proxy), (void *)CFBridgingRetain(newConnection));
    }

    qCInfo(OCC::Mac::lcFinderSyncXPC) << "FinderSync XPC connection accepted and configured";
    return YES;
}

@end

namespace OCC::Mac {

FinderSyncXPC::FinderSyncXPC(QObject *parent)
    : QObject(parent)
{
    qCInfo(lcFinderSyncXPC) << "FinderSyncXPC initializing";
}

FinderSyncXPC::~FinderSyncXPC()
{
    qCInfo(lcFinderSyncXPC) << "FinderSyncXPC destroying";

    // Clean up proxies and connections (release retained references)
    // Lock mutex to safely access _extensionProxies and _connectionToId
    {
        QMutexLocker locker(&_proxiesMutex);

        // Release all proxies
        for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
            // Manual release for non-ARC code
            // We stored with CFBridgingRetain (which does +1 retain), so we must release
            id proxy = (__bridge id)it.value();
            [proxy release];
        }
        _extensionProxies.clear();

        // Release all connection objects from reverse mapping
        for (auto it = _connectionToId.begin(); it != _connectionToId.end(); ++it) {
            id connection = (__bridge id)it.key();
            [connection release];
        }
        _connectionToId.clear();
    }

    // Clean up listener
    if (_listener) {
        NSXPCListener *listener = (__bridge NSXPCListener *)_listener;
        [listener invalidate];
        [listener release];
        _listener = nullptr;
    }

    // Clean up delegate (retained separately because listener holds weak reference)
    if (_listenerDelegate) {
        id delegate = (__bridge id)_listenerDelegate;
        [delegate release];
        _listenerDelegate = nullptr;
    }
}

void FinderSyncXPC::startListener(Mac::FinderSyncService *service)
{
    qCInfo(lcFinderSyncXPC) << "Starting FinderSync XPC listener";

    // Create a listener with a Mach service name
    // The service name should match what the extension tries to connect to
    NSString *serviceName = [[NSBundle mainBundle] bundleIdentifier];
    serviceName = [serviceName stringByAppendingString:@".FinderSyncService"];

    qCInfo(lcFinderSyncXPC) << "Creating XPC listener with service name:"
                            << QString::fromNSString(serviceName);

    NSXPCListener *listener = [[NSXPCListener alloc] initWithMachServiceName:serviceName];

    if (!listener) {
        qCWarning(lcFinderSyncXPC) << "Failed to create XPC listener for FinderSync";
        return;
    }

    FinderSyncXPCListenerDelegate *delegate = [[FinderSyncXPCListenerDelegate alloc] init];
    delegate.finderSyncXPC = this;
    delegate.service = service;
    delegate.connectionCounter = 0;

    listener.delegate = delegate;
    [listener resume];

    // Store listener with retained reference (works with and without ARC)
    _listener = (void *)CFBridgingRetain(listener);

    // Store delegate with retained reference (NSXPCListener holds weak reference to delegate)
    // Without this, delegate would be deallocated immediately after this method returns
    _listenerDelegate = (void *)CFBridgingRetain(delegate);

    qCInfo(lcFinderSyncXPC) << "FinderSync XPC listener started successfully";
}

bool FinderSyncXPC::hasActiveConnections() const
{
    QMutexLocker locker(&_proxiesMutex);
    return !_extensionProxies.isEmpty();
}

void FinderSyncXPC::storeExtensionProxy(const QString &connectionId, void *proxy, void *connection)
{
    QMutexLocker locker(&_proxiesMutex);
    _extensionProxies.insert(connectionId, proxy);
    _connectionToId.insert(connection, connectionId);
    qCDebug(lcFinderSyncXPC) << "Stored extension proxy with ID:" << connectionId
                             << "Total connections:" << _extensionProxies.size();
}

void FinderSyncXPC::removeExtensionProxy(void *connection)
{
    QMutexLocker locker(&_proxiesMutex);

    // Look up the connection ID from the connection object
    auto it = _connectionToId.find(connection);
    if (it == _connectionToId.end()) {
        qCWarning(lcFinderSyncXPC) << "Connection not found in reverse mapping, cannot remove proxy";
        // Release the connection we retained in invalidation handler
        id conn = (__bridge id)connection;
        [conn release];
        return;
    }

    const QString connectionId = it.value();
    qCInfo(lcFinderSyncXPC) << "Removing extension proxy with ID:" << connectionId;

    // Remove and release the proxy
    auto proxyIt = _extensionProxies.find(connectionId);
    if (proxyIt != _extensionProxies.end()) {
        id proxy = (__bridge id)proxyIt.value();
        [proxy release];
        _extensionProxies.erase(proxyIt);
    }

    // Remove the connection from reverse mapping and release it
    id conn = (__bridge id)connection;
    [conn release];
    _connectionToId.erase(it);

    qCDebug(lcFinderSyncXPC) << "Removed extension proxy. Remaining connections:" << _extensionProxies.size();
}

void FinderSyncXPC::registerPath(const QString &path)
{
    QMutexLocker locker(&_proxiesMutex);

    if (_extensionProxies.isEmpty()) {
        qCDebug(lcFinderSyncXPC) << "No FinderSync extensions connected, cannot register path:" << path;
        return;
    }

    NSString *nsPath = path.toNSString();
    qCDebug(lcFinderSyncXPC) << "Registering path with" << _extensionProxies.size() << "FinderSync extensions:" << path;

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy registerPath:nsPath];
    }
}

void FinderSyncXPC::unregisterPath(const QString &path)
{
    QMutexLocker locker(&_proxiesMutex);

    if (_extensionProxies.isEmpty()) {
        qCDebug(lcFinderSyncXPC) << "No FinderSync extensions connected, cannot unregister path:" << path;
        return;
    }

    NSString *nsPath = path.toNSString();
    qCDebug(lcFinderSyncXPC) << "Unregistering path with" << _extensionProxies.size() << "FinderSync extensions:" << path;

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy unregisterPath:nsPath];
    }
}

void FinderSyncXPC::updateViewAtPath(const QString &path)
{
    QMutexLocker locker(&_proxiesMutex);

    if (_extensionProxies.isEmpty()) {
        return; // Common case, don't log
    }

    NSString *nsPath = path.toNSString();

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy updateViewAtPath:nsPath];
    }
}

void FinderSyncXPC::setStatusResult(const QString &status, const QString &path)
{
    QMutexLocker locker(&_proxiesMutex);

    if (_extensionProxies.isEmpty()) {
        return; // Common case, don't log
    }

    NSString *nsStatus = status.toNSString();
    NSString *nsPath = path.toNSString();

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy setStatusResult:nsStatus forPath:nsPath];
    }
}

void FinderSyncXPC::setLocalizedString(const QString &key, const QString &value)
{
    QMutexLocker locker(&_proxiesMutex);

    if (_extensionProxies.isEmpty()) {
        qCDebug(lcFinderSyncXPC) << "No FinderSync extensions connected, cannot set localized string";
        return;
    }

    NSString *nsKey = key.toNSString();
    NSString *nsValue = value.toNSString();

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy setLocalizedString:nsValue forKey:nsKey];
    }
}

void FinderSyncXPC::resetMenuItems()
{
    QMutexLocker locker(&_proxiesMutex);

    if (_extensionProxies.isEmpty()) {
        return;
    }

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy resetMenuItems];
    }
}

void FinderSyncXPC::addMenuItem(const QString &command, const QString &flags, const QString &text)
{
    QMutexLocker locker(&_proxiesMutex);

    if (_extensionProxies.isEmpty()) {
        return;
    }

    NSString *nsCommand = command.toNSString();
    NSString *nsFlags = flags.toNSString();
    NSString *nsText = text.toNSString();

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy addMenuItemWithCommand:nsCommand flags:nsFlags text:nsText];
    }
}

void FinderSyncXPC::menuItemsComplete()
{
    QMutexLocker locker(&_proxiesMutex);

    if (_extensionProxies.isEmpty()) {
        return;
    }

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy menuItemsComplete];
    }
}

} // namespace OCC::Mac
