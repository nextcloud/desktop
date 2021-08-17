#include "iconutils.h"

#include <theme.h>

#include <QFile>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace OCC {
namespace Ui {
namespace IconUtils {
QPixmap pixmapForBackground(const QString &fileName, const QColor &backgroundColor)
{
    Q_ASSERT(!fileName.isEmpty());

    // some icons are present in white or black only, so, we need to check both when needed
    const auto iconBaseColors = QStringList({ QStringLiteral("black"), QStringLiteral("white") });

    const QString pixmapColor = backgroundColor.isValid() && !Theme::isDarkColor(backgroundColor) ? "black" : "white";

    const QString cacheKey = fileName + QLatin1Char(',') + pixmapColor;

    QPixmap cachedPixmap;

    if (!QPixmapCache::find(cacheKey, &cachedPixmap)) {
        if (iconBaseColors.contains(pixmapColor)) {
            cachedPixmap = QPixmap::fromImage(QImage(QString(Theme::themePrefix) + pixmapColor + QLatin1Char('/') + fileName));
            QPixmapCache::insert(cacheKey, cachedPixmap);
            return cachedPixmap;
        }

        const auto drawSvgWithCustomFillColor = [](const QString &sourceSvgPath, const QString &fillColor) {
            QSvgRenderer svgRenderer;

            if (!svgRenderer.load(sourceSvgPath)) {
                return QPixmap();
            }

            // render source image
            QImage svgImage(svgRenderer.defaultSize(), QImage::Format_ARGB32);
            {
                QPainter svgImagePainter(&svgImage);
                svgImage.fill(Qt::GlobalColor::transparent);
                svgRenderer.render(&svgImagePainter);
            }

            // draw target image with custom fillColor
            QImage image(svgRenderer.defaultSize(), QImage::Format_ARGB32);
            image.fill(QColor(fillColor));
            {
                QPainter imagePainter(&image);
                imagePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                imagePainter.drawImage(0, 0, svgImage);
            }

            return QPixmap::fromImage(image);
        };

        // find the first matching svg among base colors, if any
        const QString sourceSvg = [&]() {
            for (const auto &color : iconBaseColors) {
                const QString baseSVG(QString(Theme::themePrefix) + color + QLatin1Char('/') + fileName);

                if (QFile(baseSVG).exists()) {
                    return baseSVG;
                }
            }
            return QString();
        }();

        Q_ASSERT(!sourceSvg.isEmpty());
        if (sourceSvg.isEmpty()) {
            qWarning("Failed to find base svg for %s", qPrintable(cacheKey));
            return {};
        }

        cachedPixmap = drawSvgWithCustomFillColor(sourceSvg, pixmapColor);
        QPixmapCache::insert(cacheKey, cachedPixmap);

        Q_ASSERT(!cachedPixmap.isNull());
        if (cachedPixmap.isNull()) {
            qWarning("Failed to load pixmap for %s", qPrintable(cacheKey));
            return {};
        }
    }

    return cachedPixmap;
}
}
}
}
