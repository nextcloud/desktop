/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "FinderSyncXPCManager.h"
#import "Services/FinderSyncProtocol.h"
#import "Services/FinderSyncAppProtocol.h"

#import <os/log.h>

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

@interface FinderSyncXPCManager () <FinderSyncProtocol>
{
    NSXPCConnection *_connection;
    id<FinderSyncAppProtocol> _appProxy;
    BOOL _isConnected;
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
        _isConnected = NO;
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

- (void)establishConnection
{
    dispatch_async(_connectionQueue, ^{
        if (self->_connection) {
            os_log_debug(self->_log, "Connection already exists");
            return;
        }

        // Get the bundle identifier to construct the service name
        NSString *bundleId = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"PARENT_BUNDLE_ID"];

        if (!bundleId) {
            // Fallback: try to construct it from the extension bundle ID
            bundleId = [[NSBundle mainBundle] bundleIdentifier];
            // Remove the extension suffix (e.g., ".FinderSyncExt")
            if ([bundleId hasSuffix:@".FinderSyncExt"]) {
                bundleId = [bundleId substringToIndex:bundleId.length - 14];
            }
        }

        NSString *serviceName = [NSString stringWithFormat:@"%@.FinderSyncService", bundleId];
        os_log_info(self->_log, "Attempting to connect to service: %{public}@", serviceName);

        NSXPCConnection *connection = [[NSXPCConnection alloc] initWithMachServiceName:serviceName
                                                                                options:0];

        if (!connection) {
            os_log_error(self->_log, "Failed to create XPC connection");
            [self scheduleReconnect];
            return;
        }

        // Set up the connection interfaces
        connection.remoteObjectInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncAppProtocol)];
        connection.exportedInterface = [NSXPCInterface interfaceWithProtocol:@protocol(FinderSyncProtocol)];
        connection.exportedObject = self;

        // Set up interruption and invalidation handlers
        __weak FinderSyncXPCManager *weakSelf = self;

        connection.interruptionHandler = ^{
            FinderSyncXPCManager *strongSelf = weakSelf;
            if (strongSelf) {
                os_log_info(strongSelf->_log, "XPC connection interrupted");
                strongSelf->_isConnected = NO;
                [strongSelf scheduleReconnect];
            }
        };

        connection.invalidationHandler = ^{
            FinderSyncXPCManager *strongSelf = weakSelf;

            if (strongSelf) {
                os_log_info(strongSelf->_log, "XPC connection invalidated");
                strongSelf->_isConnected = NO;
                strongSelf->_connection = nil;
                strongSelf->_appProxy = nil;
                [strongSelf scheduleReconnect];
            }
        };

        os_log_info(self->_log, "Resuming XPC connection");
        [connection resume];

        self->_connection = connection;

        self->_appProxy = [connection remoteObjectProxyWithErrorHandler:^(NSError *error) {
            os_log_error(self->_log, "Error getting remote proxy: %{public}@", error.localizedDescription);
        }];

        self->_isConnected = YES;

        // Request initial localized strings
        [self requestLocalizedStrings];
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
        [self->_connection invalidate];
        self->_connection = nil;
        self->_appProxy = nil;
        self->_isConnected = NO;
    });
}

- (BOOL)isConnected
{
    return _isConnected;
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
