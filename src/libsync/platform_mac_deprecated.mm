/*
 * Copyright (C) by Erik Verbruggen <erik@verbruggen.consulting>
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

#include "common/utility.h"

#include <QCoreApplication>
#include <QDir>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

// This file contains migration code, that uses deprecated API. In order to suppress these "known"
// warnings, we suppress them. If in a future version of macOS the API is actually removed, we can
// drop this whole file, because we cannot migrate.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace OCC {

namespace Deprecated {

    bool hasLaunchOnStartup()
    {
        // this is quite some duplicate code with setLaunchOnStartup, at some point we should fix this FIXME.
        bool returnValue = false;
        QString filePath = QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).absolutePath();
        CFStringRef folderCFStr = CFStringCreateWithCString(nullptr, filePath.toUtf8().data(), kCFStringEncodingUTF8);
        CFURLRef urlRef = CFURLCreateWithFileSystemPath(nullptr, folderCFStr, kCFURLPOSIXPathStyle, true);
        if (LSSharedFileListRef loginItems = LSSharedFileListCreate(nullptr, kLSSharedFileListSessionLoginItems, nullptr)) {
            // We need to iterate over the items and check which one is "ours".
            UInt32 seedValue;
            CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
            CFStringRef appUrlRefString = CFURLGetString(urlRef); // no need for release
            for (int i = 0, ei = CFArrayGetCount(itemsArray); i < ei; i++) {
                LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(itemsArray, i);
                CFURLRef itemUrlRef = nullptr;

                if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, nullptr) == noErr && itemUrlRef) {
                    CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                    if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                        returnValue = true;
                    }
                    CFRelease(itemUrlRef);
                }
            }
            CFRelease(itemsArray);
            CFRelease(loginItems);
        }
        CFRelease(urlRef);
        CFRelease(folderCFStr);
        return returnValue;
    }

    void setLaunchOnStartup(bool enable)
    {
        QString filePath = QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).absolutePath();
        CFStringRef folderCFStr = CFStringCreateWithCString(nullptr, filePath.toUtf8().data(), kCFStringEncodingUTF8);
        CFURLRef urlRef = CFURLCreateWithFileSystemPath(nullptr, folderCFStr, kCFURLPOSIXPathStyle, true);
        if (LSSharedFileListRef loginItems = LSSharedFileListCreate(nullptr, kLSSharedFileListSessionLoginItems, nullptr)) {
            if (enable) {
                // Insert an item to the list.
                LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(loginItems,
                    kLSSharedFileListItemLast, nullptr, nullptr,
                    urlRef, nullptr, nullptr);
                if (item) {
                    CFRelease(item);
                } else {
                    qCWarning(lcUtility) << "Failed to insert ourself into launch on startup list";
                }
            } else {
                // We need to iterate over the items and check which one is "ours".
                UInt32 seedValue;
                CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
                CFStringRef appUrlRefString = CFURLGetString(urlRef);
                for (int i = 0, ei = CFArrayGetCount(itemsArray); i < ei; i++) {
                    // The line below has idiomatic Objective-C, but modern C++ doesn't like these casts.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wcast-qual"
                    LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(itemsArray, i);
#pragma clang diagnostic pop

                    CFURLRef itemUrlRef = nullptr;

                    if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, nullptr) == noErr && itemUrlRef) {
                        CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                        if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                            LSSharedFileListItemRemove(loginItems, item); // remove it!
                        }
                        CFRelease(itemUrlRef);
                    }
                }
                CFRelease(itemsArray);
            }
            CFRelease(loginItems);
        } else {
            qCWarning(lcUtility) << "Failed to retrieve loginItems";
        }

        CFRelease(folderCFStr);
        CFRelease(urlRef);
    }

} // Deprecated namespace

/// Migrate from deprecated API of "Login Items" to launchd plist files (if needed).
void migrateLaunchOnStartup()
{
    Q_ASSERT(QCoreApplication::instance() != nullptr);

    bool hasDeprecatedLaunchOnStartup = Deprecated::hasLaunchOnStartup();

    // Three possibilities here for `hasDeprecatedLaunchOnStartup`:
    //  - true: (deprecated) launch on startup
    //  - false 1: no launch on startup, but not migrated yet
    //  - false 2: migrated, and the deprecated launch on startup has been removed
    //
    // So, now check if the LaunchAgents plist file is there.

    bool hasLaunchAgentsPlist = QFile::exists(QStringLiteral("%1/Library/LaunchAgents/%2.plist").arg(QDir::homePath(), QCoreApplication::organizationDomain()));

    qCInfo(lcUtility) << "migrateLaunchOnStartup: has launch agent plist:" << hasLaunchAgentsPlist
                      << "has deprecated launch on startup:" << hasDeprecatedLaunchOnStartup;

    // Simple case first:
    if (hasLaunchAgentsPlist && !hasDeprecatedLaunchOnStartup) {
        // Migration has already happened, so nothing to do.
        return;
    }

    // Weird case: yes there is a plist file, but hasDeprecatedLaunchOnStartup is true. In that
    // case we'll assume migration had a problem, or whatever. So just treat this as a not (fully)
    // migrated case.

    // Ok, so now we need to migrate.

    // First write the new plist file.
    Utility::setLaunchOnStartup({}, {}, hasDeprecatedLaunchOnStartup);

    // Then remove the deprecated entry.
    Deprecated::setLaunchOnStartup(false);

    // And we're done.
}

} // OCC namespace
