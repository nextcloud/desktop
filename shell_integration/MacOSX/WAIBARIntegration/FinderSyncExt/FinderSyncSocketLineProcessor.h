/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
