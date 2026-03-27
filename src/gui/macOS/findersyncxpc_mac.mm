/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "findersyncxpc.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import "FinderSyncProtocol.h"
#import "FinderSyncAppProtocol.h"

#include <QLoggingCategory>
#include <QMetaObject>

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

    // Validate that the connecting client is our FinderSync extension by checking
    // that it shares the same team identifier as the main app bundle.
    // This prevents unauthorized processes from connecting to this XPC service.
    NSString *expectedTeamId = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"TeamIdentifierPrefix"];
    if (!expectedTeamId) {
        // Fall back to extracting team ID from code signing info
        SecCodeRef code = NULL;
        OSStatus status = SecCodeCopySelf(kSecCSDefaultFlags, &code);
        if (status == errSecSuccess && code) {
            CFDictionaryRef info = NULL;
            status = SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info);
            if (status == errSecSuccess && info) {
                // Retain + autorelease before releasing the dictionary that owns this string.
                // Without the explicit retain, CFRelease(info) below would drop the dictionary's
                // retain count to zero, deallocating it (and all its values) while
                // expectedTeamId still holds a pointer into it — a classic MRC dangling pointer.
                expectedTeamId = [[((__bridge NSDictionary *)info)[@"teamid"] retain] autorelease];
                CFRelease(info);
            }
            CFRelease(code);
        }
    }

    if (!expectedTeamId || expectedTeamId.length == 0) {
        qCWarning(OCC::Mac::lcFinderSyncXPC) << "Cannot determine team ID for XPC validation, rejecting connection for safety";
        return NO;
    }

    pid_t pid = newConnection.processIdentifier;
    SecCodeRef clientCode = NULL;
    NSDictionary *attrs = @{(__bridge NSString *)kSecGuestAttributePid: @(pid)};
    OSStatus status = SecCodeCopyGuestWithAttributes(NULL, (__bridge CFDictionaryRef)attrs, kSecCSDefaultFlags, &clientCode);
    if (status == errSecSuccess && clientCode) {
        CFDictionaryRef clientInfo = NULL;
        status = SecCodeCopySigningInformation(clientCode, kSecCSSigningInformation, &clientInfo);
        if (status == errSecSuccess && clientInfo) {
            NSString *clientTeamId = ((__bridge NSDictionary *)clientInfo)[@"teamid"];
            if (![expectedTeamId isEqualToString:clientTeamId]) {
                qCWarning(OCC::Mac::lcFinderSyncXPC) << "Rejecting XPC connection from untrusted client, team:"
                                                      << QString::fromNSString(clientTeamId ?: @"(none)");
                CFRelease(clientInfo);
                CFRelease(clientCode);
                return NO;
            }
            CFRelease(clientInfo);
        }
        CFRelease(clientCode);
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
    qCDebug(OCC::Mac::lcFinderSyncXPC) << "FinderSync XPC connection resumed";

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

    // Store listener with retained reference (works with and without ARC).
    // CFBridgingRetain adds +1, so release the original alloc/init +1 to avoid a leak.
    _listener = (void *)CFBridgingRetain(listener);
    [listener release];

    // Store delegate with retained reference (NSXPCListener holds weak reference to delegate).
    // Without this, delegate would be deallocated immediately after this method returns.
    _listenerDelegate = (void *)CFBridgingRetain(delegate);
    [delegate release];

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

    // Notify Application (on the main thread) so it can push all currently-registered
    // sync folder paths to the newly-connected extension. Must marshal to the owning
    // thread because emitting Qt signals from a non-Qt thread causes thread-affinity issues.
    QMetaObject::invokeMethod(this, &FinderSyncXPC::extensionConnected, Qt::QueuedConnection);
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

// Helper: snapshot proxy list under the mutex, then release it before making XPC calls.
// This avoids holding _proxiesMutex during potentially-blocking XPC dispatch, which could
// deadlock if a proxy call triggers an invalidation handler that also wants the mutex.
QList<void *> FinderSyncXPC::currentProxies() const
{
    QMutexLocker locker(&_proxiesMutex);
    return _extensionProxies.values();
}

void FinderSyncXPC::registerPath(const QString &path)
{
    const auto proxies = currentProxies();
    if (proxies.isEmpty()) {
        qCDebug(lcFinderSyncXPC) << "No FinderSync extensions connected, cannot register path:" << path;
        return;
    }

    NSString *nsPath = path.toNSString();
    qCDebug(lcFinderSyncXPC) << "Registering path with" << proxies.size() << "FinderSync extensions:" << path;

    for (auto *p : proxies) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)p;
        [proxy registerPath:nsPath];
    }
}

void FinderSyncXPC::unregisterPath(const QString &path)
{
    const auto proxies = currentProxies();
    if (proxies.isEmpty()) {
        qCDebug(lcFinderSyncXPC) << "No FinderSync extensions connected, cannot unregister path:" << path;
        return;
    }

    NSString *nsPath = path.toNSString();
    qCDebug(lcFinderSyncXPC) << "Unregistering path with" << proxies.size() << "FinderSync extensions:" << path;

    for (auto *p : proxies) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)p;
        [proxy unregisterPath:nsPath];
    }
}

void FinderSyncXPC::updateViewAtPath(const QString &path)
{
    const auto proxies = currentProxies();
    if (proxies.isEmpty()) {
        return;
    }

    NSString *nsPath = path.toNSString();

    for (auto *p : proxies) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)p;
        [proxy updateViewAtPath:nsPath];
    }
}

void FinderSyncXPC::setStatusResult(const QString &status, const QString &path)
{
    const auto proxies = currentProxies();
    if (proxies.isEmpty()) {
        return;
    }

    NSString *nsStatus = status.toNSString();
    NSString *nsPath = path.toNSString();

    for (auto *p : proxies) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)p;
        [proxy setStatusResult:nsStatus forPath:nsPath];
    }
}

void FinderSyncXPC::setLocalizedString(const QString &key, const QString &value)
{
    const auto proxies = currentProxies();
    if (proxies.isEmpty()) {
        qCDebug(lcFinderSyncXPC) << "No FinderSync extensions connected, cannot set localized string";
        return;
    }

    NSString *nsKey = key.toNSString();
    NSString *nsValue = value.toNSString();

    for (auto *p : proxies) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)p;
        [proxy setLocalizedString:nsValue forKey:nsKey];
    }
}

void FinderSyncXPC::resetMenuItems()
{
    const auto proxies = currentProxies();
    if (proxies.isEmpty()) {
        return;
    }

    for (auto *p : proxies) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)p;
        [proxy resetMenuItems];
    }
}

void FinderSyncXPC::addMenuItem(const QString &command, const QString &flags, const QString &text)
{
    const auto proxies = currentProxies();
    if (proxies.isEmpty()) {
        return;
    }

    NSString *nsCommand = command.toNSString();
    NSString *nsFlags = flags.toNSString();
    NSString *nsText = text.toNSString();

    for (auto *p : proxies) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)p;
        [proxy addMenuItemWithCommand:nsCommand flags:nsFlags text:nsText];
    }
}

void FinderSyncXPC::menuItemsComplete()
{
    const auto proxies = currentProxies();
    if (proxies.isEmpty()) {
        return;
    }

    for (auto *p : proxies) {
        NSObject<FinderSyncProtocol> *proxy = (__bridge NSObject<FinderSyncProtocol> *)p;
        [proxy menuItemsComplete];
    }
}

} // namespace OCC::Mac
