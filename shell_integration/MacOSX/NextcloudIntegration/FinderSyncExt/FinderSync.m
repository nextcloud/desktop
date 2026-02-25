/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "FinderSync.h"
#import <os/log.h>

@interface FinderSync()
{
    NSMutableSet *_registeredDirectories;
    NSString *_shareMenuTitle;
    NSMutableDictionary *_strings;
    NSMutableArray *_menuItems;
    NSCondition *_menuIsComplete;
    os_log_t _log;
}
@end

static os_log_t getFinderSyncLogger(void) {
    static dispatch_once_t onceToken;
    static os_log_t logger = NULL;
    dispatch_once(&onceToken, ^{
        NSBundle *bundle = [NSBundle bundleForClass:[FinderSync class]];
        NSString *subsystem = bundle.bundleIdentifier ?: @"FinderSyncExt";
        logger = os_log_create(subsystem.UTF8String, "FinderSync");
    });
    return logger;
}

@implementation FinderSync

- (instancetype)init
{
	self = [super init];

    if (self) {
        _log = getFinderSyncLogger();
        os_log_debug(_log, "Initializing...");
        FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
        NSBundle *extBundle = [NSBundle bundleForClass:[self class]];
        // This was added to the bundle's Info.plist to get it from the build system
        NSString *groupIdentifier = [extBundle objectForInfoDictionaryKey:@"NCApplicationGroupIdentifier"];

        NSImage *ok = [extBundle imageForResource:@"ok.icns"];
        NSImage *ok_swm = [extBundle imageForResource:@"ok_swm.icns"];
        NSImage *sync = [extBundle imageForResource:@"sync.icns"];
        NSImage *warning = [extBundle imageForResource:@"warning.icns"];
        NSImage *error = [extBundle imageForResource:@"error.icns"];

        [syncController setBadgeImage:ok label:@"Up to date" forBadgeIdentifier:@"OK"];
        [syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"SYNC"];
        [syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"NEW"];
        [syncController setBadgeImage:warning label:@"Ignored" forBadgeIdentifier:@"IGNORE"];
        [syncController setBadgeImage:error label:@"Error" forBadgeIdentifier:@"ERROR"];
        [syncController setBadgeImage:ok_swm label:@"Shared" forBadgeIdentifier:@"OK+SWM"];
        [syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"SYNC+SWM"];
        [syncController setBadgeImage:sync label:@"Synchronizing" forBadgeIdentifier:@"NEW+SWM"];
        [syncController setBadgeImage:warning label:@"Ignored" forBadgeIdentifier:@"IGNORE+SWM"];
        [syncController setBadgeImage:error label:@"Error" forBadgeIdentifier:@"ERROR+SWM"];

        NSURL *container = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:groupIdentifier];
        NSURL *library = [container URLByAppendingPathComponent:@"Library" isDirectory:true];
        NSURL *applicationSupport = [library URLByAppendingPathComponent:@"Application Support" isDirectory:true];
        NSURL *socketPath = [applicationSupport URLByAppendingPathComponent:@"s" isDirectory:NO];

        os_log_debug(_log, "Socket path: %{public}@", socketPath.path);

        if (socketPath.path && [[NSFileManager defaultManager] fileExistsAtPath:socketPath.path]) {
            os_log_debug(_log, "Socket path determined and exists: %{public}@", socketPath.path);
            self.lineProcessor = [[FinderSyncSocketLineProcessor alloc] initWithDelegate:self];
            self.localSocketClient = [[LocalSocketClient alloc] initWithSocketPath:socketPath.path
                                                                     lineProcessor:self.lineProcessor];
            [self.localSocketClient start];
            [self.localSocketClient askOnSocket:@"" query:@"GET_STRINGS"];
        } else {
            if (socketPath.path) {
                os_log_error(_log, "Socket path determined but file does not exist: %{public}@", socketPath.path);
            } else {
                os_log_error(_log, "No socket path available. Not initiating local socket client.");
            }

            self.localSocketClient = nil;
        }

        _registeredDirectories = NSMutableSet.set;
        _strings = NSMutableDictionary.dictionary;
        _menuIsComplete = [[NSCondition alloc] init];
        os_log_debug(_log, "Initialization completed.");
    }

    return self;
}

#pragma mark - Primary Finder Sync protocol methods

- (void)requestBadgeIdentifierForURL:(NSURL *)url
{
	os_log_debug(_log, "Requesting badge identifier for URL: %{public}@", url.path);
	BOOL isDir;
	if ([[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory: &isDir] == NO) {
		os_log_error(_log, "Could not determine file type of %{public}@", [url path]);
		isDir = NO;
	}

	NSString* normalizedPath = [[url path] decomposedStringWithCanonicalMapping];
	[self.localSocketClient askForIcon:normalizedPath isDirectory:isDir];
	os_log_debug(_log, "Badge identifier request completed for: %{public}@", normalizedPath);
}

#pragma mark - Menu and toolbar item support

- (NSString*) selectedPathsSeparatedByRecordSeparator
{
	os_log_debug(_log, "Building selected paths string with record separators");
	FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	NSMutableString *string = [[NSMutableString alloc] init];
	[syncController.selectedItemURLs enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx, BOOL *stop) {
		if (string.length > 0) {
			[string appendString:@"\x1e"]; // record separator
		}
		NSString* normalizedPath = [[obj path] decomposedStringWithCanonicalMapping];
		[string appendString:normalizedPath];
	}];
	os_log_debug(_log, "Selected paths string built: %lu paths", (unsigned long)syncController.selectedItemURLs.count);
	return string;
}

- (void)waitForMenuToArrive
{
    os_log_debug(_log, "Waiting for menu to arrive");
    [self->_menuIsComplete lock];
    [self->_menuIsComplete wait];
    [self->_menuIsComplete unlock];
    os_log_debug(_log, "Menu arrival wait completed");
}

- (NSMenu *)menuForMenuKind:(FIMenuKind)whichMenu
{
    os_log_debug(_log, "Building menu for menu kind: %lu", (unsigned long)whichMenu);
    if(![self.localSocketClient isConnected]) {
        os_log_error(_log, "Local socket client not connected, cannot build menu");
        return nil;
    }
    
	FIFinderSyncController *syncController = [FIFinderSyncController defaultController];
	NSMutableSet *rootPaths = [[NSMutableSet alloc] init];
	[syncController.directoryURLs enumerateObjectsUsingBlock: ^(id obj, BOOL *stop) {
		[rootPaths addObject:[obj path]];
	}];

	// The server doesn't support sharing a root directory so do not show the option in this case.
	// It is still possible to get a problematic sharing by selecting both the root and a child,
	// but this is so complicated to do and meaningless that it's not worth putting this check
	// also in shareMenuAction.
	__block BOOL onlyRootsSelected = YES;
	[syncController.selectedItemURLs enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx, BOOL *stop) {
		if (![rootPaths member:[obj path]]) {
			onlyRootsSelected = NO;
			*stop = YES;
		}
	}];
	os_log_debug(_log, "Root directories check: onlyRootsSelected = %d", onlyRootsSelected);

	NSString *paths = [self selectedPathsSeparatedByRecordSeparator];
	[self.localSocketClient askOnSocket:paths query:@"GET_MENU_ITEMS"];
    
    // Since the LocalSocketClient communicates asynchronously. wait here until the menu
    // is delivered by another thread
    [self waitForMenuToArrive];

	id contextMenuTitle = [_strings objectForKey:@"CONTEXT_MENU_TITLE"];
	if (contextMenuTitle && !onlyRootsSelected) {
		os_log_debug(_log, "Creating context menu with title: %{public}@", contextMenuTitle);
		NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
		NSMenu *subMenu = [[NSMenu alloc] initWithTitle:@""];
		NSMenuItem *subMenuItem = [menu addItemWithTitle:contextMenuTitle action:nil keyEquivalent:@""];
		subMenuItem.submenu = subMenu;
		subMenuItem.image = [[NSBundle mainBundle] imageForResource:@"app.icns"];

		// There is an annoying bug in macOS (at least 10.13.3), it does not use/copy over the representedObject of a menu item
		// So we have to use tag instead.
		int idx = 0;
		for (NSArray* item in _menuItems) {
			NSMenuItem *actionItem = [subMenu addItemWithTitle:[item valueForKey:@"text"]
														action:@selector(subMenuActionClicked:)
												 keyEquivalent:@""];
			[actionItem setTag:idx];
			[actionItem setTarget:self];
			NSString *flags = [item valueForKey:@"flags"]; // e.g. "d"
			if ([flags rangeOfString:@"d"].location != NSNotFound) {
				[actionItem setEnabled:false];
			}
			idx++;
		}
		os_log_debug(_log, "Context menu created with %d items", idx);
		return menu;
	}
	os_log_debug(_log, "No context menu created: contextMenuTitle=%@, onlyRootsSelected=%d", contextMenuTitle != nil ? @"present" : @"absent", onlyRootsSelected);
	return nil;
}

- (void)subMenuActionClicked:(id)sender {
	long idx = [(NSMenuItem*)sender tag];
	os_log_debug(_log, "Menu item clicked at index: %ld", idx);
	NSString *command = [[_menuItems objectAtIndex:idx] valueForKey:@"command"];
	NSString *paths = [self selectedPathsSeparatedByRecordSeparator];
	os_log_debug(_log, "Executing command: %{public}@", command);
	[self.localSocketClient askOnSocket:paths query:command];
	os_log_debug(_log, "Command execution completed");
}

#pragma mark - SyncClientProxyDelegate implementation

- (void)setResult:(NSString *)result forPath:(NSString*)path
{
    os_log_debug(_log, "Setting result: %{public}@ for path: %{public}@", result, path);
    NSString *const normalizedPath = path.decomposedStringWithCanonicalMapping;
    NSURL *const urlForPath = [NSURL fileURLWithPath:normalizedPath];
    if (urlForPath == nil) {
        os_log_error(_log, "Failed to create URL for path: %{public}@", normalizedPath);
        return;
    }
    [FIFinderSyncController.defaultController setBadgeIdentifier:result forURL:urlForPath];
    os_log_debug(_log, "Badge identifier set successfully");
}

- (void)reFetchFileNameCacheForPath:(NSString*)path
{
    os_log_debug(_log, "Refetching file name cache for path: %{public}@", path);
}

- (void)registerPath:(NSString*)path
{
	os_log_debug(_log, "Registering path: %{public}@", path);
	NSAssert(_registeredDirectories, @"Registered directories should be a valid set!");
	[_registeredDirectories addObject:[NSURL fileURLWithPath:path]];
	[FIFinderSyncController defaultController].directoryURLs = _registeredDirectories;
	os_log_debug(_log, "Path registration completed");
}

- (void)unregisterPath:(NSString*)path
{
	os_log_debug(_log, "Unregistering path: %{public}@", path);
	[_registeredDirectories removeObject:[NSURL fileURLWithPath:path]];
	[FIFinderSyncController defaultController].directoryURLs = _registeredDirectories;
	os_log_debug(_log, "Path unregistration completed");
}

- (void)setString:(NSString*)key value:(NSString*)value
{
	os_log_debug(_log, "Setting string: %{public}@ = %{public}@", key, value);
	[_strings setObject:value forKey:key];
}

- (void)resetMenuItems
{
	os_log_debug(_log, "Resetting menu items");
	_menuItems = [[NSMutableArray alloc] init];
	os_log_debug(_log, "Menu items reset completed");
}
- (void)addMenuItem:(NSDictionary *)item {
    os_log_debug(_log, "Adding menu item with title: %{public}@", [item valueForKey:@"text"] ?: @"(no title)");
	[_menuItems addObject:item];
	os_log_debug(_log, "Menu item added, total items: %lu", (unsigned long)_menuItems.count);
}

- (void)menuHasCompleted
{
    os_log_debug(_log, "Menu completion signal received");
    [self->_menuIsComplete signal];
    os_log_debug(_log, "Menu signal emitted");
}

- (void)connectionDidDie
{
	os_log_error(_log, "Connection to sync client died");
	[_strings removeAllObjects];
	[_registeredDirectories removeAllObjects];
	// For some reason the FIFinderSync cache doesn't seem to be cleared for the root item when
	// we reset the directoryURLs (seen on macOS 10.12 at least).
	// First setting it to the FS root and then setting it to nil seems to work around the issue.
	[FIFinderSyncController defaultController].directoryURLs = [NSSet setWithObject:[NSURL fileURLWithPath:@"/"]];
	// This will tell Finder that this extension isn't attached to any directory
	// until we can reconnect to the sync client.
	[FIFinderSyncController defaultController].directoryURLs = nil;
	os_log_error(_log, "Connection cleanup completed, waiting for reconnection");
}

@end


