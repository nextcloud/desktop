/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "findersyncxpc.h"

#import <Foundation/Foundation.h>
#import "../../../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncProtocol.h"
#import "../../../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncAppProtocol.h"
#import "../../../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncBrokerProtocol.h"

#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>

#include "findersyncbrokeridentity.h"
#include "findersyncbrokerregistrar.h"
#include "findersyncservice.h"

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFinderSyncXPC, "nextcloud.gui.macos.findersync.xpc", QtInfoMsg)

namespace {
    //! How long to wait for evidence that a publish arrived before treating silence as failure.
    //! Covers a launchd cold start of the login item plus the broker's own 5 s self-check, so a
    //! slow first launch does not spend the recovery budget.
    constexpr int kBrokerPublishWatchdogMs = 15000;

    //! Pacing between publish attempts once a failure has been reported explicitly.
    constexpr int kBrokerPublishRetryMs = 2000;
}

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

    // Peer authentication is not done here. The requirement is installed once on the listener
    // with -setConnectionCodeSigningRequirement:, so XPC rejects connections that fail it
    // before this delegate is ever called.
    //
    // This replaced a hand-rolled check that read the peer's team identifier via
    // kSecGuestAttributePid. That was wrong twice over: it resolved the peer by process
    // identifier, which is recycled — a peer could exit between the lookup and the check and
    // be replaced at the same PID by a different, legitimately signed binary — and its
    // expected-team lookup read a TeamIdentifierPrefix key that the CMake-generated
    // Info.plist does not contain, so every locally built client fell into the
    // "cannot determine team ID, rejecting for safety" branch and refused all connections.

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

    // Drop the broker link first: its handler blocks capture `this`, so they must not be able
    // to fire once the rest of this object is gone.
    releaseBrokerConnection();

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

bool FinderSyncXPC::startListener(Mac::FinderSyncService *service)
{
    qCInfo(lcFinderSyncXPC) << "Starting FinderSync XPC listener";

    // Anonymous, not named. A plain app is not a launchd job and may not advertise a Mach
    // service name; the FinderSyncBroker login item does that for us and hands this endpoint
    // to the extension. An anonymous listener needs no entitlement and cannot fail to
    // activate, because there is no bootstrap name to register.
    NSXPCListener *listener = [[NSXPCListener anonymousListener] retain];

    FinderSyncXPCListenerDelegate *delegate = [[FinderSyncXPCListenerDelegate alloc] init];
    delegate.finderSyncXPC = this;
    delegate.service = service;
    delegate.connectionCounter = 0;

    listener.delegate = delegate;

    // Blanket peer authentication, installed before -resume so XPC rejects anything that fails
    // it without our delegate seeing it.
    [listener setConnectionCodeSigningRequirement:Mac::FinderSyncBrokerIdentity::peerRequirement().toNSString()];

    [listener resume];

    // Store listener with retained reference (works with and without ARC).
    // CFBridgingRetain adds +1, so release the original +1 to avoid a leak.
    _listener = (void *)CFBridgingRetain(listener);
    [listener release];

    // Store delegate with retained reference (NSXPCListener holds weak reference to delegate).
    // Without this, delegate would be deallocated immediately after this method returns.
    _listenerDelegate = (void *)CFBridgingRetain(delegate);
    [delegate release];

    qCInfo(lcFinderSyncXPC) << "Anonymous FinderSync listener created; publishing endpoint to broker"
                            << Mac::FinderSyncBrokerIdentity::brokerServiceName();

    publishEndpointToBroker();

    return true;
}

void FinderSyncXPC::releaseBrokerConnection()
{
    if (!_brokerConnection) {
        return;
    }

    NSXPCConnection *connection = (__bridge NSXPCConnection *)_brokerConnection;
    connection.interruptionHandler = nil;
    connection.invalidationHandler = nil;
    [connection invalidate];
    [connection release];
    _brokerConnection = nullptr;
}

void FinderSyncXPC::publishEndpointToBroker()
{
    if (!_listener) {
        qCWarning(lcFinderSyncXPC) << "No listener to publish; not contacting the broker";
        return;
    }

    releaseBrokerConnection();

    const QString serviceName = Mac::FinderSyncBrokerIdentity::brokerServiceName();
    NSXPCConnection *connection =
        [[NSXPCConnection alloc] initWithMachServiceName:serviceName.toNSString() options:0];
    connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncBrokerProtocol)];

    // Authenticate the broker before handing it an endpoint that grants full access to the
    // FinderSync protocol. macOS 13+ API, evaluated by the system against the message's audit
    // token — do not reimplement this from a PID, which is racy.
    [connection setCodeSigningRequirement:Mac::FinderSyncBrokerIdentity::peerRequirement().toNSString()];

    // Captured by value into the blocks below instead of a raw `this`. The interruption and
    // invalidation handlers are cleared by releaseBrokerConnection(), but the proxy error handler
    // further down cannot be, and a block already in flight can outlive this object — dereferencing
    // `this` to reach QMetaObject::invokeMethod would then be undefined before any event is even
    // posted. QPointer is copyable, so an Objective-C++ block captures it correctly.
    //
    // The attempt number scopes each callback to the connection that installed it, so a late
    // failure from a superseded attempt cannot disturb the ladder for its replacement.
    QPointer<FinderSyncXPC> self(this);
    const auto attempt = ++_brokerAttempt;

    connection.interruptionHandler = ^{
        // The broker died. It is a launchd job and will be relaunched on the next lookup, but
        // it comes back with no stored endpoint, so we must publish again.
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, attempt] {
            if (self) {
                self->handlePublishFailure(attempt, QStringLiteral("broker connection interrupted"));
            }
        }, Qt::QueuedConnection);
    };

    connection.invalidationHandler = ^{
        // A Mach name that is absent from the bootstrap namespace produces *invalidation*, not
        // interruption — so this, not the handler above, is the path taken when the login item is
        // not registered. It used to only flip the flag, which is why the client made exactly one
        // publish attempt per run while the extension retried every 8 s forever.
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, attempt] {
            if (self) {
                self->handlePublishFailure(attempt, QStringLiteral("broker connection invalidated"));
            }
        }, Qt::QueuedConnection);
    };

    [connection resume];
    _brokerConnection = (void *)connection;

    NSXPCListener *listener = (__bridge NSXPCListener *)_listener;

    id<FinderSyncBrokerProtocol> broker = [connection remoteObjectProxyWithErrorHandler:^(NSError *error) {
        // The overwhelmingly common cause is that the login item is not registered yet, or the
        // user switched it off in System Settings. Both leave FinderSync completely inert, so
        // this is an error, not a debug note.
        const auto description = QString::fromNSString(error.localizedDescription);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, attempt, description] {
            if (self) {
                self->handlePublishFailure(attempt, description);
            }
        }, Qt::QueuedConnection);
    }];

    [broker publishAppEndpoint:listener.endpoint reply:^(uint64_t generation) {
        qCInfo(lcFinderSyncXPC) << "Broker accepted our endpoint, generation" << generation;
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, attempt] {
            // Scoped to the attempt: a success reply that arrives after a newer publish attempt
            // has already started must not mark that newer attempt as done.
            if (self && attempt == self->_brokerAttempt) {
                self->setEndpointPublished(true);
            }
        }, Qt::QueuedConnection);
    }];

    // Nothing above proves the publish arrived. If the name resolves but the broker is wedged, or
    // its listener never activated, no reply and no handler ever comes — so arm the ladder to find
    // out. Generous, because a cold launchd start plus the broker's own 5 s self-check is legitimate
    // startup latency, not a fault.
    scheduleNextPublishAttempt(kBrokerPublishWatchdogMs);

    // Replacing the app bundle does not restart an already-running broker: its Service
    // Management registration survives the upgrade, so launchd keeps the previous binary alive
    // and we would go on talking to last version's broker indefinitely.
    [broker brokerVersionWithReply:^(NSString *version) {
        const auto brokerVersion = QString::fromNSString(version);
        // Deliberately the literal key rather than kCFBundleVersionKey: that constant would add
        // a CoreFoundation data symbol to this translation unit for no benefit, and the other
        // Info.plist reads here use literals too.
        const auto ourVersion = QString::fromNSString(
            [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"]);

        if (brokerVersion == ourVersion) {
            qCDebug(lcFinderSyncXPC) << "Broker version matches ours:" << brokerVersion;
            return;
        }

        qCInfo(lcFinderSyncXPC) << "Broker is running version" << brokerVersion << "but we are"
                                << ourVersion << "- restarting it";

        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, brokerVersion] {
            if (self) {
                self->requestBrokerRestart(BrokerRestartReason::VersionMismatch, brokerVersion);
            }
        }, Qt::QueuedConnection);
    }];
}

bool FinderSyncXPC::isEndpointPublished() const
{
    QMutexLocker locker(&_proxiesMutex);
    return _endpointPublished;
}

void FinderSyncXPC::setEndpointPublished(bool published)
{
    if (published) {
        // Deliberately before the unchanged-value early return below. A republish that merely
        // re-confirms an already-published endpoint must still stop the timer, otherwise it fires
        // later and starts climbing the ladder against a perfectly healthy channel.
        if (_publishTimer) {
            _publishTimer->stop();
        }
        resetSelfHeal();
    }

    {
        QMutexLocker locker(&_proxiesMutex);
        if (_endpointPublished == published) {
            return;
        }
        _endpointPublished = published;
    }

    emit brokerReachableChanged(published);
}

void FinderSyncXPC::resetSelfHeal()
{
    // Only ever called once an endpoint has actually been published. A healthy period earns a
    // fresh budget, so a user who enables the login item in System Settings recovers without
    // restarting the app — while a channel that never works cannot reach any stage twice.
    _selfHealStage = SelfHealStage::Republish;
    _republishAttemptsLeft = 2;
    _brokerRestartSpent = false;
}

void FinderSyncXPC::scheduleNextPublishAttempt(int delayMs)
{
    if (isEndpointPublished()) {
        return;
    }

    if (!_publishTimer) {
        _publishTimer = new QTimer(this);
        _publishTimer->setSingleShot(true);
        connect(_publishTimer, &QTimer::timeout, this, &FinderSyncXPC::advanceSelfHeal);
    }

    _publishTimer->start(delayMs);
}

void FinderSyncXPC::handlePublishFailure(quint64 attempt, const QString &reason)
{
    if (attempt != _brokerAttempt) {
        qCDebug(lcFinderSyncXPC) << "Ignoring failure from superseded publish attempt:" << reason;
        return;
    }

    if (isEndpointPublished()) {
        // A stale handler from a superseded connection. Ignore it rather than tearing down a
        // channel that is currently working.
        qCDebug(lcFinderSyncXPC) << "Ignoring stale broker failure while published:" << reason;
        return;
    }

    setEndpointPublished(false);

    qCWarning(lcFinderSyncXPC) << "Could not publish our endpoint to the FinderSync broker:" << reason;

    // Pace the retry rather than reacting immediately: republishing straight from the invalidation
    // handler of a name that does not exist would spin as fast as XPC can fail.
    scheduleNextPublishAttempt(kBrokerPublishRetryMs);
}

void FinderSyncXPC::advanceSelfHeal()
{
    if (isEndpointPublished()) {
        return;
    }

    switch (_selfHealStage) {
    case SelfHealStage::Republish:
        if (_republishAttemptsLeft > 0) {
            --_republishAttemptsLeft;
            qCInfo(lcFinderSyncXPC) << "Retrying the endpoint publish;" << _republishAttemptsLeft
                                    << "attempt(s) left before touching the login item";
            publishEndpointToBroker();
            return;
        }
        _selfHealStage = SelfHealStage::Register;
        [[fallthrough]];

    case SelfHealStage::Register: {
        const auto status = FinderSyncBrokerRegistrar::status();
        qCInfo(lcFinderSyncXPC) << "Endpoint publish keeps failing; login item is"
                                << FinderSyncBrokerRegistrar::describe(status);

        if (status == FinderSyncBrokerRegistrar::Status::RequiresApproval) {
            // The user's decision. Retrying earns kSMErrorLaunchDeniedByUser, and re-registering
            // could erase their explicit "off", so stop here.
            qCCritical(lcFinderSyncXPC)
                << "FinderSync integration is switched off in System Settings; badges and the "
                   "context menu will not work until it is enabled there";
            _selfHealStage = SelfHealStage::GaveUp;
            return;
        }

        if (status != FinderSyncBrokerRegistrar::Status::Enabled) {
            const auto result = FinderSyncBrokerRegistrar::ensureRegistered();
            _selfHealStage = SelfHealStage::Reregister;

            if (result == FinderSyncBrokerRegistrar::Status::Enabled) {
                publishEndpointToBroker();
                return;
            }

            if (result == FinderSyncBrokerRegistrar::Status::NotFound) {
                // Packaging fault, not a transient one: the bundle is missing or its identifier
                // does not match its wrapper filename. Restarting it cannot help.
                qCCritical(lcFinderSyncXPC)
                    << "The FinderSync login item cannot be resolved, so it will never register; "
                       "this is a packaging fault, not something the app can recover from";
                _selfHealStage = SelfHealStage::GaveUp;
                return;
            }
        }

        // Registered and enabled, yet not serving. Restarting it is the remaining remedy.
        _selfHealStage = SelfHealStage::Reregister;
        [[fallthrough]];
    }

    case SelfHealStage::Reregister:
        _selfHealStage = SelfHealStage::GaveUp;
        requestBrokerRestart(BrokerRestartReason::Unreachable);
        return;

    case SelfHealStage::GaveUp:
        // Reported once already. Only setEndpointPublished(true) revives the ladder.
        return;
    }
}

void FinderSyncXPC::requestBrokerRestart(BrokerRestartReason reason, const QString &brokerVersion)
{
    if (_brokerRestartInFlight) {
        // Registering before a previous unregistration has completed fails with
        // SMAppServiceErrorDomain code 1. Drop rather than queue: whatever the second caller
        // wanted, the restart already in flight will produce it.
        qCDebug(lcFinderSyncXPC) << "Broker restart already in flight; ignoring duplicate request";
        return;
    }

    if (reason == BrokerRestartReason::VersionMismatch && _restartedForBrokerVersion == brokerVersion) {
        // Without this the version check loops: restart, republish, ask the version again, still
        // mismatched, restart… each iteration mutating login item state.
        qCWarning(lcFinderSyncXPC) << "Already restarted the broker for version" << brokerVersion
                                   << "and it came back the same; leaving it alone";
        return;
    }

    if (_brokerRestartSpent) {
        // One restart per healthy period, shared by both reasons: if a restart just happened and
        // did not help, doing it again will not either.
        qCWarning(lcFinderSyncXPC) << "Broker restart budget already spent this session; not "
                                      "restarting the login item again";
        return;
    }

    // Recorded only now that a restart is actually going ahead. Recording it before the budget
    // check would permanently blacklist a version we never actually attempted.
    if (reason == BrokerRestartReason::VersionMismatch) {
        _restartedForBrokerVersion = brokerVersion;
    }

    _brokerRestartSpent = true;
    _brokerRestartInFlight = true;

    QPointer<FinderSyncXPC> self(this);

    FinderSyncBrokerRegistrar::reregisterAsync([self](FinderSyncBrokerRegistrar::Status status) {
        if (!self) {
            return;
        }

        self->_brokerRestartInFlight = false;

        if (status != FinderSyncBrokerRegistrar::Status::Enabled) {
            qCWarning(lcFinderSyncXPC) << "Broker login item is" << FinderSyncBrokerRegistrar::describe(status)
                                       << "after restarting; not publishing again";
            return;
        }

        // The replacement process starts with no stored endpoint.
        self->setEndpointPublished(false);
        self->publishEndpointToBroker();
    });
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
