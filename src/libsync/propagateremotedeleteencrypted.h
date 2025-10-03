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

#include "abstractpropagateremotedeleteencrypted.h"

namespace OCC {

class PropagateRemoteDeleteEncrypted : public AbstractPropagateRemoteDeleteEncrypted
{
    Q_OBJECT
public:
    PropagateRemoteDeleteEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent);

    virtual void start() Q_DECL_OVERRIDE;

private:
    void slotFolderUnLockedSuccessfully(const QByteArray &folderId) override;
    void slotFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode) override;
};

}
