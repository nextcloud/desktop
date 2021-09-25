/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "UnifiedSearchResultImageProvider.h"

#include "UserModel.h"

#include <QImage>

#include <QPainter>
#include <QSvgRenderer>

namespace OCC {
class AsyncImageResponse : public QQuickImageResponse
{
public:
    AsyncImageResponse(const QString &id, const QSize &requestedSize)
    {
        _imagePaths = id.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        _requestedImageSize = requestedSize;

        if (_imagePaths.isEmpty()) {
            emitFinished(QImage());
            return;
        }

        processNextImage();
    }

    void emitFinished(QImage image)
    {
        _image = image;
        emit finished();
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(_image);
    }

private:
    slots 
    
    void processNextImage()
    {
        if (_imagePaths.isEmpty() || _index < 0 || _index >= _imagePaths.size()) {
            // no valid images in the list
            emitFinished(QIcon(QStringLiteral(":/client/theme/black/state-warning.svg")).pixmap(_requestedImageSize).toImage());
            return;
        }

        if (_requestedImageSize.width() == 0 || _requestedImageSize.height() == 0) {
            // invalid size requested
            emitFinished(QImage());
            return;
        }

        if (_imagePaths.at(_index).startsWith(QStringLiteral(":/client"))) {
            // return a local file
            emitFinished(QIcon(_imagePaths.at(_index)).pixmap(_requestedImageSize).toImage());
            return;
        }

        if (auto currentAccount = UserModel::instance()->currentUser()->account()) {
            // fetch the remote resource
            const QUrl iconUrl = QUrl(_imagePaths.at(_index));
            ++_index;
            const auto reply = currentAccount->sendRawRequest(QByteArrayLiteral("GET"), iconUrl);
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                const QByteArray imageData = reply->readAll();
                // server returns "[]" for some some file previews (have no idea why), so, we use another image from the list if available
                if (imageData.isEmpty() || imageData == QByteArrayLiteral("[]")) {
                    processNextImage();
                } else {
                    if (imageData.startsWith(QByteArrayLiteral("<svg"))) {
                        // SVG image needs proper scaling, let's do it with QPainter and QSvgRenderer
                        QSvgRenderer svgRenderer;
                        if (!svgRenderer.load(imageData)) {
                            emitFinished(QImage::fromData(imageData));
                            return;
                        }

                        QImage scaledSvg(_requestedImageSize, QImage::Format_ARGB32);
                        scaledSvg.fill("transparent");

                        QPainter painterForSvg(&scaledSvg);
                        svgRenderer.render(&painterForSvg);
                        emitFinished(scaledSvg);
                    } else {
                        emitFinished(QImage::fromData(imageData));
                    }
                }
            });
        }
    }

private:
    QImage _image;
    QStringList _imagePaths;
    QSize _requestedImageSize;
    int _index = 0;
};

QQuickImageResponse *UnifiedSearchResultImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new AsyncImageResponse(id, requestedSize);
}
}