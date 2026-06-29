/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "systray.h"
#include "accountmanager.h"
#include "iconjob.h"
#include "tray/trayaccountappsmodel.h"
#include "tray/usermodel.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QMimeDatabase>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>

#import <Cocoa/Cocoa.h>

// Keep behavior and menu taxonomy aligned with src/gui/trayaccountpopup_qt.cpp.

static const CGFloat kPopupWidth   = 300.0;
static const CGFloat kRowHeight    = 48.0;
static const CGFloat kAvatarSize   = 34.0;
static const CGFloat kTopPadding = 4.0;
static const CGFloat kActionHeight = 26.0;
static const CGFloat kPreviewActionHeight = 52.0;
static const CGFloat kDetailedPreviewActionHeight = 58.0;
static const CGFloat kActionVerticalPadding = 8.0;
static const CGFloat kCornerRadius = 14.0;
static const CGFloat kHPad         = 14.0;
static const CGFloat kScreenEdgePadding = 8.0;
static const CGFloat kStatusItemLeadingOffset = 3.0;
static const CGFloat kStatusItemVerticalOffset = 2.0;
static const CGFloat kHoverMargin = 5.0;
static const CGFloat kHoverRadius = 5.0;
static const CGFloat kAccountHoverVerticalMargin = 4.0;
static const CGFloat kCompactSeparatorVerticalMargin = 2.0;
static const CGFloat kAccountActionsPopupWidth = 340.0;
static const CGFloat kAppsPopupWidth = 220.0;
static const CGFloat kNotificationActionsPopupWidth = 160.0;
static const CGFloat kSectionHeaderHeight = 24.0;
static const CGFloat kActivityPreviewIconSize = 16.0;

typedef void (^NCActionHoverBlock)(NSView *row);

static QHash<QString, QImage> s_remoteAppIconCache;

static CGFloat clampedPopupOriginCoordinate(const CGFloat origin, const CGFloat minEdge, const CGFloat maxEdge, const CGFloat size)
{
    const auto minOrigin = minEdge + kScreenEdgePadding;
    const auto maxOrigin = maxEdge - size - kScreenEdgePadding;
    if (maxOrigin < minOrigin) {
        return minOrigin;
    }
    return origin < minOrigin ? minOrigin : (origin > maxOrigin ? maxOrigin : origin);
}

static NSPoint clampedPopupOrigin(const NSPoint origin, const NSSize size, const NSRect visibleFrame)
{
    return NSMakePoint(clampedPopupOriginCoordinate(origin.x, NSMinX(visibleFrame), NSMaxX(visibleFrame), size.width),
                       clampedPopupOriginCoordinate(origin.y, NSMinY(visibleFrame), NSMaxY(visibleFrame), size.height));
}

static NSScreen *nsScreenForQtScreen(QScreen *qtScreen)
{
    if (!qtScreen) {
        return NSScreen.mainScreen ?: NSScreen.screens.firstObject;
    }

    const auto qtScreenName = qtScreen->name().toNSString();
    for (NSScreen *candidate in NSScreen.screens) {
        if ([candidate.localizedName isEqualToString:qtScreenName]) {
            return candidate;
        }
    }

    const auto qtScreens = QGuiApplication::screens();
    const auto screenIndex = qtScreens.indexOf(qtScreen);
    if (screenIndex >= 0 && screenIndex < static_cast<int>(NSScreen.screens.count)) {
        return [NSScreen.screens objectAtIndex:screenIndex];
    }

    return NSScreen.mainScreen ?: NSScreen.screens.firstObject;
}

static QString remoteAppIconCacheKey(const OCC::AccountStatePtr &accountState, const QUrl &url)
{
    if (!accountState || !accountState->account() || !url.isValid() || url.scheme().isEmpty()) {
        return {};
    }

    return QStringLiteral("%1:%2").arg(accountState->account()->id(), url.toString());
}

static void addOwnedArrangedSubview(NSStackView *stack, NSView *view)
{
    [stack addArrangedSubview:view];
    [view release];
}

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
    if (!rep || !rep.bitmapData) {
        [rep release];
        return nil;
    }
    memcpy(rep.bitmapData, rgba.constBits(), (size_t)rgba.bytesPerLine() * rgba.height());
    NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(rgba.width(), rgba.height())];
    [img addRepresentation:rep];
    [rep release];
    return [img autorelease];
}

static QImage qImageFromImageData(const QByteArray &imageData, const QSize &requestedSize)
{
    if (imageData.isEmpty()) return {};

    const auto mimetype = QMimeDatabase().mimeTypeForData(imageData);
    if (mimetype.isValid() && mimetype.inherits(QStringLiteral("image/svg+xml"))) {
        QSvgRenderer renderer;
        if (!renderer.load(imageData)) return {};

        auto image = QImage(requestedSize, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        const auto scaledSize = renderer.defaultSize().scaled(requestedSize, Qt::KeepAspectRatio);
        const auto targetRect = QRectF(QPointF((requestedSize.width() - scaledSize.width()) / 2.0,
                                               (requestedSize.height() - scaledSize.height()) / 2.0),
                                       scaledSize);
        renderer.render(&painter, targetRect);
        return image;
    }

    auto image = QImage::fromData(imageData);
    if (!image.isNull() && requestedSize.isValid()) {
        image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

static NSImage *nsImageFromImageData(const QByteArray &imageData, const QSize &requestedSize)
{
    return nsImageFromQImage(qImageFromImageData(imageData, requestedSize));
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

static NSImage *systemSymbolImage(const QString &symbolName, const CGFloat pointSize)
{
    auto image = [NSImage imageWithSystemSymbolName:symbolName.toNSString() accessibilityDescription:nil];
    if (!image) {
        image = [NSImage imageWithSystemSymbolName:@"doc" accessibilityDescription:nil];
    }
    return [image imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:pointSize weight:NSFontWeightRegular]];
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
    auto trackingAreas = [self.trackingAreas copy];
    for (NSTrackingArea *ta in trackingAreas) {
        [self removeTrackingArea:ta];
    }
    [trackingAreas release];

    auto trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
               owner:self
            userInfo:nil];
    [self addTrackingArea:trackingArea];
    [trackingArea release];
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

    _hoverView = [[[NSView alloc] init] autorelease];
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
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
        showsSubmenuIndicator:(BOOL)showsSubmenuIndicator;
- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     subtitle:(NSString *)subtitle
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
        showsSubmenuIndicator:(BOOL)showsSubmenuIndicator;
- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction;
- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     subtitle:(NSString *)subtitle
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction;
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
- (void)setIcon:(NSImage *)icon;
- (void)setIconTintedToLabelColor:(BOOL)tinted;
@end

@implementation NCActionRow {
    dispatch_block_t _action;
    NCActionHoverBlock _hoverAction;
    NSView *_hoverView;
    NSImageView *_iconView;
    NSTextField *_label;
    BOOL _actionEnabled;
    BOOL _iconTintedToLabelColor;
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
    return [self initWithTitle:title
                          icon:icon
                      subtitle:nil
                      dateTime:nil
                         width:width
                       enabled:enabled
                        action:action
                   hoverAction:hoverAction
         showsSubmenuIndicator:showsSubmenuIndicator];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
{
    return [self initWithTitle:title
                          icon:icon
                      subtitle:nil
                      dateTime:dateTime
                         width:width
                       enabled:enabled
                        action:action
                   hoverAction:hoverAction
         showsSubmenuIndicator:NO];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
        showsSubmenuIndicator:(BOOL)showsSubmenuIndicator
{
    return [self initWithTitle:title
                          icon:icon
                      subtitle:nil
                      dateTime:dateTime
                         width:width
                       enabled:enabled
                        action:action
                   hoverAction:hoverAction
         showsSubmenuIndicator:showsSubmenuIndicator];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     subtitle:(NSString *)subtitle
                     dateTime:(NSString *)dateTime
                        width:(CGFloat)width
                      enabled:(BOOL)enabled
                       action:(dispatch_block_t)action
                  hoverAction:(NCActionHoverBlock)hoverAction
{
    return [self initWithTitle:title
                          icon:icon
                      subtitle:subtitle
                      dateTime:dateTime
                         width:width
                       enabled:enabled
                        action:action
                   hoverAction:hoverAction
         showsSubmenuIndicator:NO];
}

- (instancetype)initWithTitle:(NSString *)title
                         icon:(NSImage *)icon
                     subtitle:(NSString *)subtitle
                     dateTime:(NSString *)dateTime
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

    _hoverView = [[[NSView alloc] init] autorelease];
    _hoverView.wantsLayer = YES;
    _hoverView.layer.backgroundColor = hoverColor().CGColor;
    _hoverView.layer.cornerRadius = kHoverRadius;
    _hoverView.hidden = YES;
    [self addSubview:_hoverView];

    _label = [NSTextField labelWithString:title];
    const auto isPreviewRow = dateTime.length > 0;
    const auto hasSubtitle = subtitle.length > 0;
    _label.font = isPreviewRow ? [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold] : [NSFont systemFontOfSize:13];
    _label.textColor = enabled ? NSColor.labelColor : NSColor.tertiaryLabelColor;
    _label.lineBreakMode = isPreviewRow && !hasSubtitle ? NSLineBreakByWordWrapping : NSLineBreakByTruncatingTail;
    _label.maximumNumberOfLines = isPreviewRow && !hasSubtitle ? 2 : 1;
    _label.translatesAutoresizingMaskIntoConstraints = NO;

    NSView *textContainer = nil;
    NSTextField *subtitleLabel = nil;
    NSTextField *dateTimeLabel = nil;
    if (isPreviewRow) {
        textContainer = [[[NSView alloc] init] autorelease];
        textContainer.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:textContainer];
        [textContainer addSubview:_label];

        if (hasSubtitle) {
            subtitleLabel = [NSTextField labelWithString:subtitle];
            subtitleLabel.font = [NSFont systemFontOfSize:13];
            subtitleLabel.textColor = enabled ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
            subtitleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
            subtitleLabel.maximumNumberOfLines = 1;
            subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
            [textContainer addSubview:subtitleLabel];
        }

        dateTimeLabel = [NSTextField labelWithString:dateTime];
        dateTimeLabel.font = [NSFont systemFontOfSize:11];
        dateTimeLabel.textColor = enabled ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
        dateTimeLabel.alignment = NSTextAlignmentLeft;
        dateTimeLabel.lineBreakMode = NSLineBreakByTruncatingTail;
        dateTimeLabel.maximumNumberOfLines = 1;
        dateTimeLabel.translatesAutoresizingMaskIntoConstraints = NO;
        [textContainer addSubview:dateTimeLabel];
    } else {
        [self addSubview:_label];
    }

    auto constraints = [NSMutableArray arrayWithArray:@[
        [self.heightAnchor constraintEqualToConstant:isPreviewRow ? (hasSubtitle ? kDetailedPreviewActionHeight : kPreviewActionHeight) : kActionHeight],
        [self.widthAnchor constraintEqualToConstant:width],
    ]];

    if (isPreviewRow) {
        [constraints addObjectsFromArray:@[
            [textContainer.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [textContainer.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor constant:4.0],
            [textContainer.bottomAnchor constraintLessThanOrEqualToAnchor:self.bottomAnchor constant:-4.0],
            [_label.topAnchor constraintEqualToAnchor:textContainer.topAnchor],
            [_label.leadingAnchor constraintEqualToAnchor:textContainer.leadingAnchor],
            [_label.trailingAnchor constraintEqualToAnchor:textContainer.trailingAnchor],
            [dateTimeLabel.leadingAnchor constraintEqualToAnchor:textContainer.leadingAnchor],
            [dateTimeLabel.trailingAnchor constraintEqualToAnchor:textContainer.trailingAnchor],
            [dateTimeLabel.bottomAnchor constraintEqualToAnchor:textContainer.bottomAnchor],
        ]];
        if (hasSubtitle) {
            [constraints addObjectsFromArray:@[
                [subtitleLabel.topAnchor constraintEqualToAnchor:_label.bottomAnchor],
                [subtitleLabel.leadingAnchor constraintEqualToAnchor:textContainer.leadingAnchor],
                [subtitleLabel.trailingAnchor constraintEqualToAnchor:textContainer.trailingAnchor],
                [dateTimeLabel.topAnchor constraintEqualToAnchor:subtitleLabel.bottomAnchor],
            ]];
        } else {
            [constraints addObject:[dateTimeLabel.topAnchor constraintEqualToAnchor:_label.bottomAnchor]];
        }
    } else {
        [constraints addObject:[_label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor]];
    }

    if (showsSubmenuIndicator) {
        NSImageView *chevron = [[[NSImageView alloc] init] autorelease];
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
        ]];
        if (isPreviewRow) {
            [constraints addObject:[textContainer.trailingAnchor constraintLessThanOrEqualToAnchor:chevron.leadingAnchor constant:-8]];
        } else {
            [constraints addObject:[_label.trailingAnchor constraintLessThanOrEqualToAnchor:chevron.leadingAnchor constant:-8]];
        }
    } else {
        if (isPreviewRow) {
            [constraints addObject:[textContainer.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad]];
        } else {
            [constraints addObject:[_label.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad]];
        }
    }

    if (icon) {
        _iconView = [[[NSImageView alloc] init] autorelease];
        _iconView.image = icon;
        _iconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        _iconView.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:_iconView];
        [constraints addObjectsFromArray:@[
            [_iconView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad],
            [_iconView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [_iconView.widthAnchor constraintEqualToConstant:18.0],
            [_iconView.heightAnchor constraintEqualToConstant:18.0],
        ]];
        if (isPreviewRow) {
            [constraints addObject:[textContainer.leadingAnchor constraintEqualToAnchor:_iconView.trailingAnchor constant:8.0]];
        } else {
            [constraints addObject:[_label.leadingAnchor constraintEqualToAnchor:_iconView.trailingAnchor constant:8.0]];
        }
    } else {
        if (isPreviewRow) {
            [constraints addObject:[textContainer.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad]];
        } else {
            [constraints addObject:[_label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad]];
        }
    }

    [NSLayoutConstraint activateConstraints:constraints];
    return self;
}

- (void)setIcon:(NSImage *)icon
{
    if (icon && _iconView) {
        if (_iconTintedToLabelColor) {
            auto templateIcon = [icon copy];
            [templateIcon setTemplate:YES];
            _iconView.contentTintColor = NSColor.labelColor;
            _iconView.image = templateIcon;
            [templateIcon release];
        } else {
            _iconView.contentTintColor = nil;
            _iconView.image = icon;
        }
    }
}

- (void)setIconTintedToLabelColor:(BOOL)tinted
{
    _iconTintedToLabelColor = tinted;
    if (_iconView.image) {
        [self setIcon:_iconView.image];
    }
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

- (void)dealloc
{
    [_action release];
    [_hoverAction release];
    [super dealloc];
}

@end

@interface NCSectionHeaderRow : NSView
- (instancetype)initWithTitle:(NSString *)title width:(CGFloat)width;
@end

@implementation NCSectionHeaderRow

- (instancetype)initWithTitle:(NSString *)title width:(CGFloat)width
{
    self = [super init];
    if (!self) return nil;
    self.translatesAutoresizingMaskIntoConstraints = NO;

    auto label = [NSTextField labelWithString:title];
    label.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    label.textColor = NSColor.secondaryLabelColor;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:label];

    [NSLayoutConstraint activateConstraints:@[
        [self.heightAnchor constraintEqualToConstant:kSectionHeaderHeight],
        [self.widthAnchor constraintEqualToConstant:width],
        [label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad],
        [label.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad],
        [label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    ]];
    return self;
}

@end

@interface NCStaticInfoRow : NSView
- (instancetype)initWithTitle:(NSString *)title icon:(NSImage *)icon width:(CGFloat)width;
@end

@implementation NCStaticInfoRow

- (instancetype)initWithTitle:(NSString *)title icon:(NSImage *)icon width:(CGFloat)width
{
    self = [super init];
    if (!self) return nil;
    self.translatesAutoresizingMaskIntoConstraints = NO;

    auto label = [NSTextField labelWithString:title];
    label.font = [NSFont systemFontOfSize:13];
    label.textColor = NSColor.labelColor;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:label];

    auto constraints = [NSMutableArray arrayWithArray:@[
        [self.heightAnchor constraintEqualToConstant:kActionHeight],
        [self.widthAnchor constraintEqualToConstant:width],
        [label.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [label.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kHPad],
    ]];

    if (icon) {
        auto iconView = [[[NSImageView alloc] init] autorelease];
        iconView.image = icon;
        iconView.contentTintColor = NSColor.secondaryLabelColor;
        iconView.imageScaling = NSImageScaleProportionallyUpOrDown;
        iconView.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:iconView];
        [constraints addObjectsFromArray:@[
            [iconView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad],
            [iconView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
            [iconView.widthAnchor constraintEqualToConstant:kActivityPreviewIconSize],
            [iconView.heightAnchor constraintEqualToConstant:kActivityPreviewIconSize],
            [label.leadingAnchor constraintEqualToAnchor:iconView.trailingAnchor constant:8.0],
        ]];
    } else {
        [constraints addObject:[label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad]];
    }

    [NSLayoutConstraint activateConstraints:constraints];
    return self;
}

@end

@interface NCPointingHandButton : NSButton
@end

@implementation NCPointingHandButton

- (void)resetCursorRects
{
    [super resetCursorRects];
    [self addCursorRect:self.bounds cursor:[NSCursor pointingHandCursor]];
}

@end

@interface NCAlertBoxRow : NSView
- (instancetype)initWithTitle:(NSString *)title action:(dispatch_block_t)action hoverAction:(NCActionHoverBlock)hoverAction;
@end

@implementation NCAlertBoxRow {
    dispatch_block_t _action;
    NCActionHoverBlock _hoverAction;
}

- (instancetype)initWithTitle:(NSString *)title action:(dispatch_block_t)action hoverAction:(NCActionHoverBlock)hoverAction
{
    self = [super init];
    if (!self) return nil;

    _action = [action copy];
    _hoverAction = [hoverAction copy];
    self.translatesAutoresizingMaskIntoConstraints = NO;

    auto label = [NSTextField labelWithString:title];
    label.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    label.textColor = NSColor.labelColor;
    label.lineBreakMode = NSLineBreakByTruncatingTail;
    label.maximumNumberOfLines = 2;
    label.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:label];

    auto resolveButton = [[[NCPointingHandButton alloc] init] autorelease];
    resolveButton.title = QCoreApplication::translate("TrayAccountPopup", "Resolve").toNSString();
    resolveButton.target = self;
    resolveButton.action = @selector(resolveButtonClicked:);
    resolveButton.bezelStyle = NSBezelStyleRounded;
    resolveButton.controlSize = NSControlSizeSmall;
    resolveButton.font = [NSFont systemFontOfSize:11 weight:NSFontWeightRegular];
    resolveButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:resolveButton];

    [NSLayoutConstraint activateConstraints:@[
        [self.widthAnchor constraintEqualToConstant:kPopupWidth],
        [self.heightAnchor constraintGreaterThanOrEqualToConstant:kActionHeight],

        [label.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kHPad + kAvatarSize + 10.0],
        [label.trailingAnchor constraintLessThanOrEqualToAnchor:resolveButton.leadingAnchor constant:-8.0],
        [label.topAnchor constraintEqualToAnchor:self.topAnchor constant:kAccountHoverVerticalMargin],
        [label.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-kAccountHoverVerticalMargin],

        [resolveButton.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-kHPad],
        [resolveButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [resolveButton.widthAnchor constraintGreaterThanOrEqualToConstant:76.0],
    ]];

    return self;
}

- (void)dealloc
{
    [_action release];
    [_hoverAction release];
    [super dealloc];
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    auto trackingAreas = [self.trackingAreas copy];
    for (NSTrackingArea *area in trackingAreas) {
        [self removeTrackingArea:area];
    }
    [trackingAreas release];

    auto trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                     options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways
                                                       owner:self
                                                    userInfo:nil];
    [self addTrackingArea:trackingArea];
    [trackingArea release];
}

- (void)mouseEntered:(NSEvent *)event
{
    if (_hoverAction) {
        _hoverAction(self);
    }
}

- (void)mouseUp:(NSEvent *)event
{
    if (_action) {
        _action();
    }
}

- (void)resolveButtonClicked:(id)sender
{
    if (_action) {
        _action();
    }
}

@end

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
- (void)openAssistantForIndex:(int)index;
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

    NSView *container = [[[NSView alloc] init] autorelease];
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    self.contentView = container;

    NSVisualEffectView *vev = [[[NSVisualEffectView alloc] init] autorelease];
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
    auto arrangedSubviews = [_stack.arrangedSubviews copy];
    for (NSView *v in arrangedSubviews) {
        [_stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }
    [arrangedSubviews release];

    __unsafe_unretained NCTrayPopup *weakOwner = owner;
    auto appsModel = OCC::TrayAccountAppsModel::instance();
    appsModel->setUserId(userIndex);
    const auto accounts = OCC::AccountManager::instance()->accounts();
    const auto accountState = userIndex >= 0 && userIndex < accounts.size()
        ? accounts.at(userIndex)
        : OCC::AccountStatePtr{};
    auto fallbackIcon = [[NSImage imageWithSystemSymbolName:@"app" accessibilityDescription:nil]
        imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:14 weight:NSFontWeightRegular]];
    addOwnedArrangedSubview(_stack, [[NCSpacerView alloc] initWithHeight:kActionVerticalPadding width:kAppsPopupWidth]);
    for (auto row = 0; row < appsModel->rowCount(); ++row) {
        const auto appIndex = appsModel->index(row);
        const auto appUrl = appsModel->data(appIndex, OCC::TrayAccountAppsModel::UrlRole).toUrl();
        const auto appIconUrl = appsModel->data(appIndex, OCC::TrayAccountAppsModel::IconUrlRole).toUrl();
        const auto appIconCacheKey = remoteAppIconCacheKey(accountState, appIconUrl);
        auto appIcon = nsImageFromQUrl(appIconUrl);
        if (!appIcon && !appIconCacheKey.isEmpty() && s_remoteAppIconCache.contains(appIconCacheKey)) {
            appIcon = nsImageFromQImage(s_remoteAppIconCache.value(appIconCacheKey));
        }
        auto actionRow = [[NCActionRow alloc] initWithTitle:appsModel->data(appIndex, OCC::TrayAccountAppsModel::NameRole).toString().toNSString()
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
            QObject::connect(iconJob, &OCC::IconJob::jobFinished, iconJob, [retainedRow, appIconCacheKey](const QByteArray &iconData) {
                const auto image = qImageFromImageData(iconData, QSize(18, 18));
                if (!image.isNull()) {
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

@interface NCNotificationActionsPopup : NSPanel
- (void)populateForUserIndex:(int)userIndex activityIndex:(int)activityIndex actions:(QVariantList)actions owner:(NCTrayPopup *)owner;
@end

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

    self.level = NSPopUpMenuWindowLevel;
    self.hasShadow = YES;
    self.releasedWhenClosed = NO;
    self.backgroundColor = NSColor.clearColor;
    self.opaque = NO;

    NSView *container = [[[NSView alloc] init] autorelease];
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    self.contentView = container;

    NSVisualEffectView *vev = [[[NSVisualEffectView alloc] init] autorelease];
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

- (void)populateForUserIndex:(int)userIndex activityIndex:(int)activityIndex actions:(QVariantList)actions owner:(NCTrayPopup *)owner
{
    auto arrangedSubviews = [_stack.arrangedSubviews copy];
    for (NSView *v in arrangedSubviews) {
        [_stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }
    [arrangedSubviews release];

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
            if (dismisses) {
                OCC::UserModel::instance()->dismissNotification(userIndex, activityIndex);
                [weakSelf orderOut:nil];
                return;
            }
            if (opensActivities) {
                [weakOwner openActivitiesForIndex:userIndex];
                return;
            }

            [weakOwner closeAllPopups];
            OCC::UserModel::instance()->triggerNotificationAction(userIndex, activityIndex, actionIndex);
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

@interface NCAccountActionsPopup : NSPanel
- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner;
- (void)populateForUserIndex:(int)userIndex owner:(NCTrayPopup *)owner refreshActivities:(BOOL)refreshActivities;
- (BOOL)isShowingActivitiesForUserIndex:(int)userIndex;
- (void)clearActiveSubmenuRow;
- (void)hideAppsPopup;
- (void)showNotificationActionsPopupFromRow:(NSView *)row forUserIndex:(int)userIndex activityIndex:(int)activityIndex actions:(QVariantList)actions;
@end

@implementation NCAccountActionsPopup {
    NSStackView *_stack;
    NCAppsPopup *_appsPopup;
    NCNotificationActionsPopup *_notificationActionsPopup;
    NCActionRow *_activeSubmenuRow;
    __unsafe_unretained NCTrayPopup *_owner;
    QMetaObject::Connection _recentActivitiesConnection;
    int _userIndex;
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
    _userIndex = -1;

    NSView *container = [[[NSView alloc] init] autorelease];
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    self.contentView = container;

    NSVisualEffectView *vev = [[[NSVisualEffectView alloc] init] autorelease];
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

    if (popupOrigin.x + popupWidth > rightEdge && rowTopLeftOnScreen.x - popupWidth >= leftEdge) {
        popupOrigin.x = rowTopLeftOnScreen.x - popupWidth;
    }
    popupOrigin = clampedPopupOrigin(popupOrigin, NSMakeSize(popupWidth, popupHeight), visibleFrame);

    [_appsPopup setFrameOrigin:popupOrigin];
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

    const auto rowTopLeftInWindow = [row convertPoint:NSMakePoint(NSMinX(row.bounds) + kHPad, NSMaxY(row.bounds)) toView:nil];
    const auto rowTopRightInWindow = [row convertPoint:NSMakePoint(NSMaxX(row.bounds) - kHPad, NSMaxY(row.bounds)) toView:nil];
    const auto rowTopLeftOnScreen = [row.window convertPointToScreen:rowTopLeftInWindow];
    auto rowTopRightOnScreen = [row.window convertPointToScreen:rowTopRightInWindow];

    const auto popupWidth = _notificationActionsPopup.frame.size.width;
    const auto popupHeight = _notificationActionsPopup.frame.size.height;
    auto popupOrigin = rowTopRightOnScreen;
    popupOrigin.y -= popupHeight;

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
    popupOrigin = clampedPopupOrigin(popupOrigin, NSMakeSize(popupWidth, popupHeight), visibleFrame);

    [_notificationActionsPopup setFrameOrigin:popupOrigin];
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

    auto arrangedSubviews = [_stack.arrangedSubviews copy];
    for (NSView *v in arrangedSubviews) {
        [_stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }
    [arrangedSubviews release];

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
    addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:QCoreApplication::translate("TrayFoldersMenuButton", "Open local folder").toNSString()
                                                                 width:kAccountActionsPopupWidth
                                                               enabled:YES
                                                                action:^{
        [weakOwner openLocalFolderForIndex:userIndex];
    } hoverAction:^(NSView *) {
        [weakSelf hideAppsPopup];
    }]);
    if (assistantEnabled) {
        addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:QCoreApplication::translate("MainWindow", "Ask Assistant\302\240\342\200\246").toNSString()
                                                                      width:kAccountActionsPopupWidth
                                                                    enabled:YES
                                                                     action:^{
            [weakOwner openAssistantForIndex:userIndex];
        } hoverAction:^(NSView *) {
            [weakSelf hideAppsPopup];
        }]);
    }
    addOwnedArrangedSubview(_stack, [[NCActionRow alloc] initWithTitle:QCoreApplication::translate("TrayWindowHeader", "More apps").toNSString()
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

    NSView *container = [[[NSView alloc] init] autorelease];
    container.wantsLayer = YES;
    container.layer.cornerRadius = kCornerRadius;
    container.layer.masksToBounds = YES;
    self.contentView = container;

    NSVisualEffectView *vev = [[[NSVisualEffectView alloc] init] autorelease];
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

    return row;
}

- (void)populate
{
    [_accountActionsPopup orderOut:nil];
    [self clearActiveAccountRow];

    auto arrangedSubviews = [_stack.arrangedSubviews copy];
    for (NSView *v in arrangedSubviews) {
        [_stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }
    [arrangedSubviews release];

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
    popupOrigin = clampedPopupOrigin(popupOrigin, NSMakeSize(popupWidth, _accountActionsPopup.frame.size.height), visibleFrame);

    [_accountActionsPopup setFrameOrigin:popupOrigin];
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

    auto qtScreen = iconRect.isValid() && !iconRect.isNull()
        ? QGuiApplication::screenAt(iconRect.center())
        : nullptr;
    NSScreen *screen = nsScreenForQtScreen(qtScreen);
    if (!screen) {
        screen = NSScreen.screens.firstObject;
    }

    const CGFloat popupW  = s_popup.frame.size.width;
    const CGFloat popupH  = s_popup.frame.size.height;
    const NSRect visibleFrame = screen.visibleFrame;

    CGFloat x, y;
    if (iconRect.isValid() && !iconRect.isNull() && qtScreen) {
        const auto qtScreenGeometry = qtScreen->geometry();
        x = NSMinX(screen.frame) + iconRect.x() - qtScreenGeometry.x() - kStatusItemLeadingOffset;
        y = NSMaxY(screen.frame) - (iconRect.y() + iconRect.height() - qtScreenGeometry.y()) - popupH - kStatusItemVerticalOffset;
    } else {
        x = NSMaxX(visibleFrame) - popupW - kScreenEdgePadding;
        y = NSMaxY(visibleFrame) - popupH;
    }

    const auto popupOrigin = clampedPopupOrigin(NSMakePoint(x, y), NSMakeSize(popupW, popupH), visibleFrame);

    [s_popup setFrameOrigin:popupOrigin];
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
