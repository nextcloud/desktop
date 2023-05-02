/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 * Copyright (C) by Erik Verbruggen <erik@verbruggen.consulting>
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

#include "platform_mac.h"

#include <QApplication>
#include <QLoggingCategory>

#import <AppKit/NSApplication.h>

#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>


// defined in platform_mac_deprecated.mm
namespace OCC {

void migrateLaunchOnStartup();

Q_LOGGING_CATEGORY(lcPlatform, "platform.macos")
} // OCC namespace

@interface OwnAppDelegate : NSObject <NSApplicationDelegate>
- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag;
@end

@implementation OwnAppDelegate {
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
    if (auto *app = QApplication::instance()) {
        QMetaObject::invokeMethod(app, "showSettingsWindow", Qt::QueuedConnection);
    } else {
        qCDebug(OCC::lcPlatform) << "Failed to call showSettingsWindow slot";
    }
    return YES;
}

@end

namespace {

// Inspired by https://ladydebug.com/blog/2020/05/21/programmatically-capture-energy-saver-event-on-mac/
class PowerNotificationsListener
{
public:
    void registerForNotifications()
    {
        rootPowerDomain = IORegisterForSystemPower(this, &notifyPortRef, sleepWakeupCallBack, &notifierObj);
        if (rootPowerDomain == IO_OBJECT_NULL) {
            qCWarning(OCC::lcPlatform) << "Failed to register for system power notifications!";
            return;
        }

        qCDebug(OCC::lcPlatform) << "IORegisterForSystemPower OK! Root port:" << rootPowerDomain;

        // add the notification port to the application runloop
        CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(notifyPortRef), kCFRunLoopCommonModes);
    }

private:
    static void sleepWakeupCallBack(void *refParam, io_service_t service, natural_t messageType, void *messageArgument)
    {
        Q_UNUSED(service)

        auto listener = static_cast<PowerNotificationsListener *>(refParam);

        switch (messageType) {
        case kIOMessageCanSystemSleep:
            /* Idle sleep is about to kick in. This message will not be sent for forced sleep.
             * Applications have a chance to prevent sleep by calling IOCancelPowerChange.
             * Most applications should not prevent idle sleep. Power Management waits up to
             * 30 seconds for you to either allow or deny idle sleep. If you don’t acknowledge
             * this power change by calling either IOAllowPowerChange or IOCancelPowerChange,
             * the system will wait 30 seconds then go to sleep.
             */

            qCInfo(OCC::lcPlatform) << "System power message: can system sleep?";

            // Uncomment to cancel idle sleep
            // IOCancelPowerChange(thiz->rootPowerDomain, reinterpret_cast<long>(messageArgument));

            // Allow idle sleep
            IOAllowPowerChange(listener->rootPowerDomain, reinterpret_cast<long>(messageArgument));
            break;

        case kIOMessageSystemWillNotSleep:
            /* Announces that the system has retracted a previous attempt to sleep; it
             * follows `kIOMessageCanSystemSleep`.
             */
            qCInfo(OCC::lcPlatform) << "System power message: system will NOT sleep.";
            break;

        case kIOMessageSystemWillSleep:
            /* The system WILL go to sleep. If you do not call IOAllowPowerChange or
             * IOCancelPowerChange to acknowledge this message, sleep will be delayed by
             * 30 seconds.
             *
             * NOTE: If you call IOCancelPowerChange to deny sleep it returns kIOReturnSuccess,
             * however the system WILL still go to sleep.
             */

            qCInfo(OCC::lcPlatform) << "System power message: system WILL sleep.";

            IOAllowPowerChange(listener->rootPowerDomain, reinterpret_cast<long>(messageArgument));
            break;

        case kIOMessageSystemWillPowerOn:
            /* Announces that the system is beginning to power the device tree; most devices
             * are still unavailable at this point.
             */
            qCInfo(OCC::lcPlatform) << "System power message: system will power on.";
            break;

        case kIOMessageSystemHasPoweredOn:
            /* Announces that the system and its devices have woken up. */
            qCInfo(OCC::lcPlatform) << "System power message: system has powered on.";
            break;

        default:
            qCInfo(OCC::lcPlatform) << "System power message: other event: " << messageType;
            /* Not a system sleep and wake notification. */
            break;
        }
    }

private:
    IONotificationPortRef notifyPortRef = nullptr; // notification port allocated by IORegisterForSystemPower
    io_object_t notifierObj = IO_OBJECT_NULL; // notifier object, used to deregister later
    io_connect_t rootPowerDomain = IO_OBJECT_NULL; // a reference to the Root Power Domain IOService
};

} // anonynous namespace

namespace OCC {

class MacPlatformPrivate
{
public:
    ~MacPlatformPrivate() { [autoReleasePool release]; }
    NSAutoreleasePool *autoReleasePool = [[NSAutoreleasePool alloc] init];
    OwnAppDelegate *appDelegate;
    PowerNotificationsListener listener;
};

MacPlatform::MacPlatform()
    : d_ptr(new MacPlatformPrivate)
{
    Q_D(MacPlatform);

    NSApplicationLoad();
    d->appDelegate = [[OwnAppDelegate alloc] init];
    [[NSApplication sharedApplication] setDelegate:d->appDelegate];

    signal(SIGPIPE, SIG_IGN);
}

MacPlatform::~MacPlatform()
{
    Q_D(MacPlatform);
    [d->appDelegate release];
}

void MacPlatform::migrate()
{
    Platform::migrate();

    migrateLaunchOnStartup();
}

void MacPlatform::startServices()
{
    Q_D(MacPlatform);

    d->listener.registerForNotifications();
}

} // namespace OCC
