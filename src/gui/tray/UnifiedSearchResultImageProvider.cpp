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

#include "tray/UserModel.h"

#include <QImage>

namespace OCC {
class AsyncImageResponse : public QQuickImageResponse
{
public:
    AsyncImageResponse(const QString &id, const QSize &requestedSize)
    {
        if (id.isEmpty()) {
            emitFinished(QImage());
            return;
        }

        const QUrl iconUrl = QUrl(id);

        if (!iconUrl.isValid() || iconUrl.scheme().isEmpty()) {
            // return a local file
            emitFinished(QIcon(id).pixmap(requestedSize).toImage());
            return;
        }

        if (auto currentAccount = UserModel::instance()->currentUser()->account()) {
            // fetch remote resource
            auto reply = currentAccount->sendRawRequest("GET", iconUrl);
            connect(reply, &QNetworkReply::finished, this, [this, reply]() {
                emitFinished(QImage::fromData(reply->readAll()));
            });
            return;
        }

        emitFinished(QImage());
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
    QImage _image;
};

QQuickImageResponse *UnifiedSearchResultImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new AsyncImageResponse(id, requestedSize);
}
}