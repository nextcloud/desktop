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

namespace OCC {
class AsyncImageResponse : public QQuickImageResponse
{
public:
    AsyncImageResponse(const QString &id, const QSize &requestedSize)
    {
        _imagePaths = id.split(";", Qt::SkipEmptyParts);
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
        if (_index >= _imagePaths.size()) {
            emitFinished(QImage());
            return;
        }

        const QUrl iconUrl = QUrl(_imagePaths.at(_index));

        if (_imagePaths.at(_index).startsWith(":/client")) {
            // return a local file
            QImage fromLocalFile = QIcon(_imagePaths.at(_index)).pixmap(_requestedImageSize).toImage();
            emitFinished(fromLocalFile);
            return;
        }

        if (auto currentAccount = UserModel::instance()->currentUser()->account()) {
            // fetch remote resource
            ++_index;
            auto reply = currentAccount->sendRawRequest("GET", iconUrl);
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                const QByteArray data = reply->readAll();
                if (data.isEmpty() || data == QByteArrayLiteral("[]")) {
                    processNextImage();
                } else {
                    emitFinished(QImage::fromData(data));
                }
            });
        } else {
            emitFinished(QImage());
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