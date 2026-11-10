/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "outputdirectory.h"
#include <QDateTime>
#include <QDir>
#include <QUuid>

namespace OCC::DocAssets
{
QString newExportDirectory(const QString &parent)
{
    if (parent.isEmpty() || !QDir::isAbsolutePath(parent)) {
        return {};
    }
    const auto name = QStringLiteral("Nextcloud-doc-assets-%1-%2")
                          .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss")), QUuid::createUuid().toString(QUuid::WithoutBraces));
    return QDir(parent).filePath(name);
}

QString downloadsExportDirectory()
{
    // This macOS tool exports to the user's Downloads even while Qt test paths isolate its settings.
    return newExportDirectory(QDir::home().filePath(QStringLiteral("Downloads")));
}
}
