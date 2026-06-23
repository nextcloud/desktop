/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "theme_mac.h"

#include <QByteArray>
#include <QImage>
#include <QPixmap>

#import <AppKit/AppKit.h>

namespace OCC {

namespace {

QPixmap pixmapFromNSImage(NSImage *image, int side)
{
    if (!image) {
        return {};
    }

    NSImage *resized = [[NSImage alloc] initWithSize:NSMakeSize(side, side)];
    [resized lockFocus];
    [image drawInRect:NSMakeRect(0, 0, side, side)
            fromRect:NSZeroRect
           operation:NSCompositingOperationCopy
            fraction:1.0
      respectFlipped:YES
               hints:nil];
    [resized unlockFocus];

    NSData *tiff = [resized TIFFRepresentation];
    if (!tiff) {
        return {};
    }

    NSBitmapImageRep *bitmap = [NSBitmapImageRep imageRepWithData:tiff];
    NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if (!png) {
        return {};
    }

    const QByteArray bytes(reinterpret_cast<const char *>(png.bytes), static_cast<int>(png.length));
    QImage decoded;
    if (!decoded.loadFromData(bytes, "PNG")) {
        return {};
    }
    return QPixmap::fromImage(std::move(decoded));
}

}

QIcon loadAppIconFromBundle()
{
    @autoreleasepool {
        NSImage *appIcon = [NSImage imageNamed:@"AppIcon"];
        if (!appIcon) {
            return {};
        }

        QIcon icon;
        for (int side : {16, 32, 64, 128, 256, 512, 1024}) {
            const auto pixmap = pixmapFromNSImage(appIcon, side);
            if (!pixmap.isNull()) {
                icon.addPixmap(pixmap);
            }
        }
        return icon;
    }
}

}
