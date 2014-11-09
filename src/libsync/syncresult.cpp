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

#include "syncresult.h"

namespace OCC
{

SyncResult::SyncResult()
    : _status( Undefined ),
      _warnCount(0)
{
}

SyncResult::SyncResult(SyncResult::Status status )
    : _status(status),
      _warnCount(0)
{
}

SyncResult::Status SyncResult::status() const
{
    return _status;
}

QString SyncResult::statusString() const
{
    QString re;
    Status stat = status();

    switch( stat ){
    case Undefined:
        re = QLatin1String("Undefined");
        break;
    case NotYetStarted:
        re = QLatin1String("Not yet Started");
        break;
    case SyncRunning:
        re = QLatin1String("Sync Running");
        break;
    case Success:
        re = QLatin1String("Success");
        break;
    case Error:
        re = QLatin1String("Error");
        break;
    case SetupError:
        re = QLatin1String("SetupError");
        break;
    case SyncPrepare:
        re = QLatin1String("SyncPrepare");
        break;
    case Problem:
        re = QLatin1String("Success, some files were ignored.");
        break;
    case SyncAbortRequested:
        re = QLatin1String("Sync Request aborted by user");
        break;
    case Paused:
        re = QLatin1String("Sync Paused");
        break;
    }
    return re;
}

void SyncResult::setStatus( Status stat )
{
    _status = stat;
    _syncTime = QDateTime::currentDateTime();
}

void SyncResult::setSyncFileItemVector( const SyncFileItemVector& items )
{
    _syncItems = items;
}

SyncFileItemVector SyncResult::syncFileItemVector() const
{
    return _syncItems;
}

QDateTime SyncResult::syncTime() const
{
    return _syncTime;
}

void SyncResult::setWarnCount(int wc)
{
    _warnCount = wc;
}

int SyncResult::warnCount() const
{
    return _warnCount;
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
    if( _errors.isEmpty() ) return QString::null;
    return _errors.first();
}

void SyncResult::clearErrors()
{
    _errors.clear();
}

void SyncResult::setFolder(const QString& folder)
{
    _folder = folder;
}

QString SyncResult::folder() const
{
    return _folder;
}

SyncResult::~SyncResult()
{

}

} // ns mirall
