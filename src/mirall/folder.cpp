/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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
#include "config.h"

#include "mirall/folder.h"
#include "mirall/folderwatcher.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/syncresult.h"
#include "mirall/logger.h"
#include "mirall/owncloudinfo.h"
#include "mirall/credentialstore.h"
#include "mirall/utility.h"

#include <QDebug>
#include <QTimer>
#include <QUrl>
#include <QFileSystemWatcher>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>

namespace Mirall {

void csyncLogCatcher(CSYNC *ctx,
                     int verbosity,
                     const char *function,
                     const char *buffer,
                     void *userdata)
{
  Logger::instance()->csyncLog( QString::fromUtf8(buffer) );
}

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

Folder::Folder(const QString &alias, const QString &path, const QString& secondPath, QObject *parent)
    : QObject(parent)
      , _path(path)
      , _secondPath(secondPath)
      , _alias(alias)
      , _enabled(true)
      , _thread(0)
      , _csync(0)
      , _csyncError(false)
      , _csyncUnavail(false)
      , _csync_ctx(0)
{
    qsrand(QTime::currentTime().msec());

    _watcher = new FolderWatcher(path, this);

    MirallConfigFile cfg;
    _watcher->addIgnoreListFile( cfg.excludeFile(MirallConfigFile::SystemScope) );
    _watcher->addIgnoreListFile( cfg.excludeFile(MirallConfigFile::UserScope) );

    QObject::connect(_watcher, SIGNAL(folderChanged(const QStringList &)),
                     SLOT(slotChanged(const QStringList &)));

    _syncResult.setStatus( SyncResult::NotYetStarted );

    ServerActionNotifier *notifier = new ServerActionNotifier(this);
    connect(notifier, SIGNAL(guiLog(QString,QString)), Logger::instance(), SIGNAL(optionalGuiLog(QString,QString)));
    connect(this, SIGNAL(syncFinished(SyncResult)), notifier, SLOT(slotSyncFinished(SyncResult)));

    // check if the local path exists
    checkLocalPath();
}

bool Folder::init()
{
    QString url = replaceScheme(ownCloudInfo::instance()->webdavUrl() + secondPath());
    QString localpath = path();

    if( csync_create( &_csync_ctx, localpath.toUtf8().data(), url.toUtf8().data() ) < 0 ) {
        qDebug() << "Unable to create csync-context!";
        slotCSyncError(tr("Unable to create csync-context"));
        _csync_ctx = 0;
    } else {
        csync_set_log_callback(   _csync_ctx, csyncLogCatcher );
        csync_set_log_verbosity(_csync_ctx, 11);

        MirallConfigFile cfgFile;
        csync_set_config_dir( _csync_ctx, cfgFile.configPath().toUtf8() );

        csync_enable_conflictcopys(_csync_ctx);
        setIgnoredFiles();
        csync_set_auth_callback( _csync_ctx, getauth );

        if( csync_init( _csync_ctx ) < 0 ) {
            qDebug() << "Could not initialize csync!" << csync_get_error(_csync_ctx) << csync_get_error_string(_csync_ctx);
            slotCSyncError(CSyncThread::csyncErrorToString(csync_get_error(_csync_ctx), csync_get_error_string(_csync_ctx)));
            csync_destroy(_csync_ctx);
            _csync_ctx = 0;
        }
    }
    return _csync_ctx;
}
Folder::~Folder()
{
    if( _thread ) {
        _thread->quit();
        csync_request_abort(_csync_ctx);
        _thread->wait();
    }
    delete _csync;
    // Destroy csync here.
    csync_destroy(_csync_ctx);
}

void Folder::checkLocalPath()
{
    QFileInfo fi(_path);

    if( fi.isDir() && fi.isReadable() ) {
        qDebug() << "Checked local path ok";
    } else {
        if( !fi.exists() ) {
            // try to create the local dir
            QDir d(_path);
            if( d.mkpath(_path) ) {
                qDebug() << "Successfully created the local dir " << _path;
            }
        }
        // Check directory again
        if( !fi.exists() ) {
            _syncResult.setErrorString(tr("Local folder %1 does not exist.").arg(_path));
            _syncResult.setStatus( SyncResult::SetupError );
        } else if( !fi.isDir() ) {
            _syncResult.setErrorString(tr("%1 should be a directory but is not.").arg(_path));
            _syncResult.setStatus( SyncResult::SetupError );
        } else if( !fi.isReadable() ) {
            _syncResult.setErrorString(tr("%1 is not readable.").arg(_path));
            _syncResult.setStatus( SyncResult::SetupError );
        }
    }

    // if all is fine, connect a FileSystemWatcher
    if( _syncResult.status() != SyncResult::SetupError ) {
        _pathWatcher = new QFileSystemWatcher(this);
        _pathWatcher->addPath( _path );
        connect(_pathWatcher, SIGNAL(directoryChanged(QString)),
                SLOT(slotLocalPathChanged(QString)));
    }
}

QString Folder::alias() const
{
    return _alias;
}

QString Folder::path() const
{
    QString p(_path);
    if( ! p.endsWith(QLatin1Char('/')) ) {
        p.append(QLatin1Char('/'));
    }
    return p;
}

bool Folder::isBusy() const
{
    return ( _thread && _thread->isRunning() );
}

QString Folder::secondPath() const
{
    return _secondPath;
}

QString Folder::nativePath() const
{
    return QDir::toNativeSeparators(_path);
}

bool Folder::syncEnabled() const
{
  return _enabled;
}

void Folder::setSyncEnabled( bool doit )
{
  _enabled = doit;
  _watcher->setEventsEnabled( doit );

  qDebug() << "setSyncEnabled - ############################ " << doit;
  if( doit ) {
      // undefined until next sync
      _syncResult.setStatus( SyncResult::NotYetStarted);
      _syncResult.clearErrors();
      evaluateSync( QStringList() );
  } else {
      // disable folder. Done through the _enabled-flag set above
  }
}

void Folder::setSyncState(SyncResult::Status state)
{
    _syncResult.setStatus(state);
}

SyncResult Folder::syncResult() const
{
  return _syncResult;
}

void Folder::evaluateSync(const QStringList &pathList)
{
  if( !_enabled ) {
    qDebug() << "*" << alias() << "sync skipped, disabled!";
    return;
  }

  _syncResult.setStatus( SyncResult::NotYetStarted );
  emit scheduleToSync( alias() );

}

void Folder::slotPollTimerTimeout()
{
    qDebug() << "* Polling" << alias() << "for changes. Ignoring all pending events until now";
    _watcher->clearPendingEvents();
    evaluateSync(QStringList());
}

void Folder::slotChanged(const QStringList &pathList)
{
    qDebug() << "** Changed was notified on " << pathList;
    evaluateSync(pathList);
}

void Folder::slotSyncStarted()
{
    // disable events until syncing is done
    _watcher->setEventsEnabled(false);
}

void Folder::slotSyncFinished(const SyncResult &result)
{
    _watcher->setEventsEnabledDelayed(2000);

    qDebug() << "OO folder slotSyncFinished: result: " << int(result.status());
    emit syncStateChange();
}

void Folder::slotLocalPathChanged( const QString& dir )
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

void Folder::setConfigFile( const QString& file )
{
    _configFile = file;
}

QString Folder::configFile()
{
    return _configFile;
}

void Folder::slotThreadTreeWalkResult(const SyncFileItemVector& items)
{
    _syncResult.setSyncFileItemVector(items);
}

void Folder::slotTerminateSync()
{
    qDebug() << "folder " << alias() << " Terminating!";
    MirallConfigFile cfg;
    QString configDir = cfg.configPath();
    qDebug() << "csync's Config Dir: " << configDir;

    if( _thread && _csync ) {
        csync_request_abort(_csync_ctx);
        _thread->quit();
        _thread->wait();
        _csync->deleteLater();
        delete _thread;
        _csync = 0;
        _thread = 0;
        csync_resume(_csync_ctx);
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

// This removes the csync File database if the sync folder definition is removed
// permanentely. This is needed to provide a clean startup again in case another
// local folder is synced to the same ownCloud.
// See http://bugs.owncloud.org/thebuggenie/owncloud/issues/oc-788
void Folder::wipe()
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
}

void Folder::setIgnoredFiles()
{
    MirallConfigFile cfgFile;
    csync_clear_exclude_list( _csync_ctx );
    QString excludeList = cfgFile.excludeFile( MirallConfigFile::SystemScope );
    if( !excludeList.isEmpty() ) {
        qDebug() << "==== added system ignore list to csync:" << excludeList.toUtf8();
        csync_add_exclude_list( _csync_ctx, excludeList.toUtf8() );
    }
    excludeList = cfgFile.excludeFile( MirallConfigFile::UserScope );
    if( !excludeList.isEmpty() ) {
        qDebug() << "==== added user defined ignore list to csync:" << excludeList.toUtf8();
        csync_add_exclude_list( _csync_ctx, excludeList.toUtf8() );
    }
}

void Folder::setProxy()
{
    if( _csync_ctx ) {
        /* Store proxy */
        QUrl proxyUrl(ownCloudInfo::instance()->webdavUrl());
        QList<QNetworkProxy> proxies = QNetworkProxyFactory::proxyForQuery(proxyUrl);
        // We set at least one in Application
        Q_ASSERT(proxies.count() > 0);
        QNetworkProxy proxy = proxies.first();
        if (proxy.type() == QNetworkProxy::NoProxy) {
            qDebug() << "Passing NO proxy to csync for" << proxyUrl;
        } else {
            qDebug() << "Passing" << proxy.hostName() << "of proxy type " << proxy.type()
                     << " to csync for" << proxyUrl;
        }
        int proxyPort = proxy.port();

        csync_set_module_property(_csync_ctx, "proxy_type", (char*) proxyTypeToCStr(proxy.type()) );
        csync_set_module_property(_csync_ctx, "proxy_host", proxy.hostName().toUtf8().data() );
        csync_set_module_property(_csync_ctx, "proxy_port", &proxyPort );
        csync_set_module_property(_csync_ctx, "proxy_user", proxy.user().toUtf8().data()     );
        csync_set_module_property(_csync_ctx, "proxy_pwd" , proxy.password().toUtf8().data() );
    }
}


const char* Folder::proxyTypeToCStr(QNetworkProxy::ProxyType type)
{
    switch (type) {
    case QNetworkProxy::NoProxy:
        return "NoProxy";
    case QNetworkProxy::DefaultProxy:
        return "DefaultProxy";
    case QNetworkProxy::Socks5Proxy:
        return "Socks5Proxy";
    case QNetworkProxy::HttpProxy:
        return "HttpProxy";
    case QNetworkProxy::HttpCachingProxy:
        return "HttpCachingProxy";
    case QNetworkProxy::FtpCachingProxy:
        return "FtpCachingProxy";
    default:
        return "NoProxy";
    }
}

int Folder::getauth(const char *prompt,
                         char *buf,
                         size_t len,
                         int echo,
                         int verify,
                         void *userdata
                         )
{
    int re = 0;
    QMutex mutex;

    QString qPrompt = QString::fromLatin1( prompt ).trimmed();
    QString user = CredentialStore::instance()->user();
    QString pwd  = CredentialStore::instance()->password();

    if( qPrompt == QLatin1String("Enter your username:") ) {
        // qDebug() << "OOO Username requested!";
        QMutexLocker locker( &mutex );
        qstrncpy( buf, user.toUtf8().constData(), len );
    } else if( qPrompt == QLatin1String("Enter your password:") ) {
        QMutexLocker locker( &mutex );
        // qDebug() << "OOO Password requested!";
        qstrncpy( buf, pwd.toUtf8().constData(), len );
    } else {
        if( qPrompt.startsWith( QLatin1String("There are problems with the SSL certificate:"))) {
            // SSL is requested. If the program came here, the SSL check was done by mirall
            // It needs to be checked if the  chain is still equal to the one which
            // was verified by the user.
            QRegExp regexp("fingerprint: ([\\w\\d:]+)");
            bool certOk = false;

            int pos = 0;

            // This is the set of certificates which QNAM accepted, so we should accept
            // them as well
            QList<QSslCertificate> certs = ownCloudInfo::instance()->certificateChain();

            while (!certOk && (pos = regexp.indexIn(qPrompt, 1+pos)) != -1) {
                QString neon_fingerprint = regexp.cap(1);

                foreach( const QSslCertificate& c, certs ) {
                    QString verified_shasum = Utility::formatFingerprint(c.digest(QCryptographicHash::Sha1).toHex());
                    qDebug() << "SSL Fingerprint from neon: " << neon_fingerprint << " compared to verified: " << verified_shasum;
                    if( verified_shasum == neon_fingerprint ) {
                        certOk = true;
                        break;
                    }
                }
            }
            // certOk = false;     DEBUG setting, keep disabled!
            if( !certOk ) { // Problem!
                qstrcpy( buf, "no" );
                re = -1;
            } else {
                qstrcpy( buf, "yes" ); // Certificate is fine!
            }
        } else {
            qDebug() << "Unknown prompt: <" << prompt << ">";
            re = -1;
        }
    }
    return re;
}

void Folder::startSync(const QStringList &pathList)
{
    Q_UNUSED(pathList)
    if (!_csync_ctx) {
        // no _csync_ctx yet,  initialize it.
        init();

        if (!_csync_ctx) {
            qDebug() << Q_FUNC_INFO << "init failed.";
            // the error should already be set
            QMetaObject::invokeMethod(this, "slotCSyncFinished", Qt::QueuedConnection);
            return;
        }
    }

    if (_thread && _thread->isRunning()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }
    if (_thread)
        _thread->quit();
    delete _csync;
    delete _thread;
    _errors.clear();
    _csyncError = false;
    _csyncUnavail = false;

    _syncResult.clearErrors();
    _syncResult.setStatus( SyncResult::SyncPrepare );
    emit syncStateChange();


    qDebug() << "*** Start syncing";
    _thread = new QThread(this);
    _thread->setPriority(QThread::LowPriority);
    setIgnoredFiles();
    _csync = new CSyncThread( _csync_ctx );
    _csync->setLastAuthCookies(ownCloudInfo::instance()->getLastAuthCookies());
    _csync->moveToThread(_thread);


    qRegisterMetaType<SyncFileItemVector>("SyncFileItemVector");
    qRegisterMetaType<SyncFileItem::Direction>("SyncFileItem::Direction");

    connect( _csync, SIGNAL(treeWalkResult(const SyncFileItemVector&)),
              this, SLOT(slotThreadTreeWalkResult(const SyncFileItemVector&)), Qt::QueuedConnection);

    connect(_csync, SIGNAL(started()),  SLOT(slotCSyncStarted()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(finished()), SLOT(slotCSyncFinished()), Qt::QueuedConnection);
    connect(_csync, SIGNAL(csyncError(QString)), SLOT(slotCSyncError(QString)), Qt::QueuedConnection);
    connect(_csync, SIGNAL(csyncUnavailable()), SLOT(slotCsyncUnavailable()), Qt::QueuedConnection);

    //blocking connection so the message box happens in this thread, but block the csync thread.
    connect(_csync, SIGNAL(aboutToRemoveAllFiles(SyncFileItem::Direction,bool*)),
                    SLOT(slotAboutToRemoveAllFiles(SyncFileItem::Direction,bool*)), Qt::BlockingQueuedConnection);
    connect(_csync, SIGNAL(transmissionProgress(Progress::Info)), this, SLOT(slotTransmissionProgress(Progress::Info)));

    _thread->start();
    QMetaObject::invokeMethod(_csync, "startSync", Qt::QueuedConnection);
    emit syncStarted();
}

void Folder::slotCSyncError(const QString& err)
{
    _errors.append( err );
    _csyncError = true;
}

void Folder::slotCSyncStarted()
{
    qDebug() << "    * csync thread started";
    _syncResult.setStatus(SyncResult::SyncRunning);
    emit syncStateChange();
}

void Folder::slotCsyncUnavailable()
{
    _csyncUnavail = true;
}

void Folder::slotCSyncFinished()
{
    qDebug() << "-> CSync Finished slot with error " << _csyncError;

    if (_csyncError) {
        _syncResult.setStatus(SyncResult::Error);

        qDebug() << "  ** error Strings: " << _errors;
        _syncResult.setErrorStrings( _errors );
        qDebug() << "    * owncloud csync thread finished with error";
    } else if (_csyncUnavail) {
        _syncResult.setStatus(SyncResult::Unavailable);
    } else if( _syncResult.warnCount() > 0 ) {
        // there have been warnings on the way.
        _syncResult.setStatus(SyncResult::Problem);
    } else {
        _syncResult.setStatus(SyncResult::Success);
    }

    if( _thread && _thread->isRunning() ) {
        _thread->quit();
    }
    ownCloudInfo::instance()->getQuotaRequest("/");
    emit syncFinished( _syncResult );
}

void Folder::slotTransmissionProgress(const Progress::Info& progress)
{
    Progress::Info newInfo = progress;
    newInfo.folder = alias();

    if(newInfo.current_file.startsWith(QLatin1String("ownclouds://")) ||
            newInfo.current_file.startsWith(QLatin1String("owncloud://")) ) {
        // rip off the whole ownCloud URL.
        QString regexp = QString("^owncloud[s]*://.*/remote.php/webdav/%1/").arg(secondPath());
        QRegExp re( regexp );
        re.setMinimal(true);
        newInfo.current_file.remove(re);
    }
    QString localPath = path();
    if( newInfo.current_file.startsWith(localPath) ) {
        // remove the local dir.
        newInfo.current_file = newInfo.current_file.right( newInfo.current_file.length() - localPath.length());
    }

    // remember problems happening to set the correct Sync status in slot slotCSyncFinished.
    if( newInfo.kind == Progress::StartSync ) {
        _syncResult.setWarnCount(0);
    }
    if( newInfo.kind == Progress::Error ) {
        _syncResult.setWarnCount( _syncResult.warnCount()+1 );
    }

    ProgressDispatcher::instance()->setProgressInfo(alias(), newInfo);
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
            case CSYNC_INSTRUCTION_ERROR:
                qDebug() << "Got Instruction ERROR. " << result.errorString();
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
            emit guiLog(tr("Files removed"), tr("'%1' and %n other file(s) have been removed.",
                                                      "", removedItems-1).arg(file));
    }
    if (updatedItems > 0) {
        QString file = QDir::toNativeSeparators(firstItemUpdated._file);
        if (updatedItems == 1)
            emit guiLog(tr("File updated"), tr("'%1' has been updated.").arg(file));
        else
            emit guiLog(tr("Files updated"), tr("'%1' and %n other file(s) have been updated.",
                                                      "", updatedItems-1).arg(file));
    }
}

void Folder::slotAboutToRemoveAllFiles(SyncFileItem::Direction direction, bool *cancel)
{
    QString msg = direction == SyncFileItem::Down ?
        tr("This sync would remove all the files in the local sync folder '%1'.\n"
           "If you or your administrator have reset your account on the server, choose "
           "\"Keep files\". If you want your data to be removed, choose \"Remove all files\".") :
        tr("This sync would remove all the files in the sync folder '%1'.\n"
           "This might be because the folder was silently reconfigured, or that all "
           "the file were manually removed.\n"
           "Are you sure you want to perform this operation?");
    QMessageBox msgBox(QMessageBox::Warning, tr("Remove All Files?"),
                       msg.arg(alias()));
    msgBox.addButton(tr("Remove all files"), QMessageBox::DestructiveRole);
    QPushButton* keepBtn = msgBox.addButton(tr("Keep files"), QMessageBox::ActionRole);
    if (msgBox.exec() == -1) {
        *cancel = true;
        return;
    }
    *cancel = msgBox.clickedButton() == keepBtn;
    if (*cancel) {
        wipe();
    }
}


} // namespace Mirall

