/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncappspopup.h"

#import "ncactionrow.h"
#import "ncspacerview.h"
#import "nctraypopup.h"
#import "trayaccountpopupimageutils.h"
#import "trayaccountpopupmetrics.h"
#import "trayaccountpopupviewutils.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "iconjob.h"
#include "tray/trayaccountappsmodel.h"

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QSize>
#include <QString>
#include <QUrl>

using namespace OCC::Mac::TrayPopupImageUtils;
using namespace OCC::Mac::TrayPopupViewUtils;

// Process-wide cache of remotely fetched app icons, keyed by remoteAppIconCacheKey().
static QHash<QString, QImage> s_remoteAppIconCache;

@implementation NCAppsPopup {
    NSStackView *_stack;
}

- (instancetype)init
{
    self = [super initWithContentRect:NSMakeRect(0, 0, kAppsPopupWidth, 1)
                            styleMask:NSWindowStyleMaskBorderless
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (!self) return nil;

    _stack = configurePopupPanel(self);
    return self;
}

- (BOOL)canBecomeKeyWindow { return NO; }

- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner
{
    clearStack(_stack);

    __unsafe_unretained NCTrayPopup *weakOwner = owner;
    auto appsModel = OCC::TrayAccountAppsModel::instance();
    appsModel->setUserId(userIndex);
    const auto accounts = OCC::AccountManager::instance()->accounts();
    const auto accountState = userIndex >= 0 && userIndex < accounts.size()
        ? accounts.at(userIndex)
        : OCC::AccountStatePtr{};
    auto fallbackIcon = [[NSImage imageWithSystemSymbolName:@"app" accessibilityDescription:nil]
        imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:14 weight:NSFontWeightRegular]];
    const auto appIconRequestedSize = pixelSizeForPointSize(kActionIconSize, backingScaleFactorForWindow(self));
    const auto appIconDevicePixelRatio = static_cast<qreal>(appIconRequestedSize.width()) / kActionIconSize;
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAppsPopupWidth]);
    for (auto row = 0; row < appsModel->rowCount(); ++row) {
        const auto appIndex = appsModel->index(row);
        const auto appName = appsModel->data(appIndex, static_cast<int>(OCC::TrayAccountAppsModel::Roles::NameRole)).toString();
        const auto appUrl = appsModel->data(appIndex, static_cast<int>(OCC::TrayAccountAppsModel::Roles::UrlRole)).toUrl();
        const auto appIconUrl = appsModel->data(appIndex, static_cast<int>(OCC::TrayAccountAppsModel::Roles::IconUrlRole)).toUrl();
        const auto appIconCacheKey = remoteAppIconCacheKey(accountState, appIconUrl, appIconRequestedSize);
        auto appIcon = nsImageFromQUrl(appIconUrl);
        if (!appIcon && !appIconCacheKey.isEmpty() && s_remoteAppIconCache.contains(appIconCacheKey)) {
            appIcon = nsImageFromQImage(s_remoteAppIconCache.value(appIconCacheKey));
        }
        auto actionRow = [[NCActionRow alloc] initWithTitle:appName.toNSString()
                                                       icon:appIcon != nil ? appIcon : fallbackIcon
                                                      width:kAppsPopupWidth
                                                    enabled:YES
                                                     action:^{
            [weakOwner closeAllPopups];
            appsModel->openAppUrl(appUrl);
        }];
        [actionRow setIconTintedToLabelColor:YES];
        [_stack addArrangedSubview:actionRow];

        if (!appIcon && !appIconCacheKey.isEmpty()) {
            auto retainedRow = [actionRow retain];
            auto iconJob = new OCC::IconJob(accountState->account(), appIconUrl);
            QObject::connect(iconJob, &OCC::IconJob::jobFinished, iconJob, [retainedRow, appIconCacheKey, appIconRequestedSize, appIconDevicePixelRatio](const QByteArray &iconData) {
                auto image = qImageFromImageData(iconData, appIconRequestedSize);
                if (!image.isNull()) {
                    image.setDevicePixelRatio(appIconDevicePixelRatio);
                    s_remoteAppIconCache.insert(appIconCacheKey, image);
                    [retainedRow setIcon:nsImageFromQImage(image)];
                }
                [retainedRow release];
            });
            QObject::connect(iconJob, &OCC::IconJob::error, iconJob, [retainedRow](auto) {
                [retainedRow release];
            });
        }
        [actionRow release];
    }
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAppsPopupWidth]);

    [self.contentView layoutSubtreeIfNeeded];
    NSRect frame = self.frame;
    frame.size.width = kAppsPopupWidth;
    frame.size.height = _stack.fittingSize.height;
    [self setFrame:frame display:NO];
    [self invalidateShadow];
}

@end
