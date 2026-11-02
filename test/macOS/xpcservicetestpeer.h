/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import "../../shell_integration/MacOSX/NextcloudIntegration/FinderSyncExt/Services/FinderSyncProtocol.h"
#import <Foundation/Foundation.h>

@interface XPCServiceTestPeer : NSObject <FinderSyncProtocol>
@property (nonatomic, strong) NSXPCInterface *exportedInterface;
@property (nonatomic, strong) NSXPCInterface *remoteObjectInterface;
@property (nonatomic, strong) id exportedObject;
@property (nonatomic, strong) id remoteObject;
@property (nonatomic, copy) void (^interruptionHandler)(void);
@property (nonatomic, copy) void (^invalidationHandler)(void);
@property (nonatomic, copy) void (^didInvalidate)(void);
@property (nonatomic, copy) void (^registerPathHandler)(NSString *);
@property (nonatomic, copy) NSString *registeredPath;
- (void)resume;
- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError *))handler;
- (void)invalidate;
@end
