/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTime>

#include <csync.h>

#include "mirall/csyncthread.h"

namespace Mirall {

/* static variables to hold the credentials */
QString CSyncThread::_user;
QString CSyncThread::_passwd;

CSyncThread::CSyncThread(const QString &source, const QString &target, bool localCheckOnly)

    : _source(source)
    , _target(target)
    , _localCheckOnly( localCheckOnly )
    , _error(0)
    , _localChanges(false)
    , _walkedFiles(-1)
{

}

CSyncThread::~CSyncThread()
{

}

bool CSyncThread::error() const
{
    return _error != 0;
}

QMutex CSyncThread::_mutex;

void CSyncThread::run()
{
    QMutexLocker locker(&_mutex);

    CSYNC *csync;

    _error = csync_create(&csync,
                          _source.toLocal8Bit().data(),
                          _target.toLocal8Bit().data());
    if (error())
        return;
    qDebug() << "## CSync Thread local only: " << _localCheckOnly;
    csync_set_auth_callback( csync, getauth );
    csync_enable_conflictcopys(csync);

    QTime t;
    t.start();

    if( _localCheckOnly ) {
        _error = csync_set_local_only( csync, true );
        if(error())
            goto cleanup;
    }

    _error = csync_init(csync);
    if (error())
        goto cleanup;

    qDebug() << "############################################################### >>";
    _error = csync_update(csync);
    qDebug() << "<<###############################################################";
    if (error())
        goto cleanup;



    UPDATE_METRICS met;
    met.filesUpdated = 0;
    met.filesNew  = 0;
    met.filesWalked = 0;

    csync_update_metrics(  csync, &met );

    qDebug() << "New     files: " << met.filesNew;
    qDebug() << "Updated files: " << met.filesUpdated;
    qDebug() << "Walked  files: " << met.filesWalked;

    _walkedFiles = met.filesWalked;

    if( _localCheckOnly ) {
        if( met.filesNew + met.filesUpdated > 0 ) {
            _localChanges = true;

            qDebug() << "OO there are local changes!";
        }
        // we have to go out here as its local check only.
        goto cleanup;
    } else {
        _error = csync_reconcile(csync);
        if (error())
            goto cleanup;

        _error = csync_propagate(csync);
        _localChanges = false;
    }
cleanup:
    csync_destroy(csync);
    qDebug() << "CSync run took " << t.elapsed() << " Milliseconds";
}


void CSyncThread::setUserPwd( const QString& user, const QString& passwd )
{
    _user = user;
    _passwd = passwd;
}

int CSyncThread::getauth(const char *prompt,
                         char *buf,
                         size_t len,
                         int echo,
                         int verify,
                         void *userdata
                         )
{
    QString qPrompt = QString::fromLocal8Bit( prompt ).trimmed();

    if( qPrompt == QString::fromLocal8Bit("Enter your username:") ) {
        qDebug() << "OOO Username requested!";
        strncpy( buf, _user.toLocal8Bit().constData(), len );
    } else if( qPrompt == QString::fromLocal8Bit("Enter your password:") ) {
        qDebug() << "OOO Password requested!";
        strncpy( buf, _passwd.toLocal8Bit().constData(), len );
    } else {
        qDebug() << "Unknown prompt: <" << prompt << ">";
    }
}

int64_t CSyncThread::walkedFiles()
{
    return _walkedFiles;
}

bool CSyncThread::hasLocalChanges( int64_t prevWalked ) const
{
    if( _localChanges ) return true;

    if( _walkedFiles == -1 || prevWalked == -1 ) { // we don't know how
        return false;
    } else {
        if( prevWalked < _walkedFiles ) {
            qDebug() << "Files were added!";
            return true;
        } else if( prevWalked > _walkedFiles ){
            qDebug() << "Files were removed!";
            return true;
        } else {
            return false;
        }
    }
}

}
