/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "ipccommon.h"
#include "common/shellextensionutils.h"
#include "common/utility.h"
#include <QDir>

namespace VfsShellExtensions {
QString findServerNameForPath(const QString &filePath)
{
    // SyncRootManager Registry key contains all registered folders for Cf API. It will give us the correct name of the
    // current app based on the folder path
    QString serverName;
    constexpr auto syncRootManagerRegKey = R"(SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SyncRootManager)";

    if (OCC::Utility::registryKeyExists(HKEY_LOCAL_MACHINE, syncRootManagerRegKey)) {
        OCC::Utility::registryWalkSubKeys(
            HKEY_LOCAL_MACHINE, syncRootManagerRegKey, [&](HKEY, const QString &syncRootId) {
                const QString syncRootIdUserSyncRootsRegistryKey =
                    syncRootManagerRegKey + QStringLiteral("\\") + syncRootId + QStringLiteral(R"(\UserSyncRoots\)");
                OCC::Utility::registryWalkValues(HKEY_LOCAL_MACHINE, syncRootIdUserSyncRootsRegistryKey,
                    [&](const QString &userSyncRootName, bool *done) {
                        const auto userSyncRootValue = QDir::fromNativeSeparators(OCC::Utility::registryGetKeyValue(
                            HKEY_LOCAL_MACHINE, syncRootIdUserSyncRootsRegistryKey, userSyncRootName)
                                                                                      .toString());
                        if (QDir::fromNativeSeparators(filePath).startsWith(userSyncRootValue)) {
                            const auto syncRootIdSplit = syncRootId.split(QLatin1Char('!'), Qt::SkipEmptyParts);
                            if (!syncRootIdSplit.isEmpty()) {
                                serverName = VfsShellExtensions::serverNameForApplicationName(syncRootIdSplit.first());
                                *done = true;
                            }
                        }
                    });
            });
    }
    return serverName;
}
}
