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
#include "protocolitem.h"

#include "folderman.h"
#include "progressdispatcher.h"

#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QPointer>


using namespace OCC;


ProtocolItem::ProtocolItem(Folder *folder, const SyncFileItemPtr &item)
    : _path(item->destination())
    , _folder(folder)
    , _size(item->_size)
    , _status(item->_status)
    , _direction(item->_direction)
    , _message(item->_errorString)
    , _sizeIsRelevant(ProgressInfo::isSizeDependent(*item))
{
    if (!item->_responseTimeStamp.isEmpty()) {
        _timestamp = QDateTime::fromString(QString::fromUtf8(item->_responseTimeStamp), Qt::RFC2822Date);
    } else {
        _timestamp = QDateTime::currentDateTimeUtc();
    }
    if (_message.isEmpty()) {
        _message = Progress::asResultString(*item);
    }
}

QString ProtocolItem::path() const
{
    return _path;
}

Folder *ProtocolItem::folder() const
{
    return _folder;
}

QDateTime ProtocolItem::timestamp() const
{
    return _timestamp;
}

qint64 ProtocolItem::size() const
{
    return _size;
}

SyncFileItem::Status ProtocolItem::status() const
{
    return _status;
}

SyncFileItem::Direction ProtocolItem::direction() const
{
    return _direction;
}

QString ProtocolItem::message() const
{
    return _message;
}

bool ProtocolItem::isSizeRelevant() const
{
    return _sizeIsRelevant;
}
