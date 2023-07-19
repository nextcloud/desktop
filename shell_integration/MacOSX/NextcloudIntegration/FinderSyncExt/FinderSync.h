/*
 * Copyright (C) by Jocelyn Turcotte <jturcotte@woboq.com>
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


#import <Cocoa/Cocoa.h>
#import <FinderSync/FinderSync.h>
#import <NCDesktopClientSocketKit/LocalSocketClient.h>

#import "SyncClient.h"
#import "FinderSyncSocketLineProcessor.h"

@interface FinderSync : FIFinderSync <SyncClientDelegate>

@property FinderSyncSocketLineProcessor *lineProcessor;
@property LocalSocketClient *localSocketClient;

@end
