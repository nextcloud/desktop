/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "iconutils.h"

#include <theme.h>

#include <QFile>
#include <QLoggingCategory>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace {

QString findSvgFilePath(const QString &fileName, const QStringList &possibleColors)
{
    auto result = QString{OCC::Theme::themePrefix + fileName};

    if (QFile::exists(result)) {
        return result;
    } else {
        for (const auto &color : possibleColors) {
            result = QString{OCC::Theme::themePrefix + color + QStringLiteral("/") + fileName};

            if (QFile::exists(result)) {
                return result;
            }
        }
        result.clear();
    }

    return result;
}

QImage findImageWithCustomColor(const QString &fileName,
                                const QColor &customColor,
                                const QStringList &iconBaseColors,
                                const QSize &requestedSize)
{
    // check if there is an existing image matching the custom color
    const auto customColorName = [&customColor]() {
        auto result = customColor.name();
        if (result.startsWith(QStringLiteral("#"))) {
            if (result == QStringLiteral("#000000")) {
                result = QStringLiteral("black");
            }
            if (result == QStringLiteral("#ffffff")) {
                result = QStringLiteral("white");
            }
        }
        return result;
    }();

    if (const auto possiblePath = QString(OCC::Theme::themePrefix + customColorName + QStringLiteral("/") + fileName);
            iconBaseColors.contains(customColorName) && QFile::exists(possiblePath)) {

        if (requestedSize.width() > 0 && requestedSize.height() > 0) {
            return QIcon(possiblePath).pixmap(requestedSize).toImage();
        } else {
            return QImage{possiblePath};
        }
    }

    return {};
}

}

namespace OCC {
namespace Ui {
namespace IconUtils {

Q_LOGGING_CATEGORY(lcIconUtils, "nextcloud.gui.iconutils", QtInfoMsg)

QPixmap pixmapForBackground(const QString &fileName, const QColor &backgroundColor)
{
    Q_ASSERT(!fileName.isEmpty());

    const auto pixmapColor = backgroundColor.isValid() && !Theme::isDarkColor(backgroundColor)
        ? QColorConstants::Svg::black
        : QColorConstants::Svg::white;

    return createSvgPixmapWithCustomColorCached(fileName, pixmapColor);
}

QImage createSvgImageWithCustomColor(const QString &fileName,
                                     const QColor &customColor,
                                     QSize *originalSize,
                                     const QSize &requestedSize)
{
    Q_ASSERT(!fileName.isEmpty());
    Q_ASSERT(customColor.isValid());

    if (fileName.isEmpty() || !customColor.isValid()) {
        qWarning(lcIconUtils) << "invalid fileName or customColor";
        return {};
    }

    const auto sizeToUse = requestedSize.isValid() || originalSize == nullptr ? requestedSize : *originalSize;

    // some icons are present in white or black only, so, we need to check both when needed
    const auto iconBaseColors = QStringList{QStringLiteral("black"), QStringLiteral("white")};
    const auto customColorImage = findImageWithCustomColor(fileName, customColor, iconBaseColors, sizeToUse);

    if (!customColorImage.isNull()) {
        return customColorImage;
    }
    
    // find the first matching svg file
    const auto sourceSvg = findSvgFilePath(fileName, iconBaseColors);
    Q_ASSERT(!sourceSvg.isEmpty());

    if (sourceSvg.isEmpty()) {
        qWarning(lcIconUtils) << "Failed to find base SVG file for" << fileName;
        return {};
    }

    const auto result = drawSvgWithCustomFillColor(sourceSvg, customColor, originalSize, sizeToUse);
    Q_ASSERT(!result.isNull());

    if (result.isNull()) {
        qCWarning(lcIconUtils) << "Failed to load pixmap for" << fileName;
    }

    return result;
}

QPixmap createSvgPixmapWithCustomColorCached(const QString &fileName, const QColor &customColor, QSize *originalSize, const QSize &requestedSize)
{
    QPixmap cachedPixmap;

    const auto customColorName = customColor.name();

    const QString cacheKey = fileName + QStringLiteral(",") + customColorName;

    // check for existing QPixmap in cache
    if (QPixmapCache::find(cacheKey, &cachedPixmap)) {
        if (originalSize) {
            *originalSize = {};
        }
        return cachedPixmap;
    }

    cachedPixmap = QPixmap::fromImage(createSvgImageWithCustomColor(fileName, customColor, originalSize, requestedSize));

    if (!cachedPixmap.isNull()) {
        QPixmapCache::insert(cacheKey, cachedPixmap);
    }

    return cachedPixmap;
}

QImage drawSvgWithCustomFillColor(const QString &sourceSvgPath,
                                  const QColor &fillColor,
                                  QSize *originalSize,
                                  const QSize &requestedSize)
{
    QSvgRenderer svgRenderer;

    if (!svgRenderer.load(sourceSvgPath)) {
        qCWarning(lcIconUtils) << "Could no load initial SVG image";
        return {};
    }

    const auto reqSize = (requestedSize.isValid() && requestedSize.height() && requestedSize.height()) ? requestedSize : svgRenderer.defaultSize();
    if (!reqSize.isValid() || !reqSize.height() || !reqSize.height()) {
        return {};
    }

    if (originalSize) {
        *originalSize = svgRenderer.defaultSize();
    }

    // render source image
    QImage svgImage(reqSize, QImage::Format_ARGB32);
    {
        QPainter svgImagePainter(&svgImage);
        svgImage.fill(Qt::GlobalColor::transparent);
        svgRenderer.render(&svgImagePainter);
    }

    // draw target image with custom fillColor
    QImage image(reqSize, QImage::Format_ARGB32);
    image.fill(QColor(fillColor));
    {
        QPainter imagePainter(&image);
        imagePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        imagePainter.drawImage(0, 0, svgImage);
    }

    return image;
}
}
}
}
