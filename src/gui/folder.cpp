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

#include "account.h"
#include "folder.h"
#include "folderman.h"
#include "logger.h"
#include "mirallconfigfile.h"
#include "networkjobs.h"
#include "syncjournalfilerecord.h"
#include "syncresult.h"
#include "utility.h"
#include "clientproxy.h"
#include "syncengine.h"
#include "syncrunfilelog.h"

#include "creds/abstractcredentials.h"

#include <QDebug>
#include <QTimer>
#include <QUrl>
#include <QDir>

#include <QMessageBox>
#include <QPushButton>

namespace Mirall {

static void csyncLogCatcher(int /*verbosity*/,
                        const char */*function*/,
                        const char *buffer,
                        void */*userdata*/)
{
    Logger::instance()->csyncLog( QString::fromUtf8(buffer) );
}


Folder::Folder(const QString &alias, const QString &path, const QString& secondPath, QObject *parent)
    : QObject(parent)
      , _path(path)
      , _remotePath(secondPath)
      , _alias(alias)
      , _paused(false)
      , _csyncError(false)
      , _csyncUnavail(false)
      , _wipeDb(false)
      , _proxyDirty(true)
      , _journal(path)
      , _csync_ctx(0)
{
    qsrand(QTime::currentTime().msec());
    _timeSinceLastSync.start();

    MirallConfigFile cfg;

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
    Account *account = AccountManager::instance()->account();
    if (!account) {
        // Normaly this should not happen, but it could be that there is something
        // wrong with the config and it is better not to crash.
        qWarning() << "WRN: No account  configured, can't sync";
        return false;
    }

    // We need to reconstruct the url because the path need to be fully decoded, as csync will  re-encode the path:
    //  Remember that csync will just append the filename to the path and pass it to the vio plugin.
    //  csync_owncloud will then re-encode everything.
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    QUrl url = remoteUrl();
    QString url_string = url.scheme() + QLatin1String("://") + url.authority(QUrl::EncodeDelimiters) + url.path(QUrl::FullyDecoded);
#else
    // Qt4 was broken anyway as it did not encode the '#' as it should have done  (it was actually a provlem when parsing the path from QUrl::setPath
    QString url_string = remoteUrl().toString();
#endif
    url_string = Utility::toCSyncScheme(url_string);

    QString localpath = path();

    if( csync_create( &_csync_ctx, localpath.toUtf8().data(), url_string.toUtf8().data() ) < 0 ) {
        qDebug() << "Unable to create csync-context!";
        slotSyncError(tr("Unable to create csync-context"));
        _csync_ctx = 0;
    } else {
        csync_set_log_callback( csyncLogCatcher );
        csync_set_log_level( 11 );

        if (Account *account = AccountManager::instance()->account()) {
            account->credentials()->syncContextPreInit(_csync_ctx);
        } else {
            qDebug() << Q_FUNC_INFO << "No default Account object, huh?";
        }

        if( csync_init( _csync_ctx ) < 0 ) {
            qDebug() << "Could not initialize csync!" << csync_get_status(_csync_ctx) << csync_get_status_string(_csync_ctx);
            QString errStr = SyncEngine::csyncErrorToString(CSYNC_STATUS(csync_get_status(_csync_ctx)));
            const char *errMsg = csync_get_status_string(_csync_ctx);
            if( errMsg ) {
                errStr += QLatin1String("<br/>");
                errStr += QString::fromUtf8(errMsg);
            }
            slotSyncError(errStr);
            csync_destroy(_csync_ctx);
            _csync_ctx = 0;
        }
    }
    return _csync_ctx;
}

Folder::~Folder()
{
    if( _engine ) {
        _engine->abort();
        _engine.reset(0);
    }
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
    return !_engine.isNull();
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
    return url;
}

QString Folder::nativePath() const
{
    return QDir::toNativeSeparators(_path);
}

bool Folder::syncPaused() const
{
  return _paused;
}

void Folder::setSyncPaused( bool paused )
{
  _paused = paused;

  if( !paused ) {
      // qDebug() << "Syncing enabled on folder " << name();
  } else {
      // do not stop or start the watcher here, that is done internally by
      // folder class. Even if the watcher fires, the folder does not
      // schedule itself because it checks the var. _enabled before.
      _pollTimer.stop();
      setSyncState(SyncResult::Paused);
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

void Folder::prepareToSync()
{
    _syncResult.setStatus( SyncResult::NotYetStarted );
    _syncResult.clearErrors();
}

void Folder::slotPollTimerTimeout()
{
    qDebug() << "* Polling" << alias() << "for changes. (time since last sync:" << (_timeSinceLastSync.elapsed() / 1000) << "s)";

    if (_paused || AccountManager::instance()->account()->state() != Account::Connected) {
        qDebug() << "Not syncing.  :" << _paused << AccountManager::instance()->account()->state();
        return;
    }

    if (quint64(_timeSinceLastSync.elapsed()) > MirallConfigFile().forceSyncInterval() ||
            !(_syncResult.status() == SyncResult::Success ||_syncResult.status() == SyncResult::Problem)) {
        qDebug() << "** Force Sync now, state is " << _syncResult.statusString();
        emit scheduleToSync(alias());
    } else {
        // do the ordinary etag chech for the root folder.
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
        emit scheduleToSync(alias());
    }
}

void Folder::slotNetworkUnavailable()
{
    Account *account = AccountManager::instance()->account();
    if (account && account->state() == Account::Connected) {
        account->setState(Account::Disconnected);
    }
    emit syncStateChange();
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

    SyncRunFileLog syncFileLog;

    syncFileLog.start(path(), _engine ? _engine->stopWatch() : Utility::StopWatch() );

    QElapsedTimer timer;
    timer.start();

    foreach (const SyncFileItem &item, _syncResult.syncFileItemVector() ) {
        // Log the item
        syncFileLog.logItem( item );

        // and process the item to the gui
        if( item._status == SyncFileItem::FatalError || item._status == SyncFileItem::NormalError ) {
            slotSyncError( tr("%1: %2").arg(item._file, item._errorString) );
            logger->postOptionalGuiLog(item._file, item._errorString);
        } else {
            // add new directories or remove gone away dirs to the watcher
            if (item._isDirectory && item._instruction == CSYNC_INSTRUCTION_NEW ) {
                FolderMan::instance()->addMonitorPath( alias(), path()+item._file );
            }
            if (item._isDirectory && item._instruction == CSYNC_INSTRUCTION_REMOVE ) {
                FolderMan::instance()->removeMonitorPath( alias(), path()+item._file );
            }

            if (item._direction == SyncFileItem::Down) {
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
            } else if( item._direction == SyncFileItem::None ) { // ignored files counting.
                if( item._instruction == CSYNC_INSTRUCTION_IGNORE ) {
                    ignoredItems++;
                }
            }
        }
    }
    syncFileLog.close();

    qDebug() << "Processing result list and logging took " << timer.elapsed() << " Milliseconds.";
    _syncResult.setWarnCount(ignoredItems);

    createGuiLog( firstItemNew._file,     SyncFileStatus(SyncFileStatus::STATUS_NEW), newItems );
    createGuiLog( firstItemDeleted._file, SyncFileStatus(SyncFileStatus::STATUS_REMOVE), removedItems );
    createGuiLog( firstItemUpdated._file, SyncFileStatus(SyncFileStatus::STATUS_UPDATED), updatedItems );

    if( !firstItemRenamed.isEmpty() ) {
        SyncFileStatus status(SyncFileStatus::STATUS_RENAME);
        // if the path changes it's rather a move
        QDir renTarget = QFileInfo(firstItemRenamed._renameTarget).dir();
        QDir renSource = QFileInfo(firstItemRenamed._file).dir();
        if(renTarget != renSource) {
            status.set(SyncFileStatus::STATUS_MOVE);
        }
        createGuiLog( firstItemRenamed._file, status, renamedItems, firstItemRenamed._renameTarget );
    }

    qDebug() << "OO folder slotSyncFinished: result: " << int(_syncResult.status());
}

void Folder::createGuiLog( const QString& filename, SyncFileStatus status, int count,
                           const QString& renameTarget )
{
    if(count > 0) {
        Logger *logger = Logger::instance();

        QString file = QDir::toNativeSeparators(filename);
        QString text;

        // not all possible values of status are evaluated here because the others
        // are not used in the calling function. Please check there.
        switch (status.tag()) {
        case SyncFileStatus::STATUS_REMOVE:
            if( count > 1 ) {
                text = tr("%1 and %2 other files have been removed.", "%1 names a file.").arg(file).arg(count-1);
            } else {
                text = tr("%1 has been removed.", "%1 names a file.").arg(file);
            }
            break;
        case SyncFileStatus::STATUS_NEW:
            if( count > 1 ) {
                text = tr("%1 and %2 other files have been downloaded.", "%1 names a file.").arg(file).arg(count-1);
            } else {
                text = tr("%1 has been downloaded.", "%1 names a file.").arg(file);
            }
            break;
        case SyncFileStatus::STATUS_UPDATED:
            if( count > 1 ) {
                text = tr("%1 and %2 other files have been updated.").arg(file).arg(count-1);
            } else {
                text = tr("%1 has been updated.", "%1 names a file.").arg(file);
            }
            break;
        case SyncFileStatus::STATUS_RENAME:
            if( count > 1 ) {
                text = tr("%1 has been renamed to %2 and %3 other files have been renamed.").arg(file).arg(renameTarget).arg(count-1);
            } else {
                text = tr("%1 has been renamed to %2.", "%1 and %2 name files.").arg(file).arg(renameTarget);
            }
            break;
        case SyncFileStatus::STATUS_MOVE:
            if( count > 1 ) {
                text = tr("%1 has been moved to %2 and %3 other files have been moved.").arg(file).arg(renameTarget).arg(count-1);
            } else {
                text = tr("%1 has been moved to %2.").arg(file).arg(renameTarget);
            }
            break;
        default:
            break;
        }

        if( !text.isEmpty() ) {
            logger->postOptionalGuiLog( tr("Sync Activity"), text );
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

    if( _engine ) {
        _engine->abort();

        // Do not display an error message, user knows his own actions.
        // _errors.append( tr("The CSync thread terminated.") );
        // _csyncError = true;
        FolderMan::instance()->slotSetFolderPaused(alias(), true);
        setSyncState(SyncResult::SyncAbortRequested);
        return;
    }
}

// This removes the csync File database
// This is needed to provide a clean startup again in case another
// local folder is synced to the same ownCloud.
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

void Folder::setProxyDirty(bool value)
{
    _proxyDirty = value;
}

bool Folder::proxyDirty()
{
    return _proxyDirty;
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
        _clientProxy.setCSyncProxy(AccountManager::instance()->account()->url(), _csync_ctx);
    } else if (proxyDirty()) {
        _clientProxy.setCSyncProxy(AccountManager::instance()->account()->url(), _csync_ctx);
        setProxyDirty(false);
    }

    if (isBusy()) {
        qCritical() << "* ERROR csync is still running and new sync requested.";
        return;
    }
    _errors.clear();
    _csyncError = false;
    _csyncUnavail = false;

    _syncResult.clearErrors();
    _syncResult.setStatus( SyncResult::SyncPrepare );
    emit syncStateChange();


    qDebug() << "*** Start syncing";
    setIgnoredFiles();
    _engine.reset(new SyncEngine( _csync_ctx, path(), remoteUrl().path(), _remotePath, &_journal));

    qRegisterMetaType<SyncFileItemVector>("SyncFileItemVector");
    qRegisterMetaType<SyncFileItem::Direction>("SyncFileItem::Direction");

    connect( _engine.data(), SIGNAL(treeWalkResult(const SyncFileItemVector&)),
              this, SLOT(slotThreadTreeWalkResult(const SyncFileItemVector&)), Qt::QueuedConnection);

    connect(_engine.data(), SIGNAL(started()),  SLOT(slotSyncStarted()), Qt::QueuedConnection);
    connect(_engine.data(), SIGNAL(finished()), SLOT(slotSyncFinished()), Qt::QueuedConnection);
    connect(_engine.data(), SIGNAL(csyncError(QString)), SLOT(slotSyncError(QString)), Qt::QueuedConnection);
    connect(_engine.data(), SIGNAL(csyncUnavailable()), SLOT(slotCsyncUnavailable()), Qt::QueuedConnection);

    //direct connection so the message box is blocking the sync.
    connect(_engine.data(), SIGNAL(aboutToRemoveAllFiles(SyncFileItem::Direction,bool*)),
                    SLOT(slotAboutToRemoveAllFiles(SyncFileItem::Direction,bool*)));
    connect(_engine.data(), SIGNAL(folderDiscovered(bool,QString)), this, SLOT(slotFolderDiscovered(bool,QString)));
    connect(_engine.data(), SIGNAL(transmissionProgress(Progress::Info)), this, SLOT(slotTransmissionProgress(Progress::Info)));
    connect(_engine.data(), SIGNAL(jobCompleted(SyncFileItem)), this, SLOT(slotJobCompleted(SyncFileItem)));
    connect(_engine.data(), SIGNAL(syncItemDiscovered(SyncFileItem)), this, SLOT(slotSyncItemDiscovered(SyncFileItem)));

    setDirtyNetworkLimits();
    _engine->setSelectiveSyncBlackList(selectiveSyncBlackList());

    QMetaObject::invokeMethod(_engine.data(), "startSync", Qt::QueuedConnection);

    // disable events until syncing is done
    // _watcher->setEventsEnabled(false);
    _pollTimer.stop();
    emit syncStarted();
}

void Folder::setDirtyNetworkLimits()
{
    if (_engine) {

        MirallConfigFile cfg;
        int downloadLimit = 0;
        if (cfg.useDownloadLimit()) {
            downloadLimit = cfg.downloadLimit() * 1000;
        }
        int uploadLimit = -75; // 75%
        int useUpLimit = cfg.useUploadLimit();
        if ( useUpLimit >= 1) {
            uploadLimit = cfg.uploadLimit() * 1000;
        } else if (useUpLimit == 0) {
            uploadLimit = 0;
        }

        _engine->setNetworkLimits(uploadLimit, downloadLimit);
    }
}

void Folder::slotSyncError(const QString& err)
{
    _errors.append( err );
    _csyncError = true;
}

void Folder::slotSyncStarted()
{
    qDebug() << "    * csync thread started";
    _syncResult.setStatus(SyncResult::SyncRunning);
    emit syncStateChange();
}

void Folder::slotCsyncUnavailable()
{
    _csyncUnavail = true;
}

void Folder::slotSyncFinished()
{
    qDebug() << "-> CSync Finished slot with error " << _csyncError << "warn count" << _syncResult.warnCount();

    bubbleUpSyncResult();

    _engine.reset(0);
    // _watcher->setEventsEnabledDelayed(2000);
    _pollTimer.start();
    _timeSinceLastSync.restart();


    if (_csyncError) {
        _syncResult.setStatus(SyncResult::Error);
        qDebug() << "  ** error Strings: " << _errors;
        _syncResult.setErrorStrings( _errors );
        qDebug() << "    * owncloud csync thread finished with error";
    } else if (_csyncUnavail) {
        _syncResult.setStatus(SyncResult::Error);
        qDebug() << "  ** csync not available.";
    } else if( _syncResult.warnCount() > 0 ) {
        // there have been warnings on the way.
        _syncResult.setStatus(SyncResult::Problem);
    } else {
        _syncResult.setStatus(SyncResult::Success);
    }

    emit syncStateChange();

    // The syncFinished result that is to be triggered here makes the folderman
    // clearing the current running sync folder marker.
    // Lets wait a bit to do that because, as long as this marker is not cleared,
    // file system change notifications are ignored for that folder. And it takes
    // some time under certain conditions to make the file system notifications
    // all come in.
    QTimer::singleShot(200, this, SLOT(slotEmitFinishedDelayed() ));

}

void Folder::slotEmitFinishedDelayed()
{
    emit syncFinished( _syncResult );
}


void Folder::slotFolderDiscovered(bool, QString folderName)
{
    Progress::Info pi;
    pi._currentDiscoveredFolder = folderName;
    ProgressDispatcher::instance()->setProgressInfo(alias(), pi);
}


// the progress comes without a folder and the valid path set. Add that here
// and hand the result over to the progress dispatcher.
void Folder::slotTransmissionProgress(const Progress::Info &pi)
{
    if( pi._completedFileCount ) {
        // No job completed yet, this is the beginning of a sync, set the warning level to 0
        _syncResult.setWarnCount(0);
    }
    ProgressDispatcher::instance()->setProgressInfo(alias(), pi);
}

// a job is completed: count the errors and forward to the ProgressDispatcher
void Folder::slotJobCompleted(const SyncFileItem &item)
{
    if (Progress::isWarningKind(item._status)) {
        // Count all error conditions.
        _syncResult.setWarnCount(_syncResult.warnCount()+1);
    }
    emit ProgressDispatcher::instance()->jobCompleted(alias(), item);
}

void Folder::slotSyncItemDiscovered(const SyncFileItem & item)
{
    emit ProgressDispatcher::instance()->syncItemDiscovered(alias(), item);
}


void Folder::slotAboutToRemoveAllFiles(SyncFileItem::Direction, bool *cancel)
{
    QString msg =
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
        // speed up next sync
        _lastEtag = QString();
        QTimer::singleShot(50, this, SLOT(slotPollTimerTimeout()));
    }
}
} // namespace Mirall

