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

#include "svgimageprovider.h"
#include "iconutils.h"

#include <QLoggingCategory>

namespace OCC {
namespace Ui {
    Q_LOGGING_CATEGORY(lcSvgImageProvider, "nextcloud.gui.svgimageprovider", QtInfoMsg)

    SvgImageProvider::SvgImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage SvgImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
    {
        Q_ASSERT(!id.isEmpty());

        const auto idSplit = id.split(QStringLiteral("/"), Qt::SkipEmptyParts);

        if (idSplit.isEmpty()) {
            qCWarning(lcSvgImageProvider) << "Image id is incorrect!";
            return {};
        }

        const auto pixmapName = idSplit.at(0);
        const auto pixmapColor = idSplit.size() > 1 ? QColor(idSplit.at(1)) : QColorConstants::Svg::black;

        if (pixmapName.isEmpty() || !pixmapColor.isValid()) {
            qCWarning(lcSvgImageProvider) << "Image id is incorrect!";
            return {};
        }

        if (size != nullptr && (size->width() <= 0 || size->height() <= 0)) {
            *size = QSize(64, 64);
        }

        return IconUtils::createSvgImageWithCustomColor(pixmapName, pixmapColor, size, requestedSize);
    }
}
}
