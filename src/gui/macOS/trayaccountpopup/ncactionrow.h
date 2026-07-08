/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import "nchoverview.h"
#import "trayaccountpopupmetrics.h"

/**
 * @brief A selectable menu row with a title and optional icon, subtitle,
 *        timestamp and submenu chevron.
 *
 * Runs its action block when clicked and its hover block when the pointer
 * enters it, provided the row is enabled. A disabled row is dimmed and ignores
 * clicks and hovers.
 */
@interface NCActionRow : NCHoverView
// The initializers below are convenience overloads; each forwards to the
// designated initializer (the one taking subtitle, dateTime and
// showsSubmenuIndicator) with the omitted arguments defaulted.
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
/**
 * @brief Designated initializer.
 * @param title The row's primary text.
 * @param icon Optional leading icon; pass nil for none.
 * @param subtitle Optional secondary line (shown only on preview rows, i.e. when @c dateTime is set); pass nil to omit.
 * @param dateTime Optional timestamp line; when set the row uses the taller preview layout. Pass nil to omit.
 * @param width Fixed width of the row.
 * @param enabled When NO, the row is dimmed and ignores clicks and hovers.
 * @param action Block run when the row is clicked.
 * @param hoverAction Block run when the pointer enters the row, receiving the row itself.
 * @param showsSubmenuIndicator When YES, a trailing chevron marks that the row opens a submenu.
 */
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
/** @brief Keeps an enabled row highlighted even when not hovered (e.g. while its submenu is open). */
- (void)setPersistentHighlight:(BOOL)persistentHighlight;
/** @brief Replaces the leading icon (only if the row was created with one), applying the current label-tint setting. */
- (void)setIcon:(NSImage *)icon;
/** @brief When YES, renders the icon as a template image tinted to the label colour. */
- (void)setIconTintedToLabelColor:(BOOL)tinted;
@end
