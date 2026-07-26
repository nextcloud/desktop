/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>
#include <QUrl>

#import <Cocoa/Cocoa.h>

namespace OCC::Mac::TrayPopupImageUtils {

// Decode raw image data (raster or SVG) into a QImage scaled into requestedSize.
QImage qImageFromImageData(const QByteArray &imageData, const QSize &requestedSize);

// Bridge a QImage into an autoreleased NSImage, honouring the device pixel ratio.
NSImage *nsImageFromQImage(const QImage &qimg);

// Load a QImage from a local-file or qrc URL. Any other URL is handed to QImage
// as a plain file path, so it is not fetched over the network.
QImage qImageFromQUrl(const QUrl &url);

// Convenience: load a URL straight into an autoreleased NSImage.
NSImage *nsImageFromQUrl(const QUrl &url);

// Resolve an SF Symbol by name at the given point size, falling back to "doc".
NSImage *systemSymbolImage(const QString &symbolName, const CGFloat pointSize);

// Build the cache key used to memoise remotely fetched app icons, or an empty
// string when the inputs cannot form a valid key.
QString remoteAppIconCacheKey(const OCC::AccountStatePtr &accountState, const QUrl &url, const QSize &requestedSize);

// Backing scale factor of the screen the window is on, falling back to the main
// screen and finally 1.0.
qreal backingScaleFactorForWindow(NSWindow *window);

// Convert a point size to an integer pixel size for the given scale (>= 1px).
QSize pixelSizeForPointSize(const CGFloat pointSize, const qreal scale);

}
