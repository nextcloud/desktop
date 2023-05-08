/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "rootencryptedfolderinfo.h"

namespace OCC
{
RootEncryptedFolderInfo::RootEncryptedFolderInfo()
{
    *this = RootEncryptedFolderInfo::makeDefault();
}

RootEncryptedFolderInfo::RootEncryptedFolderInfo(const QString &remotePath,
                                                                 const QByteArray &encryptionKey,
                                                                 const QByteArray &decryptionKey,
                                                                 const QSet<QByteArray> &checksums,
                                                                 const quint64 counter)
    : path(remotePath)
    , keyForEncryption(encryptionKey)
    , keyForDecryption(decryptionKey)
    , keyChecksums(checksums)
    , counter(counter)
{
}

RootEncryptedFolderInfo RootEncryptedFolderInfo::makeDefault()
{
    return RootEncryptedFolderInfo{QStringLiteral("/")};
}

QString RootEncryptedFolderInfo::createRootPath(const QString &currentPath, const QString &topLevelPath)
{
    const auto currentPathNoLeadingSlash = currentPath.startsWith(QLatin1Char('/')) ? currentPath.mid(1) : currentPath;
    const auto topLevelPathNoLeadingSlash = topLevelPath.startsWith(QLatin1Char('/')) ? topLevelPath.mid(1) : topLevelPath;

    return currentPathNoLeadingSlash == topLevelPathNoLeadingSlash ? QStringLiteral("/") : topLevelPath;
}

bool RootEncryptedFolderInfo::keysSet() const
{
    return !keyForEncryption.isEmpty() && !keyForDecryption.isEmpty() && !keyChecksums.isEmpty();
}
}
