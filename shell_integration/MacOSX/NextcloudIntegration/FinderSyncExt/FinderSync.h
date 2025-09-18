/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <Cocoa/Cocoa.h>
#import <FinderSync/FinderSync.h>
#import <NCDesktopClientSocketKit/LocalSocketClient.h>

#import "SyncClient.h"
#import "FinderSyncSocketLineProcessor.h"

@interface FinderSync : FIFinderSync <SyncClientDelegate>

@property FinderSyncSocketLineProcessor *lineProcessor;
@property LocalSocketClient *localSocketClient;

@end
