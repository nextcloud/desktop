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

#pragma once
#include "gui/spaces/spaceslib.h"

#include "libsync/account.h"

#include <QQuickImageProvider>

namespace OCC::Spaces {
class SPACES_EXPORT SpaceImageProvider : public QQuickImageProvider
{
    Q_OBJECT
public:
    SpaceImageProvider(const AccountPtr &account);
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;


private:
    AccountPtr _account;
};

}
