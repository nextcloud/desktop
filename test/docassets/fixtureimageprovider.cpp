/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fixtureimageprovider.h"
namespace OCC::DocAssets
{
QQuickImageResponse *FixtureImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // The production provider resolves remote images through registered accounts.
    // Keep those accounts absent and route only our named avatars to a bundled resource.
    if (id == QStringLiteral("https://cloud.example.com/index.php/avatar/alex/64")
        || id == QStringLiteral("https://cloud.example.com/index.php/avatar/jamie/64")) {
        return TrayImageProvider::requestImageResponse(QStringLiteral(":/client/theme/black/user.svg"), requestedSize);
    }
    for (const auto &path : id.split(u';', Qt::SkipEmptyParts)) {
        if (!path.startsWith(QStringLiteral(":/client/")) || path.contains(QStringLiteral(".."))) {
            _rejected.store(true);
            return TrayImageProvider::requestImageResponse({}, requestedSize);
        }
    }
    return TrayImageProvider::requestImageResponse(id, requestedSize);
}
}
