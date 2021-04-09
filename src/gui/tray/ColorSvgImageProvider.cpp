#include <QPainter>
#include <QLoggingCategory>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QPalette>
#include <QSvgRenderer>
#include <QNetworkReply>

#include "ColorSvgImageProvider.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcColorSvgImageProvider, "nextcloud.gui.tray.colorsvgimageprovider", QtInfoMsg)

ColorSvgImageProvider::ColorSvgImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}


QImage ColorSvgImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // TODO: Clean up
    if (size) {
        *size = requestedSize;
    }
    qCDebug(lcColorSvgImageProvider) << "Load" << id;
    QSvgRenderer svgRenderer;
    if (id.startsWith("data")) {
        const QString prefix("data:image/svg+xml;utf8,");
        Q_ASSERT(id.startsWith(prefix));
        const auto svgData = QUrl::fromPercentEncoding(id.toLocal8Bit()).mid(prefix.size()).toLocal8Bit();
        Q_ASSERT(svgRenderer.load(svgData));
    } else if (id.startsWith("http")) {
        const auto url = QUrl::fromUserInput(id);
        auto networkAccessManager = new QNetworkAccessManager();
        auto reply = networkAccessManager->get(QNetworkRequest(url));

        // TODO: This loop is a little bit ugly but had no better idea yet to do synchronious network request
        // I think doing a syncronious network request is fine because I hope that requestImage() gets executed async.
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::readyRead, [&] {
            qCDebug(lcColorSvgImageProvider) << "Loaded http";
            svgRenderer.load(reply->readAll());
            loop.quit();
        });
        QObject::connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
        loop.exec();
    } else {
        const QString realId = QLatin1Char(':') + id;
        Q_ASSERT(svgRenderer.load(realId));
    }
    QImage svgImage(requestedSize.width(), requestedSize.height(), QImage::Format_ARGB32);
    QPainter svgImagePainter(&svgImage);
    svgImage.fill(Qt::GlobalColor::transparent);
    svgRenderer.render(&svgImagePainter);
    svgImagePainter.end();

    QImage image(requestedSize.width(), requestedSize.height(), QImage::Format_ARGB32);
    image.fill(_palette.windowText().color());
    QPainter imagePainter(&image);
    imagePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    imagePainter.drawImage(0, 0, svgImage);
    imagePainter.end();

    return image;
}
}
