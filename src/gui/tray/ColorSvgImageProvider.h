#pragma once

#include <qimage.h>
#include <qpalette.h>
#include <qquickimageprovider.h>
namespace OCC {

class ColorSvgImageProvider : public QQuickImageProvider
{
public:
    ColorSvgImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QPalette _palette;
};
}
