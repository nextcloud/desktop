/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "folder.h"

#include "csync/csync.h"
#include "libsync/syncfileitem.h"

#include "common/fixedsizeringbuffer.h"

namespace OCC {

class ProtocolItem
{
    Q_GADGET
public:
    ProtocolItem() = default;
    explicit ProtocolItem(Folder *folder, const SyncFileItemPtr &item);
    QString path() const;

    Folder *folder() const;

    /**
     * UTC Time
     */
    QDateTime timestamp() const;

    qint64 size() const;

    SyncFileItem::Status status() const;

    SyncFileItem::Direction direction() const;

    QString message() const;

    bool isSizeRelevant() const;

private:
    QString _path;
    Folder *_folder;
    QDateTime _timestamp;
    qint64 _size;
    SyncFileItem::Status _status BITFIELD(4);
    SyncFileItem::Direction _direction BITFIELD(3);

    QString _message;
    bool _sizeIsRelevant;

    friend class TestProtocolModel;
};

}
