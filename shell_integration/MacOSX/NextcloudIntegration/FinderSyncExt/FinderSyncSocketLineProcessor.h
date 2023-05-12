/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#import <NCDesktopClientSocketKit/LineProcessor.h>

#import "SyncClient.h"

#ifndef FinderSyncSocketLineProcessor_h
#define FinderSyncSocketLineProcessor_h

/// This class is in charge of dispatching all work that must be done on the UI side of the extension.
/// Tasks are dispatched on the main UI thread for this reason.
///
/// These tasks are parsed from byte data (UTF8 strings) acquired from the socket; look at the
/// LocalSocketClient for more detail on how data is read from and written to the socket.

@interface FinderSyncSocketLineProcessor : NSObject<LineProcessor>

@property(nonatomic, weak) id<SyncClientDelegate> delegate;

- (instancetype)initWithDelegate:(id<SyncClientDelegate>)delegate;

@end
#endif /* LineProcessor_h */
