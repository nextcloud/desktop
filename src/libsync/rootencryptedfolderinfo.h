#pragma once
/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QByteArray>
#include <QSet>
#include <QString>
#include <csync.h>
#include <owncloudlib.h>

namespace OCC
{
// required parts from root E2EE folder's metadata for version 2.0+
struct OWNCLOUDSYNC_EXPORT RootEncryptedFolderInfo {
    RootEncryptedFolderInfo();
    explicit RootEncryptedFolderInfo(const QString &remotePath,
                                     const QByteArray &encryptionKey = {},
                                     const QByteArray &decryptionKey = {},
                                     const QSet<QByteArray> &checksums = {},
                                     const quint64 counter = 0);

    static RootEncryptedFolderInfo makeDefault();

    static QString createRootPath(const QString &currentPath, const QString &topLevelPath);

    QString path;
    QByteArray keyForEncryption; // it can be different from keyForDecryption when new metadatKey is generated in root E2EE foler
    QByteArray keyForDecryption; // always storing previous metadataKey to be able to decrypt nested E2EE folders' previous metadata
    QSet<QByteArray> keyChecksums;
    quint64 counter = 0;
    [[nodiscard]] bool keysSet() const;
};

} // namespace OCC
