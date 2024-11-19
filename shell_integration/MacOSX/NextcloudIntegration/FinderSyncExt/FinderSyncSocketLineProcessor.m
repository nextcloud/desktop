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
        NSString *result = [split objectAtIndex:1];
        NSArray *pathSplit = [split subarrayWithRange:NSMakeRange(2, [split count] - 2)]; // Get everything after location 2
        NSString *path = [pathSplit componentsJoinedByString:@":"];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Setting result %@ for path %@", result, path);
            [self.delegate setResult:result forPath:path];
        });
    } else if([command isEqualToString:@"UPDATE_VIEW"]) {
        NSString *path = [split objectAtIndex:1];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Re-fetching filename cache for path %@", path);
            [self.delegate reFetchFileNameCacheForPath:path];
        });
    } else if([command isEqualToString:@"REGISTER_PATH"]) {
        NSString *path = [split objectAtIndex:1];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Registering path %@", path);
            [self.delegate registerPath:path];
        });
    } else if([command isEqualToString:@"UNREGISTER_PATH"]) {
        NSString *path = [split objectAtIndex:1];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Unregistering path %@", path);
            [self.delegate unregisterPath:path];
        });
    } else if([command isEqualToString:@"GET_STRINGS"]) {
        // BEGIN and END messages, do nothing.
        return;
    } else if([command isEqualToString:@"STRING"]) {
        NSString *key = [split objectAtIndex:1];
        NSString *value = [split objectAtIndex:2];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"Setting string %@ to value %@", key, value);
            [self.delegate setString:key value:value];
        });
    } else if([command isEqualToString:@"GET_MENU_ITEMS"]) {
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
