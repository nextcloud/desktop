/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "xpcservicetestpeer.h"

static_assert(__has_feature(objc_arc), "XPC service test peer requires ARC.");

@implementation XPCServiceTestPeer

- (void)resume
{
}

- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *))handler
{
    return self.remoteObject;
}

- (void)invalidate
{
    if (self.didInvalidate) {
        self.didInvalidate();
    }
    if (self.invalidationHandler) {
        self.invalidationHandler();
    }
}

- (void)registerPath:(NSString *)path
{
    self.registeredPath = path;
    if (self.registerPathHandler) {
        self.registerPathHandler(path);
    }
}

- (void)unregisterPath:(NSString *)path
{
    self.registeredPath = nil;
}

- (void)updateViewAtPath:(NSString *)path
{
}

- (void)setStatusResult:(NSString *)status forPath:(NSString *)path
{
}

- (void)setLocalizedString:(NSString *)value forKey:(NSString *)key
{
}

- (void)resetMenuItems
{
}

- (void)addMenuItemWithCommand:(NSString *)command flags:(NSString *)flags text:(NSString *)text
{
}

- (void)menuItemsComplete
{
}

@end
