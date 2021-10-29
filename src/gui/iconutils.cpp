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
    const QString baseSvgNoColor{QString{OCC::Theme::themePrefix} + fileName};
    if (QFile::exists(baseSvgNoColor)) {
        return baseSvgNoColor;
    }

    for (const auto &color : possibleColors) {
        const QString baseSVG{QString{OCC::Theme::themePrefix} + color + QLatin1Char('/') + fileName};

        if (QFile::exists(baseSVG)) {
            return baseSVG;
        }
    }

    return {};
}
}

namespace OCC {
namespace Ui {
    Q_LOGGING_CATEGORY(lcIconUtils, "nextcloud.gui.iconutils", QtInfoMsg)
    namespace IconUtils {
        QPixmap pixmapForBackground(const QString &fileName, const QColor &backgroundColor)
        {
            Q_ASSERT(!fileName.isEmpty());

            const auto pixmapColor = backgroundColor.isValid()
                && !Theme::isDarkColor(backgroundColor)
                ? QColorConstants::Svg::black
                : QColorConstants::Svg::white;

            return createSvgPixmapWithCustomColor(fileName, pixmapColor);
        }

        QPixmap createSvgPixmapWithCustomColor(const QString &fileName, const QColor &customColor, const QSize &size)
        {
            Q_ASSERT(!fileName.isEmpty());
            Q_ASSERT(customColor.isValid());

            if (fileName.isEmpty()) {
                qWarning(lcIconUtils) << "fileName is empty";
            }

            if (!customColor.isValid()) {
                qWarning(lcIconUtils) << "customColor is invalid";
            }

            const auto customColorName = customColor.name();

            const QString cacheKey = fileName + QLatin1Char(',') + customColorName;

            QPixmap cachedPixmap;

            // check for existing QPixmap in cache
            if (QPixmapCache::find(cacheKey, &cachedPixmap)) {
                return cachedPixmap;
            }

            // some icons are present in white or black only, so, we need to check both when needed
            const auto iconBaseColors = QStringList{QStringLiteral("black"), QStringLiteral("white")};

            // check if there is an existing pixmap matching the custom color
            if (iconBaseColors.contains(customColorName)) {
                cachedPixmap = QPixmap::fromImage(QImage{QString{OCC::Theme::themePrefix} + customColorName + QLatin1Char('/') + fileName});
                QPixmapCache::insert(cacheKey, cachedPixmap);
                return cachedPixmap;
            }

            // find the first matching svg file
            const auto sourceSvg = findSvgFilePath(fileName, iconBaseColors);

            Q_ASSERT(!sourceSvg.isEmpty());
            if (sourceSvg.isEmpty()) {
                qWarning(lcIconUtils) << "Failed to find base SVG file for" << cacheKey;
                return {};
            }

            cachedPixmap = drawSvgWithCustomFillColor(sourceSvg, customColor, size);

            Q_ASSERT(!cachedPixmap.isNull());
            if (cachedPixmap.isNull()) {
                qWarning(lcIconUtils) << "Failed to load pixmap for" << cacheKey;
                return {};
            }

            QPixmapCache::insert(cacheKey, cachedPixmap);

            return cachedPixmap;
        }

        QPixmap drawSvgWithCustomFillColor(const QString &sourceSvgPath, const QColor &fillColor, const QSize &size)
        {
            QSvgRenderer svgRenderer;

            if (!svgRenderer.load(sourceSvgPath)) {
                qCWarning(lcIconUtils) << "Could no load initial SVG image";
                return {};
            }

            const auto requestedSize = size.isValid() ? size : svgRenderer.defaultSize();

            // render source image
            QImage svgImage(requestedSize, QImage::Format_ARGB32);
            {
                QPainter svgImagePainter(&svgImage);
                svgImage.fill(Qt::GlobalColor::transparent);
                svgRenderer.render(&svgImagePainter);
            }

            // draw target image with custom fillColor
            QImage image(requestedSize, QImage::Format_ARGB32);
            image.fill(QColor(fillColor));
            {
                QPainter imagePainter(&image);
                imagePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                imagePainter.drawImage(0, 0, svgImage);
            }

            return QPixmap::fromImage(image);
        }
    }
}
}
