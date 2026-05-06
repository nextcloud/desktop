/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "systray.h"
#include "tray/usermodel.h"

#include <QImage>

#import <Cocoa/Cocoa.h>

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

static const CGFloat kPopupWidth    = 340.0;
static const CGFloat kRowHeight     = 64.0;
static const CGFloat kAvatarSize    = 40.0;
static const CGFloat kActionHeight  = 38.0;
static const CGFloat kCornerRadius  = 14.0;
static const CGFloat kHPad          = 14.0;
static const CGFloat kVPad          = 12.0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// NCAccountRow — a hoverable NSView wrapping one account entry
// ---------------------------------------------------------------------------

@protocol NCAccountRowDelegate
- (void)onAccountRowClicked:(int)index;
@end

@interface NCAccountRow : NSView
@property (nonatomic, assign) int userIndex;
@property (nonatomic, assign) id<NCAccountRowDelegate> popupDelegate;
@end

@implementation NCAccountRow

- (void)mouseUp:(NSEvent *)event
{
    [self.popupDelegate onAccountRowClicked:self.userIndex];
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
    NSTrackingAreaOptions opts = NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways;
    [self addTrackingArea:[[NSTrackingArea alloc] initWithRect:self.bounds options:opts owner:self userInfo:nil]];
}

@end

// ---------------------------------------------------------------------------
// NCTrayPopup
// ---------------------------------------------------------------------------

@interface NCTrayPopup : NSPanel <NCAccountRowDelegate>
- (void)populate;
@end

@implementation NCTrayPopup {
    NSStackView *_stack;
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

    NSVisualEffectView *vev = [[NSVisualEffectView alloc] init];
    vev.material = NSVisualEffectMaterialHUDWindow;
    vev.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    vev.state = NSVisualEffectStateActive;
    vev.wantsLayer = YES;
    vev.layer.cornerRadius = kCornerRadius;
    vev.layer.masksToBounds = YES;
    self.contentView = vev;

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
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
}

// ---- Account row ----------------------------------------------------------

- (NCAccountRow *)makeRowForIndex:(int)index
                             name:(NSString *)name
                           server:(NSString *)server
                           avatar:(NSImage *)avatar
                           syncOk:(BOOL)syncOk
{
    NCAccountRow *row = [[NCAccountRow alloc] init];
    row.wantsLayer = YES;
    row.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);
    row.translatesAutoresizingMaskIntoConstraints = NO;
    row.userIndex = index;
    row.popupDelegate = self;

    // Avatar
    NSImageView *avatarView = [[NSImageView alloc] init];
    avatarView.image = avatar != nil ? avatar : [NSImage imageWithSystemSymbolName:@"person.circle.fill"
                                                              accessibilityDescription:nil];
    avatarView.wantsLayer = YES;
    avatarView.layer.cornerRadius = kAvatarSize / 2.0;
    avatarView.layer.masksToBounds = YES;
    avatarView.imageScaling = NSImageScaleProportionallyUpOrDown;
    avatarView.translatesAutoresizingMaskIntoConstraints = NO;

    // Name label
    NSTextField *nameLabel = [NSTextField labelWithString:name];
    nameLabel.font = [NSFont systemFontOfSize:13 weight:NSFontWeightSemibold];
    nameLabel.textColor = NSColor.labelColor;
    nameLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    nameLabel.translatesAutoresizingMaskIntoConstraints = NO;

    // Server label
    NSTextField *serverLabel = [NSTextField labelWithString:server];
    serverLabel.font = [NSFont systemFontOfSize:11];
    serverLabel.textColor = NSColor.secondaryLabelColor;
    serverLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    serverLabel.translatesAutoresizingMaskIntoConstraints = NO;

    // Sync status icon
    NSImageView *statusView = [[NSImageView alloc] init];
    NSString *symbol = syncOk ? @"checkmark.circle.fill" : @"arrow.triangle.2.circlepath";
    NSImage *statusImg = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:nil];
    NSImageSymbolConfiguration *cfg = [NSImageSymbolConfiguration
        configurationWithPointSize:16 weight:NSFontWeightMedium];
    statusView.image = [statusImg imageWithSymbolConfiguration:cfg];
    statusView.contentTintColor = syncOk ? NSColor.systemGreenColor : NSColor.systemBlueColor;
    statusView.translatesAutoresizingMaskIntoConstraints = NO;

    // Chevron
    NSImageView *chevron = [[NSImageView alloc] init];
    NSImageSymbolConfiguration *chevCfg = [NSImageSymbolConfiguration
        configurationWithPointSize:11 weight:NSFontWeightMedium];
    chevron.image = [[NSImage imageWithSystemSymbolName:@"chevron.right"
                                  accessibilityDescription:nil]
                     imageWithSymbolConfiguration:chevCfg];
    chevron.contentTintColor = NSColor.tertiaryLabelColor;
    chevron.translatesAutoresizingMaskIntoConstraints = NO;

    [row addSubview:avatarView];
    [row addSubview:nameLabel];
    [row addSubview:serverLabel];
    [row addSubview:statusView];
    [row addSubview:chevron];

    [NSLayoutConstraint activateConstraints:@[
        // Avatar — vertically centered, left edge
        [avatarView.leadingAnchor constraintEqualToAnchor:row.leadingAnchor constant:kHPad],
        [avatarView.centerYAnchor constraintEqualToAnchor:row.centerYAnchor],
        [avatarView.widthAnchor constraintEqualToConstant:kAvatarSize],
        [avatarView.heightAnchor constraintEqualToConstant:kAvatarSize],

        // Name — top half, next to avatar
        [nameLabel.leadingAnchor constraintEqualToAnchor:avatarView.trailingAnchor constant:10],
        [nameLabel.topAnchor constraintEqualToAnchor:row.topAnchor constant:kVPad],
        [nameLabel.trailingAnchor constraintLessThanOrEqualToAnchor:statusView.leadingAnchor constant:-8],

        // Server — below name
        [serverLabel.leadingAnchor constraintEqualToAnchor:nameLabel.leadingAnchor],
        [serverLabel.topAnchor constraintEqualToAnchor:nameLabel.bottomAnchor constant:2],
        [serverLabel.trailingAnchor constraintLessThanOrEqualToAnchor:statusView.leadingAnchor constant:-8],

        // Chevron — far right, centered
        [chevron.trailingAnchor constraintEqualToAnchor:row.trailingAnchor constant:-kHPad],
        [chevron.centerYAnchor constraintEqualToAnchor:row.centerYAnchor],
        [chevron.widthAnchor constraintEqualToConstant:8],
        [chevron.heightAnchor constraintEqualToConstant:13],

        // Status icon — left of chevron
        [statusView.trailingAnchor constraintEqualToAnchor:chevron.leadingAnchor constant:-8],
        [statusView.centerYAnchor constraintEqualToAnchor:row.centerYAnchor],
        [statusView.widthAnchor constraintEqualToConstant:18],
        [statusView.heightAnchor constraintEqualToConstant:18],

        // Row size
        [row.heightAnchor constraintEqualToConstant:kRowHeight],
        [row.widthAnchor constraintEqualToConstant:kPopupWidth],
    ]];

    return row;
}

// ---- Action button --------------------------------------------------------

- (NSButton *)makeActionButton:(NSString *)title
{
    NSButton *btn = [NSButton buttonWithTitle:title target:nil action:nil];
    btn.bordered = NO;
    btn.alignment = NSTextAlignmentLeft;
    btn.font = [NSFont systemFontOfSize:13];
    btn.translatesAutoresizingMaskIntoConstraints = NO;

    NSMutableParagraphStyle *ps = [[NSMutableParagraphStyle alloc] init];
    ps.firstLineHeadIndent = kHPad;
    btn.attributedTitle = [[NSAttributedString alloc]
        initWithString:title
            attributes:@{
                NSFontAttributeName: btn.font,
                NSForegroundColorAttributeName: NSColor.labelColor,
                NSParagraphStyleAttributeName: ps
            }];

    [NSLayoutConstraint activateConstraints:@[
        [btn.heightAnchor constraintEqualToConstant:kActionHeight],
        [btn.widthAnchor constraintEqualToConstant:kPopupWidth],
    ]];
    return btn;
}

// ---- Populate -------------------------------------------------------------

- (void)populate
{
    for (NSView *v in _stack.arrangedSubviews.copy) {
        [_stack removeArrangedSubview:v];
        [v removeFromSuperview];
    }

    OCC::UserModel *model = OCC::UserModel::instance();
    for (int i = 0; i < model->rowCount(); ++i) {
        const QModelIndex idx = model->index(i);
        NSString *name   = model->data(idx, OCC::UserModel::NameRole).toString().toNSString();
        NSString *server = model->data(idx, OCC::UserModel::ServerRole).toString().toNSString();
        const bool syncOk = model->data(idx, OCC::UserModel::SyncStatusOkRole).toBool();
        const QImage qavatar = model->avatarForRow(i);
        NSImage *avatar = nsImageFromQImage(qavatar);

        [_stack addArrangedSubview:[self makeRowForIndex:i
                                                    name:name
                                                  server:server
                                                  avatar:avatar
                                                  syncOk:syncOk]];
    }

    // Separator
    NSBox *sep = [[NSBox alloc] init];
    sep.boxType = NSBoxSeparator;
    sep.translatesAutoresizingMaskIntoConstraints = NO;
    [_stack addArrangedSubview:sep];
    [sep.widthAnchor constraintEqualToConstant:kPopupWidth].active = YES;

    // Action buttons
    NSButton *settingsBtn = [self makeActionButton:@"Settings"];
    [settingsBtn setTarget:self];
    [settingsBtn setAction:@selector(onSettings:)];
    [_stack addArrangedSubview:settingsBtn];

    NSButton *quitBtn = [self makeActionButton:@"Quit"];
    [quitBtn setTarget:self];
    [quitBtn setAction:@selector(onQuit:)];
    [_stack addArrangedSubview:quitBtn];

    // Resize window to fit content
    [self.contentView layoutSubtreeIfNeeded];
    NSRect frame = self.frame;
    frame.size.height = _stack.fittingSize.height;
    [self setFrame:frame display:NO];
}

// ---- Row click ------------------------------------------------------------

- (void)onAccountRowClicked:(int)index
{
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
    OCC::UserModel::instance()->setCurrentUserId(index);
    OCC::Systray::instance()->showWindow();
}

// ---- Action handlers ------------------------------------------------------

- (void)onSettings:(id)sender
{
    [self orderOut:nil];
    OCC::Systray::instance()->setIsOpen(false);
    OCC::Systray::instance()->openSettings();
}

- (void)onQuit:(id)sender
{
    OCC::Systray::instance()->shutdown();
}

@end

// ---------------------------------------------------------------------------
// C++ bridge
// ---------------------------------------------------------------------------

namespace OCC {

static NCTrayPopup *s_popup = nil;

void showMacOSTrayPopup(const QRect &iconRect)
{
    if (!s_popup) {
        s_popup = [[NCTrayPopup alloc] init];
    }

    [s_popup populate];

    NSScreen *screen = NSScreen.screens.firstObject;
    const CGFloat screenH = screen.frame.size.height;
    const CGFloat popupW  = s_popup.frame.size.width;
    const CGFloat popupH  = s_popup.frame.size.height;

    CGFloat x, y;
    if (iconRect.isValid()) {
        x = iconRect.x() + (iconRect.width() - popupW) / 2.0;
        y = screenH - iconRect.bottom() - popupH;
    } else {
        const NSRect visible = screen.visibleFrame;
        x = visible.origin.x + visible.size.width - popupW - 8;
        y = visible.origin.y + visible.size.height - popupH;
    }

    const CGFloat xMax = screen.frame.size.width - popupW - 8;
    x = x < 8 ? 8 : (x > xMax ? xMax : x);

    [s_popup setFrameOrigin:NSMakePoint(x, y)];
    [NSApp activateIgnoringOtherApps:YES];
    [s_popup makeKeyAndOrderFront:nil];
}

void hideMacOSTrayPopup()
{
    [s_popup orderOut:nil];
}

} // namespace OCC
