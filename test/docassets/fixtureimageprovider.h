/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "tray/trayimageprovider.h"
#include <atomic>
namespace OCC::DocAssets
{
class FixtureImageProvider : public TrayImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
    bool rejectedRequest() const
    {
        return _rejected.load();
    }

private:
    std::atomic_bool _rejected = false;
};
}
