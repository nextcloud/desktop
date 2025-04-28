/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QMap>

#include "basepropagateremotedeleteencrypted.h"
#include "syncfileitem.h"

namespace OCC {

class PropagateRemoteDeleteEncryptedRootFolder : public BasePropagateRemoteDeleteEncrypted
{
    Q_OBJECT
public:
    PropagateRemoteDeleteEncryptedRootFolder(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent);

    void start() override;

private:
    void slotFolderUnLockFinished(const QByteArray &folderId, int statusCode) override;
    void slotFetchMetadataJobFinished(int statusCode, const QString &message) override;
    void slotUpdateMetadataJobFinished(int statusCode, const QString &message) override;
    void slotDeleteNestedRemoteItemFinished();

    void deleteNestedRemoteItem(const QString &filename);
    void decryptAndRemoteDelete();

    QMap<QString, OCC::SyncJournalFileRecord> _nestedItems; // Nested files and folders
};

}
