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

#include "mirall/csyncthread.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"
#include "mirall/logger.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QApplication>

namespace Mirall {

/* static variables to hold the credentials */
QString CSyncThread::_user;
QString CSyncThread::_passwd;
QNetworkProxy CSyncThread::_proxy;

QString CSyncThread::_csyncConfigDir;  // to be able to remove the lock file.

QMutex CSyncThread::_mutex;

void csyncLogCatcher(CSYNC *ctx,
                     int verbosity,
                     const char *function,
                     const char *buffer,
                     void *userdata)
{
  Logger::instance()->csyncLog( QString::fromUtf8(function) + QLatin1String("> ") + QString::fromUtf8(buffer) );
}

walkStats_s::walkStats_s() {
    errorType = 0;

    eval = 0;
    removed = 0;
    renamed = 0;
    newFiles = 0;
    conflicts = 0;
    ignores = 0;
    sync = 0;
    error = 0;

    dirPermErrors = 0;

    seenFiles = 0;

}

CSyncThread::CSyncThread(const QString &source, const QString &target)
    : _source(source)
    , _target(target)

{
    _mutex.lock();
    if( ! _source.endsWith(QLatin1Char('/'))) _source.append(QLatin1Char('/'));
    _mutex.unlock();
}

CSyncThread::~CSyncThread()
{

}

static char* proxyTypeToCStr(QNetworkProxy::ProxyType type)
{
    switch (type) {
    case QNetworkProxy::NoProxy:
        return qstrdup("NoProxy");
    case QNetworkProxy::DefaultProxy:
        return qstrdup("DefaultProxy");
    case QNetworkProxy::Socks5Proxy:
        return qstrdup("Socks5Proxy");
    case  QNetworkProxy::HttpProxy:
        return qstrdup("HttpProxy");
    case  QNetworkProxy::HttpCachingProxy:
        return qstrdup("HttpCachingProxy");
    case  QNetworkProxy::FtpCachingProxy:
        return qstrdup("FtpCachingProxy");
    default:
        return qstrdup("NoProxy");
    }
}

void CSyncThread::startSync()
{
    qDebug() << "starting to sync " << qApp->thread() << QThread::currentThread();
    CSYNC *csync;

    emit(started());

    _mutex.lock();

    if( csync_create(&csync,
                     _source.toUtf8().data(),
                     _target.toUtf8().data()) < 0 ) {
        emit csyncError( tr("CSync create failed.") );
    }
    _csyncConfigDir = QString::fromUtf8( csync_get_config_dir( csync ));
    _mutex.unlock();

    csync_set_auth_callback( csync, getauth );
    csync_set_log_callback( csync, csyncLogCatcher );

    csync_enable_conflictcopys(csync);


    MirallConfigFile cfg;
    QString excludeList = cfg.excludeFile();

    if( !excludeList.isEmpty() ) {
        qDebug() << "==== added CSync exclude List: " << excludeList.toAscii();
        csync_add_exclude_list( csync, excludeList.toAscii() );
    }

    csync_set_config_dir( csync, cfg.configPath().toUtf8() );

    QTime t;
    t.start();

    csync_set_log_verbosity(csync, 10);

    if( csync_init(csync) < 0 ) {
        CSYNC_ERROR_CODE err = csync_get_error( csync );
        QString errStr;

        switch( err ) {
        case CSYNC_ERR_LOCK:
            errStr = tr("CSync failed to create a lock file.");
            break;
        case CSYNC_ERR_STATEDB_LOAD:
            errStr = tr("CSync failed to load the state db.");
            break;
        case CSYNC_ERR_TIMESKEW:
            errStr = tr("The system time on this client is different than the system time on the server. "
                        "Please use a time synchronization service (NTP) on the server and client machines "
                        "so that the times remain the same.");
            break;
        case CSYNC_ERR_FILESYSTEM:
            errStr = tr("CSync could not detect the filesystem type.");
            break;
        case CSYNC_ERR_TREE:
            errStr = tr("CSync got an error while processing internal trees.");
            break;
        case CSYNC_ERR_ACCESS_FAILED:
            errStr = tr("<p>The target directory %1 does not exist.</p><p>Please check the sync setup.</p>").arg(_target);
            // this is critical. The database has to be removed.
            emitStateDb(csync); // to make the name of the csync db known.
            emit wipeDb();
            break;
        case CSYNC_ERR_MODULE:
            errStr = tr("<p>The %1 plugin for csync could not be loaded.<br/>Please verify the installation!</p>").arg(Theme::instance()->appName());
            break;
        case CSYNC_ERR_LOCAL_CREATE:
        case CSYNC_ERR_LOCAL_STAT:
            errStr = tr("The local filesystem can not be written. Please check permissions.");
            break;
        case CSYNC_ERR_REMOTE_CREATE:
        case CSYNC_ERR_REMOTE_STAT:
            errStr = tr("A remote file can not be written. Please check the remote access.");
            break;
        default:
            errStr = tr("An internal error number %1 happend.").arg( (int) err );
        }
        qDebug() << " #### ERROR String emitted: " << errStr;
        emit csyncError(errStr);
        goto cleanup;
    }

    // set module properties, mainly the proxy information.
    // do not use QLatin1String here because that has to be real const char* for C.
    csync_set_module_property(csync, "proxy_type", proxyTypeToCStr( _proxy.type())         );
    csync_set_module_property(csync, "proxy_host", _proxy.hostName().toAscii().data() );
    csync_set_module_property(csync, "proxy_user", _proxy.user().toAscii().data()     );
    csync_set_module_property(csync, "proxy_pwd" , _proxy.password().toAscii().data() );

    emitStateDb(csync);

    qDebug() << "#### Update start #################################################### >>";
    if( csync_update(csync) < 0 ) {
        CSYNC_ERROR_CODE err = csync_get_error( csync );
        QString errStr;

        switch( err ) {
        case CSYNC_ERR_PROXY:
            errStr = tr("CSync failed to reach the host. Either host or proxy settings are not valid.");
            break;
        default:
            errStr = tr("CSync Update failed.");
            break;
        }
        emit csyncError( errStr );
        goto cleanup;
    }
    qDebug() << "<<#### Update end ###########################################################";

    csync_set_userdata(csync, this);

    if( csync_reconcile(csync) < 0 ) {
        emit csyncError(tr("CSync reconcile failed."));
        goto cleanup;
    }
    if( csync_propagate(csync) < 0 ) {
        emit csyncError(tr("File exchange with ownCloud failed. Sync was stopped."));
        goto cleanup;
    }
cleanup:
    csync_destroy(csync);

    /*
     * Attention: do not delete the wStat memory here. it is deleted in the
     * slot catching the signel treeWalkResult because this thread can faster
     * die than the slot has read out the data.
     */
    qDebug() << "CSync run took " << t.elapsed() << " Milliseconds";

    qDebug() << "CSync Waiting a bit to let OS finish up IO";
#ifdef Q_OS_WIN
    Sleep(2000);
#else
    ::sleep(2);
#endif
    qDebug() << "CSync End Waiting";

    emit(finished());
}

void CSyncThread::emitStateDb( CSYNC *csync )
{
    // After csync_init the statedb file name can be emitted
    const char *statedb = csync_get_statedb_file( csync );
    if( statedb ) {
        QString stateDbFile = QString::fromUtf8(statedb);
        free((void*)statedb);

        emit csyncStateDbFile( stateDbFile );
    } else {
        qDebug() << "WRN: Unable to get csync statedb file name";
    }
}

void CSyncThread::setConnectionDetails( const QString &user, const QString &passwd, const QNetworkProxy &proxy )
{
    _mutex.lock();
    _user = user;
    _passwd = passwd;
    _proxy = proxy;
    _mutex.unlock();
}

QString CSyncThread::csyncConfigDir()
{
    return _csyncConfigDir;
}

int CSyncThread::getauth(const char *prompt,
                         char *buf,
                         size_t len,
                         int echo,
                         int verify,
                         void *userdata
                         )
{
    int re = 0;

    QString qPrompt = QString::fromLocal8Bit( prompt ).trimmed();
    _mutex.lock();

    if( qPrompt == QLatin1String("Enter your username:") ) {
        // qDebug() << "OOO Username requested!";
        qstrncpy( buf, _user.toUtf8().constData(), len );
    } else if( qPrompt == QLatin1String("Enter your password:") ) {
        // qDebug() << "OOO Password requested!";
        qstrncpy( buf, _passwd.toUtf8().constData(), len );
    } else {
        if( qPrompt.startsWith( QLatin1String("There are problems with the SSL certificate:"))) {
            // SSL is requested. If the program came here, the SSL check was done by mirall
            // the answer is simply yes here.
            qstrcpy( buf, "yes" );
        } else {
            qDebug() << "Unknown prompt: <" << prompt << ">";
            re = -1;
        }
    }
    _mutex.unlock();
    return re;
}

}
