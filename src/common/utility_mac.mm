/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "utility.h"

#include <QLoggingCategory>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#import <Foundation/NSUserDefaults.h>
#import <ServiceManagement/SMAppService.h>

namespace OCC {

QVector<Utility::ProcessInfosForOpenFile> Utility::queryProcessInfosKeepingFileOpen(const QString &filePath)
{
    Q_UNUSED(filePath)
    return {};
}

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

void Utility::removeFavLink(const QString &folder)
{
    Q_UNUSED(folder)
}

void Utility::migrateFavLink(const QString &folder)
{
    Q_UNUSED(folder)
}

void Utility::setupDesktopIni(const QString &folder, const QString localizedResourceName)
{
    Q_UNUSED(folder)
    Q_UNUSED(localizedResourceName)
}

QString Utility::syncFolderDisplayName(const QString &folder, const QString &displayName)
{
    Q_UNUSED(folder)
    Q_UNUSED(displayName)
    return {};
}

bool Utility::hasLaunchOnStartup(const QString &appName)
{
    Q_UNUSED(appName)

    @autoreleasepool {
        const auto status = [SMAppService mainAppService].status;
        qCDebug(lcUtility) << "Login item status:" << static_cast<NSInteger>(status);
        // Treat RequiresApproval as registered: the user's intent is set and the item
        // is pending approval in System Settings → General → Login Items.
        return status == SMAppServiceStatusEnabled
            || status == SMAppServiceStatusRequiresApproval;
    }
}

bool Utility::launchOnStartupRequiresApproval()
{
    @autoreleasepool {
        return [SMAppService mainAppService].status == SMAppServiceStatusRequiresApproval;
    }
}

void Utility::setLaunchOnStartup(const QString &appName, const QString &guiName, bool enable)
{
    Q_UNUSED(appName)
    Q_UNUSED(guiName)

    @autoreleasepool {
        SMAppService *service = [SMAppService mainAppService];
        NSError *error = nil;

        if (enable) {
            if (![service registerAndReturnError:&error]) {
                qCWarning(lcUtility) << "Failed to register login item:"
                                     << QString::fromNSString(error.localizedDescription);
                return;
            }
            qCInfo(lcUtility) << "Successfully registered login item";
        } else {
            if (![service unregisterAndReturnError:&error]) {
                qCWarning(lcUtility) << "Failed to unregister login item:"
                                     << QString::fromNSString(error.localizedDescription);
                return;
            }
            qCInfo(lcUtility) << "Successfully unregistered login item";
        }

        const auto resultStatus = service.status;
        const auto effectivelyEnabled = resultStatus == SMAppServiceStatusEnabled
            || resultStatus == SMAppServiceStatusRequiresApproval;
        if ((enable && !effectivelyEnabled) || (!enable && effectivelyEnabled)) {
            qCWarning(lcUtility) << "Login item status after update does not match intent."
                                 << "Expected enabled:" << enable
                                 << "Actual status:" << static_cast<NSInteger>(resultStatus);
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

QString Utility::getCurrentUserName()
{
    return {};
}

void Utility::registerUriHandlerForLocalEditing() { /* URI handler is registered via MacOSXBundleInfo.plist.in */ }

} // namespace OCC
