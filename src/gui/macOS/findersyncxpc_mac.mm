/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "findersyncxpc.h"

#import <Foundation/Foundation.h>
#import "FinderSyncProtocol.h"

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
    __weak NSXPCConnection *weakConnection = newConnection;
    newConnection.interruptionHandler = ^{
        qCWarning(OCC::Mac::lcFinderSyncXPC) << "FinderSync XPC connection interrupted";
        NSXPCConnection *strongConnection = weakConnection;
        if (strongConnection) {
            [strongConnection invalidate];
        }
    };

    newConnection.invalidationHandler = ^{
        qCInfo(OCC::Mac::lcFinderSyncXPC) << "FinderSync XPC connection invalidated";
        if (_finderSyncXPC) {
            // Clean up the connection from our tracking
            // This will be done when the connection is fully released
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

        // Store the proxy in the C++ object (transfer ownership to ARC bridge)
        _finderSyncXPC->_extensionProxies.insert(connectionId, (__bridge_retained void *)proxy);
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

    // Clean up proxies
    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject *proxy = (__bridge_transfer NSObject *)it.value();
        (void)proxy; // Released when this scope ends
    }
    _extensionProxies.clear();

    // Clean up listener
    if (_listener) {
        NSXPCListener *listener = (__bridge_transfer NSXPCListener *)_listener;
        [listener invalidate];
        _listener = nullptr;
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

    _listener = (__bridge_retained void *)listener;

    qCInfo(lcFinderSyncXPC) << "FinderSync XPC listener started successfully";
}

bool FinderSyncXPC::hasActiveConnections() const
{
    return !_extensionProxies.isEmpty();
}

void FinderSyncXPC::registerPath(const QString &path)
{
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
    if (_extensionProxies.isEmpty()) {
        return;
    }

    for (auto it = _extensionProxies.begin(); it != _extensionProxies.end(); ++it) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)it.value();
        [proxy menuItemsComplete];
    }
}

} // namespace OCC::Mac
