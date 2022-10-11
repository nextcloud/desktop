/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "result.h"
#include "utility.h"

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QtMacExtras/QtMacExtras>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSUserDefaults.h>

namespace OCC {

void Utility::setupFavLink(const QString &folder)
{
    // Finder: Place under "Places"/"Favorites" on the left sidebar
    CFStringRef folderCFStr = CFStringCreateWithCString(nullptr, folder.toUtf8().data(), kCFStringEncodingUTF8);
    QScopeGuard freeFolder([folderCFStr]() { CFRelease(folderCFStr); });

    CFURLRef urlRef = CFURLCreateWithFileSystemPath(nullptr, folderCFStr, kCFURLPOSIXPathStyle, true);
    QScopeGuard freeUrl([urlRef]() { CFRelease(urlRef); });

    LSSharedFileListRef placesItems = LSSharedFileListCreate(nullptr, kLSSharedFileListFavoriteItems, nullptr);
    QScopeGuard freePlaces([placesItems]() { CFRelease(placesItems); });

    if (placesItems) {
        //Insert an item to the list.
        if (LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(placesItems,
                kLSSharedFileListItemLast, nullptr, nullptr, urlRef, nullptr, nullptr)) {
            CFRelease(item);
        }
    }
}

bool Utility::hasSystemLaunchOnStartup(const QString &appName)
{
    Q_UNUSED(appName)
    return false;
}

static Result<void, QString> writePlistToFile(NSString *plistFile, NSDictionary *plist)
{
    NSError *error = nil;

    // Check if the directory exists. On a fresh installation for example, the directory does not
    // exist, so writing the plist file below will fail.
    QDir plistDir = QFileInfo(QString::fromNSString(plistFile)).dir();
    if (!plistDir.exists()) {
        if (!QDir().mkpath(plistDir.path())) {
            return QString(QStringLiteral("Failed to create directory '%1'")).arg(plistDir.path());
        }

        // Permissions always seem to be 0700, so set that.
        // Note: the Permission enum documentation states that on unix the owner permissions are
        // returned, but: "This behavior might change in a future Qt version." So we play it safe,
        // and set both the user and the owner permissions to rwx.
        if (!QFile(plistDir.path()).setPermissions(QFileDevice::ReadOwner | QFileDevice::ReadUser | QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::ExeOwner | QFileDevice::ExeUser)) {
            qCInfo(lcUtility()) << "Failed to set directory permmissions for" << plistDir.path();
        }
    }

    // Now write the file.
    if (![plist writeToURL:[NSURL fileURLWithPath:plistFile isDirectory:NO] error:&error]) {
        return QString::fromNSString(error.localizedDescription);
    }

    return {};
}

static Result<NSDictionary *, QString> readPlistFromFile(NSString *plistFile)
{
    NSError *error = nil;
    if (NSDictionary *plist = [NSDictionary dictionaryWithContentsOfURL:[NSURL fileURLWithPath:plistFile isDirectory:NO] error:&error]) {
        return plist;
    } else {
        return QString::fromNSString(error.localizedDescription);
    }
}

bool Utility::hasLaunchOnStartup(const QString &appName)
{
    Q_UNUSED(appName)

    @autoreleasepool {
        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSString *appIdentifier = QCoreApplication::organizationDomain().toNSString();
        NSString *plistFile = [NSHomeDirectory() stringByAppendingFormat:@"/Library/LaunchAgents/%@.plist", appIdentifier];

        if ([fileManager fileExistsAtPath:plistFile]) {
            auto maybePlist = readPlistFromFile(plistFile);
            if (!maybePlist) {
                qCInfo(lcUtility()).nospace() << "Cannot read '" << QString::fromNSString(plistFile)
                                              << "', probably not a valid plist file";
                return false;
            }

            if (NSDictionary *plist = *maybePlist) {
                // yes, there is a valid plist file...
                if (id label = plist[@"Label"]) {
                    // ... with a label...
                    if ([appIdentifier isEqualToString:label]) {
                        // ... and yes, it's the correct app-id...
                        if (id program = plist[@"Program"]) {
                            // .. and there is a program mentioned ...
                            // (Note: case insensitive compare, because most fs setups on mac are case insensitive)
                            if ([QCoreApplication::applicationFilePath().toNSString() compare:program options:NSCaseInsensitiveSearch] == NSOrderedSame) {
                                // ... and it's our executable ..
                                if (NSNumber *value = plist[@"RunAtLoad"]) {
                                    // yes, there is even a RunAtLoad key, so use it!
                                    return [value boolValue];
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}

static Result<void, QString> writeNewPlistFile(NSString *plistFile, NSString *fullPath, bool enable)
{
    NSDictionary *plistTemplate = @{
        @"Label" : QCoreApplication::organizationDomain().toNSString(),
        @"KeepAlive" : @ {
            @"Crashed" : @NO, // To have launchd restart the client after a crash, change this to @YES
            @"SuccessfulExit" : @NO
        },
        @"Program" : fullPath,
        @"RunAtLoad" : enable ? @YES : @NO
    };

    return writePlistToFile(plistFile, plistTemplate);
}

static Result<void, QString> modifyPlist(NSString *plistFile, NSDictionary *plist, bool enable)
{
    if (NSNumber *value = plist[@"RunAtLoad"]) {
        // ok, there is a key
        if ([value boolValue] == enable) {
            // nothing to do
            return {};
        }
    }

    // now either the key was missing, or it had the wrong value, so set the key and write the plist back
    NSMutableDictionary *newPlist = [plist mutableCopy];
    newPlist[@"RunAtLoad"] = enable ? @YES : @NO;
    return writePlistToFile(plistFile, newPlist);
}

void Utility::setLaunchOnStartup(const QString &appName, const QString &guiName, bool enable)
{
    Q_UNUSED(appName)
    Q_UNUSED(guiName)

    @autoreleasepool {
        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSString *fullPath = QCoreApplication::applicationFilePath().toNSString();
        NSString *appIdentifier = QCoreApplication::organizationDomain().toNSString();
        NSString *plistFile = [NSHomeDirectory() stringByAppendingFormat:@"/Library/LaunchAgents/%@.plist", appIdentifier];

        // An error might occur in the code below, but we cannot report anything, so we just ignore them.

        if ([fileManager fileExistsAtPath:plistFile]) {
            if (enable) {
                auto maybePlist = readPlistFromFile(plistFile);
                if (!maybePlist) {
                    // broken plist, overwrite it
                    auto result = writeNewPlistFile(plistFile, fullPath, enable);
                    if (!result) {
                        qCWarning(lcUtility) << Q_FUNC_INFO << result.error();
                    }
                    return;
                }
                NSDictionary *plist = *maybePlist;

                id programValue = plist[@"Program"];
                if (programValue == nil) {
                    // broken plist, overwrite it
                    auto result = writeNewPlistFile(plistFile, fullPath, enable);
                    if (!result) {
                        qCWarning(lcUtility) << result.error();
                    }
                } else if (![fileManager fileExistsAtPath:programValue]) {
                    // Ok, a plist from some removed program, overwrite it
                    auto result = writeNewPlistFile(plistFile, fullPath, enable);
                    if (!result) {
                        qCWarning(lcUtility) << result.error();
                    }
                } else if ([fullPath compare:programValue options:NSCaseInsensitiveSearch] == NSOrderedSame) { // (Note: case insensitive compare, because most fs setups on mac are case insensitive)
                    // Wohoo, it's ours! Now carefully change only the RunAtLoad entry. If any value for
                    // e.g. KeepAlive was changed, we leave it as-is.
                    auto result = modifyPlist(plistFile, plist, enable);
                    if (!result) {
                        qCWarning(lcUtility) << result.error();
                    }
                } else if ([fullPath hasPrefix:@"/Applications/"]) {
                    // ok, we seem to be an officially installed application, overwrite the file
                    auto result = writeNewPlistFile(plistFile, fullPath, enable);
                    if (!result) {
                        qCWarning(lcUtility) << result.error();
                    }
                } else {
                    qCInfo(lcUtility) << "We're not an installed application, there is anoter executable "
                                         "mentioned in the plist file, and that executable seems to exist, "
                                         "so let's not touch the file.";
                }
            } else {
                // Disable launch-on-startup: remove the plist file
                NSError *error = nil;
                if (![fileManager removeItemAtPath:plistFile error:&error]) {
                    qCWarning(lcUtility) << "Could not remove plist file:" << QString::fromNSString(error.localizedDescription);
                }
            }
        } else {
            if (enable) {
                // plist doens't exist, write a new one.
                auto result = writeNewPlistFile(plistFile, fullPath, enable);
                if (!result) {
                    qCWarning(lcUtility) << result.error();
                }
            } else {
                // Do nothing: if the file doesn't exist, the client won't be auto-started.
            }
        }
    }
}

#ifndef TOKEN_AUTH_ONLY
bool Utility::hasDarkSystray()
{
    @autoreleasepool {
        if (auto style = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"]) {
            return [style isEqualToString:@"Dark"];
        }
    }

    return false;
}
#endif

} // namespace OCC
