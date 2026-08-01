/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "FinderSyncXPCManager.h"
#import "Services/FinderSyncProtocol.h"
#import "Services/FinderSyncAppProtocol.h"
#import "Services/FinderSyncBrokerProtocol.h"
#import "Services/FinderSyncBrokerClientProtocol.h"

#import <os/log.h>
#import <stdatomic.h>

// If the app accepted the Mach lookup but never replies to the handshake (e.g. it is
// still launching), no error handler fires — so we time the handshake out and retry.
static const NSTimeInterval kFinderSyncHandshakeTimeoutSeconds = 5.0;

static os_log_t getXPCManagerLogger(void) {
    static dispatch_once_t onceToken;
    static os_log_t logger = NULL;

    dispatch_once(&onceToken, ^{
        NSBundle *bundle = [NSBundle bundleForClass:[FinderSyncXPCManager class]];
        NSString *subsystem = bundle.bundleIdentifier ?: @"FinderSyncExt";
        logger = os_log_create(subsystem.UTF8String, "FinderSyncXPCManager");
    });

    return logger;
}

@interface FinderSyncXPCManager () <FinderSyncProtocol, FinderSyncBrokerClientProtocol>
{
    // Connection to the broker login item. Its only job is to hand us the endpoint of the
    // client's anonymous listener, and to tell us when that endpoint changes. Kept open for the
    // latter; no FinderSync traffic goes through it.
    NSXPCConnection *_brokerConnection;
    // Identifies the current broker link. Every asynchronous broker callback — the fetch reply,
    // the proxy error handler, interruption, invalidation — captures the epoch it belongs to and
    // is ignored if a newer link has superseded it. Without this a failure callback from a dead
    // broker link tears down the healthy one that replaced it.
    NSUInteger _brokerEpoch;
    // Generation of the endpoint _connection was built from, as reported by the broker. Lets a
    // redundant push racing an in-flight fetch be recognised and ignored rather than tearing
    // down a healthy peer connection. Zero means "no endpoint".
    uint64_t _endpointGeneration;
    // Highest generation seen on the *current* broker link, so a delayed reply carrying an older
    // generation cannot replace a newer endpoint, and a delayed withdrawal cannot disconnect one.
    //
    // Deliberately per-link rather than absolute: a broker *process* restart resets its own
    // counter to zero, so an absolute high-water mark would reject every endpoint from the
    // replacement broker forever. Reset wherever the link is rebuilt.
    uint64_t _endpointGenerationHighWater;
    // Peer connection to the client, established over the endpoint above.
    NSXPCConnection *_connection;
    id<FinderSyncAppProtocol> _appProxy;
    // Only true after a handshake round-trip to the app succeeds. Atomic because it is
    // written on _connectionQueue but read from Finder's thread via -isConnected.
    atomic_bool _isConnected;
    // Identifies the current connection attempt. Every asynchronous callback (handshake
    // reply, handshake error, interruption, invalidation, timeout) captures the generation
    // it belongs to and is ignored if a newer attempt has superseded it — so a late
    // callback from a dead connection can never tear down a newer, healthy one.
    NSUInteger _connectionGeneration;
    NSMutableDictionary *_statusCache;
    dispatch_queue_t _connectionQueue;
    NSUInteger _reconnectDelay;
    BOOL _reconnectPending;  // Flag to prevent concurrent reconnection attempts
    os_log_t _log;
}
@end

@implementation FinderSyncXPCManager

- (instancetype)initWithDelegate:(id<SyncClientDelegate>)delegate
{
    self = [super init];

    if (self) {
        _log = getXPCManagerLogger();
        _delegate = delegate;
        atomic_store(&_isConnected, false);
        _connectionGeneration = 0;
        _statusCache = [NSMutableDictionary dictionary];
        _connectionQueue = dispatch_queue_create("com.nextcloud.FinderSync.XPCQueue", DISPATCH_QUEUE_SERIAL);
        _reconnectDelay = 1;
        _reconnectPending = NO;
    }

    return self;
}

- (void)dealloc
{
    [self invalidateConnection];
}

- (void)start
{
    os_log_info(_log, "Starting XPC connection");
    [self establishConnection];
}

///
/// Mach service name vended by the broker login item.
///
/// The broker's bundle identifier *is* the service name — a Service Management login item may
/// only advertise a name equal to its own identifier — and that identifier is the shared App
/// Group plus one component. So deriving it from NCApplicationGroupIdentifier, which the build
/// system already templates into our Info.plist, keeps this in step with the broker and with
/// the client's own derivation in findersyncbrokeridentity.cpp.
///
static NSString *brokerServiceName(void)
{
    NSString *appGroup = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"NCApplicationGroupIdentifier"];

    if (appGroup.length == 0) {
        return nil;
    }

    return [appGroup stringByAppendingString:@".FinderSyncBroker"];
}

//
// No -setCodeSigningRequirement: on our connection to the broker. That is deliberate, and it
// is the one direction of the three where peer validation cannot be done.
//
// It was tried. A Finder Sync extension evaluating the broker's signature fails the check even
// when the broker is correctly signed and satisfies the very same requirement statically:
//
//     codesign --verify -R <req> …FinderSyncBroker.app     -> satisfies
//     runtime, from this process:
//     "Received message forbidden due to code signing requirement:
//      xpc_support_check_token: anchor apple generic and
//      certificate leaf[subject.OU] = "…" status: -67030"      (errSecCSReqFailed)
//
// The client, also sandboxed, validates the same broker over the same protocol without any
// trouble, so what differs is this process: app extensions run in a custom sandbox that, as
// Apple DTS puts it, "doesn't always align with the standard App Sandbox used by apps"
// (developer.apple.com/forums/thread/802817). Setting a requirement that cannot succeed is
// worse than setting none — the connection is invalidated on the first reply, which is exactly
// how this manifested: the broker handed over the endpoint and we then rejected it, forever.
//
// The security property is still covered from both other directions:
//   * the broker requires *us* to satisfy its requirement before it accepts our connection,
//     and it does the same for the client;
//   * the client validates the broker before publishing an endpoint to it.
// And the broker's Mach service name lives inside the shared App Group, so registering it at
// all means being a login item whose bundle identifier is prefixed with our team identifier.
//
// If this is ever revisited, verify it from an *installed, Finder-launched* build — not from
// Xcode, and not against a bundle in a build directory, where signature evaluation has its own
// separate failure modes.
//
- (void)establishConnection
{
    dispatch_async(_connectionQueue, ^{
        // Deliberately not gated on the peer connection existing. The broker link and the peer
        // connection have independent lifetimes — the broker only brokers the introduction — and
        // endpoint-change pushes arrive over the broker link. Bailing out here whenever the peer
        // connection happened to be alive meant that after a broker failure the scheduled
        // reconnect did nothing, so the extension silently lost every future endpoint change
        // while still looking healthy.
        [self connectToBrokerAndRequestEndpoint];
    });
}

///
/// Open (or reuse) the connection to the broker and ask it for the client's endpoint.
///
/// Must be called on _connectionQueue.
///
- (void)connectToBrokerAndRequestEndpoint
{
    if (_brokerConnection) {
        if (_connection) {
            // Both halves are up; nothing to do. Re-fetching here would only race the push that
            // the broker link already delivers.
            os_log_debug(_log, "Broker link and peer connection are both established");
            return;
        }

        [self requestEndpointFromBroker];
        return;
    }

    NSString *serviceName = brokerServiceName();

    if (!serviceName) {
        // A missing key is a packaging fault, not a transient condition, so retrying would
        // just spin. Say so once and stop.
        os_log_error(_log,
                     "Info.plist is missing NCApplicationGroupIdentifier; "
                     "cannot reach the broker, so badges and menus will not work");
        return;
    }

    os_log_info(_log, "Connecting to FinderSync broker: %{public}@", serviceName);

    NSXPCConnection *broker = [[NSXPCConnection alloc] initWithMachServiceName:serviceName options:0];
    broker.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncBrokerProtocol)];

    // Let the broker push endpoint changes instead of us polling: the client usually publishes
    // long after Finder has started us, and waiting out the reconnect backoff would leave the
    // user without badges for up to eight seconds after the app is up.
    broker.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncBrokerClientProtocol)];
    broker.exportedObject = self;

    // Tag this link. Every callback below is scoped to it, so a late failure from a superseded
    // link cannot tear down its replacement.
    const NSUInteger epoch = ++_brokerEpoch;
    _endpointGenerationHighWater = 0;

    __weak FinderSyncXPCManager *weakSelf = self;

    broker.interruptionHandler = ^{
        [weakSelf handleBrokerFailureForEpoch:epoch reason:@"broker connection interrupted"];
    };

    broker.invalidationHandler = ^{
        [weakSelf handleBrokerFailureForEpoch:epoch reason:@"broker connection invalidated"];
    };

    [broker resume];
    _brokerConnection = broker;

    [self requestEndpointFromBroker];
}

///
/// Must be called on _connectionQueue.
///
- (void)requestEndpointFromBroker
{
    __weak FinderSyncXPCManager *weakSelf = self;
    const NSUInteger epoch = _brokerEpoch;

    id<FinderSyncBrokerProtocol> proxy = [_brokerConnection remoteObjectProxyWithErrorHandler:^(NSError *error) {
        // Overwhelmingly this means the login item is not registered yet, or the user switched
        // it off in System Settings.
        FinderSyncXPCManager *strongSelf = weakSelf;

        if (!strongSelf) {
            return;
        }

        os_log_error(strongSelf->_log, "Cannot reach the FinderSync broker: %{public}@", error.localizedDescription);
        [strongSelf handleBrokerFailureForEpoch:epoch reason:@"broker unreachable"];
    }];

    [proxy fetchAppEndpointWithReply:^(NSXPCListenerEndpoint *endpoint, uint64_t generation) {
        FinderSyncXPCManager *strongSelf = weakSelf;

        if (!strongSelf) {
            return;
        }

        // This block runs on an arbitrary queue. -connectToClientWithEndpoint: mutates
        // _connection, _appProxy and the generation counters, all of which belong to
        // _connectionQueue, so hop before touching any of it — otherwise the reply races broker
        // pushes, failure handlers and the reconnect timer.
        dispatch_async(strongSelf->_connectionQueue, ^{
            if (epoch != strongSelf->_brokerEpoch) {
                os_log_debug(strongSelf->_log, "Ignoring endpoint reply from superseded broker link");
                return;
            }

            if (!endpoint) {
                // Routine at login: Finder starts us well before the client runs. The broker will
                // push the endpoint as soon as one exists, so there is nothing to retry here.
                os_log_debug(strongSelf->_log, "Broker has no client endpoint yet; waiting for it to publish");
                return;
            }

            [strongSelf connectToClientWithEndpoint:endpoint endpointGeneration:generation];
        });
    }];
}

///
/// Tear down the broker link and schedule another attempt.
///
- (void)handleBrokerFailureForEpoch:(NSUInteger)epoch reason:(NSString *)reason
{
    dispatch_async(_connectionQueue, ^{
        if (epoch != self->_brokerEpoch) {
            // A callback from a broker link we have already replaced. Acting on it would tear
            // down the healthy link that superseded it.
            os_log_debug(self->_log, "Ignoring failure from superseded broker link (%{public}@)", reason);
            return;
        }

        if (!self->_brokerConnection) {
            return;
        }

        os_log_error(self->_log, "FinderSync broker link lost (%{public}@)", reason);

        NSXPCConnection *dead = self->_brokerConnection;
        self->_brokerConnection = nil;
        dead.interruptionHandler = nil;
        dead.invalidationHandler = nil;
        [dead invalidate];

        // The next link starts a fresh generation sequence, because a replacement broker process
        // counts from zero again.
        self->_endpointGenerationHighWater = 0;

        // The peer connection is independent and may still be healthy — the broker only ever
        // brokered the introduction — so it is deliberately left alone here.
        [self scheduleReconnect];
    });
}

#pragma mark - FinderSyncBrokerClientProtocol

- (void)appEndpointDidChange:(NSXPCListenerEndpoint *)endpoint generation:(uint64_t)generation
{
    dispatch_async(_connectionQueue, ^{
        if (!endpoint) {
            // A withdrawal carries a generation too, and is held to the same ordering rule as an
            // endpoint: a delayed withdrawal must not disconnect an endpoint that is newer than
            // it. Endpoints themselves are checked in -connectToClientWithEndpoint:, which is
            // the single place both this push and the fetch reply funnel through.
            if (generation != 0 && generation <= self->_endpointGenerationHighWater) {
                os_log_debug(self->_log, "Ignoring stale endpoint withdrawal %llu (already at %llu)",
                             generation, self->_endpointGenerationHighWater);
                return;
            }

            if (generation != 0) {
                self->_endpointGenerationHighWater = generation;
            }

            // The client went away. Its listener died with it, so the endpoint we hold is a
            // dud; dropping the peer connection now is what stops us reconnecting forever to
            // something that cannot answer.
            os_log_info(self->_log, "Broker reports no client endpoint; dropping peer connection");
            self->_endpointGeneration = 0;
            [self handleConnectionFailureForGeneration:self->_connectionGeneration
                                                reason:@"client endpoint withdrawn"];
            return;
        }

        os_log_info(self->_log, "Broker pushed endpoint generation %llu", generation);
        [self connectToClientWithEndpoint:endpoint endpointGeneration:generation];
    });
}

///
/// Open the peer connection to the client over an endpoint obtained from the broker.
///
/// Must be called on _connectionQueue.
///
- (void)connectToClientWithEndpoint:(NSXPCListenerEndpoint *)endpoint
                 endpointGeneration:(uint64_t)endpointGeneration
{
    // Only ever move forward. Fetch replies and broker pushes are independent and asynchronous,
    // so a delayed generation N can arrive after N+1 — and the previous equality-only test let it
    // through, at which point the branch below tore down the healthy N+1 connection and rebuilt
    // on the dead N endpoint. `<=` also subsumes the "already applied" case.
    //
    // The high-water mark is per broker link, reset wherever the link is rebuilt, because a
    // replacement broker process starts counting from zero; an absolute mark would reject every
    // endpoint it ever publishes.
    if (endpointGeneration != 0 && endpointGeneration <= _endpointGenerationHighWater) {
        os_log_debug(_log, "Ignoring stale endpoint generation %llu (already at %llu)",
                     endpointGeneration, _endpointGenerationHighWater);
        return;
    }

    if (endpointGeneration != 0) {
        _endpointGenerationHighWater = endpointGeneration;
    }

    if (_connection) {
        // A newer endpoint supersedes whatever we were talking to.
        NSXPCConnection *stale = _connection;
        _connection = nil;
        _appProxy = nil;
        atomic_store(&_isConnected, false);
        _connectionGeneration++;
        [stale invalidate];
    }

    _endpointGeneration = endpointGeneration;

    // Connecting to an endpoint performs no bootstrap name lookup, so the App Sandbox has
    // nothing to check and no entitlement is involved.
    NSXPCConnection *connection = [[NSXPCConnection alloc] initWithListenerEndpoint:endpoint];

    {
        // Set up the peer connection interfaces
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncAppProtocol)];
        connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncProtocol)];
        connection.exportedObject = self;

        // Tag this attempt. Every callback below is guarded by this generation so a stale
        // callback from a superseded connection cannot disturb a newer, healthy one.
        const NSUInteger generation = ++self->_connectionGeneration;

        __weak FinderSyncXPCManager *weakSelf = self;

        connection.interruptionHandler = ^{
            [weakSelf handleConnectionFailureForGeneration:generation reason:@"connection interrupted"];
        };

        connection.invalidationHandler = ^{
            [weakSelf handleConnectionFailureForGeneration:generation reason:@"connection invalidated"];
        };

        os_log_info(self->_log, "Resuming XPC connection (generation %lu)", (unsigned long)generation);
        [connection resume];

        self->_connection = connection;

        // NSXPCConnection is lazy: -resume and the proxy below succeed even when no peer has
        // accepted the connection yet (e.g. the extension launched before the app at login).
        // So we do NOT mark ourselves connected here — we withhold that until a real handshake
        // round-trip to the app succeeds. This is the fix for the phantom-connected state
        // behind issues #10032/#8471/#8363, where the extension believed it was connected to
        // a dead peer and never recovered until the user toggled it or relaunched Finder.
        id<FinderSyncAppProtocol> proxy = [connection remoteObjectProxyWithErrorHandler:^(NSError *error) {
            os_log_error(self->_log, "FinderSync XPC message to app failed: %{public}@", error.localizedDescription);
            [weakSelf handleConnectionFailureForGeneration:generation reason:@"remote proxy error"];
        }];

        os_log_info(self->_log, "Performing XPC handshake with app (generation %lu)", (unsigned long)generation);

        [proxy performHandshakeWithReply:^{
            FinderSyncXPCManager *strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }

            dispatch_async(strongSelf->_connectionQueue, ^{
                if (generation != strongSelf->_connectionGeneration) {
                    os_log_info(strongSelf->_log, "Ignoring stale handshake reply (generation %lu, current %lu)",
                                (unsigned long)generation, (unsigned long)strongSelf->_connectionGeneration);
                    return;
                }

                strongSelf->_appProxy = proxy;
                atomic_store(&strongSelf->_isConnected, true);
                strongSelf->_reconnectDelay = 1; // reset backoff after a healthy connection
                os_log_info(strongSelf->_log, "FinderSync XPC handshake succeeded; connection to app is live (generation %lu)",
                            (unsigned long)generation);

                // Only now that the app is confirmed live do we pull the localized strings.
                [strongSelf requestLocalizedStrings];
            });
        }];

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kFinderSyncHandshakeTimeoutSeconds * NSEC_PER_SEC)), self->_connectionQueue, ^{
            FinderSyncXPCManager *strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }

            if (generation == strongSelf->_connectionGeneration && !atomic_load(&strongSelf->_isConnected)) {
                os_log_error(strongSelf->_log, "FinderSync XPC handshake timed out after %.0f s (generation %lu)",
                             (double)kFinderSyncHandshakeTimeoutSeconds, (unsigned long)generation);
                [strongSelf handleConnectionFailureForGeneration:generation reason:@"handshake timeout"];
            }
        });
    }
}

- (void)handleConnectionFailureForGeneration:(NSUInteger)generation reason:(NSString *)reason
{
    dispatch_async(_connectionQueue, ^{
        if (generation != self->_connectionGeneration) {
            // A newer connection attempt already superseded this one; nothing to do.
            return;
        }

        os_log_error(self->_log, "FinderSync XPC connection lost (%{public}@); scheduling reconnect", reason);

        atomic_store(&self->_isConnected, false);

        // Bump the generation so the invalidation handler we trigger below — and any
        // in-flight handshake reply/timeout for this attempt — are recognised as stale.
        self->_connectionGeneration++;

        NSXPCConnection *deadConnection = self->_connection;
        self->_connection = nil;
        self->_appProxy = nil;
        self->_endpointGeneration = 0;
        [deadConnection invalidate];

        // Drop cached statuses. They were answers from a client that is now gone, and the
        // cache is consulted before every status request, so keeping them would serve stale
        // badges indefinitely after the client restarts — there is no expiry.
        @synchronized(self->_statusCache) {
            [self->_statusCache removeAllObjects];
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            // Release Finder's menu thread if it is blocked waiting for a reply that will
            // never arrive, then let the extension drop its now-stale badges/registrations.
            if ([self.delegate respondsToSelector:@selector(menuHasCompleted)]) {
                [self.delegate menuHasCompleted];
            }
            if ([self.delegate respondsToSelector:@selector(connectionDidDie)]) {
                [self.delegate connectionDidDie];
            }
        });

        [self scheduleReconnect];
    });
}

- (void)scheduleReconnect
{
    // All access to _reconnectPending happens on _connectionQueue (serial queue)
    // so no additional synchronization needed
    dispatch_async(_connectionQueue, ^{
        if (self->_reconnectPending) {
            os_log_debug(self->_log, "Reconnect already pending, ignoring duplicate request");
            return;
        }

        self->_reconnectPending = YES;
        os_log_info(self->_log, "Scheduling reconnect in %lu seconds", (unsigned long)self->_reconnectDelay);

        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, self->_reconnectDelay * NSEC_PER_SEC), self->_connectionQueue, ^{
            self->_reconnectPending = NO;
            [self establishConnection];
        });

        // Exponential backoff: 1s, 2s, 4s, 8s (max)
        self->_reconnectDelay = MIN(self->_reconnectDelay * 2, 8);
    });
}

- (void)invalidateConnection
{
    dispatch_sync(_connectionQueue, ^{
        // Bump the generation so the invalidation handler for this connection is treated
        // as stale and does not schedule a reconnect after we are being torn down.
        self->_connectionGeneration++;
        [self->_connection invalidate];
        self->_connection = nil;
        self->_appProxy = nil;
        self->_endpointGeneration = 0;
        atomic_store(&self->_isConnected, false);

        // Clear the handlers first: they would otherwise fire during teardown and schedule a
        // reconnect for an object that is going away.
        self->_brokerConnection.interruptionHandler = nil;
        self->_brokerConnection.invalidationHandler = nil;
        [self->_brokerConnection invalidate];
        self->_brokerConnection = nil;
    });
}

- (BOOL)isConnected
{
    return atomic_load(&_isConnected);
}

- (void)requestLocalizedStrings
{
    if (!_appProxy) {
        os_log_debug(_log, "Cannot request strings, no app proxy");
        return;
    }

    [_appProxy getLocalizedStringsWithCompletionHandler:^(NSDictionary<NSString *, NSString *> *strings, NSError *error) {
        if (error) {
            os_log_error(self->_log, "Error getting localized strings: %{public}@", error.localizedDescription);
            return;
        }

        os_log_debug(self->_log, "Received %lu localized strings", (unsigned long)strings.count);

        // Send strings to delegate
        for (NSString *key in strings) {
            NSString *value = strings[key];
            dispatch_async(dispatch_get_main_queue(), ^{
                if ([self.delegate respondsToSelector:@selector(setString:value:)]) {
                    [self.delegate setString:key value:value];
                }
            });
        }
    }];
}

- (void)askForIcon:(NSString *)path isDirectory:(BOOL)isDirectory
{
    if (!_appProxy) {
        os_log_debug(_log, "Cannot ask for icon, not connected");
        return;
    }

    // Check cache first (thread-safe access)
    NSString *cachedStatus;
    @synchronized(_statusCache) {
        cachedStatus = _statusCache[path];
    }

    if (cachedStatus) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if ([self.delegate respondsToSelector:@selector(setResult:forPath:)]) {
                [self.delegate setResult:cachedStatus forPath:path];
            }
        });
        return;
    }

    // Request from server
    if (isDirectory) {
        [_appProxy retrieveFolderStatusForPath:path completionHandler:^(NSString *status, NSError *error) {
            if (error) {
                os_log_error(self->_log, "Error retrieving folder status for %{public}@: %{public}@", path, error.localizedDescription);
                return;
            }

            if (status) {
                // Thread-safe cache update
                @synchronized(self->_statusCache) {
                    self->_statusCache[path] = status;
                }
                dispatch_async(dispatch_get_main_queue(), ^{
                    if ([self.delegate respondsToSelector:@selector(setResult:forPath:)]) {
                        [self.delegate setResult:status forPath:path];
                    }
                });
            }
        }];
    } else {
        [_appProxy retrieveFileStatusForPath:path completionHandler:^(NSString *status, NSError *error) {
            if (error) {
                os_log_error(self->_log, "Error retrieving file status for %{public}@: %{public}@", path, error.localizedDescription);
                return;
            }

            if (status) {
                // Thread-safe cache update
                @synchronized(self->_statusCache) {
                    self->_statusCache[path] = status;
                }
                dispatch_async(dispatch_get_main_queue(), ^{
                    if ([self.delegate respondsToSelector:@selector(setResult:forPath:)]) {
                        [self.delegate setResult:status forPath:path];
                    }
                });
            }
        }];
    }
}

- (void)askOnSocket:(NSString *)arg query:(NSString *)query
{
    if (!_appProxy) {
        os_log_debug(_log, "Cannot send query, not connected");
        return;
    }

    os_log_debug(_log, "Query: %{public}@ for: %{public}@", query, arg);

    if ([query isEqualToString:@"GET_MENU_ITEMS"]) {
        // Parse paths from arg (record separator 0x1e)
        NSArray *paths = [arg componentsSeparatedByString:@"\x1e"];

        [_appProxy getMenuItemsForPaths:paths completionHandler:^(NSArray<NSDictionary *> *menuItems, NSError *error) {
            if (error) {
                os_log_error(self->_log, "Error getting menu items: %{public}@", error.localizedDescription);
                dispatch_async(dispatch_get_main_queue(), ^{
                    if ([self.delegate respondsToSelector:@selector(menuHasCompleted)]) {
                        [self.delegate menuHasCompleted];
                    }
                });
                return;
            }

            dispatch_async(dispatch_get_main_queue(), ^{
                if ([self.delegate respondsToSelector:@selector(resetMenuItems)]) {
                    [self.delegate resetMenuItems];
                }

                for (NSDictionary *item in menuItems) {
                    if ([self.delegate respondsToSelector:@selector(addMenuItem:)]) {
                        [self.delegate addMenuItem:item];
                    }
                }

                if ([self.delegate respondsToSelector:@selector(menuHasCompleted)]) {
                    [self.delegate menuHasCompleted];
                }
            });
        }];
    } else if ([query isEqualToString:@"GET_STRINGS"]) {
        [self requestLocalizedStrings];
    } else {
        // Execute menu command
        NSArray *paths = [arg componentsSeparatedByString:@"\x1e"];
        
        [_appProxy executeMenuCommand:query forPaths:paths completionHandler:^(NSError *error) {
            if (error) {
                os_log_error(self->_log, "Error executing command %{public}@: %{public}@", query, error.localizedDescription);
            } else {
                os_log_debug(self->_log, "Command %{public}@ executed successfully", query);
            }
        }];
    }
}

#pragma mark - FinderSyncProtocol Implementation

- (void)registerPath:(NSString *)path
{
    os_log_debug(_log, "Registering path: %{public}@", path);

    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(registerPath:)]) {
            [self.delegate registerPath:path];
        }
    });
}

- (void)unregisterPath:(NSString *)path
{
    os_log_debug(_log, "Unregistering path: %{public}@", path);

    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(unregisterPath:)]) {
            [self.delegate unregisterPath:path];
        }
    });
}

- (void)updateViewAtPath:(NSString *)path
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(reFetchFileNameCacheForPath:)]) {
            [self.delegate reFetchFileNameCacheForPath:path];
        }
    });
}

- (void)setStatusResult:(NSString *)status forPath:(NSString *)path
{
    // Thread-safe cache update
    @synchronized(_statusCache) {
        _statusCache[path] = status;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(setResult:forPath:)]) {
            [self.delegate setResult:status forPath:path];
        }
    });
}

- (void)setLocalizedString:(NSString *)value forKey:(NSString *)key
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(setString:value:)]) {
            [self.delegate setString:key value:value];
        }
    });
}

- (void)resetMenuItems
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(resetMenuItems)]) {
            [self.delegate resetMenuItems];
        }
    });
}

- (void)addMenuItemWithCommand:(NSString *)command flags:(NSString *)flags text:(NSString *)text
{
    NSDictionary *item = @{
        @"command": command,
        @"flags": flags,
        @"text": text
    };

    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(addMenuItem:)]) {
            [self.delegate addMenuItem:item];
        }
    });
}

- (void)menuItemsComplete
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if ([self.delegate respondsToSelector:@selector(menuHasCompleted)]) {
            [self.delegate menuHasCompleted];
        }
    });
}

@end
