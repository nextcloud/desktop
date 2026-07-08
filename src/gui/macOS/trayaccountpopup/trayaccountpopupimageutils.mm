/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "trayaccountpopupimageutils.h"

#include "account.h"
#include "accountstate.h"

#include <QMimeDatabase>
#include <QMimeType>
#include <QPainter>
#include <QRectF>
#include <QSvgRenderer>

namespace OCC::Mac::TrayPopupImageUtils {

NSImage *nsImageFromQImage(const QImage &qimg)
{
    if (qimg.isNull()) return nil;
    const auto devicePixelRatio = qimg.devicePixelRatio() > 0.0 ? qimg.devicePixelRatio() : 1.0;
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
    const auto imageSize = NSMakeSize(rgba.width() / devicePixelRatio, rgba.height() / devicePixelRatio);
    rep.size = imageSize;
    NSImage *img = [[NSImage alloc] initWithSize:imageSize];
    [img addRepresentation:rep];
    [rep release];
    return [img autorelease];
}

QImage qImageFromImageData(const QByteArray &imageData, const QSize &requestedSize)
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

QImage qImageFromQUrl(const QUrl &url)
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

NSImage *nsImageFromQUrl(const QUrl &url)
{
    return nsImageFromQImage(qImageFromQUrl(url));
}

NSImage *systemSymbolImage(const QString &symbolName, const CGFloat pointSize)
{
    auto image = [NSImage imageWithSystemSymbolName:symbolName.toNSString() accessibilityDescription:nil];
    if (!image) {
        image = [NSImage imageWithSystemSymbolName:@"doc" accessibilityDescription:nil];
    }
    return [image imageWithSymbolConfiguration:[NSImageSymbolConfiguration configurationWithPointSize:pointSize weight:NSFontWeightRegular]];
}

QString remoteAppIconCacheKey(const OCC::AccountStatePtr &accountState, const QUrl &url, const QSize &requestedSize)
{
    if (!accountState || !accountState->account() || !url.isValid() || url.scheme().isEmpty() || !requestedSize.isValid()) {
        return {};
    }

    return QStringLiteral("%1:%2x%3:%4").arg(
        accountState->account()->id(),
        QString::number(requestedSize.width()),
        QString::number(requestedSize.height()),
        url.toString());
}

qreal backingScaleFactorForWindow(NSWindow *window)
{
    auto scale = window.screen.backingScaleFactor;
    if (scale <= 0.0) {
        scale = NSScreen.mainScreen.backingScaleFactor;
    }
    return scale > 0.0 ? scale : 1.0;
}

QSize pixelSizeForPointSize(const CGFloat pointSize, const qreal scale)
{
    auto pixelSize = static_cast<int>(pointSize * scale + 0.5);
    if (pixelSize < 1) {
        pixelSize = 1;
    }
    return QSize(pixelSize, pixelSize);
}

}
