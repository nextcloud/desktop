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
#include <QDebug>

#include "mirall/csyncthread.h"

namespace Mirall {

/* static variables to hold the credentials */
QString CSyncThread::_user;
QString CSyncThread::_passwd;


 int CSyncThread::checkPermissions( TREE_WALK_FILE* file, void *data )
{
     WalkStats *wStats = static_cast<WalkStats*>(data);

     if( !wStats ) {
         qDebug() << "WalkStats is zero - must not be!";
         return -1;
     }

     wStats->seenFiles++;

     switch(file->instruction) {
     case CSYNC_INSTRUCTION_NONE:

         break;
     case CSYNC_INSTRUCTION_EVAL:
         wStats->eval++;
         break;
     case CSYNC_INSTRUCTION_REMOVE:
         wStats->removed++;
         break;
     case CSYNC_INSTRUCTION_RENAME:
         wStats->renamed++;
         break;
     case CSYNC_INSTRUCTION_NEW:
         wStats->newFiles++;
         break;
     case CSYNC_INSTRUCTION_CONFLICT:
         wStats->conflicts++;
         break;
     case CSYNC_INSTRUCTION_IGNORE:
         wStats->ignores++;
         break;
     case CSYNC_INSTRUCTION_SYNC:
         wStats->sync++;
         break;
     case CSYNC_INSTRUCTION_STAT_ERROR:
     case CSYNC_INSTRUCTION_ERROR:
     /* instructions for the propagator */
     case CSYNC_INSTRUCTION_DELETED:
     case CSYNC_INSTRUCTION_UPDATED:
         wStats->error++;
         break;
     default:
         wStats->error++;
         break;
     }

    // qDebug() << "XXXX " << cur->type << " uid: " << cur->uid;
    qDebug() << wStats->seenFiles << ". Path: " << file->path << ": uid= " << file->uid << " - type: " << file->type;
    return 1;
}

CSyncThread::CSyncThread(const QString &source, const QString &target, bool localCheckOnly)

    : _source(source)
    , _target(target)
    , _localCheckOnly( localCheckOnly )
    , _error(0)

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
    CSYNC *csync;
    WalkStats *wStats = new WalkStats;
    memset(wStats, 0, sizeof(WalkStats));

    _mutex.lock();
    _error = csync_create(&csync,
                          _source.toLocal8Bit().data(),
                          _target.toLocal8Bit().data());
    _mutex.unlock();

    if (error())
        return;
    qDebug() << "## CSync Thread local only: " << _localCheckOnly;
    csync_set_auth_callback( csync, getauth );
    csync_enable_conflictcopys(csync);

    QTime t;
    t.start();

    _mutex.lock();
    if( _localCheckOnly ) {
        _error = csync_set_local_only( csync, true );
        if(error()) {
            _mutex.unlock();
            goto cleanup;
        }
    }
    _mutex.unlock();

    _error = csync_init(csync);
    if (error())
        goto cleanup;

    qDebug() << "############################################################### >>";
    _error = csync_update(csync);
    qDebug() << "<<###############################################################";
    if (error())
        goto cleanup;

    csync_set_userdata(csync, wStats);

    csync_walk_local_tree(csync, &checkPermissions, 0);

    qDebug() << "New     files: " << wStats->newFiles;
    qDebug() << "Updated files: " << wStats->eval;
    qDebug() << "Walked  files: " << wStats->seenFiles;
    emit treeWalkResult(wStats);

    _mutex.lock();
    if( _localCheckOnly ) {
        _mutex.unlock();
        /* check if there are happend changes in the file system */
        if( (wStats->newFiles + wStats->eval + wStats->removed + wStats->renamed) > 0 ) {
             qDebug() << "OO there are local changes!";
        }
        // we have to go out here as its local check only.
        goto cleanup;
    } else {
        _mutex.unlock();
        // check if we can write all over.

        _error = csync_reconcile(csync);
        if (error())
            goto cleanup;

        _error = csync_propagate(csync);
    }
cleanup:
    csync_destroy(csync);
    delete wStats;
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

}
