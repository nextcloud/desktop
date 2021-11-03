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

#ifndef ICONUTILS_H
#define ICONUTILS_H

#include <QColor>
#include <QPixmap>

namespace OCC {
namespace Ui {
namespace IconUtils {
QPixmap pixmapForBackground(const QString &fileName, const QColor &backgroundColor);
QImage createSvgImageWithCustomColor(const QString &fileName, const QColor &customColor, QSize *originalSize = nullptr, const QSize &requestedSize = {});
QPixmap createSvgPixmapWithCustomColorCached(const QString &fileName, const QColor &customColor, QSize *originalSize = nullptr, const QSize &requestedSize = {});
QImage drawSvgWithCustomFillColor(const QString &sourceSvgPath, const QColor &fillColor, QSize *originalSize = nullptr, const QSize &requestedSize = {});
}
}
}
#endif // ICONUTILS_H
