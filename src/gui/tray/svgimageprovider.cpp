/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
