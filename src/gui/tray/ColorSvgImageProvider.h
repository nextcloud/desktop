#pragma once

#include <QImage>
#include <QPalette>
#include <QQuickImageProvider>
#include <QQuickAsyncImageProvider>
#include <QThreadPool>

namespace OCC {

class ColorSvgImageProvider : public QQuickImageProvider
{
public:
    ColorSvgImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QPalette _palette;
};

class AsyncColorSvgImageProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    QThreadPool _pool;
};
}
