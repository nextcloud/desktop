#import <Cocoa/Cocoa.h>

#import "LNStandardVersionComparator.h"

#define EXPORT __attribute__((visibility("default")))

#define WAIT_FOR_APPLE_EVENT_TO_ENTER_HANDLER_IN_SECONDS 1.0
#define FINDER_MIN_TESTED_VERSION @"10.7"
#define FINDER_MAX_TESTED_VERSION @"10.8.5"
#define LIFERAYNATIVITY_INJECTED_NOTIFICATION @"SyncStateInjectedNotification"

EXPORT OSErr HandleLoadEvent(const AppleEvent* ev, AppleEvent* reply, long refcon);

static NSString* globalLock = @"I'm the global lock to prevent concruent handler executions";

// SIMBL-compatible interface
@interface OwnCloudShell : NSObject { }
-(void) install;
-(void) uninstall;
@end

// just a dummy class for locating our bundle
@interface OwnCloudInjector : NSObject { }
@end

@implementation OwnCloudInjector { }
@end

static bool liferayNativityLoaded = false;
static NSString* liferayNativityBundleName = @"SyncStateFinder";

typedef struct {
  NSString* location;
} configuration;

static OSErr AEPutParamString(AppleEvent* event, AEKeyword keyword, NSString* string) {
  UInt8* textBuf;
  CFIndex length, maxBytes, actualBytes;

  length = CFStringGetLength((CFStringRef)string);
  maxBytes = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
  textBuf = malloc(maxBytes);
  if (textBuf) {
    CFStringGetBytes((CFStringRef)string, CFRangeMake(0, length), kCFStringEncodingUTF8, 0, true, (UInt8*)textBuf, maxBytes, &actualBytes);
    OSErr err = AEPutParamPtr(event, keyword, typeUTF8Text, textBuf, actualBytes);
    free(textBuf);
    return err;
  } else {
    return memFullErr;
  }
}

static void reportError(AppleEvent* reply, NSString* msg) {
  NSLog(@"LiferayNativityInjector: %@", msg);
  AEPutParamString(reply, keyErrorString, msg);
}

typedef enum {
  InvalidBundleType,
  LiferayNativityBundleType,
} LNBundleType;

static OSErr loadBundle(LNBundleType type, AppleEvent* reply, long refcon) {
  bool isLoaded = false;
  NSString* bundleName = nil;
  NSString* targetAppName = nil;
  NSString* versionCheckKey = nil;
  NSString* maxVersion = nil;
  NSString* minVersion = nil;

  switch (type) {
    case LiferayNativityBundleType:
      isLoaded = liferayNativityLoaded;
      bundleName = liferayNativityBundleName;
      targetAppName = @"Finder";
      versionCheckKey = @"LiferayNativityFinderVersionCheck";
      maxVersion = FINDER_MAX_TESTED_VERSION;
      minVersion = FINDER_MIN_TESTED_VERSION;
      break;
    default:
      NSLog(@"SyncStateInjector: Failed to load bundle for type %d", type);
      return 8;

      break;
  }

  if (isLoaded) {
    // NSLog(@"SyncStateInjector: %@ already loaded.", bundleName);
    return noErr;
  }

  @try {
    NSBundle* mainBundle = [NSBundle mainBundle];
    if (!mainBundle) {
      reportError(reply, [NSString stringWithFormat:@"Unable to locate main %@ bundle!", targetAppName]);
      return 4;
    }

    NSString* mainVersion = [mainBundle objectForInfoDictionaryKey:@"CFBundleVersion"];
    if (!mainVersion || ![mainVersion isKindOfClass:[NSString class]]) {
      reportError(reply, [NSString stringWithFormat:@"Unable to determine %@ version!", targetAppName]);
      return 5;
    }

    // future compatibility check
    if (type == LiferayNativityBundleType) {
      // in Dock we cannot use NSAlert and similar UI stuff - this would hang the Dock process and cause 100% CPU load
      NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
      if ([defaults boolForKey:versionCheckKey]) {
        LNStandardVersionComparator* comparator = [LNStandardVersionComparator defaultComparator];
        if (([comparator compareVersion:mainVersion toVersion:maxVersion] == NSOrderedDescending) ||
            ([comparator compareVersion:mainVersion toVersion:minVersion] == NSOrderedAscending)) {
          NSAlert* alert = [NSAlert new];
          [alert setMessageText:[NSString stringWithFormat:@"You have %@ version %@", targetAppName, mainVersion]];
          [alert setInformativeText:[NSString stringWithFormat:@"But %@ was properly tested only with %@ versions in range %@ - %@\n\nYou have probably updated your system and %@ version got bumped by Apple developers.\n\nYou may expect a new LiferayNativity release soon.", bundleName, targetAppName, targetAppName, minVersion, maxVersion]];
          [alert setShowsSuppressionButton:YES];
          [alert addButtonWithTitle:@"Launch LiferayNativity anyway"];
          [alert addButtonWithTitle:@"Cancel"];
          NSInteger res = [alert runModal];
          if ([[alert suppressionButton] state] == NSOnState) {
            [defaults setBool:NO forKey:versionCheckKey];
          }
          if (res != NSAlertFirstButtonReturn) {
            // cancel
            return noErr;
          }
        }
      }
    }

    NSBundle* liferayNativityInjectorBundle = [NSBundle bundleForClass:[OwnCloudInjector class]];
    NSString* liferayNativityLocation = [liferayNativityInjectorBundle pathForResource:bundleName ofType:@"bundle"];
    NSBundle* pluginBundle = [NSBundle bundleWithPath:liferayNativityLocation];
    if (!pluginBundle) {
      reportError(reply, [NSString stringWithFormat:@"Unable to create bundle from path: %@ [%@]", liferayNativityLocation, liferayNativityInjectorBundle]);
      return 2;
    }

    NSError* error;
    if (![pluginBundle loadAndReturnError:&error]) {
      reportError(reply, [NSString stringWithFormat:@"Unable to load bundle from path: %@ error: %@", liferayNativityLocation, [error localizedDescription]]);
      return 6;
    }

    Class principalClass = [pluginBundle principalClass];
    if (!principalClass) {
      reportError(reply, [NSString stringWithFormat:@"Unable to retrieve principalClass for bundle: %@", pluginBundle]);
      return 3;
    }
    id principalClassObject = NSClassFromString(NSStringFromClass(principalClass));
    if ([principalClassObject respondsToSelector:@selector(install)]) {
      // NSLog(@"SyncStateInjector: Installing %@ ...", bundleName);
      [principalClassObject install];
    }

    return noErr;
  } @catch (NSException* exception) {
    reportError(reply, [NSString stringWithFormat:@"Failed to load %@ with exception: %@", bundleName, exception]);
  }

  return 1;
}

static LNBundleType mainBundleType(AppleEvent* reply) {
  @try {
    NSBundle* mainBundle = [NSBundle mainBundle];
    if (!mainBundle) {
      reportError(reply, [NSString stringWithFormat:@"Unable to locate main bundle!"]);
      return InvalidBundleType;
    }

    if ([[mainBundle bundleIdentifier] isEqualToString:@"com.apple.finder"]) {
      return LiferayNativityBundleType;
    }
  } @catch (NSException* exception) {
    reportError(reply, [NSString stringWithFormat:@"Failed to load main bundle with exception: %@", exception]);
  }

  return InvalidBundleType;
}

EXPORT OSErr HandleLoadEvent(const AppleEvent* ev, AppleEvent* reply, long refcon) {
  @synchronized(globalLock) {
    @autoreleasepool {
      NSBundle* injectorBundle = [NSBundle bundleForClass:[OwnCloudInjector class]];
      NSString* injectorVersion = [injectorBundle objectForInfoDictionaryKey:@"CFBundleVersion"];

      if (!injectorVersion || ![injectorVersion isKindOfClass:[NSString class]]) {
        reportError(reply, [NSString stringWithFormat:@"Unable to determine SyncStateInjector version!"]);
        return 7;
      }

      @try {
        OSErr err = loadBundle(mainBundleType(reply), reply, refcon);

        if (err != noErr)
        {
          return err;
        }

        pid_t pid = [[NSProcessInfo processInfo] processIdentifier];

        [[NSDistributedNotificationCenter defaultCenter]postNotificationName:LIFERAYNATIVITY_INJECTED_NOTIFICATION object:[[NSBundle mainBundle]bundleIdentifier] userInfo:@{@"pid": @(pid)}];

        liferayNativityLoaded = true;

        return noErr;
      } @catch (NSException* exception) {
        reportError(reply, [NSString stringWithFormat:@"Failed to load OwnCloudFinder with exception: %@", exception]);
      }

      return 1;
    }
  }
}

EXPORT OSErr HandleLoadedEvent(const AppleEvent* ev, AppleEvent* reply, long refcon) {
  @synchronized(globalLock) {
    @autoreleasepool {
      LNBundleType type = mainBundleType(reply);
      if ((type == LiferayNativityBundleType) && liferayNativityLoaded) {
        return noErr;
      }
      reportError(reply, @"LiferayNativity not loaded");
      return 1;
    }
  }
}

EXPORT OSErr HandleUnloadEvent(const AppleEvent* ev, AppleEvent* reply, long refcon) {
  @synchronized(globalLock) {
    @autoreleasepool {
      @try {
        if (!liferayNativityLoaded) {
          // NSLog(@"SyncStateInjector: not loaded.");
          return noErr;
        }

        NSString* bundleName = liferayNativityBundleName;

        NSBundle* liferayNativityInjectorBundle = [NSBundle bundleForClass:[OwnCloudInjector class]];
        NSString* liferayNativityLocation = [liferayNativityInjectorBundle pathForResource:bundleName ofType:@"bundle"];
        NSBundle* pluginBundle = [NSBundle bundleWithPath:liferayNativityLocation];
        if (!pluginBundle) {
          reportError(reply, [NSString stringWithFormat:@"Unable to create bundle from path: %@ [%@]", liferayNativityLocation, liferayNativityInjectorBundle]);
          return 2;
        }

        Class principalClass = [pluginBundle principalClass];
        if (!principalClass) {
          reportError(reply, [NSString stringWithFormat:@"Unable to retrieve principalClass for bundle: %@", pluginBundle]);
          return 3;
        }
        id principalClassObject = NSClassFromString(NSStringFromClass(principalClass));
        if ([principalClassObject respondsToSelector:@selector(uninstall)]) {
          // NSLog(@"SyncStateInjector: Uninstalling %@ ...", bundleName);
          [principalClassObject uninstall];
        }

        liferayNativityLoaded = false;

        return noErr;
      } @catch (NSException* exception) {
        reportError(reply, [NSString stringWithFormat:@"Failed to unload OwnCloudFinder with exception: %@", exception]);
      }

      return 1;
    }
  }
}