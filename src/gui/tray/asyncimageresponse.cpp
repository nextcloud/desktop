/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "asyncimageresponse.h"

#include <QIcon>
#include <QPainter>
#include <QSvgRenderer>
#include <QMimeDatabase>
#include <QPixmap>

#include "accountmanager.h"

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
        
        // Check if it's an image file that we can generate a thumbnail for
        QMimeDatabase mimeDb;
        const auto mimeType = mimeDb.mimeTypeForFile(fileInfo);
        
        if (mimeType.name().startsWith("image/") && fileInfo.exists() && fileInfo.isFile()) {
            // Try to load the image directly for thumbnail generation
            QPixmap originalPixmap(filePath);
            if (!originalPixmap.isNull() && !_requestedImageSize.isEmpty()) {
                // Scale the image to the requested size while maintaining aspect ratio
                QPixmap scaledPixmap = originalPixmap.scaled(_requestedImageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                setImageAndEmitFinished(scaledPixmap.toImage());
                return;
            }
        }
        
        // Fall back to file icon if it's not an image or couldn't be loaded
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

