/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SCREENSHOTSYNCSTATUSSUMMARY_H
#define SCREENSHOTSYNCSTATUSSUMMARY_H

#include <QObject>
#include <QUrl>

namespace OCC {

/** @brief Supplies a deterministic all-synced state through the production QML contract. */
class ScreenshotSyncStatusSummary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double syncProgress READ syncProgress CONSTANT)
    Q_PROPERTY(QUrl syncIcon READ syncIcon CONSTANT)
    Q_PROPERTY(bool syncing READ syncing CONSTANT)
    Q_PROPERTY(QString syncStatusString READ syncStatusString CONSTANT)
    Q_PROPERTY(QString syncStatusDetailString READ syncStatusDetailString CONSTANT)
    Q_PROPERTY(qint64 totalFiles READ totalFiles CONSTANT)
    Q_PROPERTY(bool needsSandboxReapproval READ needsSandboxReapproval CONSTANT)

public:
    using QObject::QObject;

    /** @brief Returns complete progress. */
    [[nodiscard]] double syncProgress() const;
    /** @brief Returns the production all-synced icon. */
    [[nodiscard]] QUrl syncIcon() const;
    /** @brief Returns false because the fixture is settled. */
    [[nodiscard]] bool syncing() const;
    /** @brief Returns the deterministic headline. */
    [[nodiscard]] QString syncStatusString() const;
    /** @brief Returns the deterministic detail text. */
    [[nodiscard]] QString syncStatusDetailString() const;
    /** @brief Returns the fictional synchronized-file count. */
    [[nodiscard]] qint64 totalFiles() const;
    /** @brief Returns false because no sandbox repair is needed. */
    [[nodiscard]] bool needsSandboxReapproval() const;

private:
    Q_DISABLE_COPY_MOVE(ScreenshotSyncStatusSummary)
};

}

#endif // SCREENSHOTSYNCSTATUSSUMMARY_H
