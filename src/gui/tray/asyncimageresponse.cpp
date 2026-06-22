/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "asyncimageresponse.h"

#include <QIcon>
#include <QMimeDatabase>
#include <QPainter>
#include <QSvgRenderer>

#include "accountmanager.h"

using namespace Qt::StringLiterals;

AsyncImageResponse::AsyncImageResponse(const QString &id, const QSize &requestedSize)
{
    if (id.isEmpty()) {
        setImageAndEmitFinished();
        return;
    }

    auto actualId = id;
    const auto idSplit = id.split('/'_L1, Qt::SkipEmptyParts);
    const auto color = QColor(idSplit.last());

    if(color.isValid()) {
        _svgRecolor = color;
        actualId.remove("/" % idSplit.last());
    }

    _imagePaths = actualId.split(';'_L1, Qt::SkipEmptyParts);
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
    if (imagePath.startsWith(u":/client"_s)) {
        setImageAndEmitFinished(QIcon(imagePath).pixmap(_requestedImageSize).toImage());
        return;
    } else if (imagePath.startsWith(u":/fileicon"_s)) {
        const auto filePath = imagePath.mid(10);
        const auto fileInfo = QFileInfo(filePath);
        setImageAndEmitFinished(_fileIconProvider.icon(fileInfo).pixmap(_requestedImageSize).toImage());
        return;
    }

    OCC::AccountPtr accountInRequestedServer = nullptr;

    const auto accountsList = OCC::AccountManager::instance()->accounts();
    for (const auto &account : accountsList) {
        if (account && account->account() && imagePath.startsWith(account->account()->url().toString())) {
           accountInRequestedServer = account->account();
           break;
        }
    }

    if (accountInRequestedServer) {
        const QUrl iconUrl(_imagePaths.at(_index));
        if (iconUrl.isValid() && !iconUrl.scheme().isEmpty()) {
            // fetch the remote resource in the thread of the account (or rather its QNAM)
            // for some reason trying to use `accountInRequestedServer` causes clang 21 to crash for me :(
            const auto accountQnam = accountInRequestedServer->networkAccessManager();
            QMetaObject::invokeMethod(accountQnam, [this, accountInRequestedServer, iconUrl]() -> void {
                const auto reply = accountInRequestedServer->sendRawRequest("GET"_ba, iconUrl);
                connect(reply, &QNetworkReply::finished, this, [this, reply]() -> void {
                    QMetaObject::invokeMethod(this, [this, reply]() -> void {
                        processNetworkReply(reply);
                    });
                });
            });

            ++_index;
            return;
        }
    }

    setImageAndEmitFinished();
}

void AsyncImageResponse::processNetworkReply(QNetworkReply *reply)
{
    if (!reply) {
        setImageAndEmitFinished();
        return;
    }

    const QByteArray imageData = reply->readAll();
    // server returns "[]" for some some file previews (have no idea why), so, we use another image
    // from the list if available
    if (imageData.isEmpty() || imageData == "[]"_ba) {
        processNextImage();
        return;
    }

    // Some graphics programs export SVGs that begin with `<?xml version="1.0" [...]` and/or include some comments after that (e.g.
    // `<!-- Generator: ...`), so we can't rely on just the response to start with `<svg`.
    if (const auto mimetype = QMimeDatabase().mimeTypeForData(imageData);
        !(mimetype.isValid() && mimetype.inherits("image/svg+xml"_L1))) {
        // Not an SVG: let's let QImage deal with the response.
        setImageAndEmitFinished(QImage::fromData(imageData).scaled(_requestedImageSize));
        return;
    }

    // SVG image needs proper scaling, let's do it with QPainter and QSvgRenderer
    QSvgRenderer svgRenderer;
    if (!svgRenderer.load(imageData)) {
        processNextImage();
        return;
    }

    QImage scaledSvg(_requestedImageSize, QImage::Format_ARGB32);
    scaledSvg.fill("transparent");
    QPainter painterForSvg(&scaledSvg);
    svgRenderer.render(&painterForSvg);

    if (!_svgRecolor.isValid()) {
        setImageAndEmitFinished(scaledSvg);
        return;
    }

    QImage image(_requestedImageSize, QImage::Format_ARGB32);
    image.fill(_svgRecolor);
    QPainter imagePainter(&image);
    imagePainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    imagePainter.drawImage(0, 0, scaledSvg);
    setImageAndEmitFinished(image);
}

