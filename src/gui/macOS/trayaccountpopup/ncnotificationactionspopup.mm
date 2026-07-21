/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncnotificationactionspopup.h"

#import "ncactionrow.h"
#import "ncspacerview.h"
#import "nctraypopup.h"
#import "trayaccountpopupmetrics.h"
#import "trayaccountpopupviewutils.h"

#include "notifications/notificationmanager.h"
#include "tray/usermodel.h"

#include <QVariantMap>

using namespace OCC::Mac::TrayPopupViewUtils;

@implementation NCNotificationActionsPopup {
    NSStackView *_stack;
}

- (instancetype)init
{
    self = [super initWithContentRect:NSMakeRect(0, 0, kNotificationActionsPopupWidth, 1)
                            styleMask:NSWindowStyleMaskBorderless
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (!self) return nil;

    _stack = configurePopupPanel(self);
    return self;
}

- (BOOL)canBecomeKeyWindow { return NO; }

- (void)populateForUserIndex:(int)userIndex activityIndex:(int)activityIndex actions:(QVariantList)actions owner:(NCTrayPopup *)owner
{
    clearStack(_stack);

    __unsafe_unretained NCTrayPopup *weakOwner = owner;
    __unsafe_unretained NCNotificationActionsPopup *weakSelf = self;
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kNotificationActionsPopupWidth]);
    for (const auto &actionVariant : actions) {
        const auto actionData = actionVariant.toMap();
        const auto title = actionData.value(QStringLiteral("label")).toString();
        if (title.isEmpty()) {
            continue;
        }

        const auto actionType = actionData.value(QStringLiteral("actionType")).toString();
        const auto actionIndex = actionData.value(QStringLiteral("actionIndex")).toInt();
        const auto dismisses = actionType == QStringLiteral("dismiss");
        const auto opensActivities = actionType == QStringLiteral("openActivities");
        if (!dismisses && !opensActivities && actionIndex < 0) {
            continue;
        }

        addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:title.toNSString()
                                                                     width:kNotificationActionsPopupWidth
                                                                   enabled:YES
                                                                    action:^{
            const auto user = OCC::UserModel::instance()->user(userIndex);
            const auto notificationManager = user ? user->notificationManager() : nullptr;
            if (!notificationManager) {
                return;
            }
            if (dismisses) {
                notificationManager->dismissNotification(activityIndex);
                [weakSelf orderOut:nil];
                return;
            }
            if (opensActivities) {
                [weakOwner openActivitiesForIndex:userIndex];
                return;
            }

            [weakOwner closeAllPopups];
            notificationManager->triggerNotificationAction(activityIndex, actionIndex);
        }]);
    }
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kNotificationActionsPopupWidth]);

    [self.contentView layoutSubtreeIfNeeded];
    NSRect frame = self.frame;
    frame.size.width = kNotificationActionsPopupWidth;
    frame.size.height = _stack.fittingSize.height;
    [self setFrame:frame display:NO];
    [self invalidateShadow];
}

@end
