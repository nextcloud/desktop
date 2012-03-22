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
: _status( Undefined )
{
}

SyncResult::SyncResult(SyncResult::Status status )
    : _status(status)
{
}

SyncResult::Status SyncResult::status() const
{
    return _status;
}

void SyncResult::setStatus( Status stat )
{
    _status = stat;
}

void SyncResult::setErrorStrings( const QStringList& list )
{
    _errors = list;
}

QStringList SyncResult::errorStrings() const
{
    return _errors;
}

void SyncResult::setErrorString( const QString& err )
{
    _errors.append( err );
}

QString SyncResult::errorString() const
{
    if( _errors.isEmpty() ) return QString();
    return _errors.first();
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
