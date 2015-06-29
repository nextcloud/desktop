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
#include <QDateTime>

#include "owncloudlib.h"
#include "syncfileitem.h"

namespace OCC
{

/**
 * @brief The SyncResult class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT SyncResult
{
public:
    enum Status
    {
      Undefined,
      NotYetStarted,
      SyncPrepare,
      SyncRunning,
      SyncAbortRequested,
      Success,
      Problem,
      Error,
      SetupError,
      Paused
    };

    SyncResult();
    SyncResult( Status status );
    ~SyncResult();
    void    setErrorString( const QString& );
    void    setErrorStrings( const QStringList& );
    QString errorString() const;
    QStringList errorStrings() const;
    int     warnCount() const;
    void    setWarnCount(int wc);
    void    clearErrors();

    // handle a list of changed items.
    void    setSyncFileItemVector( const SyncFileItemVector& );
    SyncFileItemVector syncFileItemVector() const;

    void setStatus( Status );
    Status status() const;
    QString statusString() const;
    QDateTime syncTime() const;
    void setFolder(const QString& folder);
    QString folder() const;

private:
    Status             _status;
    SyncFileItemVector _syncItems;
    QDateTime          _syncTime;
    QString            _folder;
    /**
     * when the sync tool support this...
     */
    QStringList        _errors;
    int                _warnCount;
};

}

#endif
