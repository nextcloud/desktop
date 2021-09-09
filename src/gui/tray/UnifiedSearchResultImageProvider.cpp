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

#include "accountmanager.h"

#include "tray/UserModel.h"

#include <QImage>

namespace OCC {

class AsyncImageResponse : public QQuickImageResponse
{
public:
    AsyncImageResponse(const QString &id, const QSize &requestedSize)
        : _account(UserModel::instance()->currentUser()->account())
    {
        const QUrl iconUrl = QUrl(id);

        if (!iconUrl.isValid() || iconUrl.scheme().isEmpty()) {
            handleDone(QImage());
            return;
        }

        auto reply = _account->sendRawRequest("GET",  iconUrl);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            handleDone(QImage::fromData(reply->readAll()));
        });
    }

    ~AsyncImageResponse() override
    {

    }

    void handleDone(QImage image)
    {
        m_image = image;
        emit finished();
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    AccountPtr _account;
    QImage m_image;
};

QQuickImageResponse *UnifiedSearchResultImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    AsyncImageResponse *response = new AsyncImageResponse(id, requestedSize);
    return response;
}

}
#include "UnifiedSearchResultImageProvider.moc"