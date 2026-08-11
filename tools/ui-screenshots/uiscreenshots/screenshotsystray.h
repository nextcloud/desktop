/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SCREENSHOTSYSTRAY_H
#define SCREENSHOTSYSTRAY_H

#include <QObject>
#include <QVariant>

namespace OCC {

/** @brief Provides side-effect-free invokables referenced by production screenshot QML. */
class ScreenshotSystray : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /** @brief No-op file-sharing navigation action. */
    Q_INVOKABLE void presentShareViewInTray(const QString &path);
    /** @brief No-op file-action navigation action. */
    Q_INVOKABLE void presentFileActionsViewInSystray(const QString &path);
    /** @brief No-op conflict-dialog action. */
    Q_INVOKABLE void createResolveConflictsDialog(const QVariant &conflicts);
    /** @brief No-op sandbox-settings action. */
    Q_INVOKABLE void openSettingsForSandboxReapproval();

private:
    Q_DISABLE_COPY_MOVE(ScreenshotSystray)
};

}

#endif // SCREENSHOTSYSTRAY_H
