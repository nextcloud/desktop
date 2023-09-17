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

    auto actualId = id;
    const auto idSplit = id.split(QStringLiteral("/"), Qt::SkipEmptyParts);
    const auto color = QColor(idSplit.last());

    if(color.isValid()) {
        _svgRecolor = color;
        actualId.remove("/" % idSplit.last());
    }

    _imagePaths = actualId.split(QLatin1Char(';'), Qt::SkipEmptyParts);
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

    const auto imagePath = _imagePaths.at(_index);
    if (imagePath.startsWith(QStringLiteral(":/client"))) {
        setImageAndEmitFinished(QIcon(imagePath).pixmap(_requestedImageSize).toImage());
        return;
    } else if (imagePath.startsWith(QStringLiteral(":/fileicon"))) {
        const auto filePath = imagePath.mid(10);
        const auto fileInfo = QFileInfo(filePath);
        setImageAndEmitFinished(_fileIconProvider.icon(fileInfo).pixmap(_requestedImageSize).toImage());
        return;
    }

    OCC::AccountPtr accountInRequestedServer;

    const auto accountsList = OCC::AccountManager::instance()->accounts();
    for (const auto &account : accountsList) {
        if (account && account->account() && imagePath.startsWith(account->account()->url().toString())) {
           accountInRequestedServer = account->account();
        }
    }

    if (accountInRequestedServer) {
        const QUrl iconUrl(_imagePaths.at(_index));
        if (iconUrl.isValid() && !iconUrl.scheme().isEmpty()) {
            // fetch the remote resource
            const auto reply = accountInRequestedServer->sendRawRequest(QByteArrayLiteral("GET"), iconUrl);
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

                if(!_svgRecolor.isValid()) {
                    setImageAndEmitFinished(scaledSvg);
                    return;
                }

                QImage image(_requestedImageSize, QImage::Format_ARGB32);
                image.fill(_svgRecolor);
                QPainter imagePainter(&image);
                imagePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                imagePainter.drawImage(0, 0, scaledSvg);
                setImageAndEmitFinished(image);
                return;
            } else {
                processNextImage();
            }
        } else {
            setImageAndEmitFinished(QImage::fromData(imageData));
        }
    }
}

