/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import <Foundation/Foundation.h>
#import <os/log.h>
#import "FinderSyncSocketLineProcessor.h"

static os_log_t getFinderSyncSocketLineProcessorLogger(void) {
    static dispatch_once_t onceToken;
    static os_log_t logger = NULL;
    dispatch_once(&onceToken, ^{
        NSBundle *bundle = [NSBundle bundleForClass:[FinderSyncSocketLineProcessor class]];
        NSString *subsystem = bundle.bundleIdentifier ?: @"FinderSyncExt";
        logger = os_log_create(subsystem.UTF8String, "FinderSyncSocketLineProcessor");
    });
    return logger;
}

@interface FinderSyncSocketLineProcessor()
{
    os_log_t _log;
}
@end

@implementation FinderSyncSocketLineProcessor

-(instancetype)initWithDelegate:(id<SyncClientDelegate>)delegate
{
    os_log_t logger = getFinderSyncSocketLineProcessorLogger();
    os_log_debug(logger, "Initializing FinderSyncSocketLineProcessor with delegate");
    self = [super init];
    if (self) {
        _log = logger;
        self.delegate = delegate;
        os_log_debug(logger, "FinderSyncSocketLineProcessor initialization completed");
    }
    return self;
}

-(void)process:(NSString*)line
{
    os_log_debug(_log, "Processing line: %{public}@", line);
    NSArray *split = [line componentsSeparatedByString:@":"];
    NSString *command = [split objectAtIndex:0];
    
    os_log_debug(_log, "Command: %{public}@", command);
    
    if([command isEqualToString:@"STATUS"]) {
        NSString *result = [split objectAtIndex:1];
        NSArray *pathSplit = [split subarrayWithRange:NSMakeRange(2, [split count] - 2)]; // Get everything after location 2
        NSString *path = [pathSplit componentsJoinedByString:@":"];
        os_log_debug(_log, "STATUS command: result=%{public}@, path=%{public}@", result, path);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            os_log_debug(_log, "Setting result %{public}@ for path %{public}@", result, path);
            [self.delegate setResult:result forPath:path];
        });
    } else if([command isEqualToString:@"UPDATE_VIEW"]) {
        NSString *path = [split objectAtIndex:1];
        os_log_debug(_log, "UPDATE_VIEW command: path=%{public}@", path);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            os_log_debug(_log, "Re-fetching filename cache for path %{public}@", path);
            [self.delegate reFetchFileNameCacheForPath:path];
        });
    } else if([command isEqualToString:@"REGISTER_PATH"]) {
        NSString *path = [split objectAtIndex:1];
        os_log_debug(_log, "REGISTER_PATH command: path=%{public}@", path);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            os_log_debug(_log, "Registering path %{public}@", path);
            [self.delegate registerPath:path];
        });
    } else if([command isEqualToString:@"UNREGISTER_PATH"]) {
        NSString *path = [split objectAtIndex:1];
        os_log_debug(_log, "UNREGISTER_PATH command: path=%{public}@", path);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            os_log_debug(_log, "Unregistering path %{public}@", path);
            [self.delegate unregisterPath:path];
        });
    } else if([command isEqualToString:@"GET_STRINGS"]) {
        os_log_debug(_log, "GET_STRINGS command: %{public}@", [split objectAtIndex:1] ?: @"(no subcommand)");
        // BEGIN and END messages, do nothing.
        return;
    } else if([command isEqualToString:@"STRING"]) {
        NSString *key = [split objectAtIndex:1];
        NSString *value = [split objectAtIndex:2];
        os_log_debug(_log, "STRING command: key=%{public}@, value=%{public}@", key, value);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            os_log_debug(_log, "Setting string %{public}@ to value %{public}@", key, value);
            [self.delegate setString:key value:value];
        });
    } else if([command isEqualToString:@"GET_MENU_ITEMS"]) {
        os_log_debug(_log, "GET_MENU_ITEMS command: subcommand=%{public}@", [split objectAtIndex:1] ?: @"(no subcommand)");
        if([[split objectAtIndex:1] isEqualToString:@"BEGIN"]) {
            dispatch_async(dispatch_get_main_queue(), ^{
                os_log_debug(_log, "Resetting menu items");
                [self.delegate resetMenuItems];
            });
        } else {
            os_log_debug(_log, "Emitting menu has completed signal");
            [self.delegate menuHasCompleted];
        }
    } else if([command isEqualToString:@"MENU_ITEM"]) {
        NSDictionary *item = @{@"command": [split objectAtIndex:1], @"flags": [split objectAtIndex:2], @"text": [split objectAtIndex:3]};
        os_log_debug(_log, "MENU_ITEM command: command=%{public}@, flags=%{public}@, text=%{public}@", [split objectAtIndex:1], [split objectAtIndex:2], [split objectAtIndex:3]);
        
        dispatch_async(dispatch_get_main_queue(), ^{
            os_log_debug(_log, "Adding menu item with command %{public}@, flags %{public}@, and text %{public}@", [split objectAtIndex:1], [split objectAtIndex:2], [split objectAtIndex:3]);
            [self.delegate addMenuItem:item];
        });
    } else {
        os_log_error(_log, "Unknown command: %{public}@", command);
    }
    os_log_debug(_log, "Line processing completed");
}

@end
