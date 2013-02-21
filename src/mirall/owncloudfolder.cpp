/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.org>
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

#include "mirall/owncloudfolder.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/owncloudinfo.h"
#include "mirall/credentialstore.h"
#include "mirall/logger.h"

#include <csync.h>

#include <QDebug>
#include <QDir>
#include <QUrl>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>

namespace Mirall {


static QString replaceScheme(const QString &urlStr)
{

    QUrl url( urlStr );
    if( url.scheme() == QLatin1String("http") ) {
        url.setScheme( QLatin1String("owncloud") );
    } else {
        // connect SSL!
        url.setScheme( QLatin1String("ownclouds") );
    }
    return url.toString();
}

ownCloudFolder::ownCloudFolder(const QString &alias,
                               const QString &path,
                               const QString &secondPath,
                               QObject *parent)
    : Folder(alias, path, secondPath, parent)
    , _secondPath(secondPath)
    , _thread(0)
    , _csync(0)
    , _csyncError(false)
    , _csyncUnavail(false)
    , _wipeDb(false)
{
    ServerActionNotifier *notifier = new ServerActionNotifier(this);
    connect(notifier, SIGNAL(guiLog(QString,QString)), Logger::instance(), SIGNAL(guiLog(QString,QString)));
    connect(this, SIGNAL(syncFinished(SyncResult)), notifier, SLOT(slotSyncFinished(SyncResult)));
    qDebug() << "****** ownCloud folder using watcher *******";
    // The folder interval is set in the folder parent class.
}

ownCloudFolder::~ownCloudFolder()
{

}

bool ownCloudFolder::isBusy() const
{
    return ( _thread && _thread->isRunning() );
}

QString ownCloudFolder::secondPath() const
{
    QString re(_secondPath);
    MirallConfigFile cfg;
    const QString ocUrl = cfg.ownCloudUrl(QString::null, true);
    // qDebug() << "**** " << ocUrl << " <-> " << re;
    if( re.startsWith( ocUrl ) ) {
        re.remove( ocUrl );
    }

    return re;
}

QString ownCloudFolder::nativeSecondPath() const
{
    // TODO: fold into secondPath() after 1.1.0 release
    QString path = secondPath();
    if (!path.startsWith(QLatin1Char('/')) || path.isEmpty())
        path.prepend(QLatin1Char('/'));
    return path;
}

void ownCloudFolder::startSync()
{
    startSync( QStringList() );
}

void ownCloudFolder::startSync(const QStringList &pathList)
{
    if (_thread && _thread->isRunning()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }

    delete _csync;
    delete _thread;
    _errors.clear();
    _csyncError = false;
    _csyncUnavail = false;
    _wipeDb = false;

    MirallConfigFile cfgFile;

    _syncResult.clearErrors();
    // we now have watchers for everything, so every sync is remote.
    _syncResult.setLocalRunOnly( false );
    _syncResult.setStatus( SyncResult::SyncPrepare );
    emit syncStateChange();

    QString url = replaceScheme(_secondPath);

    qDebug() << "*** Start syncing url to ownCloud: " << url;
    _thread = new QThread(this);
    _csync = new CSyncThread( path(), url);
    _csync->moveToThread(_thread);

    QList<QNetworkProxy> proxies = QNetworkProxyFactory::proxyForQuery(QUrl(cfgFile.ownCloudUrl()));
    // We set at least one in Application
    Q_ASSERT(proxies.count() > 0);
    QNetworkProxy proxy = proxies.first();

    _csync->setConnectionDetails( CredentialStore::instance()->user(),
                                  CredentialStore::instance()->password(),
                                  proxy );
    qRegisterMetaType<SyncFileItemVector>("SyncFileItemVector");
    connect( _csync, SIGNAL(treeWalkResult(const SyncFileItemVector&)),
              this, SLOT(slotThreadTreeWalkResult(const SyncFileItemVector&)), Qt::QueuedConnection);

    connect(_csync, SIGNAL(started()),  SLOT(slotCSyncStarted()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(csyncError(QString)), SLOT(slotCSyncError(QString)), Qt::QueuedConnection);
    connect(_csync, SIGNAL(csyncUnavailable()), SLOT(slotCsyncUnavailable()), Qt::QueuedConnection);
    _thread->start();
    QMetaObject::invokeMethod(_csync, "startSync", Qt::QueuedConnection);
    emit syncStarted();
}

void ownCloudFolder::slotCSyncStarted()
{
    qDebug() << "    * csync thread started";
    _syncResult.setStatus(SyncResult::SyncRunning);
    emit syncStateChange();
}

void ownCloudFolder::slotCSyncError(const QString& err)
{
    _errors.append( err );
    _csyncError = true;
}

void ownCloudFolder::slotCsyncUnavailable()
{
    _csyncUnavail = true;
}

void ownCloudFolder::slotCSyncFinished()
{
    qDebug() << "-> CSync Finished slot with error " << _csyncError;

    if (_csyncError) {
        _syncResult.setStatus(SyncResult::Error);

        qDebug() << "  ** error Strings: " << _errors;
        _syncResult.setErrorStrings( _errors );
        qDebug() << "    * owncloud csync thread finished with error";
        if( _wipeDb ) wipe();
    } else if (_csyncUnavail) {
        _syncResult.setStatus(SyncResult::Unavailable);
    } else {
        _syncResult.setStatus(SyncResult::Success);
    }

    if( _thread && _thread->isRunning() ) {
        _thread->quit();
    }
    emit syncFinished( _syncResult );
}

void ownCloudFolder::slotThreadTreeWalkResult(const SyncFileItemVector& items)
{
    _syncResult.setSyncFileItemVector(items);
}

void ownCloudFolder::slotTerminateSync()
{
    qDebug() << "folder " << alias() << " Terminating!";
    QString configDir = _csync->csyncConfigDir();
    qDebug() << "csync's Config Dir: " << configDir;

    if( _thread ) {
        _thread->terminate();
        _thread->wait();
        _csync->deleteLater();
        delete _thread;
        _csync = 0;
        _thread = 0;
    }

    if( ! configDir.isEmpty() ) {
        QFile file( configDir + QLatin1String("/lock"));
        if( file.exists() ) {
            qDebug() << "After termination, lock file exists and gets removed.";
            file.remove();
        }
    }

    _errors.append( tr("The CSync thread terminated.") );
    _csyncError = true;
    qDebug() << "-> CSync Terminated!";
    slotCSyncFinished();
}

void ownCloudFolder::slotLocalPathChanged( const QString& dir )
{
    QDir notifiedDir(dir);
    QDir localPath( path() );

    if( notifiedDir.absolutePath() == localPath.absolutePath() ) {
        if( !localPath.exists() ) {
            qDebug() << "XXXXXXX The sync folder root was removed!!";
            if( _thread && _thread->isRunning() ) {
                qDebug() << "CSync currently running, set wipe flag!!";
            } else {
                qDebug() << "CSync not running, wipe it now!!";
                wipe();
            }

            qDebug() << "ALARM: The local path was DELETED!";
        }
    }
}

// This removes the csync File database if the sync folder definition is removed
// permanentely. This is needed to provide a clean startup again in case another
// local folder is synced to the same ownCloud.
// See http://bugs.owncloud.org/thebuggenie/owncloud/issues/oc-788
void ownCloudFolder::wipe()
{
    QString stateDbFile = path()+QLatin1String(".csync_journal.db");

    QFile file(stateDbFile);
    if( file.exists() ) {
        if( !file.remove()) {
            qDebug() << "WRN: Failed to remove existing csync StateDB " << stateDbFile;
        } else {
            qDebug() << "wipe: Removed csync StateDB " << stateDbFile;
        }
    } else {
        qDebug() << "WRN: statedb is empty, can not remove.";
    }
    // Check if the tmp database file also exists
    QString ctmpName = path() + QLatin1String(".csync_journal.db.ctmp");
    QFile ctmpFile( ctmpName );
    if( ctmpFile.exists() ) {
        ctmpFile.remove();
    }
    _wipeDb = false;
}

ServerActionNotifier::ServerActionNotifier(QObject *parent)
    : QObject(parent)
{
}

void ServerActionNotifier::slotSyncFinished(const SyncResult &result)
{
    SyncFileItemVector items = result.syncFileItemVector();
    if (items.count() == 0)
        return;

    int newItems = 0;
    int removedItems = 0;
    int updatedItems = 0;
    SyncFileItem firstItemNew;
    SyncFileItem firstItemDeleted;
    SyncFileItem firstItemUpdated;
    foreach (const SyncFileItem &item, items) {
        if (item._dir == SyncFileItem::Down) {
            switch (item._instruction) {
            case CSYNC_INSTRUCTION_NEW:
                newItems++;
                if (firstItemNew.isEmpty())
                    firstItemNew = item;
                break;
            case CSYNC_INSTRUCTION_REMOVE:
                removedItems++;
                if (firstItemDeleted.isEmpty())
                    firstItemDeleted = item;
                break;
            case CSYNC_INSTRUCTION_UPDATED:
                updatedItems++;
                if (firstItemUpdated.isEmpty())
                    firstItemUpdated = item;
		break;
	    default:
		// nothing.
		break;
            }
        }
    }

    if (newItems > 0) {
        QString file = QDir::toNativeSeparators(firstItemNew._file);
        if (newItems == 1)
            emit guiLog(tr("New file available"), tr("'%1' has been synced to this machine.").arg(file));
        else
            emit guiLog(tr("New files available"), tr("'%1' and %n other file(s) have been synced to this machine.",
                                                      "", newItems-1).arg(file));
    }
    if (removedItems > 0) {
        QString file = QDir::toNativeSeparators(firstItemDeleted._file);
        if (removedItems == 1)
            emit guiLog(tr("File removed"), tr("'%1' has been removed.").arg(file));
        else
            emit guiLog(tr("New files available"), tr("'%1' and %n other file(s) have been removed.",
                                                      "", removedItems-1).arg(file));
    }
    if (updatedItems > 0) {
        QString file = QDir::toNativeSeparators(firstItemUpdated._file);
        if (updatedItems == 1)
            emit guiLog(tr("File removed"), tr("'%1' has been updated.").arg(file));
        else
            emit guiLog(tr("New files available"), tr("'%1' and %n other file(s) have been updated.",
                                                      "", updatedItems-1).arg(file));
    }
}

} // ns

