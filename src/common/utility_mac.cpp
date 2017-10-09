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

#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>

namespace OCC {

static void setupFavLink_private(const QString &folder)
{
    // Finder: Place under "Places"/"Favorites" on the left sidebar
    CFStringRef folderCFStr = CFStringCreateWithCString(0, folder.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);

    LSSharedFileListRef placesItems = LSSharedFileListCreate(0, kLSSharedFileListFavoriteItems, 0);
    if (placesItems) {
        //Insert an item to the list.
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(placesItems,
            kLSSharedFileListItemLast, 0, 0,
            urlRef, 0, 0);
        if (item)
            CFRelease(item);
    }
    CFRelease(placesItems);
    CFRelease(folderCFStr);
    CFRelease(urlRef);
}

bool hasLaunchOnStartup_private(const QString &)
{
    // this is quite some duplicate code with setLaunchOnStartup, at some point we should fix this FIXME.
    bool returnValue = false;
    QString filePath = QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).absolutePath();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);
    if (loginItems) {
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef); // no need for release
        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                    returnValue = true;
                }
                CFRelease(itemUrlRef);
            }
        }
        CFRelease(itemsArray);
    }
    CFRelease(loginItems);
    CFRelease(folderCFStr);
    CFRelease(urlRef);
    return returnValue;
}

void setLaunchOnStartup_private(const QString &appName, const QString &guiName, bool enable)
{
    Q_UNUSED(appName)
    Q_UNUSED(guiName)
    QString filePath = QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).absolutePath();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);

    if (loginItems && enable) {
        //Insert an item to the list.
        LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(loginItems,
            kLSSharedFileListItemLast, 0, 0,
            urlRef, 0, 0);
        if (item)
            CFRelease(item);
        CFRelease(loginItems);
    } else if (loginItems && !enable) {
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef);
        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);
                if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                    LSSharedFileListItemRemove(loginItems, item); // remove it!
                }
                CFRelease(itemUrlRef);
            }
        }
        CFRelease(itemsArray);
        CFRelease(loginItems);
    };

    CFRelease(folderCFStr);
    CFRelease(urlRef);
}

static bool hasDarkSystray_private()
{
    bool returnValue = false;
    CFStringRef interfaceStyleKey = CFSTR("AppleInterfaceStyle");
    CFStringRef interfaceStyle = NULL;
    CFStringRef darkInterfaceStyle = CFSTR("Dark");
    interfaceStyle = (CFStringRef)CFPreferencesCopyAppValue(interfaceStyleKey,
        kCFPreferencesCurrentApplication);
    if (interfaceStyle != NULL) {
        returnValue = (kCFCompareEqualTo == CFStringCompare(interfaceStyle, darkInterfaceStyle, 0));
        CFRelease(interfaceStyle);
    }
    return returnValue;
}

} // namespace OCC
