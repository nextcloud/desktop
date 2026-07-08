/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "ncaccountactionspopup.h"

#import "ncactionrow.h"
#import "ncappspopup.h"
#import "ncnotificationactionspopup.h"
#import "ncsectionheaderrow.h"
#import "ncspacerview.h"
#import "ncstaticinforow.h"
#import "nctraypopup.h"
#import "trayaccountpopupimageutils.h"
#import "trayaccountpopupmetrics.h"
#import "trayaccountpopupviewutils.h"

#include "systray.h"
#include "tray/trayaccountappsmodel.h"
#include "tray/usermodel.h"

#include <QCoreApplication>
#include <QVariantList>
#include <QVariantMap>

using namespace OCC::Mac::TrayPopupImageUtils;
using namespace OCC::Mac::TrayPopupViewUtils;

static QString statusText(OCC::UserStatus::OnlineStatus status)
{
    switch (status) {
    case OCC::UserStatus::OnlineStatus::Online:
        return QCoreApplication::translate("UserStatusSetStatusView", "Online");
    case OCC::UserStatus::OnlineStatus::Away:
        return QCoreApplication::translate("UserStatusSetStatusView", "Away");
    case OCC::UserStatus::OnlineStatus::Busy:
        return QCoreApplication::translate("UserStatusSetStatusView", "Busy");
    case OCC::UserStatus::OnlineStatus::DoNotDisturb:
        return QCoreApplication::translate("UserStatusSetStatusView", "Do not disturb");
    case OCC::UserStatus::OnlineStatus::Invisible:
        return QCoreApplication::translate("UserStatusSetStatusView", "Invisible");
    case OCC::UserStatus::OnlineStatus::Offline:
        return QCoreApplication::translate("OCC::SyncStatusSummary", "Offline");
    }
    return QCoreApplication::translate("UserStatusSetStatusView", "Online");
}

static QString statusMenuText(OCC::UserStatus::OnlineStatus status, const QString &message)
{
    const auto trimmedMessage = message.trimmed();
    return trimmedMessage.isEmpty() ? statusText(status) : trimmedMessage;
}

static NSView *accountActionsSeparator(const CGFloat verticalMargin)
{
    auto separator = [[[NSBox alloc] init] autorelease];
    separator.boxType = NSBoxSeparator;
    separator.translatesAutoresizingMaskIntoConstraints = NO;

    auto container = [[NSView alloc] init];
    container.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:separator];

    [NSLayoutConstraint activateConstraints:@[
        [container.widthAnchor constraintEqualToConstant:kAccountActionsPopupWidth],
        [container.heightAnchor constraintEqualToConstant:(2.0 * verticalMargin) + 1.0],
        [separator.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [separator.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [separator.centerYAnchor constraintEqualToAnchor:container.centerYAnchor],
        [separator.heightAnchor constraintEqualToConstant:1.0],
    ]];
    return [container autorelease];
}

static NSView *accountActionsSeparator()
{
    return accountActionsSeparator(kAccountHoverVerticalMargin);
}

static NSView *compactAccountActionsSeparator()
{
    return accountActionsSeparator(kCompactSeparatorVerticalMargin);
}

@implementation NCAccountActionsPopup {
    NSStackView *_stack;
    NCAppsPopup *_appsPopup;
    NCNotificationActionsPopup *_notificationActionsPopup;
    NCActionRow *_activeSubmenuRow; //!< The row whose sub-popup is currently shown, kept persistently highlighted.
    __unsafe_unretained NCTrayPopup *_owner;
    QMetaObject::Connection _recentActivitiesConnection; //!< Rebuilds the popup in place when the model reports new activity or notifications.
    int _userIndex;
}

- (instancetype)init
{
    self = [super initWithContentRect:NSMakeRect(0, 0, kAccountActionsPopupWidth, 1)
                            styleMask:NSWindowStyleMaskBorderless
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (!self) return nil;

    _userIndex = -1;
    _stack = configurePopupPanel(self);
    return self;
}

- (BOOL)canBecomeKeyWindow { return NO; }

- (void)dealloc
{
    [_appsPopup release];
    [_notificationActionsPopup release];
    if (_recentActivitiesConnection) {
        QObject::disconnect(_recentActivitiesConnection);
    }
    [super dealloc];
}

- (BOOL)isShowingActivitiesForUserIndex:(int)userIndex
{
    return [self isVisible] && _userIndex == userIndex;
}

- (void)orderOut:(id)sender
{
    [_appsPopup orderOut:nil];
    [_notificationActionsPopup orderOut:nil];
    [self clearActiveSubmenuRow];
    if (_recentActivitiesConnection) {
        QObject::disconnect(_recentActivitiesConnection);
        _recentActivitiesConnection = {};
    }
    _userIndex = -1;
    [super orderOut:sender];
}

- (void)clearActiveSubmenuRow
{
    [_activeSubmenuRow setPersistentHighlight:NO];
    _activeSubmenuRow = nil;
}

- (void)hideAppsPopup
{
    [_appsPopup orderOut:nil];
    [_notificationActionsPopup orderOut:nil];
    [self clearActiveSubmenuRow];
}

- (void)showAppsPopupFromRow:(NSView *)row forUserIndex:(int)userIndex
{
    if (!_appsPopup) {
        _appsPopup = [[NCAppsPopup alloc] init];
    }

    [_notificationActionsPopup orderOut:nil];
    [_appsPopup populateForUserIndex:userIndex owner:_owner];
    [self clearActiveSubmenuRow];
    if ([row isKindOfClass:[NCActionRow class]]) {
        _activeSubmenuRow = (NCActionRow *)row;
        [_activeSubmenuRow setPersistentHighlight:YES];
    }

    positionPopupFromRow(_appsPopup, row);
    [_appsPopup orderFront:nil];
}

- (void)showNotificationActionsPopupFromRow:(NSView *)row forUserIndex:(int)userIndex activityIndex:(int)activityIndex actions:(QVariantList)actions
{
    if (!_notificationActionsPopup) {
        _notificationActionsPopup = [[NCNotificationActionsPopup alloc] init];
    }

    [_appsPopup orderOut:nil];
    [_notificationActionsPopup populateForUserIndex:userIndex activityIndex:activityIndex actions:actions owner:_owner];
    [self clearActiveSubmenuRow];
    if ([row isKindOfClass:[NCActionRow class]]) {
        _activeSubmenuRow = (NCActionRow *)row;
        [_activeSubmenuRow setPersistentHighlight:YES];
    }

    positionPopupFromRow(_notificationActionsPopup, row);
    [_notificationActionsPopup orderFront:nil];
}

- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner
{
    [self populateForUserIndex:userIndex owner:owner refreshActivities:YES];
}

- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner refreshActivities:(BOOL)refreshActivities
{
    const auto preserveTopEdge = [self isVisible];
    const auto topEdge = NSMaxY(self.frame);

    _owner = owner;
    _userIndex = userIndex;
    [_appsPopup orderOut:nil];
    [self clearActiveSubmenuRow];

    clearStack(_stack);

    auto model = OCC::UserModel::instance();
    if (_recentActivitiesConnection) {
        QObject::disconnect(_recentActivitiesConnection);
        _recentActivitiesConnection = {};
    }
    if (!model || userIndex < 0 || userIndex >= model->rowCount()) {
        return;
    }

    __unsafe_unretained NCTrayPopup *weakOwner = owner;
    __unsafe_unretained NCAccountActionsPopup *weakSelf = self;
    _recentActivitiesConnection = QObject::connect(model, &QAbstractItemModel::dataChanged, model,
        [weakSelf, weakOwner, userIndex](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
            if (!weakSelf || ![weakSelf isShowingActivitiesForUserIndex:userIndex]) {
                return;
            }
            if (userIndex < topLeft.row() || userIndex > bottomRight.row()) {
                return;
            }
            if (!roles.isEmpty()
                && !roles.contains(OCC::UserModel::RecentActivitiesRole)
                && !roles.contains(OCC::UserModel::TrayNotificationsRole)) {
                return;
            }

            [weakSelf populateForUserIndex:userIndex owner:weakOwner refreshActivities:NO];
        });

    const auto userModelIndex = model->index(userIndex);
    const auto serverHasUserStatus = model->data(userModelIndex, OCC::UserModel::ServerHasUserStatusRole).toBool();
    const auto onlineStatusEnabled = model->data(userModelIndex, OCC::UserModel::IsConnectedRole).toBool()
        && serverHasUserStatus;

    auto appsModel = OCC::TrayAccountAppsModel::instance();
    appsModel->setUserId(userIndex);
    const auto appsEnabled = appsModel->rowCount() > 0;
    const auto assistantEnabled = model->data(userModelIndex, OCC::UserModel::AssistantEnabledRole).toBool();
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAccountActionsPopupWidth]);
    if (serverHasUserStatus) {
        const auto status = model->data(userModelIndex, OCC::UserModel::StatusRole).value<OCC::UserStatus::OnlineStatus>();
        const auto statusMessage = model->data(userModelIndex, OCC::UserModel::StatusMessageRole).toString();
        NSImage *statusIcon = nsImageFromQUrl(model->data(userModelIndex, OCC::UserModel::StatusIconRole).toUrl());
        addOwnedArrangedSubview(_stack, [[NCSectionHeaderRow alloc] initWithTitle:QCoreApplication::translate("TrayAccountPopup", "User status").toNSString()
                                                                            width:kAccountActionsPopupWidth]);
        addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:statusMenuText(status, statusMessage).toNSString()
                                                                      icon:statusIcon
                                                                     width:kAccountActionsPopupWidth
                                                                   enabled:onlineStatusEnabled
                                                                    action:^{
            [weakOwner openOnlineStatusForIndex:userIndex];
        } hoverAction:^(NSView *) {
            [weakSelf hideAppsPopup];
        }]);
        [_stack addArrangedSubview:accountActionsSeparator()];
    }
    addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:QCoreApplication::translate("TrayFoldersMenuButton", "Reveal in Finder").toNSString()
                                                                 width:kAccountActionsPopupWidth
                                                               enabled:YES
                                                                action:^{
        [weakOwner openLocalFolderForIndex:userIndex];
    } hoverAction:^(NSView *) {
        [weakSelf hideAppsPopup];
    }]);
    if (assistantEnabled) {
        addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:QCoreApplication::translate("MainWindow", "Open Assistant").toNSString()
                                                                      width:kAccountActionsPopupWidth
                                                                    enabled:YES
                                                                     action:^{
            [weakOwner openAssistantForIndex:userIndex];
        } hoverAction:^(NSView *) {
            [weakSelf hideAppsPopup];
        }]);
    }
    addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:QCoreApplication::translate("TrayWindowHeader", "Apps").toNSString()
                                                                  icon:nil
                                                                 width:kAccountActionsPopupWidth
                                                               enabled:appsEnabled
                                                                action:^{}
                                                           hoverAction:^(NSView *row) {
        [weakSelf showAppsPopupFromRow:row forUserIndex:userIndex];
    } showsSubmenuIndicator:YES]);

    [_stack addArrangedSubview:accountActionsSeparator()];

    const auto trayNotifications = model->data(userModelIndex, OCC::UserModel::TrayNotificationsRole).toList();
    if (!trayNotifications.isEmpty()) {
        addOwnedArrangedSubview(_stack, [[NCSectionHeaderRow alloc] initWithTitle:QCoreApplication::translate("TrayAccountPopup", "Notifications").toNSString()
                                                                            width:kAccountActionsPopupWidth]);
        for (const auto &trayNotification : trayNotifications) {
            const auto notificationData = trayNotification.toMap();
            const auto title = notificationData.value(QStringLiteral("title")).toString();
            if (title.isEmpty()) {
                continue;
            }
            const auto opensSettings = notificationData.value(QStringLiteral("opensSettings")).toBool();
            const auto notificationActions = notificationData.value(QStringLiteral("actions")).toList();
            const auto activityIndex = notificationData.value(QStringLiteral("activityIndex")).toInt();
            addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:title.toNSString()
                                                                          icon:systemSymbolImage(notificationData.value(QStringLiteral("systemIconName")).toString(), 14.0)
                                                                      dateTime:notificationData.value(QStringLiteral("dateTime")).toString().toNSString()
                                                                         width:kAccountActionsPopupWidth
                                                                       enabled:YES
                                                                        action:^{
                if (opensSettings) {
                    [weakOwner closeAllPopups];
                    OCC::Systray::instance()->openSettings();
                } else {
                    [weakOwner openActivitiesForIndex:userIndex];
                }
            } hoverAction:^(NSView *row) {
                if (!notificationActions.isEmpty()) {
                    [weakSelf showNotificationActionsPopupFromRow:row forUserIndex:userIndex activityIndex:activityIndex actions:notificationActions];
                } else {
                    [weakSelf hideAppsPopup];
                }
            } showsSubmenuIndicator:!notificationActions.isEmpty()]);
        }

        [_stack addArrangedSubview:compactAccountActionsSeparator()];
    }

    addOwnedArrangedSubview(_stack, [[NCSectionHeaderRow alloc] initWithTitle:QCoreApplication::translate("TrayAccountPopup", "Recent activity").toNSString()
                                                                        width:kAccountActionsPopupWidth]);
    const auto recentActivities = model->data(userModelIndex, OCC::UserModel::RecentActivitiesRole).toList();
    if (recentActivities.isEmpty()) {
        addOwnedArrangedSubview(_stack, [[NCStaticInfoRow alloc] initWithTitle:QCoreApplication::translate("TrayAccountPopup", "No recent activity").toNSString()
                                                                          icon:systemSymbolImage(QStringLiteral("clock"), 14.0)
                                                                         width:kAccountActionsPopupWidth]);
    }
    for (const auto &recentActivity : recentActivities) {
        const auto activityData = recentActivity.toMap();
        const auto title = activityData.value(QStringLiteral("title")).toString();
        if (title.isEmpty()) {
            continue;
        }
        addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:title.toNSString()
                                                                      icon:systemSymbolImage(activityData.value(QStringLiteral("systemIconName")).toString(), 14.0)
                                                                  subtitle:activityData.value(QStringLiteral("subtitle")).toString().toNSString()
                                                                  dateTime:activityData.value(QStringLiteral("dateTime")).toString().toNSString()
                                                                     width:kAccountActionsPopupWidth
                                                                   enabled:YES
                                                                    action:^{
            [weakOwner openActivitiesForIndex:userIndex];
        } hoverAction:^(NSView *) {
            [weakSelf hideAppsPopup];
        }]);
    }
    addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:QCoreApplication::translate("TrayAccountPopup", "More activity\342\200\246").toNSString()
                                                                 width:kAccountActionsPopupWidth
                                                               enabled:YES
                                                                action:^{
        [weakOwner openActivitiesForIndex:userIndex];
    } hoverAction:^(NSView *) {
        [weakSelf hideAppsPopup];
    }]);

    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAccountActionsPopupWidth]);

    [self.contentView layoutSubtreeIfNeeded];
    NSRect frame = self.frame;
    frame.size.width = kAccountActionsPopupWidth;
    frame.size.height = _stack.fittingSize.height;
    if (preserveTopEdge) {
        frame.origin.y = topEdge - frame.size.height;
    }
    auto screen = self.screen;
    if (!screen) {
        screen = NSScreen.mainScreen ?: NSScreen.screens.firstObject;
    }
    if (screen) {
        frame.origin = clampedPopupOrigin(frame.origin, frame.size, screen.visibleFrame);
    }
    [self setFrame:frame display:NO];
    [self invalidateShadow];

    if (refreshActivities) {
        model->fetchActivityPreview(userIndex);
    }
}

@end
