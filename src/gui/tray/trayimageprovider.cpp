/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "trayimageprovider.h"
#include "asyncimageresponse.h"

namespace OCC {

QQuickImageResponse *TrayImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    return new AsyncImageResponse(id, requestedSize);
}

}
