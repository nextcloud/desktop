/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "linuxsingleapplication.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace OCC::Utility
{

static QString canonicalConfigPath(const QString &absolutePath)
{
    const auto info = QFileInfo(absolutePath);
    if (const auto canonicalPath = info.canonicalFilePath(); !canonicalPath.isEmpty()) {
        return canonicalPath;
    }

    // The default directory may not exist at startup. Resolve its parents without creating it.
    const auto parentPath = info.absolutePath();
    if (parentPath == absolutePath) {
        return absolutePath;
    }
    return QDir(canonicalConfigPath(parentPath)).filePath(info.fileName());
}

QString linuxSingleApplicationName(const QString &applicationName, const QString &configDirectory)
{
    const auto canonicalPath = canonicalConfigPath(QDir::cleanPath(QDir(configDirectory).absolutePath()));
    const auto identity = applicationName.toUtf8() + '\0' + canonicalPath.toUtf8();
    // Keep the name short because Unix socket paths have a fixed length limit.
    return QString::fromLatin1(QCryptographicHash::hash(identity, QCryptographicHash::Sha256).first(12).toHex());
}

}
