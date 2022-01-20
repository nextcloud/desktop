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

#include <QIcon>
#include <QPainter>
#include <QSvgRenderer>

#include "asyncimageresponse.h"
#include "usermodel.h"

AsyncImageResponse::AsyncImageResponse(const QString &id, const QSize &requestedSize)
{
    if (id.isEmpty()) {
        setImageAndEmitFinished();
        return;
    }

    _imagePaths = id.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    _requestedImageSize = requestedSize;

    if (_imagePaths.isEmpty()) {
        setImageAndEmitFinished();
    } else {
        processNextImage();
    }
}

void AsyncImageResponse::setImageAndEmitFinished(const QImage &image)
{
    _image = image;
    emit finished();
}

QQuickTextureFactory* AsyncImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(_image);
}

void AsyncImageResponse::processNextImage()
{
    if (_index < 0 || _index >= _imagePaths.size()) {
        setImageAndEmitFinished();
        return;
    }

    if (_imagePaths.at(_index).startsWith(QStringLiteral(":/client"))) {
        setImageAndEmitFinished(QIcon(_imagePaths.at(_index)).pixmap(_requestedImageSize).toImage());
        return;
    }

    const auto currentUser = OCC::UserModel::instance()->currentUser();
    if (currentUser && currentUser->account()) {
        const QUrl iconUrl(_imagePaths.at(_index));
        if (iconUrl.isValid() && !iconUrl.scheme().isEmpty()) {
            // fetch the remote resource
            const auto reply = currentUser->account()->sendRawRequest(QByteArrayLiteral("GET"), iconUrl);
            connect(reply, &QNetworkReply::finished, this, &AsyncImageResponse::slotProcessNetworkReply);
            ++_index;
            return;
        }
    }

    setImageAndEmitFinished();
}

void AsyncImageResponse::slotProcessNetworkReply()
{
    const auto reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        setImageAndEmitFinished();
        return;
    }

    const QByteArray imageData = reply->readAll();
    // server returns "[]" for some some file previews (have no idea why), so, we use another image
    // from the list if available
    if (imageData.isEmpty() || imageData == QByteArrayLiteral("[]")) {
        processNextImage();
    } else {
        if (imageData.startsWith(QByteArrayLiteral("<svg"))) {
            // SVG image needs proper scaling, let's do it with QPainter and QSvgRenderer
            QSvgRenderer svgRenderer;
            if (svgRenderer.load(imageData)) {
                QImage scaledSvg(_requestedImageSize, QImage::Format_ARGB32);
                scaledSvg.fill("transparent");
                QPainter painterForSvg(&scaledSvg);
                svgRenderer.render(&painterForSvg);
                setImageAndEmitFinished(scaledSvg);
                return;
            } else {
                processNextImage();
            }
        } else {
            setImageAndEmitFinished(QImage::fromData(imageData));
        }
    }
}

