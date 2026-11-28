/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDir>
#include <QString>
#include <QStringList>

#include <algorithm>

namespace OCC
{

class NavigationPaneEntryPolicy
{
public:
    /**
     * @brief Whether an entry belongs to an earlier client that syncs one of the current folders.
     *
     * Earlier clients did not save an "ApplicationName" value, so only entries without one qualify.
     * Paths are compared ignoring separator style, trailing separators and case.
     */
    [[nodiscard]] static bool isLegacyEntryOfFolder(const QString &applicationName, const QString &targetFolderPath, const QStringList &folderPaths)
    {
        if (!applicationName.isEmpty() || targetFolderPath.isEmpty()) {
            return false;
        }

        const auto target = normalizedPath(targetFolderPath);
        return std::ranges::any_of(folderPaths, [&target](const auto &folderPath) {
            return normalizedPath(folderPath).compare(target, Qt::CaseInsensitive) == 0;
        });
    }

private:
    [[nodiscard]] static QString normalizedPath(const QString &path)
    {
        return QDir::cleanPath(QDir::fromNativeSeparators(path));
    }
};

} // namespace OCC
