/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
