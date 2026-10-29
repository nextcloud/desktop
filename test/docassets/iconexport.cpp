/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "iconexport.h"
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
namespace OCC::DocAssets
{
namespace
{
constexpr auto iconSize = 256;
}

QStringList iconFilenames(const QMap<QString, QString> &sources)
{
    QStringList result;
    for (auto it = sources.cbegin(); it != sources.cend(); ++it) {
        result.append(it.key() + QStringLiteral(".png"));
    }
    return result;
}
bool exportIcons(const QString &directory, const QMap<QString, QString> &sources, QString *error)
{
    const QDir output(directory);
    for (auto it = sources.cbegin(); it != sources.cend(); ++it) {
        QSvgRenderer renderer(it.value());
        if (!renderer.isValid()) {
            *error = QStringLiteral("%1: could not load SVG resource %2").arg(it.key(), it.value());
            return false;
        }
        QImage image(iconSize, iconSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            renderer.render(&painter);
        }
        if (!image.save(output.filePath(it.key() + QStringLiteral(".png")), "PNG")) {
            *error = QStringLiteral("%1: could not save PNG").arg(it.key());
            return false;
        }
    }
    return true;
}
}
