/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "systray.h"
#include "tray/trayaccountappsmodel.h"
#include "tray/usermodel.h"

#include <QCoreApplication>
#include <QImage>

#import <Cocoa/Cocoa.h>

// Keep behavior and layout aligned with src/gui/tray/TrayAccountPopup.qml.

static const CGFloat kPopupWidth   = 300.0;
static const CGFloat kRowHeight    = 48.0;
static const CGFloat kAvatarSize   = 34.0;
static const CGFloat kTopPadding = 4.0;
static const CGFloat kActionHeight = 26.0;
static const CGFloat kActionVerticalPadding = 8.0;
static const CGFloat kCornerRadius = 14.0;
static const CGFloat kHPad         = 14.0;
static const CGFloat kScreenEdgePadding = 8.0;
static const CGFloat kStatusItemLeadingOffset = 3.0;
static const CGFloat kStatusItemVerticalOffset = 2.0;
static const CGFloat kHoverMargin = 5.0;
static const CGFloat kHoverRadius = 5.0;
static const CGFloat kAccountHoverVerticalMargin = 4.0;
static const CGFloat kAccountActionsPopupWidth = 190.0;
static const CGFloat kAppsPopupWidth = 220.0;

typedef void (^NCActionHoverBlock)(NSView *row);

static NSColor *hoverColor()
{
    NSAppearanceName appearanceName = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua,
        NSAppearanceNameDarkAqua,
    ]];
    const BOOL isDarkMode = [appearanceName isEqualToString:NSAppearanceNameDarkAqua];
    return [(isDarkMode ? NSColor.whiteColor : NSColor.blackColor) colorWithAlphaComponent:0.08];
}

static NSImage *nsImageFromQImage(const QImage &qimg)
{
    if (qimg.isNull()) return nil;
    const QImage rgba = qimg.convertToFormat(QImage::Format_RGBA8888);
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nullptr
                      pixelsWide:rgba.width()
                      pixelsHigh:rgba.height()
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSCalibratedRGBColorSpace
                     bytesPerRow:rgba.bytesPerLine()
                    bitsPerPixel:32];
    memcpy(rep.bitmapData, rgba.constBits(), (size_t)rgba.bytesPerLine() * rgba.height());
    NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(rgba.width(), rgba.height())];
    [img addRepresentation:rep];
    return img;
}

static QImage qImageFromQUrl(const QUrl &url)
{
    if (url.isEmpty()) return {};

    auto imagePath = QString{};
    if (url.isLocalFile()) {
        imagePath = url.toLocalFile();
    } else if (url.scheme() == QStringLiteral("qrc")) {
        imagePath = QStringLiteral(":%1").arg(url.path());
    } else {
        imagePath = url.toString();
    }
    return QImage(imagePath);
}

static NSImage *nsImageFromQUrl(const QUrl &url)
{
    return nsImageFromQImage(qImageFromQUrl(url));
}

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

static QString trayFoldersMenuButtonText(const char *sourceText)
{
    return QCoreApplication::translate("TrayFoldersMenuButton", sourceText);
}

static QString mainWindowText(const char *sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}

static QString fileDetailsPageText(const char *sourceText)
{
    return QCoreApplication::translate("FileDetailsPage", sourceText);
}

static QString trayWindowHeaderText(const char *sourceText)
{
    return QCoreApplication::translate("TrayWindowHeader", sourceText);
}

static QString statusMenuText(OCC::UserStatus::OnlineStatus status, const QString &message)
{
    const auto trimmedMessage = message.trimmed();
    return trimmedMessage.isEmpty() ? statusText(status) : trimmedMessage;
}

@interface NCHoverView : NSView
@end

@implementation NCHoverView

- (instancetype)init
{
    self = [super init];
    if (!self) return nil;
    self.wantsLayer = YES;
    self.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
    self.translatesAutoresizingMaskIntoConstraints = NO;
    return self;
}

- (void)mouseEntered:(NSEvent *)event
{
    self.layer.backgroundColor = [NSColor.labelColor colorWithAlphaComponent:0.08].CGColor;
}

- (void)mouseExited:(NSEvent *)event
{
    self.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    for (NSTrackingArea *ta in self.trackingAreas.copy) [self removeTrackingArea:ta];
    [self addTrackingArea:[[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
               owner:self
            userInfo:nil]];
}

@end

@class NCAccountRow;

@protocol NCAccountRowDelegate
- (void)onAccountRowClicked:(int)index;
- (void)onAccountRowHovered:(NCAccountRow *)row;
@end

@interface NCAccountRow : NCHoverView
@property (nonatomic, assign) int userIndex;
@property (nonatomic, assign) id<NCAccountRowDelegate> popupDelegate;
- (void)setPersistentHighlight:(BOOL)persistentHighlight;
@end

@implementation NCAccountRow {
    NSView *_hoverView;
    BOOL _mouseInside;
    BOOL _persistentHighlight;
}

- (instancetype)init
{
    self = [super init];
    if (!self) return nil;
    self.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);

    _hoverView = [[NSView alloc] init];
    _hoverView.wantsLayer = YES;
    _hoverView.layer.backgroundColor = hoverColor().CGColor;
    _hoverView.layer.cornerRadius = kHoverRadius;
    _hoverView.hidden = YES;
    [self addSubview:_hoverView];
    return self;
}

- (void)updateHoverHighlight
{
    _hoverView.hidden = !(_mouseInside || _persistentHighlight);
}

- (void)setPersistentHighlight:(BOOL)persistentHighlight
{
    _persistentHighlight = persistentHighlight;
    [self updateHoverHighlight];
}

- (void)layout
{
    [super layout];
    _hoverView.frame = NSInsetRect(self.bounds, kHoverMargin, kAccountHoverVerticalMargin);
}

- (void)mouseEntered:(NSEvent *)event
{
    _mouseInside = YES;
    [self updateHoverHighlight];
    [self.popupDelegate onAccountRowHovered:self];
}

- (void)mouseExited:(NSEvent *)event
{
    _mouseInside = NO;
    [self updateHoverHighlight];
}

- (void)mouseUp:(NSEvent *)event
{
    [self.popupDelegate onAccountRowClicked:self.userIndex];
}

@end

@interface NCActionRow : NCHoverView
- (instancetype)initWithTitle:(NSString *)title action:(dispatch_block_t)action;
- (instancetype)initWithTitle:(NSString *)title
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action;
- (instancetype)initWithTitle:(NSString *)title
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction;
- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action;
- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction;
- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
        showsSubmenuIndicator:(BOOL)showsSubmenuIndicator;
- (void)setPersistentHighlight:(BOOL)persistentHighlight;
@end

@implementation NCActionRow {
    dispatch_block_t _action;
    NCActionHoverBlock _hoverAction;
    NSView *_hoverView;
    NSTextField *_label;
    BOOL _actionEnabled;
    BOOL _mouseInside;
    BOOL _persistentHighlight;
}

- (instancetype)initWithTitle:(NSString *)title action:(dispatch_block_t)action
{
    return [self initWithTitle:title width:kPopupWidth enabled:YES action:action];
}

- (instancetype)initWithTitle:(NSString *)title
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
{
    return [self initWithTitle:title icon:nil width:width enabled:enabled action:action hoverAction:nil];
}

- (instancetype)initWithTitle:(NSString *)title
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
{
    return [self initWithTitle:title icon:nil width:width enabled:enabled action:action hoverAction:hoverAction showsSubmenuIndicator:NO];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
{
    return [self initWithTitle:title icon:icon width:width enabled:enabled action:action hoverAction:nil];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
{
    return [self initWithTitle:title icon:icon width:width enabled:enabled action:action hoverAction:hoverAction showsSubmenuIndicator:NO];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
        showsSubmenuIndicator:(BOOL)showsSubmenuIndicator
{
    self = [super init];
    if (!self) return nil;
    _action = [action copy];
    _hoverAction = [hoverAction copy];
    _actionEnabled = enabled;
    self.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);

    _hoverView = [[NSView alloc] init];
    _hoverView.wantsLayer = YES;
    _hoverView.layer.backgroundColor = hoverColor().CGColor;
    _hoverView.layer.cornerRadius = kHoverRadius;
    _hoverView.hidden = YES;
    [self addSubview:_hoverView];

    _label = [NSTextField labelWithString:title];
    _label.font = [NSFont systemFontOfSize:13];
    _label.textColor = enabled ? NSColor.labelColor : NSColor.tertiaryLabelColor;
    _label.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_label];

    auto constraints = [NSMutableArray arrayWithArray:@[
        [_label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [self.heightAnchor constraintEqualToConstant:kActionHeight],
        [self.widthAnchor constraintEqualToConstant:width],
    ]];

    if (showsSubmenuIndicator) {
        NSImageView *chevron = [[NSImageView alloc] init];
        chevron.image = [[NSImage imageWithSystemSymbolName:@"chevron.right" accessibilityDescription:nil]
            imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:11 weight:NSFontWeightMedium]];
        chevron.contentTintColor = enabled ? NSColor.tertiaryLabelColor : NSColor.quaternaryLabelColor;
        chevron.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:chevron];
        [constraints addObjectsFromArray:@[
            [chevron.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-kHPad],
            [chevron.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [chevron.widthAnchor constraintEqualToConstant:8],
            [chevron.heightAnchor constraintEqualToConstant:13],
            [_label.trailingAnchor constraintLessThanOrEqualToAnchor:chevron.leadingAnchor constant:-8],
        ]];
    } else {
        [constraints addObject:[_label.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad]];
    }

    if (icon) {
        NSImageView *iconView = [[NSImageView alloc] init];
        iconView.image = icon;
        iconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        iconView.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:iconView];
        [constraints addObjectsFromArray:@[
            [iconView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad],
            [iconView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [iconView.widthAnchor constraintEqualToConstant:18.0],
            [iconView.heightAnchor constraintEqualToConstant:18.0],
            [_label.leadingAnchor constraintEqualToAnchor:iconView.trailingAnchor constant:8.0],
        ]];
    } else {
        [constraints addObject:[_label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad]];
    }

    [NSLayoutConstraint activateConstraints:constraints];
    return self;
}

- (void)updateHoverHighlight
{
    _hoverView.hidden = !_actionEnabled || !(_mouseInside || _persistentHighlight);
}

- (void)setPersistentHighlight:(BOOL)persistentHighlight
{
    _persistentHighlight = persistentHighlight;
    [self updateHoverHighlight];
}

- (void)layout
{
    [super layout];
    _hoverView.frame = NSInsetRect(self.bounds, kHoverMargin, 0.0);
}

- (void)mouseEntered:(NSEvent *)event
{
    _mouseInside = YES;
    [self updateHoverHighlight];
    if (_actionEnabled && _hoverAction) _hoverAction(self);
}

- (void)mouseExited:(NSEvent *)event
{
    _mouseInside = NO;
    [self updateHoverHighlight];
}

- (void)mouseUp:(NSEvent *)event
{
    if (_actionEnabled && _action) _action();
}

@end

@interface NCSpacerView : NSView
- (instancetype)initWithHeight:(CGFloat)height;
- (instancetype)initWithHeight:(CGFloat)height width:(CGFloat)width;
@end

@implementation NCSpacerView

- (instancetype)initWithHeight:(CGFloat)height
{
    return [self initWithHeight:height width:kPopupWidth];
}

- (instancetype)initWithHeight:(CGFloat)height width:(CGFloat)width
{
    self = [super init];
    if (!self) return nil;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [self.heightAnchor constraintEqualToConstant:height],
        [self.widthAnchor constraintEqualToConstant:width],
    ]];
    return self;
}

@end

@class NCTrayPopup;

@interface NCTrayPopup : NSPanel <NCAccountRowDelegate>
- (void)populate;
- (void)closeAllPopups;
- (void)closeAccountActionsPopup;
- (void)clearActiveAccountRow;
- (void)openActivitiesForIndex:(int)index;
- (void)openLocalFolderForIndex:(int)index;
- (void)openOnlineStatusForIndex:(int)index;
@end

@interface NCAppsPopup : NSPanel
- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner;
@end

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

    self.level = NSPopUpMenuWindowLevel;
    self.hasShadow = YES;
    self.releasedWhenClosed = NO;
    self.backgroundColor = NSColor.clearColor;
    self.opaque = NO;

    NSView *container = [[NSView alloc] init];
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    self.contentView = container;

    NSVisualEffectView *vev = [[NSVisualEffectView alloc] init];
    vev.material = NSVisualEffectMaterialHUDWindow;
    vev.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    vev.state = NSVisualEffectStateActive;
    vev.wantsLayer = YES;
    vev.layer.cornerRadius = kCornerRadius;
    vev.layer.masksToBounds = YES;
    vev.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:vev];
    [NSLayoutConstraint activateConstraints:@[
        [vev.topAnchor constraintEqualToAnchor:container.topAnchor],
        [vev.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [vev.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [vev.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    ]];

    _stack = [NSStackView stackViewWithViews:@[]];
    _stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    _stack.spacing = 0;
    _stack.translatesAutoresizingMaskIntoConstraints = NO;
    [vev addSubview:_stack];
    [NSLayoutConstraint activateConstraints:@[
        [_stack.topAnchor constraintEqualToAnchor:vev.topAnchor],
        [_stack.leadingAnchor constraintEqualToAnchor:vev.leadingAnchor],
        [_stack.trailingAnchor constraintEqualToAnchor:vev.trailingAnchor],
        [_stack.bottomAnchor constraintEqualToAnchor:vev.bottomAnchor],
    ]];
    return self;
}

- (BOOL)canBecomeKeyWindow { return NO; }

- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner
{
    for (NSView *v in _stack.arrangedSubviews.copy) {
        [_stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }

    __unsafe_unretained NCTrayPopup *weakOwner = owner;
    auto appsModel = OCC::TrayAccountAppsModel::instance();
    appsModel->setUserId(userIndex);
    [_stack addArrangedSubview:[[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAppsPopupWidth]];
    for (auto row = 0; row < appsModel->rowCount(); ++row) {
        const auto appIndex = appsModel->index(row);
        const auto appUrl = appsModel->data(appIndex, OCC::TrayAccountAppsModel::UrlRole).toUrl();
        [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:appsModel->data(appIndex, OCC::TrayAccountAppsModel::NameRole).toString().toNSString()
                                                                 icon:nsImageFromQUrl(appsModel->data(appIndex, OCC::TrayAccountAppsModel::IconUrlRole).toUrl())
                                                                width:kAppsPopupWidth
                                                              enabled:YES
                                                               action:^{
            [weakOwner closeAllPopups];
            appsModel->openAppUrl(appUrl);
        }]];
    }
    [_stack addArrangedSubview:[[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAppsPopupWidth]];

    [self.contentView layoutSubtreeIfNeeded];
    NSRect frame = self.frame;
    frame.size.width = kAppsPopupWidth;
    frame.size.height = _stack.fittingSize.height;
    [self setFrame:frame display:NO];
    [self invalidateShadow];
}

@end

@interface NCAccountActionsPopup : NSPanel
- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner;
- (void)clearActiveSubmenuRow;
- (void)hideAppsPopup;
@end

@implementation NCAccountActionsPopup {
    NSStackView *_stack;
    NCAppsPopup *_appsPopup;
    NCActionRow *_activeSubmenuRow;
    __unsafe_unretained NCTrayPopup *_owner;
}

- (instancetype)init
{
    self = [super initWithContentRect:NSMakeRect(0, 0, kAccountActionsPopupWidth, 1)
                            styleMask:NSWindowStyleMaskBorderless
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (!self) return nil;

    self.level = NSPopUpMenuWindowLevel;
    self.hasShadow = YES;
    self.releasedWhenClosed = NO;
    self.backgroundColor = NSColor.clearColor;
    self.opaque = NO;

    NSView *container = [[NSView alloc] init];
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    self.contentView = container;

    NSVisualEffectView *vev = [[NSVisualEffectView alloc] init];
    vev.material = NSVisualEffectMaterialHUDWindow;
    vev.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    vev.state = NSVisualEffectStateActive;
    vev.wantsLayer = YES;
    vev.layer.cornerRadius = kCornerRadius;
    vev.layer.masksToBounds = YES;
    vev.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:vev];
    [NSLayoutConstraint activateConstraints:@[
        [vev.topAnchor constraintEqualToAnchor:container.topAnchor],
        [vev.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [vev.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [vev.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    ]];

    _stack = [NSStackView stackViewWithViews:@[]];
    _stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    _stack.spacing = 0;
    _stack.translatesAutoresizingMaskIntoConstraints = NO;
    [vev addSubview:_stack];
    [NSLayoutConstraint activateConstraints:@[
        [_stack.topAnchor constraintEqualToAnchor:vev.topAnchor],
        [_stack.leadingAnchor constraintEqualToAnchor:vev.leadingAnchor],
        [_stack.trailingAnchor constraintEqualToAnchor:vev.trailingAnchor],
        [_stack.bottomAnchor constraintEqualToAnchor:vev.bottomAnchor],
    ]];
    return self;
}

- (BOOL)canBecomeKeyWindow { return NO; }

- (void)orderOut:(id)sender
{
    [_appsPopup orderOut:nil];
    [self clearActiveSubmenuRow];
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
    [self clearActiveSubmenuRow];
}

- (void)showAppsPopupFromRow:(NSView *)row forUserIndex:(int)userIndex
{
    if (!_appsPopup) {
        _appsPopup = [[NCAppsPopup alloc] init];
    }

    [_appsPopup populateForUserIndex:userIndex owner:_owner];
    [self clearActiveSubmenuRow];
    if ([row isKindOfClass:[NCActionRow class]]) {
        _activeSubmenuRow = (NCActionRow *)row;
        [_activeSubmenuRow setPersistentHighlight:YES];
    }

    const auto rowTopLeftInWindow = [row convertPoint:NSMakePoint(NSMinX(row.bounds) + kHPad, NSMaxY(row.bounds)) toView:nil];
    const auto rowTopRightInWindow = [row convertPoint:NSMakePoint(NSMaxX(row.bounds) - kHPad, NSMaxY(row.bounds)) toView:nil];
    const auto rowTopLeftOnScreen = [row.window convertPointToScreen:rowTopLeftInWindow];
    auto rowTopRightOnScreen = [row.window convertPointToScreen:rowTopRightInWindow];

    const auto popupWidth = _appsPopup.frame.size.width;
    const auto popupHeight = _appsPopup.frame.size.height;
    auto popupOrigin = rowTopRightOnScreen;
    popupOrigin.y -= popupHeight;

    auto screen = row.window.screen;
    if (!screen) {
        screen = NSScreen.mainScreen ?: NSScreen.screens.firstObject;
    }
    const auto visibleFrame = screen.visibleFrame;
    const auto rightEdge = NSMaxX(visibleFrame) - kScreenEdgePadding;
    const auto leftEdge = NSMinX(visibleFrame) + kScreenEdgePadding;
    const auto topEdge = NSMaxY(visibleFrame) - kScreenEdgePadding;
    const auto bottomEdge = NSMinY(visibleFrame) + kScreenEdgePadding;

    if (popupOrigin.x + popupWidth > rightEdge && rowTopLeftOnScreen.x - popupWidth >= leftEdge) {
        popupOrigin.x = rowTopLeftOnScreen.x - popupWidth;
    }
    popupOrigin.x = popupOrigin.x < leftEdge ? leftEdge : (popupOrigin.x + popupWidth > rightEdge ? rightEdge - popupWidth : popupOrigin.x);
    popupOrigin.y = popupOrigin.y < bottomEdge ? bottomEdge : (popupOrigin.y + popupHeight > topEdge ? topEdge - popupHeight : popupOrigin.y);

    [_appsPopup setFrameOrigin:popupOrigin];
    [_appsPopup orderFront:nil];
}

- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner
{
    _owner = owner;
    [_appsPopup orderOut:nil];
    [self clearActiveSubmenuRow];

    for (NSView *v in _stack.arrangedSubviews.copy) {
        [_stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }

    auto model = OCC::UserModel::instance();
    const auto userModelIndex = model->index(userIndex);
    const auto onlineStatusEnabled = model->data(userModelIndex, OCC::UserModel::IsConnectedRole).toBool()
        && model->data(userModelIndex, OCC::UserModel::ServerHasUserStatusRole).toBool();
    const auto status = model->data(userModelIndex, OCC::UserModel::StatusRole).value<OCC::UserStatus::OnlineStatus>();
    const auto statusMessage = model->data(userModelIndex, OCC::UserModel::StatusMessageRole).toString();
    NSImage *statusIcon = nsImageFromQUrl(model->data(userModelIndex, OCC::UserModel::StatusIconRole).toUrl());

    __unsafe_unretained NCTrayPopup *weakOwner = owner;
    __unsafe_unretained NCAccountActionsPopup *weakSelf = self;
    [_stack addArrangedSubview:[[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAccountActionsPopupWidth]];
    [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:statusMenuText(status, statusMessage).toNSString()
                                                             icon:statusIcon
                                                            width:kAccountActionsPopupWidth
                                                          enabled:onlineStatusEnabled
                                                           action:^{
        [weakOwner openOnlineStatusForIndex:userIndex];
    } hoverAction:^(NSView *) {
        [weakSelf hideAppsPopup];
    }]];
    [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:trayFoldersMenuButtonText("Open local folder").toNSString()
                                                            width:kAccountActionsPopupWidth
                                                          enabled:YES
                                                           action:^{
        [weakOwner openLocalFolderForIndex:userIndex];
    } hoverAction:^(NSView *) {
        [weakSelf hideAppsPopup];
    }]];
    [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:mainWindowText("Ask Assistant\302\240\342\200\246").toNSString()
                                                            width:kAccountActionsPopupWidth
                                                          enabled:NO
                                                           action:^{}]];

    NSBox *separator = [[NSBox alloc] init];
    separator.boxType = NSBoxSeparator;
    separator.translatesAutoresizingMaskIntoConstraints = NO;
    [_stack addArrangedSubview:separator];
    [separator.widthAnchor constraintEqualToConstant:kAccountActionsPopupWidth].active = YES;

    [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:fileDetailsPageText("Activity").toNSString()
                                                            width:kAccountActionsPopupWidth
                                                          enabled:YES
                                                           action:^{
        [weakOwner openActivitiesForIndex:userIndex];
    } hoverAction:^(NSView *) {
        [weakSelf hideAppsPopup];
    }]];
    auto appsModel = OCC::TrayAccountAppsModel::instance();
    appsModel->setUserId(userIndex);
    const auto appsEnabled = appsModel->rowCount() > 0;
    [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:trayWindowHeaderText("More apps").toNSString()
                                                            icon:nil
                                                           width:kAccountActionsPopupWidth
                                                         enabled:appsEnabled
                                                          action:^{}
                                                     hoverAction:^(NSView *row) {
        [weakSelf showAppsPopupFromRow:row forUserIndex:userIndex];
    } showsSubmenuIndicator:YES]];
    [_stack addArrangedSubview:[[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAccountActionsPopupWidth]];

    [self.contentView layoutSubtreeIfNeeded];
    NSRect frame = self.frame;
    frame.size.width = kAccountActionsPopupWidth;
    frame.size.height = _stack.fittingSize.height;
    [self setFrame:frame display:NO];
    [self invalidateShadow];
}

@end

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

    self.level = NSPopUpMenuWindowLevel;
    self.hasShadow = YES;
    self.releasedWhenClosed = NO;
    self.backgroundColor = NSColor.clearColor;
    self.opaque = NO;

    NSView *container = [[NSView alloc] init];
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    self.contentView = container;

    NSVisualEffectView *vev = [[NSVisualEffectView alloc] init];
    vev.material = NSVisualEffectMaterialHUDWindow;
    vev.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    vev.state = NSVisualEffectStateActive;
    vev.wantsLayer = YES;
    vev.layer.cornerRadius = kCornerRadius;
    vev.layer.masksToBounds = YES;
    vev.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:vev];
    [NSLayoutConstraint activateConstraints:@[
        [vev.topAnchor constraintEqualToAnchor:container.topAnchor],
        [vev.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [vev.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [vev.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    ]];

    _stack = [NSStackView stackViewWithViews:@[]];
    _stack.orientation = NSUserInterfaceLayoutOrientationVertical;
    _stack.spacing = 0;
    _stack.translatesAutoresizingMaskIntoConstraints = NO;
    [vev addSubview:_stack];
    [NSLayoutConstraint activateConstraints:@[
        [_stack.topAnchor constraintEqualToAnchor:vev.topAnchor],
        [_stack.leadingAnchor constraintEqualToAnchor:vev.leadingAnchor],
        [_stack.trailingAnchor constraintEqualToAnchor:vev.trailingAnchor],
        [_stack.bottomAnchor constraintEqualToAnchor:vev.bottomAnchor],
    ]];
    return self;
}

- (BOOL)canBecomeKeyWindow { return YES; }

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
    NSImageView *avatarView = [[NSImageView alloc] init];
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

    NCAccountRow *row = [[NCAccountRow alloc] init];
    row.userIndex = index;
    row.popupDelegate = self;

    NSImageView *statusView = [[NSImageView alloc] init];
    statusView.image = syncStatusImage;
    statusView.translatesAutoresizingMaskIntoConstraints = NO;

    NSImageView *chevron = [[NSImageView alloc] init];
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

    return row;
}

- (void)populate
{
    [_accountActionsPopup orderOut:nil];
    [self clearActiveAccountRow];

    for (NSView *v in _stack.arrangedSubviews.copy) {
        [_stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }

    OCC::UserModel *model = OCC::UserModel::instance();
    [_stack addArrangedSubview:[[NCSpacerView alloc] initWithHeight:kTopPadding]];
    for (int i = 0; i < model->rowCount(); ++i) {
        const QModelIndex idx = model->index(i);
        NSString *name   = model->data(idx, OCC::UserModel::NameRole).toString().toNSString();
        NSString *server = model->data(idx, OCC::UserModel::ServerRole).toString().toNSString();
        NSImage *avatar = nsImageFromQImage(model->avatarForRow(i));
        NSImage *syncStatus = nsImageFromQImage(model->syncStatusIconForRow(i));
        [_stack addArrangedSubview:[self makeRowForIndex:i
                                                    name:name
                                                  server:server
                                                  avatar:avatar
                                         syncStatusImage:syncStatus]];
    }

    if (model->rowCount() > 0) {
        NSBox *sep = [[NSBox alloc] init];
        sep.boxType = NSBoxSeparator;
        sep.translatesAutoresizingMaskIntoConstraints = NO;
        [_stack addArrangedSubview:sep];
        [sep.widthAnchor constraintEqualToConstant:kPopupWidth].active = YES;
    }
    [_stack addArrangedSubview:[[NCSpacerView alloc] initWithHeight:kActionVerticalPadding]];

    __unsafe_unretained NCTrayPopup *weakSelf = self;
    if (OCC::Systray::instance()->enableAddAccount()) {
        [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:OCC::Systray::tr("Add account").toNSString()
                                                                width:kPopupWidth
                                                              enabled:YES
                                                               action:^{
            [weakSelf orderOut:nil];
            OCC::Systray::instance()->setIsOpen(false);
            OCC::Systray::instance()->openAccountWizard();
        } hoverAction:^(NSView *) {
            [weakSelf closeAccountActionsPopup];
        }]];
    }
    [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:OCC::Systray::tr("Settings").toNSString()
                                                            width:kPopupWidth
                                                          enabled:YES
                                                           action:^{
        [weakSelf orderOut:nil];
        OCC::Systray::instance()->setIsOpen(false);
        OCC::Systray::instance()->openSettings();
    } hoverAction:^(NSView *) {
        [weakSelf closeAccountActionsPopup];
    }]];
    [_stack addArrangedSubview:[[NCActionRow alloc] initWithTitle:OCC::Systray::tr("Quit").toNSString()
                                                            width:kPopupWidth
                                                          enabled:YES
                                                           action:^{
        OCC::Systray::instance()->shutdown();
    } hoverAction:^(NSView *) {
        [weakSelf closeAccountActionsPopup];
    }]];
    [_stack addArrangedSubview:[[NCSpacerView alloc] initWithHeight:kActionVerticalPadding]];

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

    const auto rowTopLeftInWindow = [row convertPoint:NSMakePoint(NSMinX(row.bounds) + kHPad, NSMaxY(row.bounds)) toView:nil];
    const auto rowTopRightInWindow = [row convertPoint:NSMakePoint(NSMaxX(row.bounds) - kHPad, NSMaxY(row.bounds)) toView:nil];
    const auto rowTopLeftOnScreen = [row.window convertPointToScreen:rowTopLeftInWindow];
    auto rowTopRightOnScreen = [row.window convertPointToScreen:rowTopRightInWindow];

    const auto popupWidth = _accountActionsPopup.frame.size.width;
    auto popupOrigin = rowTopRightOnScreen;
    popupOrigin.y -= _accountActionsPopup.frame.size.height;

    auto screen = row.window.screen;
    if (!screen) {
        screen = NSScreen.mainScreen ?: NSScreen.screens.firstObject;
    }
    const auto visibleFrame = screen.visibleFrame;
    const auto rightEdge = NSMaxX(visibleFrame) - kScreenEdgePadding;
    const auto leftEdge = NSMinX(visibleFrame) + kScreenEdgePadding;

    if (popupOrigin.x + popupWidth > rightEdge && rowTopLeftOnScreen.x - popupWidth >= leftEdge) {
        popupOrigin.x = rowTopLeftOnScreen.x - popupWidth;
    }
    popupOrigin.x = popupOrigin.x < leftEdge ? leftEdge : (popupOrigin.x + popupWidth > rightEdge ? rightEdge - popupWidth : popupOrigin.x);

    [_accountActionsPopup setFrameOrigin:popupOrigin];
    [_accountActionsPopup orderFront:nil];
}

- (void)openActivitiesForIndex:(int)index
{
    [_accountActionsPopup orderOut:nil];
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
    OCC::UserModel::instance()->setCurrentUserId(index);
    OCC::Systray::instance()->showQMLWindow();
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

- (void)openOnlineStatusForIndex:(int)index
{
    [_accountActionsPopup orderOut:nil];
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
    OCC::Systray::instance()->showUserStatusWindow(index);
}

@end

namespace OCC {

static NCTrayPopup *s_popup = nil;

void showMacOSTrayPopup(const QRect &iconRect)
{
    if (!s_popup) {
        s_popup = [[NCTrayPopup alloc] init];
    }

    [s_popup populate];

    NSPoint iconPoint = NSMakePoint(iconRect.x(), iconRect.y());
    NSScreen *screen = nil;
    for (NSScreen *candidate in NSScreen.screens) {
        if (NSPointInRect(iconPoint, candidate.frame)) {
            screen = candidate;
            break;
        }
    }
    if (!screen) {
        screen = NSScreen.screens.firstObject;
    }

    const CGFloat screenH = screen.frame.size.height;
    const CGFloat popupW  = s_popup.frame.size.width;
    const CGFloat popupH  = s_popup.frame.size.height;
    const NSRect visibleFrame = screen.visibleFrame;

    CGFloat x, y;
    if (iconRect.isValid()) {
        x = iconRect.x() - kStatusItemLeadingOffset;
        y = screenH - iconRect.bottom() - popupH - kStatusItemVerticalOffset;
    } else {
        x = NSMaxX(visibleFrame) - popupW - kScreenEdgePadding;
        y = NSMaxY(visibleFrame) - popupH;
    }

    const CGFloat xMin = NSMinX(visibleFrame) + kScreenEdgePadding;
    const CGFloat xMax = NSMaxX(visibleFrame) - popupW - kScreenEdgePadding;
    x = x < xMin ? xMin : (x > xMax ? xMax : x);

    [s_popup setFrameOrigin:NSMakePoint(x, y)];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
    [s_popup makeKeyAndOrderFront:nil];
}

void hideMacOSTrayPopup()
{
    [s_popup orderOut:nil];
}

} // namespace OCC
