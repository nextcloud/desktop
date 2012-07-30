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

#ifndef MIRALL_SYNCRESULT_H
#define MIRALL_SYNCRESULT_H

#include <QStringList>
#include <QHash>

namespace Mirall
{

class SyncResult
{
public:
    enum Status
    {
      Undefined,
      NotYetStarted,
      SyncRunning,
      Success,
      Error,
      SetupError
    };

    SyncResult();
    SyncResult( Status status );
    ~SyncResult();
    void    setErrorString( const QString& );
    void    setErrorStrings( const QStringList& );
    QString errorString() const;
    QStringList errorStrings() const;
    void    clearErrors();

    void    setSyncChanges( const QHash<QString, QStringList> &changes );
    QHash<QString, QStringList> syncChanges() const;

    void setStatus( Status );
    Status status() const;
    QString statusString() const;

    bool localRunOnly() const;
    void setLocalRunOnly( bool );
private:
    Status _status;
    QHash<QString, QStringList> _syncChanges;

    /**
     * when the sync tool support this...
     */
    QStringList _errors;

    bool        _localRunOnly;
};

}

#endif
