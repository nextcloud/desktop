/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <Foundation/Foundation.h>
#import "FinderSyncSocketLineProcessor.h"

@implementation FinderSyncSocketLineProcessor

-(instancetype)initWithDelegate:(id<SyncClientDelegate>)delegate
{
    NSLog(@"Init line processor with delegate.");
    self = [super init];
    if (self) {
        self.delegate = delegate;
    }
    return self;
}

-(void)process:(NSString*)line
{
    NSLog(@"Processing line: '%@'", line);
    NSArray *split = [line componentsSeparatedByString:@":"];
    NSString *command = [split objectAtIndex:0];
    
    NSLog(@"Command: %@", command);
    
    if([command isEqualToString:@"STATUS"]) {
        if([split count] < 3) {
            NSLog(@"ERROR: STATUS message too short: '%@'", line);
            return;
        }
        NSString *result = [split objectAtIndex:1];
        NSArray *pathSplit = [split subarrayWithRange:NSMakeRange(2, [split count] - 2)]; // Get everything after location 2
        NSString *path = [pathSplit componentsJoinedByString:@":"];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Setting result %@ for path %@", result, path);
            [self.delegate setResult:result forPath:path];
        });
    } else if([command isEqualToString:@"UPDATE_VIEW"]) {
        if([split count] < 2) {
            NSLog(@"ERROR: UPDATE_VIEW message too short: '%@'", line);
            return;
        }
        NSString *path = [split objectAtIndex:1];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Re-fetching filename cache for path %@", path);
            [self.delegate reFetchFileNameCacheForPath:path];
        });
    } else if([command isEqualToString:@"REGISTER_PATH"]) {
        if([split count] < 2) {
            NSLog(@"ERROR: REGISTER_PATH message too short: '%@'", line);
            return;
        }
        NSString *path = [split objectAtIndex:1];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Registering path %@", path);
            [self.delegate registerPath:path];
        });
    } else if([command isEqualToString:@"UNREGISTER_PATH"]) {
        if([split count] < 2) {
            NSLog(@"ERROR: UNREGISTER_PATH message too short: '%@'", line);
            return;
        }
        NSString *path = [split objectAtIndex:1];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Unregistering path %@", path);
            [self.delegate unregisterPath:path];
        });
    } else if([command isEqualToString:@"GET_STRINGS"]) {
        // BEGIN and END messages, do nothing.
        return;
    } else if([command isEqualToString:@"STRING"]) {
        if([split count] < 3) {
            NSLog(@"ERROR: STRING message too short: '%@'", line);
            return;
        }
        NSString *key = [split objectAtIndex:1];
        NSString *value = [split objectAtIndex:2];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Setting string %@ to value %@", key, value);
            [self.delegate setString:key value:value];
        });
    } else if([command isEqualToString:@"GET_MENU_ITEMS"]) {
        if([split count] < 2) {
            NSLog(@"ERROR: GET_MENU_ITEMS message too short: '%@'", line);
            return;
        }
        if([[split objectAtIndex:1] isEqualToString:@"BEGIN"]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSLog(@"Resetting menu items.");
                [self.delegate resetMenuItems];
            });
        } else {
            NSLog(@"Emitting menu has completed signal.");
            [self.delegate menuHasCompleted];
        }
    } else if([command isEqualToString:@"MENU_ITEM"]) {
        if([split count] < 4) {
            NSLog(@"ERROR: MENU_ITEM message too short: '%@'", line);
            return;
        }
        NSDictionary *item = @{@"command": [split objectAtIndex:1], @"flags": [split objectAtIndex:2], @"text": [split objectAtIndex:3]};
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Adding menu item with command %@, flags %@, and text %@", [split objectAtIndex:1], [split objectAtIndex:2], [split objectAtIndex:3]);
            [self.delegate addMenuItem:item];
        });
    } else {
        // LOG UNKNOWN COMMAND
        NSLog(@"Unknown command: %@", command);
    }
}

@end
