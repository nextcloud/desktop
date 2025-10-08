/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QFileIconProvider>
#include <QNetworkReply>

class AsyncImageResponse : public QQuickImageResponse
{
public:
    AsyncImageResponse(const QString &id, const QSize &requestedSize);
    void setImageAndEmitFinished(const QImage &image = {});
    [[nodiscard]] QQuickTextureFactory *textureFactory() const override;

private:
    void processNextImage();
    void processNetworkReply(QNetworkReply *reply);

    QImage _image;
    QStringList _imagePaths;
    QSize _requestedImageSize;
    QColor _svgRecolor;
    QFileIconProvider _fileIconProvider;
    int _index = 0;
};
