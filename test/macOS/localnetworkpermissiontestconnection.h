/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <Foundation/Foundation.h>
#import <Network/Network.h>

@interface LocalNetworkPermissionTestConnection : NSObject
@property (nonatomic, copy) nw_connection_state_changed_handler_t stateHandler;
@property (nonatomic, copy) nw_connection_path_event_handler_t pathHandler;
@property (nonatomic) BOOL started;
@end
