#include "sesFileIconProvider.h"
#include "theme.h"
#include "whitelabeltheme.h"

#include <QFileIconProvider>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace {
// Theme::createColorAwareIcon() just inverts the source SVG's raw RGB values for dark
// mode; ses-folderIcon.svg is filled with #2F2F70, which inverts to an undesigned
// washed-out khaki/beige instead of an actual dark-mode color (same issue fixed for the
// wizard's folder/avatar/sync icons in owncloudadvancedsetuppage.cpp). Render once and
// re-tint with a real themed color via SourceIn-compositing instead.
//
// Rendered via QSvgRenderer straight into an image of the target size - same approach
// Theme::createColorAwareIcon() uses - rather than QIcon(path).pixmap(size), which for
// this non-square source SVG (58x52) picks/scales an already-rasterized pixmap and comes
// out the wrong size.
QIcon tintedThemeIcon(const QString &path, const QColor &color, const QSize &size = QSize(64, 64))
{
    QSvgRenderer renderer(path);
    QImage img(size, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter svgPainter(&img);
    renderer.render(&svgPainter);
    svgPainter.end();

    QPixmap tinted(size);
    tinted.fill(Qt::transparent);
    QPainter painter(&tinted);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(0, 0, img);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    painter.end();

    return QIcon(tinted);
}
}

QIcon SesFileIconProvider::icon(const QFileInfo &info) const
{
    QFileIconProvider provider;

    if (info.isDir())
    {
        return tintedThemeIcon(OCC::WLTheme.folderIcon("qtwidget"), QColor(OCC::WLTheme.iconDarkColor()));
    }

    if (info.suffix().isEmpty())
    {
        return QIcon(":/client/theme/ses/ses-file.svg");
    }
    
    
    return provider.icon(info);
};