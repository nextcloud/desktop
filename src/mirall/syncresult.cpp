/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include "mirall/syncresult.h"

namespace Mirall
{

SyncResult::SyncResult()
: _result( Undefined )
{
}

SyncResult::SyncResult(SyncResult::Result result)
    : _result(result)
{
}

SyncResult::Result SyncResult::result() const
{
    return _result;
}

void SyncResult::setErrorString( const QString& err )
{
    _errorMsg = err;
}

QString SyncResult::errorString() const
{
    return _errorMsg;
}

void SyncResult::setSyncChanges(const QHash< QString, QStringList >& changes)
{
    _syncChanges = changes;
}

QHash< QString, QStringList > SyncResult::syncChanges() const
{
    return _syncChanges;
}

SyncResult::~SyncResult()
{
}

} // ns mirall
