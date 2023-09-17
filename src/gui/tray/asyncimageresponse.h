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

#pragma once

#include <QImage>
#include <QQuickImageProvider>
#include <QFileIconProvider>

class AsyncImageResponse : public QQuickImageResponse
{
public:
    AsyncImageResponse(const QString &id, const QSize &requestedSize);
    void setImageAndEmitFinished(const QImage &image = {});
    [[nodiscard]] QQuickTextureFactory *textureFactory() const override;

private:
    void processNextImage();

private slots:
    void slotProcessNetworkReply();

    QImage _image;
    QStringList _imagePaths;
    QSize _requestedImageSize;
    QColor _svgRecolor;
    QFileIconProvider _fileIconProvider;
    int _index = 0;
};
