/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenshotsystray.h"

namespace OCC {

void ScreenshotSystray::presentShareViewInTray(const QString &path)
{
    Q_UNUSED(path)
}

void ScreenshotSystray::presentFileActionsViewInSystray(const QString &path)
{
    Q_UNUSED(path)
}

void ScreenshotSystray::createResolveConflictsDialog(const QVariant &conflicts)
{
    Q_UNUSED(conflicts)
}

void ScreenshotSystray::openSettingsForSandboxReapproval()
{
}

}
