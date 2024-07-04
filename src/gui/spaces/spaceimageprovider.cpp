/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
#include "spaceimageprovider.h"
#include "libsync/graphapi/spacesmanager.h"
#include "resources/resources.h"

using namespace OCC;
using namespace Spaces;

SpaceImageProvider::SpaceImageProvider(const AccountPtr &account)
    : QQuickImageProvider(QQuickImageProvider::Pixmap, QQuickImageProvider::ForceAsynchronousImageLoading)
    , _account(account)
{
}

QPixmap SpaceImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    QIcon icon;
    if (id == QLatin1String("placeholder")) {
        icon = Resources::getCoreIcon(QStringLiteral("space"));
    } else {
        const auto ids = id.split(QLatin1Char('/'));
        const auto *space = _account->spacesManager()->space(ids.last());
        if (space) {
            icon = space->image()->image();
        }
    }
    // the sourceSize of the Image must be provided
    Q_ASSERT(requestedSize.isValid());
    const QSize actualSize = requestedSize.isValid() ? requestedSize : icon.availableSizes().constFirst();
    if (size) {
        *size = actualSize;
    }
    return icon.pixmap(actualSize);
}
