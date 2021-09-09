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

#ifndef UNIFIEDSEARCHRESULTIMAGEPROVIDER_H
#define UNIFIEDSEARCHRESULTIMAGEPROVIDER_H

#include <QtCore>
#include <QQuickImageProvider>

namespace OCC {

/**
 * @brief The UnifiedSearchResultImageProvider
 * @ingroup gui
 */

class UnifiedSearchResultImageProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    QThreadPool pool;
};
}

#endif // UNIFIEDSEARCHRESULTIMAGEPROVIDER_H
