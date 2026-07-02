/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QPointF>
#include <QRectF>
#include <QSize>

namespace OCC {

inline QRectF aspectFitRect(const QSize &sourceSize, const QSize &targetSize)
{
    if (!sourceSize.isValid() || !targetSize.isValid()) {
        return QRectF(QPointF(0.0, 0.0), targetSize);
    }

    const auto scaledSize = sourceSize.scaled(targetSize, Qt::KeepAspectRatio);
    return QRectF(QPointF((targetSize.width() - scaledSize.width()) / 2.0,
                          (targetSize.height() - scaledSize.height()) / 2.0),
                  scaledSize);
}

} // namespace OCC
