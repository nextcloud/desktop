/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#import "nctraypopup.h"

#import "ncaccountactionspopup.h"
#import "ncactionrow.h"
#import "ncalertboxrow.h"
#import "ncspacerview.h"
#import "trayaccountpopupimageutils.h"
#import "trayaccountpopupmetrics.h"
#import "trayaccountpopupviewutils.h"

#include "systray.h"
#include "tray/usermodel.h"

#include <QCoreApplication>
#include <QVariantMap>

using namespace OCC::Mac::TrayPopupImageUtils;
using namespace OCC::Mac::TrayPopupViewUtils;

@implementation NCTrayPopup {
    NSStackView *_stack;
    NCAccountActionsPopup *_accountActionsPopup;
    NCAccountRow *_activeAccountRow;
}

- (instancetype)init
{
    self = [super initWithContentRect:NSMakeRect(0, 0, kPopupWidth, 120)
                            styleMask:NSWindowStyleMaskBorderless
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (!self) return nil;

    _stack = configurePopupPanel(self);
    return self;
}

- (BOOL)canBecomeKeyWindow { return YES; }

- (void)dealloc
{
    [_accountActionsPopup release];
    [super dealloc];
}

- (void)resignKeyWindow
{
    [super resignKeyWindow];
    [_accountActionsPopup orderOut:nil];
    [self clearActiveAccountRow];
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
}

- (void)closeAllPopups
{
    [_accountActionsPopup orderOut:nil];
    [self clearActiveAccountRow];
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
}

- (void)closeAccountActionsPopup
{
    [_accountActionsPopup orderOut:nil];
    [self clearActiveAccountRow];
}

- (void)clearActiveAccountRow
{
    [_activeAccountRow setPersistentHighlight:NO];
    _activeAccountRow = nil;
}

- (NCAccountRow *)makeRowForIndex:(int)index
                             name:(NSString *)name
                           server:(NSString *)server
                           avatar:(NSImage *)avatar
                  syncStatusImage:(NSImage *)syncStatusImage
{
    NSImageView *avatarView = [[[NSImageView alloc] init] autorelease];
    avatarView.image = avatar != nil ? avatar : [NSImage imageWithSystemSymbolName:@"person.circle.fill"
                                                            accessibilityDescription:nil];
    avatarView.wantsLayer = YES;
    avatarView.layer.cornerRadius = kAvatarSize / 2.0;
    avatarView.layer.masksToBounds = YES;
    avatarView.imageScaling = NSImageScaleProportionallyUpOrDown;
    avatarView.translatesAutoresizingMaskIntoConstraints = NO;

    NSTextField *nameLabel = [NSTextField labelWithString:name];
    nameLabel.font = [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold];
    nameLabel.textColor = NSColor.labelColor;
    nameLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    nameLabel.translatesAutoresizingMaskIntoConstraints = NO;

    NSTextField *serverLabel = [NSTextField labelWithString:server];
    serverLabel.font = [NSFont systemFontOfSize:11];
    serverLabel.textColor = NSColor.secondaryLabelColor;
    serverLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    serverLabel.translatesAutoresizingMaskIntoConstraints = NO;

    NCAccountRow *row = [[[NCAccountRow alloc] init] autorelease];
    row.userIndex = index;
    row.popupDelegate = self;

    NSImageView *statusView = [[[NSImageView alloc] init] autorelease];
    statusView.image = syncStatusImage;
    statusView.translatesAutoresizingMaskIntoConstraints = NO;

    NSImageView *chevron = [[[NSImageView alloc] init] autorelease];
    chevron.image = [[NSImage imageWithSystemSymbolName:@"chevron.right" accessibilityDescription:nil]
        imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:11 weight:NSFontWeightMedium]];
    chevron.contentTintColor = NSColor.tertiaryLabelColor;
    chevron.translatesAutoresizingMaskIntoConstraints = NO;

    [row addSubview:avatarView];
    [row addSubview:nameLabel];
    [row addSubview:serverLabel];
    [row addSubview:statusView];
    [row addSubview:chevron];

    [NSLayoutConstraint activateConstraints:@[
        [avatarView.leadingAnchor constraintEqualToAnchor:row.leadingAnchor constant:kHPad],
        [avatarView.centerYAnchor constraintEqualToAnchor:row.centerYAnchor],
        [avatarView.widthAnchor constraintEqualToConstant:kAvatarSize],
        [avatarView.heightAnchor constraintEqualToConstant:kAvatarSize],

        [nameLabel.leadingAnchor constraintEqualToAnchor:avatarView.trailingAnchor constant:10],
        [nameLabel.topAnchor constraintEqualToAnchor:row.topAnchor constant:9],
        [nameLabel.trailingAnchor constraintLessThanOrEqualToAnchor:statusView.leadingAnchor constant:-8],

        [serverLabel.leadingAnchor constraintEqualToAnchor:nameLabel.leadingAnchor],
        [serverLabel.topAnchor constraintEqualToAnchor:nameLabel.bottomAnchor constant:2],
        [serverLabel.trailingAnchor constraintLessThanOrEqualToAnchor:statusView.leadingAnchor constant:-8],

        [chevron.trailingAnchor constraintEqualToAnchor:row.trailingAnchor constant:-kHPad],
        [chevron.centerYAnchor constraintEqualToAnchor:row.centerYAnchor],
        [chevron.widthAnchor constraintEqualToConstant:8],
        [chevron.heightAnchor constraintEqualToConstant:13],

        [statusView.trailingAnchor constraintEqualToAnchor:chevron.leadingAnchor constant:-8],
        [statusView.centerYAnchor constraintEqualToAnchor:row.centerYAnchor],
        [statusView.widthAnchor constraintEqualToConstant:18],
        [statusView.heightAnchor constraintEqualToConstant:18],

        [row.heightAnchor constraintEqualToConstant:kRowHeight],
        [row.widthAnchor constraintEqualToConstant:kPopupWidth],
    ]];

    const auto labelAvailableWidth = kPopupWidth - kHPad - kAvatarSize - 10.0 - 8.0 - 18.0 - 8.0 - 8.0 - kHPad;
    if (labelLikelyClipsText(nameLabel, name, labelAvailableWidth) || labelLikelyClipsText(serverLabel, server, labelAvailableWidth)) {
        setSharedToolTip(menuRowToolTipText(name, server, nil), @[row, nameLabel, serverLabel]);
    }

    return row;
}

- (void)populate
{
    [_accountActionsPopup orderOut:nil];
    [self clearActiveAccountRow];

    clearStack(_stack);

    OCC::UserModel *model = OCC::UserModel::instance();
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kTopPadding]);
    for (int i = 0; i < model->rowCount(); ++i) {
        const QModelIndex idx = model->index(i);
        NSString *name   = model->data(idx, OCC::UserModel::NameRole).toString().toNSString();
        NSString *server = model->data(idx, OCC::UserModel::ServerRole).toString().toNSString();
        NSImage *avatar = nsImageFromQImage(model->avatarForRow(i));
        NSImage *syncStatus = nsImageFromQImage(model->syncStatusIconForRow(i));
        const auto accountAlert = model->data(idx, OCC::UserModel::AccountAlertRole).toMap();
        [_stack addArrangedSubview:[self makeRowForIndex:i
                                                    name:name
                                                  server:server
                                                  avatar:avatar
                                         syncStatusImage:syncStatus]];
        const auto accountAlertTitle = accountAlert.value(QStringLiteral("title")).toString();
        if (!accountAlertTitle.isEmpty()) {
            addOwnedArrangedSubview(_stack, [[NCAlertBoxRow alloc] initWithTitle:accountAlertTitle.toNSString()
                                                                          action:^{
                [self openActivitiesForIndex:i];
            } hoverAction:^(NSView *) {
                [self closeAccountActionsPopup];
            }]);
        }
    }

    if (model->rowCount() > 0) {
        NSBox *sep = [[NSBox alloc] init];
        sep.boxType = NSBoxSeparator;
        sep.translatesAutoresizingMaskIntoConstraints = NO;
        [_stack addArrangedSubview:sep];
        [sep.widthAnchor constraintEqualToConstant:kPopupWidth].active = YES;
        [sep release];
    }
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding]);

    __unsafe_unretained NCTrayPopup *weakSelf = self;
    if (OCC::Systray::instance()->enableAddAccount()) {
        addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:OCC::Systray::tr("Add account").toNSString()
                                                                      width:kPopupWidth
                                                                    enabled:YES
                                                                     action:^{
            [weakSelf orderOut:nil];
            OCC::Systray::instance()->setIsOpen(false);
            OCC::Systray::instance()->openAccountWizard();
        } hoverAction:^(NSView *) {
            [weakSelf closeAccountActionsPopup];
        }]);
    }
    addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:OCC::Systray::tr("Settings").toNSString()
                                                                 width:kPopupWidth
                                                               enabled:YES
                                                                action:^{
        [weakSelf orderOut:nil];
        OCC::Systray::instance()->setIsOpen(false);
        OCC::Systray::instance()->openSettings();
    } hoverAction:^(NSView *) {
        [weakSelf closeAccountActionsPopup];
    }]);
    addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:OCC::Systray::tr("Quit").toNSString()
                                                                 width:kPopupWidth
                                                               enabled:YES
                                                                action:^{
        OCC::Systray::instance()->shutdown();
    } hoverAction:^(NSView *) {
        [weakSelf closeAccountActionsPopup];
    }]);
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding]);

    [self.contentView layoutSubtreeIfNeeded];
    NSRect frame = self.frame;
    frame.size.height = _stack.fittingSize.height;
    [self setFrame:frame display:NO];
    [self invalidateShadow];
}

- (void)onAccountRowClicked:(int)index
{
    [self openActivitiesForIndex:index];
}

- (void)onAccountRowHovered:(NCAccountRow *)row
{
    if (!_accountActionsPopup) {
        _accountActionsPopup = [[NCAccountActionsPopup alloc] init];
    }

    if (_activeAccountRow != row) {
        [self clearActiveAccountRow];
        _activeAccountRow = row;
        [_activeAccountRow setPersistentHighlight:YES];
    }

    [_accountActionsPopup populateForUserIndex:row.userIndex owner:self];

    positionPopupFromRow(_accountActionsPopup, row);
    [_accountActionsPopup orderFront:nil];
}

- (void)openActivitiesForIndex:(int)index
{
    [_accountActionsPopup orderOut:nil];
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
    OCC::Systray::instance()->showActivitiesWindow(index);
}

- (void)openLocalFolderForIndex:(int)index
{
    [_accountActionsPopup orderOut:nil];
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);

    auto userModel = OCC::UserModel::instance();
    userModel->setCurrentUserId(index);
    auto user = userModel->currentUser();
    if (!user) {
        return;
    }

    if (user->hasLocalFolder()) {
        userModel->openCurrentAccountLocalFolder();
    }
#ifdef BUILD_FILE_PROVIDER_MODULE
    else if (user->hasFileProvider()) {
        userModel->openCurrentAccountFileProviderDomain();
    }
#endif
}

- (void)openAssistantForIndex:(int)index
{
    [_accountActionsPopup orderOut:nil];
    [self orderOut:nil];
    OCC::Systray::instance()->showAssistantWindow(index);
}

- (void)openSearchForIndex:(int)index
{
    [_accountActionsPopup orderOut:nil];
    [self orderOut:nil];
    OCC::Systray::instance()->showSearchWindow(index);
}

- (void)openOnlineStatusForIndex:(int)index
{
    [_accountActionsPopup orderOut:nil];
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
    OCC::Systray::instance()->showUserStatusWindow(index);
}

@end
