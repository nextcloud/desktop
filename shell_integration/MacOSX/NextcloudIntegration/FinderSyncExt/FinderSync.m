/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "FinderSync.h"
#import "FinderSyncXPCManager.h"
#import <os/log.h>

@interface FinderSync()
{
    NSMutableSet *_registeredDirectories;
    NSString *_shareMenuTitle;
    NSMutableDictionary *_strings;
    NSMutableArray *_menuItems;
    NSCondition *_menuIsComplete;
    BOOL _menuReady;  // Predicate for _menuIsComplete; guards against lost NSCondition signals.
    os_log_t _log;
}
@end

// Upper bound on how long -menuForMenuKind: blocks Finder's thread waiting for the app to
// return menu items. A healthy connection replies in milliseconds; this only bites when the
// app died mid-request, so we cap it rather than hang Finder indefinitely (issues #10032/#8363).
static const NSTimeInterval kFinderSyncMenuWaitTimeoutSeconds = 5.0;

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

        // Initialize XPC manager instead of socket client. The manager pulls the localized
        // strings itself once the handshake with the app succeeds, so there is no point
        // requesting them here — at init time the connection is never established yet.
        os_log_info(_log, "Initializing FinderSync XPC manager");
        self.xpcManager = [[FinderSyncXPCManager alloc] initWithDelegate:self];
        [self.xpcManager start];

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
	[self.xpcManager askForIcon:normalizedPath isDirectory:isDir];
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
    NSDate *const deadline = [NSDate dateWithTimeIntervalSinceNow:kFinderSyncMenuWaitTimeoutSeconds];
    [self->_menuIsComplete lock];
    // Loop on a predicate rather than a bare -wait: an NSCondition signal is not sticky, so if
    // -menuHasCompleted fired before we reached -wait the signal would be lost and we would block
    // forever. The deadline additionally guarantees we never hang Finder if the reply never comes.
    while (!self->_menuReady) {
        if (![self->_menuIsComplete waitUntilDate:deadline]) {
            os_log_error(_log, "Timed out waiting for menu items from app after %.0f s; returning what we have",
                         (double)kFinderSyncMenuWaitTimeoutSeconds);
            break;
        }
    }
    [self->_menuIsComplete unlock];
    os_log_debug(_log, "Menu arrival wait completed (ready=%d)", self->_menuReady);
}

- (NSMenu *)menuForMenuKind:(FIMenuKind)whichMenu
{
    os_log_debug(_log, "Building menu for menu kind: %lu", (unsigned long)whichMenu);

    if(![self.xpcManager isConnected]) {
        // Returning nil shows no Nextcloud entry at all, which the user cannot tell apart from
        // "the extension isn't installed" — that ambiguity is a large part of why 34.0.0's dead
        // channel went unreported for so long. A single disabled item costs nothing and turns an
        // invisible failure into something screenshot-able.
        os_log_error(_log, "Not connected, cannot build menu.");

        NSString *appName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleDisplayName"]
            ?: [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"] ?: @"Nextcloud";
        NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
        NSMenuItem *item = [menu addItemWithTitle:[NSString stringWithFormat:@"%@ — not connected", appName]
                                          action:nil
                                   keyEquivalent:@""];
        item.enabled = NO;
        item.image = [[NSBundle mainBundle] imageForResource:@"app.icns"];

        return menu;
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

    // Arm the predicate before issuing the async request so a reply that races back
    // before -waitForMenuToArrive cannot be missed.
    [self->_menuIsComplete lock];
    self->_menuReady = NO;
    [self->_menuIsComplete unlock];

	[self.xpcManager askOnSocket:paths query:@"GET_MENU_ITEMS"];

    // Since the XPC communication is asynchronous, wait here until the menu
    // is delivered by another thread
    [self waitForMenuToArrive];

	// Snapshot under the lock. The wait can also end on its deadline, and after a timeout the
	// delivery queue is still free to keep appending — iterating the live array while it mutates
	// would throw.
	NSArray *menuItems = nil;
	[self->_menuIsComplete lock];
	menuItems = [_menuItems copy];
	[self->_menuIsComplete unlock];

	id contextMenuTitle = nil;
	@synchronized(_strings) {
		contextMenuTitle = [_strings objectForKey:@"CONTEXT_MENU_TITLE"];
	}

	if (contextMenuTitle && !onlyRootsSelected) {
		os_log_debug(_log, "Creating context menu with title: %{public}@", contextMenuTitle);
		NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
		NSMenu *subMenu = [[NSMenu alloc] initWithTitle:@""];
		NSMenuItem *subMenuItem = [menu addItemWithTitle:contextMenuTitle action:nil keyEquivalent:@""];
		subMenuItem.submenu = subMenu;
		subMenuItem.image = [[NSBundle mainBundle] imageForResource:@"app.icns"];

		int idx = 0;
		for (NSArray* item in menuItems) {
			NSMenuItem *actionItem = [subMenu addItemWithTitle:[item valueForKey:@"text"]
														action:@selector(subMenuActionClicked:)
												 keyEquivalent:@""];

			// Finder copies the menu into its own process to display it but identifier does not survive it
			// Use tag to pass the command: an index into _menuItems that subMenuActionClicked: resolves locally
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
	const NSInteger tag = [(NSMenuItem *)sender tag];
	NSString *command = nil;
	[self->_menuIsComplete lock];
	if (tag >= 0 && tag < (NSInteger)_menuItems.count) {
		command = _menuItems[tag][@"command"];
		os_log_debug(_log, "command for menu: %{public}@", command);
	}
	[self->_menuIsComplete unlock];

	if (command.length == 0) {
		os_log_error(_log, "Menu item clicked with no command attached; ignoring");
		return;
	}

	NSString *paths = [self selectedPathsSeparatedByRecordSeparator];
	os_log_debug(_log, "Executing command: %{public}@", command);
	[self.xpcManager askOnSocket:paths query:command];
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
	// Written from the XPC delivery queue but read from Finder's menu thread in
	// -menuForMenuKind:. NSMutableDictionary is not thread-safe, and a -setObject:forKey: that
	// rehashes while -objectForKey: is walking a bucket can crash the extension outright.
	@synchronized(_strings) {
		[_strings setObject:value forKey:key];
	}
}

- (void)resetMenuItems
{
	os_log_debug(_log, "Resetting menu items");
	// _menuItems is published to Finder's menu thread, which reads it after -menuHasCompleted
	// under _menuIsComplete. Mutating it under the same lock is what makes that handoff safe;
	// see the ordering note in -menuHasCompleted.
	[self->_menuIsComplete lock];
	_menuItems = [[NSMutableArray alloc] init];
	[self->_menuIsComplete unlock];
	os_log_debug(_log, "Menu items reset completed");
}

- (void)addMenuItem:(NSDictionary *)item {
    os_log_debug(_log, "Adding menu item with title: %{public}@", [item valueForKey:@"text"] ?: @"(no title)");
	[self->_menuIsComplete lock];
	[_menuItems addObject:item];
	const unsigned long count = (unsigned long)_menuItems.count;
	[self->_menuIsComplete unlock];
	os_log_debug(_log, "Menu item added, total items: %lu", count);
}

- (void)menuHasCompleted
{
    // Ordering matters here and is the reason -resetMenuItems and -addMenuItem: take the same
    // lock. All three arrive from the XPC manager's serial queue in protocol order, so taking
    // _menuIsComplete means the waiter cannot observe _menuReady before every item that
    // preceded this signal is in _menuItems. Delivering the reset/add pair on one queue and the
    // signal on another — as the socket implementation used to — left them unordered, and the
    // waiter could wake to the *previous* request's items.
    os_log_debug(_log, "Menu completion signal received");
    [self->_menuIsComplete lock];
    self->_menuReady = YES;
    [self->_menuIsComplete signal];
    [self->_menuIsComplete unlock];
    os_log_debug(_log, "Menu signal emitted");
}

- (void)connectionDidDie
{
	os_log_error(_log, "Connection to sync client died");
	@synchronized(_strings) {
		[_strings removeAllObjects];
	}
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


