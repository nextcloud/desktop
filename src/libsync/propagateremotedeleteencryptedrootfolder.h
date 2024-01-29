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
