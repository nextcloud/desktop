/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenshotsyncstatussummary.h"

#include "theme.h"

namespace OCC {

double ScreenshotSyncStatusSummary::syncProgress() const
{
    return 1.0;
}

QUrl ScreenshotSyncStatusSummary::syncIcon() const
{
    return Theme::instance()->syncStatusOk();
}

bool ScreenshotSyncStatusSummary::syncing() const
{
    return false;
}

QString ScreenshotSyncStatusSummary::syncStatusString() const
{
    return tr("All synced!");
}

QString ScreenshotSyncStatusSummary::syncStatusDetailString() const
{
    return {};
}

qint64 ScreenshotSyncStatusSummary::totalFiles() const
{
    return 24;
}

bool ScreenshotSyncStatusSummary::needsSandboxReapproval() const
{
    return false;
}

}
