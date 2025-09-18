/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
