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

#include "mirall/account.h"
#include "mirall/folder.h"
#include "mirall/folderman.h"
#include "mirall/folderwatcher.h"
#include "mirall/logger.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/networkjobs.h"
#include "mirall/syncjournalfilerecord.h"
#include "mirall/syncresult.h"
#include "mirall/utility.h"

#include "creds/abstractcredentials.h"


extern "C" {

enum csync_exclude_type_e {
  CSYNC_NOT_EXCLUDED   = 0,
  CSYNC_FILE_SILENTLY_EXCLUDED,
  CSYNC_FILE_EXCLUDE_AND_REMOVE,
  CSYNC_FILE_EXCLUDE_LIST,
  CSYNC_FILE_EXCLUDE_INVALID_CHAR
};
typedef enum csync_exclude_type_e CSYNC_EXCLUDE_TYPE;

CSYNC_EXCLUDE_TYPE csync_excluded(CSYNC *ctx, const char *path, int filetype);

}

#include <QDebug>
#include <QTimer>
#include <QUrl>
#include <QFileSystemWatcher>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>

namespace Mirall {

Folder::Folder(const QString &alias, const QString &path, const QString& secondPath, QObject *parent)
    : QObject(parent)
      , _path(path)
      , _remotePath(secondPath)
      , _alias(alias)
      , _enabled(true)
      , _thread(0)
      , _csync(0)
      , _csyncError(false)
      , _csyncUnavail(false)
      , _wipeDb(false)
      , _proxyDirty(true)
      , _journal(path)
      , _csync_ctx(0)
{
    qsrand(QTime::currentTime().msec());
    _timeSinceLastSync.start();

    _watcher = new FolderWatcher(path, this);

    MirallConfigFile cfg;
    _watcher->addIgnoreListFile( cfg.excludeFile(MirallConfigFile::SystemScope) );
    _watcher->addIgnoreListFile( cfg.excludeFile(MirallConfigFile::UserScope) );

    QObject::connect(_watcher, SIGNAL(folderChanged(const QStringList &)),
                     SLOT(slotChanged(const QStringList &)));

    _syncResult.setStatus( SyncResult::NotYetStarted );

    // check if the local path exists
    checkLocalPath();

    int polltime = cfg.remotePollInterval();
    qDebug() << "setting remote poll timer interval to" << polltime << "msec";
    _pollTimer.setInterval( polltime );
    QObject::connect(&_pollTimer, SIGNAL(timeout()), this, SLOT(slotPollTimerTimeout()));
    _pollTimer.start();

    _syncResult.setFolder(alias);
}

bool Folder::init()
{
    QString url = Utility::toCSyncScheme(remoteUrl().toString());
    QString localpath = path();

    if( csync_create( &_csync_ctx, localpath.toUtf8().data(), url.toUtf8().data() ) < 0 ) {
        qDebug() << "Unable to create csync-context!";
        slotCSyncError(tr("Unable to create csync-context"));
        _csync_ctx = 0;
    } else {
        csync_set_log_callback( csyncLogCatcher );
        csync_set_log_level( 11 );

        MirallConfigFile cfgFile;
        csync_set_config_dir( _csync_ctx, cfgFile.configPath().toUtf8() );

        csync_enable_conflictcopys(_csync_ctx);
        setIgnoredFiles();
        if (Account *account = AccountManager::instance()->account()) {
            account->credentials()->syncContextPreInit(_csync_ctx);
        } else {
            qDebug() << Q_FUNC_INFO << "No default Account object, huh?";
        }

        if( csync_init( _csync_ctx ) < 0 ) {
            qDebug() << "Could not initialize csync!" << csync_get_status(_csync_ctx) << csync_get_status_string(_csync_ctx);
            QString errStr = CSyncThread::csyncErrorToString(CSYNC_STATUS(csync_get_status(_csync_ctx)));
            const char *errMsg = csync_get_status_string(_csync_ctx);
            if( errMsg ) {
                errStr += QLatin1String("<br/>");
                errStr += QString::fromUtf8(errMsg);
            }
            slotCSyncError(errStr);
            csync_destroy(_csync_ctx);
            _csync_ctx = 0;
        }
    }
    return _csync_ctx;
}

Folder::~Folder()
{
    if( _thread ) {
        _csync->abort();
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

QString Folder::remotePath() const
{
    return _remotePath;
}

QUrl Folder::remoteUrl() const
{
    Account *account = AccountManager::instance()->account();
    QUrl url = account->davUrl();
    QString path = url.path();
    if (!path.endsWith('/')) {
        path.append('/');
    }
    path.append(_remotePath);
    url.setPath(path);
    qDebug() << url;
    return url;
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

  if( doit ) {
      // qDebug() << "Syncing enabled on folder " << name();
  } else {
      // do not stop or start the watcher here, that is done internally by
      // folder class. Even if the watcher fires, the folder does not
      // schedule itself because it checks the var. _enabled before.
      _pollTimer.stop();
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

void Folder::evaluateSync(const QStringList &/*pathList*/)
{
  if( !_enabled ) {
    qDebug() << "*" << alias() << "sync skipped, disabled!";
    return;
  }

  _syncResult.setStatus( SyncResult::NotYetStarted );
  _syncResult.clearErrors();
  emit scheduleToSync( alias() );

}

void Folder::slotPollTimerTimeout()
{
    qDebug() << "* Polling" << alias() << "for changes. (time since last sync:" << (_timeSinceLastSync.elapsed() / 1000) << "s)";

    if (quint64(_timeSinceLastSync.elapsed()) > MirallConfigFile().forceSyncInterval() ||
            _syncResult.status() != SyncResult::Success ) {
        qDebug() << "** Force Sync now";
        evaluateSync(QStringList());
    } else {
        RequestEtagJob* job = new RequestEtagJob(AccountManager::instance()->account(), remotePath(), this);
        // check if the etag is different
        QObject::connect(job, SIGNAL(etagRetreived(QString)), this, SLOT(etagRetreived(QString)));
        QObject::connect(job, SIGNAL(networkError(QNetworkReply*)), this, SLOT(slotNetworkUnavailable()));
        job->start();
    }
}

void Folder::etagRetreived(const QString& etag)
{
    qDebug() << "* Compare etag  with previous etag: " << (_lastEtag != etag);

    // re-enable sync if it was disabled because network was down
    FolderMan::instance()->setSyncEnabled(true);

    if (_lastEtag != etag) {
        _lastEtag = etag;
        evaluateSync(QStringList());
    }
}

void Folder::slotNetworkUnavailable()
{
    AccountManager::instance()->account()->setState(Account::Disconnected);
    _syncResult.setStatus(SyncResult::Unavailable);
    emit syncStateChange();
}

void Folder::slotChanged(const QStringList &pathList)
{
    qDebug() << "** Changed was notified on " << pathList;
    evaluateSync(pathList);
}

void Folder::bubbleUpSyncResult()
{
    // count new, removed and updated items
    int newItems = 0;
    int removedItems = 0;
    int updatedItems = 0;
    int ignoredItems = 0;
    int renamedItems = 0;

    SyncFileItem firstItemNew;
    SyncFileItem firstItemDeleted;
    SyncFileItem firstItemUpdated;
    SyncFileItem firstItemRenamed;
    Logger *logger = Logger::instance();

    foreach (const SyncFileItem &item, _syncResult.syncFileItemVector() ) {
        if( item._status == SyncFileItem::FatalError || item._status == SyncFileItem::NormalError ) {
            slotCSyncError( tr("File %1: %2").arg(item._file).arg(item._errorString) );
            logger->postOptionalGuiLog(tr("File %1").arg(item._file), item._errorString);

        } else {
            if (item._dir == SyncFileItem::Down) {
                switch (item._instruction) {
                case CSYNC_INSTRUCTION_NEW:
                    newItems++;
                    if (firstItemNew.isEmpty())
                        firstItemNew = item;

                    if (item._type == SyncFileItem::Directory) {
                        _watcher->addPath(path() + item._file);
                    }

                    break;
                case CSYNC_INSTRUCTION_REMOVE:
                    removedItems++;
                    if (firstItemDeleted.isEmpty())
                        firstItemDeleted = item;

                    if (item._type == SyncFileItem::Directory) {
                        _watcher->removePath(path() + item._file);
                    }

                    break;
                case CSYNC_INSTRUCTION_CONFLICT:
                case CSYNC_INSTRUCTION_SYNC:
                    updatedItems++;
                    if (firstItemUpdated.isEmpty())
                        firstItemUpdated = item;
                    break;
                case CSYNC_INSTRUCTION_ERROR:
                    qDebug() << "Got Instruction ERROR. " << _syncResult.errorString();
                    break;
                case CSYNC_INSTRUCTION_RENAME:
                    if (firstItemRenamed.isEmpty()) {
                        firstItemRenamed = item;
                    }
                    renamedItems++;
                    break;
                default:
                    // nothing.
                    break;
                }
            } else if( item._dir == SyncFileItem::None ) { // ignored files counting.
                if( item._instruction == CSYNC_INSTRUCTION_IGNORE ) {
                    ignoredItems++;
                }
            }
        }
    }

    _syncResult.setWarnCount(ignoredItems);


    createGuiLog( firstItemNew._file,     tr("downloaded"), newItems );
    createGuiLog( firstItemDeleted._file, tr("removed"), removedItems );
    createGuiLog( firstItemUpdated._file, tr("updated"), updatedItems );

    if( !firstItemRenamed.isEmpty() ) {
        QString renameVerb = tr("renamed");
        // if the path changes it's rather a move
        QDir renTarget = QFileInfo(firstItemRenamed._renameTarget).dir();
        QDir renSource = QFileInfo(firstItemRenamed._file).dir();
        if(renTarget != renSource) {
            renameVerb = tr("moved");
        }
        createGuiLog( firstItemRenamed._file, tr("%1 to %2").arg(renameVerb).arg(firstItemRenamed._renameTarget), renamedItems );
    }

    qDebug() << "OO folder slotSyncFinished: result: " << int(_syncResult.status());
}

void Folder::createGuiLog( const QString& filename, const QString& verb, int count )
{
    if(count > 0) {
        Logger *logger = Logger::instance();

        QString file = QDir::toNativeSeparators(filename);
        if (count == 1) {
            logger->postOptionalGuiLog(tr("File %1").arg(verb), tr("'%1' has been %2.").arg(file).arg(verb));
        } else {
            logger->postOptionalGuiLog(tr("Files %1").arg(verb),
                                       tr("'%1' and %2 other files have been %3.").arg(file).arg(count-1).arg(verb));
        }
    }
}

int Folder::blackListEntryCount()
{
    return _journal.blackListEntryCount();
}

int Folder::slotWipeBlacklist()
{
    return _journal.wipeBlacklist();
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

void Folder::slotCatchWatcherError(const QString& error)
{
    Logger::instance()->postOptionalGuiLog(tr("Error"), error);
}

void Folder::slotTerminateSync(bool block)
{
    qDebug() << "folder " << alias() << " Terminating!";

    if( _thread && _csync ) {
        _csync->abort();

        // Do not display an error message, user knows his own actions.
        // _errors.append( tr("The CSync thread terminated.") );
        // _csyncError = true;
        if (!block) {
            setSyncState(SyncResult::SyncAbortRequested);
            return;
        }

        _thread->wait();
        _csync->deleteLater();
        delete _thread;
        _thread = 0;
        slotCSyncFinished();
    }
    setSyncEnabled(false);
}

// This removes the csync File database if the sync folder definition is removed
// permanentely. This is needed to provide a clean startup again in case another
// local folder is synced to the same ownCloud.
// See http://bugs.owncloud.org/thebuggenie/owncloud/issues/oc-788
void Folder::wipe()
{
    QString stateDbFile = path()+QLatin1String(".csync_journal.db");

    _journal.close(); // close the sync journal

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

    /* Store proxy */
    QUrl proxyUrl(AccountManager::instance()->account()->url());
    QList<QNetworkProxy> proxies = QNetworkProxyFactory::proxyForQuery(QNetworkProxyQuery(proxyUrl));
    // We set at least one in Application
    Q_ASSERT(proxies.count() > 0);
    QNetworkProxy proxy = proxies.first();
    if (proxy.type() == QNetworkProxy::NoProxy) {
        qDebug() << "Passing NO proxy to csync for" << proxyUrl;
    } else {
        qDebug() << "Passing" << proxy.hostName() << "of proxy type " << proxy.type()
                    << " to csync for" << proxyUrl;
    }
    _proxy_type = proxyTypeToCStr(proxy.type());
    _proxy_host = proxy.hostName().toUtf8();
    _proxy_port = proxy.port();
    _proxy_user = proxy.user().toUtf8();
    _proxy_pwd  = proxy.password().toUtf8();

    setProxyDirty(false);
}

void Folder::setProxyDirty(bool value)
{
    _proxyDirty = value;
}

bool Folder::proxyDirty()
{
    return _proxyDirty;
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
        setProxy();
    } else if (proxyDirty()) {
        setProxy();
    }
    csync_set_module_property(_csync_ctx, "proxy_type", const_cast<char*>(_proxy_type) );
    csync_set_module_property(_csync_ctx, "proxy_host", _proxy_host.data() );
    csync_set_module_property(_csync_ctx, "proxy_port", &_proxy_port );
    csync_set_module_property(_csync_ctx, "proxy_user", _proxy_user.data() );
    csync_set_module_property(_csync_ctx, "proxy_pwd", _proxy_pwd.data() );

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
    setIgnoredFiles();
    _csync = new CSyncThread( _csync_ctx, path(), remoteUrl().path(), &_journal);
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
    connect(_csync, SIGNAL(transmissionProblem(Progress::SyncProblem)), this, SLOT(slotTransmissionProblem(Progress::SyncProblem)));

    _thread->start();
    _thread->setPriority(QThread::LowPriority);

    QMetaObject::invokeMethod(_csync, "startSync", Qt::QueuedConnection);

    // disable events until syncing is done
    _watcher->setEventsEnabled(false);
    _pollTimer.stop();
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
    qDebug() << "-> CSync Finished slot with error " << _csyncError << "warn count" << _syncResult.warnCount();
    _watcher->setEventsEnabledDelayed(2000);
    _pollTimer.start();
    _timeSinceLastSync.restart();

    bubbleUpSyncResult();

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
    emit syncStateChange();
    emit syncFinished( _syncResult );
}

// the problem comes without a folder and the valid path set. Add that here
// and hand the result over to the progress dispatcher.
void Folder::slotTransmissionProblem( const Progress::SyncProblem& problem )
{
    Progress::SyncProblem newProb = problem;
    newProb.folder = alias();

    if(newProb.current_file.startsWith(QLatin1String("ownclouds://")) ||
            newProb.current_file.startsWith(QLatin1String("owncloud://")) ) {
        // rip off the whole ownCloud URL.
        newProb.current_file.remove(Utility::toCSyncScheme(remoteUrl().toString()));
    }
    QString localPath = path();
    if( newProb.current_file.startsWith(localPath) ) {
        // remove the local dir.
        newProb.current_file = newProb.current_file.right( newProb.current_file.length() - localPath.length());
    }

    // Count all error conditions.
    _syncResult.setWarnCount( _syncResult.warnCount()+1 );

    ProgressDispatcher::instance()->setProgressProblem(alias(), newProb);
}

// the progress comes without a folder and the valid path set. Add that here
// and hand the result over to the progress dispatcher.
void Folder::slotTransmissionProgress(const Progress::Info& progress)
{
    Progress::Info newInfo = progress;
    newInfo.folder = alias();

    if(newInfo.current_file.startsWith(QLatin1String("ownclouds://")) ||
            newInfo.current_file.startsWith(QLatin1String("owncloud://")) ) {
        // rip off the whole ownCloud URL.
        newInfo.current_file.remove(Utility::toCSyncScheme(remoteUrl().toString()));
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

    ProgressDispatcher::instance()->setProgressInfo(alias(), newInfo);
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

SyncFileStatus Folder::fileStatus( const QString& fileName )
{
    /*
    STATUS_NONE,
    + STATUS_EVAL,
    STATUS_REMOVE, (invalid for this case because it asks for local files)
    STATUS_RENAME,
    + STATUS_NEW,
    STATUS_CONFLICT,(probably also invalid as we know the conflict only with server involvement)
    + STATUS_IGNORE,
    + STATUS_SYNC,
    + STATUS_STAT_ERROR,
    STATUS_ERROR,
    STATUS_UPDATED
    */

    // FIXME: Find a way for STATUS_ERROR
    SyncFileStatus stat = FILE_STATUS_NONE;

    QString file = path() + fileName;
    QFileInfo fi(file);

    if( !fi.exists() ) {
        stat = FILE_STATUS_STAT_ERROR; // not really possible.
    }

    // file is ignored?
    if( fi.isSymLink() ) {
        stat = FILE_STATUS_IGNORE;
    }
    int type = CSYNC_FTW_TYPE_FILE;
    if( fi.isDir() ) {
        type = CSYNC_FTW_TYPE_DIR;
    }

    if( stat == FILE_STATUS_NONE ) {
        CSYNC_EXCLUDE_TYPE excl = csync_excluded(_csync_ctx, file.toUtf8(), type);

        if( excl != CSYNC_NOT_EXCLUDED ) {
            stat = FILE_STATUS_IGNORE;
        }
    }

    SyncJournalFileRecord rec = _journal.getFileRecord(fileName);
    if( stat == FILE_STATUS_NONE && !rec.isValid() ) {
        stat = FILE_STATUS_NEW;
    }

    // file was locally modified.
    if( stat == FILE_STATUS_NONE && fi.lastModified() != rec._modtime ) {
        stat = FILE_STATUS_EVAL;
    }

    if( stat == FILE_STATUS_NONE ) {
        stat = FILE_STATUS_SYNC;
    }

    return stat;
}

} // namespace Mirall

