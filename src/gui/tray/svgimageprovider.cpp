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

        // idSplit is normally [fileName, color], but themed icons may live in a subfolder
        // (e.g. "ses/ses-darkPlus.svg/D6E4F5"), so treat everything but the last segment as
        // the (possibly nested) file name and only the last segment as the color.
        const auto pixmapColor = idSplit.size() > 1 ? QColor(idSplit.constLast()) : QColorConstants::Svg::black;
        const auto pixmapName = idSplit.size() > 1 ? idSplit.mid(0, idSplit.size() - 1).join(QStringLiteral("/")) : idSplit.at(0);

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
