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

#include "syncengine.h"
#include "account.h"
#include "theme.h"
#include "owncloudpropagator.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include "discoveryphase.h"
#include "creds/abstractcredentials.h"
#include "csync_util.h"
#include "syncfilestatus.h"
#include "csync_private.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <climits>
#include <assert.h>

#include <QDebug>
#include <QSslSocket>
#include <QDir>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QUrl>
#include <QSslCertificate>
#include <QProcess>
#include <QElapsedTimer>

namespace OCC {

bool SyncEngine::_syncRunning = false;

SyncEngine::SyncEngine(CSYNC *ctx, const QString& localPath, const QString& remoteURL, const QString& remotePath, OCC::SyncJournalDb* journal)
  : _csync_ctx(ctx)
  , _needsUpdate(false)
  , _localPath(localPath)
  , _remoteUrl(remoteURL)
  , _remotePath(remotePath)
  , _journal(journal)
  , _hasNoneFiles(false)
  , _hasRemoveFile(false)
  , _uploadLimit(0)
  , _downloadLimit(0)
  , _anotherSyncNeeded(false)
{
    qRegisterMetaType<SyncFileItem>("SyncFileItem");
    qRegisterMetaType<SyncFileItem::Status>("SyncFileItem::Status");
    qRegisterMetaType<Progress::Info>("Progress::Info");

    _thread.setObjectName("CSync_Neon_Thread");
    _thread.start();
}

SyncEngine::~SyncEngine()
{
    _thread.quit();
    _thread.wait();
}

//Convert an error code from csync to a user readable string.
// Keep that function thread safe as it can be called from the sync thread or the main thread
QString SyncEngine::csyncErrorToString(CSYNC_STATUS err)
{
    QString errStr;

    switch( err ) {
    case CSYNC_STATUS_OK:
        errStr = tr("Success.");
        break;
    case CSYNC_STATUS_NO_LOCK:
        errStr = tr("CSync failed to create a lock file.");
        break;
    case CSYNC_STATUS_STATEDB_LOAD_ERROR:
        errStr = tr("CSync failed to load or create the journal file. "
                    "Make sure you have read and write permissions in the local sync directory.");
        break;
    case CSYNC_STATUS_STATEDB_WRITE_ERROR:
        errStr = tr("CSync failed to write the journal file.");
        break;
    case CSYNC_STATUS_NO_MODULE:
        errStr = tr("<p>The %1 plugin for csync could not be loaded.<br/>Please verify the installation!</p>").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_STATUS_TIMESKEW:
        errStr = tr("The system time on this client is different than the system time on the server. "
                    "Please use a time synchronization service (NTP) on the server and client machines "
                    "so that the times remain the same.");
        break;
    case CSYNC_STATUS_FILESYSTEM_UNKNOWN:
        errStr = tr("CSync could not detect the filesystem type.");
        break;
    case CSYNC_STATUS_TREE_ERROR:
        errStr = tr("CSync got an error while processing internal trees.");
        break;
    case CSYNC_STATUS_MEMORY_ERROR:
        errStr = tr("CSync failed to reserve memory.");
        break;
    case CSYNC_STATUS_PARAM_ERROR:
        errStr = tr("CSync fatal parameter error.");
        break;
    case CSYNC_STATUS_UPDATE_ERROR:
        errStr = tr("CSync processing step update failed.");
        break;
    case CSYNC_STATUS_RECONCILE_ERROR:
        errStr = tr("CSync processing step reconcile failed.");
        break;
    case CSYNC_STATUS_PROPAGATE_ERROR:
        errStr = tr("CSync processing step propagate failed.");
        break;
    case CSYNC_STATUS_REMOTE_ACCESS_ERROR:
        errStr = tr("<p>The target directory does not exist.</p><p>Please check the sync setup.</p>");
        break;
    case CSYNC_STATUS_REMOTE_CREATE_ERROR:
    case CSYNC_STATUS_REMOTE_STAT_ERROR:
        errStr = tr("A remote file can not be written. Please check the remote access.");
        break;
    case CSYNC_STATUS_LOCAL_CREATE_ERROR:
    case CSYNC_STATUS_LOCAL_STAT_ERROR:
        errStr = tr("The local filesystem can not be written. Please check permissions.");
        break;
    case CSYNC_STATUS_PROXY_ERROR:
        errStr = tr("CSync failed to connect through a proxy.");
        break;
    case CSYNC_STATUS_PROXY_AUTH_ERROR:
        errStr = tr("CSync could not authenticate at the proxy.");
        break;
    case CSYNC_STATUS_LOOKUP_ERROR:
        errStr = tr("CSync failed to lookup proxy or server.");
        break;
    case CSYNC_STATUS_SERVER_AUTH_ERROR:
        errStr = tr("CSync failed to authenticate at the %1 server.").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_STATUS_CONNECT_ERROR:
        errStr = tr("CSync failed to connect to the network.");
        break;
    case CSYNC_STATUS_TIMEOUT:
        errStr = tr("A network connection timeout happened.");
        break;
    case CSYNC_STATUS_HTTP_ERROR:
        errStr = tr("A HTTP transmission error happened.");
        break;
    case CSYNC_STATUS_PERMISSION_DENIED:
        errStr = tr("CSync failed due to not handled permission deniend.");
        break;
    case CSYNC_STATUS_NOT_FOUND:
        errStr = tr("CSync failed to access "); // filename gets added.
        break;
    case CSYNC_STATUS_FILE_EXISTS:
        errStr = tr("CSync tried to create a directory that already exists.");
        break;
    case CSYNC_STATUS_OUT_OF_SPACE:
        errStr = tr("CSync: No space on %1 server available.").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_STATUS_QUOTA_EXCEEDED:
        errStr = tr("CSync: No space on %1 server available.").arg(Theme::instance()->appNameGUI());
        break;
    case CSYNC_STATUS_UNSUCCESSFUL:
        errStr = tr("CSync unspecified error.");
        break;
    case CSYNC_STATUS_ABORTED:
        errStr = tr("Aborted by the user");
        break;
    case CSYNC_STATUS_SERVICE_UNAVAILABLE:
	errStr = tr("The mounted directory is temporarily not available on the server");
	break;
    default:
        errStr = tr("An internal error number %1 happened.").arg( (int) err );
    }

    return errStr;

}

bool SyncEngine::checkBlacklisting( SyncFileItem *item )
{
    if( !_journal ) {
        qWarning() << "Journal is undefined!";
        return false;
    }

    SyncJournalBlacklistRecord entry = _journal->blacklistEntry(item->_file);
    item->_hasBlacklistEntry = false;

    if( !entry.isValid() ) {
        return false;
    }

    item->_hasBlacklistEntry = true;

    // If duration has expired, it's not blacklisted anymore
    time_t now = Utility::qDateTimeToTime_t(QDateTime::currentDateTime());
    if( now > entry._lastTryTime + entry._ignoreDuration ) {
        qDebug() << "blacklist entry for " << item->_file << " has expired!";
        return false;
    }

    // If the file has changed locally or on the server, the blacklist
    // entry no longer applies
    if( item->_direction == SyncFileItem::Up ) { // check the modtime
        if(item->_modtime == 0 || entry._lastTryModtime == 0) {
            return false;
        } else if( item->_modtime != entry._lastTryModtime ) {
            qDebug() << item->_file << " is blacklisted, but has changed mtime!";
            return false;
        }
    } else if( item->_direction == SyncFileItem::Down ) {
        // download, check the etag.
        if( item->_etag.isEmpty() || entry._lastTryEtag.isEmpty() ) {
            qDebug() << item->_file << "one ETag is empty, no blacklisting";
            return false;
        } else if( item->_etag != entry._lastTryEtag ) {
            qDebug() << item->_file << " is blacklisted, but has changed etag!";
            return false;
        }
    }

    qDebug() << "Item is on blacklist: " << entry._file
             << "retries:" << entry._retryCount
             << "for another" << (entry._lastTryTime + entry._ignoreDuration - now) << "s";
    item->_instruction = CSYNC_INSTRUCTION_ERROR;
    item->_status = SyncFileItem::FileIgnored;
    item->_errorString = tr("The item is not synced because of previous errors: %1").arg(entry._errorString);

    return true;
}

void SyncEngine::deleteStaleDownloadInfos()
{
    // Find all downloadinfo paths that we want to preserve.
    QSet<QString> download_file_paths;
    foreach(const SyncFileItem& it, _syncedItems) {
        if (it._direction == SyncFileItem::Down
                && it._type == SyncFileItem::File)
        {
            download_file_paths.insert(it._file);
        }
    }

    // Delete from journal and from filesystem.
    const QVector<SyncJournalDb::DownloadInfo> deleted_infos =
            _journal->getAndDeleteStaleDownloadInfos(download_file_paths);
    foreach (const SyncJournalDb::DownloadInfo & deleted_info, deleted_infos) {
        const QString tmppath = _propagator->getFilePath(deleted_info._tmpfile);
        qDebug() << "Deleting stale temporary file: " << tmppath;
        QFile::remove(tmppath);
    }
}

void SyncEngine::deleteStaleUploadInfos()
{
    // Find all blacklisted paths that we want to preserve.
    QSet<QString> upload_file_paths;
    foreach(const SyncFileItem& it, _syncedItems) {
        if (it._direction == SyncFileItem::Up
                && it._type == SyncFileItem::File)
        {
            upload_file_paths.insert(it._file);
        }
    }

    // Delete from journal.
    _journal->deleteStaleUploadInfos(upload_file_paths);
}

void SyncEngine::deleteStaleBlacklistEntries()
{
    // Find all blacklisted paths that we want to preserve.
    QSet<QString> blacklist_file_paths;
    foreach(const SyncFileItem& it, _syncedItems) {
        if (it._hasBlacklistEntry)
            blacklist_file_paths.insert(it._file);
    }

    // Delete from journal.
    _journal->deleteStaleBlacklistEntries(blacklist_file_paths);
}

int SyncEngine::treewalkLocal( TREE_WALK_FILE* file, void *data )
{
    return static_cast<SyncEngine*>(data)->treewalkFile( file, false );
}

int SyncEngine::treewalkRemote( TREE_WALK_FILE* file, void *data )
{
    return static_cast<SyncEngine*>(data)->treewalkFile( file, true );
}

int SyncEngine::treewalkFile( TREE_WALK_FILE *file, bool remote )
{
    if( ! file ) return -1;

    QString fileUtf8 = QString::fromUtf8( file->path );

    // Gets a default-contructed SyncFileItem or the one from the first walk (=local walk)
    SyncFileItem item = _syncItemMap.value(fileUtf8);
    item._file = fileUtf8;
    item._originalFile = item._file;

    if (item._instruction == CSYNC_INSTRUCTION_NONE
            || (item._instruction == CSYNC_INSTRUCTION_IGNORE && file->instruction != CSYNC_INSTRUCTION_NONE)) {
        item._instruction = file->instruction;
        item._modtime = file->modtime;
    } else {
        if (file->instruction != CSYNC_INSTRUCTION_NONE) {
            qDebug() << "ERROR: Instruction" << item._instruction << "vs" << file->instruction << "for" << fileUtf8;
            Q_ASSERT(!"Instructions are both unequal NONE");
            return -1;
        }
    }

    if (file->file_id && strlen(file->file_id) > 0) {
        item._fileId = file->file_id;
    }
    if (file->directDownloadUrl) {
        item._directDownloadUrl = QString::fromUtf8( file->directDownloadUrl );
    }
    if (file->directDownloadCookies) {
        item._directDownloadCookies = QString::fromUtf8( file->directDownloadCookies );
    }
    if (file->remotePerm && file->remotePerm[0]) {
        item._remotePerm = QByteArray(file->remotePerm);
    }
    item._should_update_etag = item._should_update_etag || file->should_update_etag;

    // record the seen files to be able to clean the journal later
    _seenFiles.insert(item._file);

    if (remote && file->remotePerm && file->remotePerm[0]) {
        _remotePerms[item._file] = file->remotePerm;
    }

    switch(file->error_status) {
    case CSYNC_STATUS_OK:
        break;
    case CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK:
        item._errorString = tr("Symbolic links are not supported in syncing.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_IS_HARDLINK:
        item._errorString = tr("Hard links are not supported in syncing.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST:
        item._errorString = tr("File is listed on the ignore list.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS:
        item._errorString = tr("File contains invalid characters that can not be synced cross platform.");
        break;
    case CYSNC_STATUS_FILE_LOCKED_OR_OPEN:
        item._errorString = QLatin1String("File locked"); // don't translate, internal use!
        break;
    case CSYNC_STATUS_SERVICE_UNAVAILABLE:
        item._errorString = QLatin1String("Directory temporarily not available on server.");
        item._status = SyncFileItem::SoftError;
        break;
    default:
        Q_ASSERT("Non handled error-status");
        /* No error string */
    }

    item._isDirectory = file->type == CSYNC_FTW_TYPE_DIR;
    if(item._isDirectory) {
        item._affectedItems = 0; // defaults to 1 for normal items.
    }

    // The etag is already set in the previous sync phases somewhere. Maybe we should remove it there
    // and do it here so we have a consistent state about which tree stores information from which source.
    item._etag = file->etag;
    item._size = file->size;

    if (!remote) {
        item._inode = file->inode;
    }

    switch( file->type ) {
    case CSYNC_FTW_TYPE_DIR:
        item._type = SyncFileItem::Directory;
        break;
    case CSYNC_FTW_TYPE_FILE:
        item._type = SyncFileItem::File;
        break;
    case CSYNC_FTW_TYPE_SLINK:
        item._type = SyncFileItem::SoftLink;
        break;
    default:
        item._type = SyncFileItem::UnknownType;
    }

    SyncFileItem::Direction dir;

    int re = 0;
    switch(file->instruction) {
    case CSYNC_INSTRUCTION_NONE:
        if (remote && item._should_update_etag && !item._isDirectory && item._instruction == CSYNC_INSTRUCTION_NONE) {
            // Update the database now already:  New fileid or Etag or RemotePerm
            // Or for files that were detected as "resolved conflict".
            // They should have been a conflict because they both were new, or both
            // had their local mtime or remote etag modified, but the size and mtime
            // is the same on the server.  This typically happen when the database is removed.
            // Nothing will be done for those file, but we still need to update the database.

            // Even if the mtime is different on the server, we always want to keep the mtime from
            // the file system in the DB, this is to avoid spurious upload on the next sync
            item._modtime = file->other.modtime;

            _journal->setFileRecord(SyncJournalFileRecord(item, _localPath + item._file));
            item._should_update_etag = false;
        }
        if (item._isDirectory && (remote || file->should_update_etag)) {
            // Because we want still to update etags of directories
            dir = SyncFileItem::None;
        } else {
            // No need to do anything.
            _hasNoneFiles = true;

            emit syncItemDiscovered(item);
            return re;
        }
        break;
    case CSYNC_INSTRUCTION_RENAME:
        dir = !remote ? SyncFileItem::Down : SyncFileItem::Up;
        item._renameTarget = QString::fromUtf8( file->rename_path );
        if (item._isDirectory)
            _renamedFolders.insert(item._file, item._renameTarget);
        break;
    case CSYNC_INSTRUCTION_REMOVE:
        _hasRemoveFile = true;
        dir = !remote ? SyncFileItem::Down : SyncFileItem::Up;
        break;
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_IGNORE:
    case CSYNC_INSTRUCTION_ERROR:
        dir = SyncFileItem::None;
        break;
    case CSYNC_INSTRUCTION_EVAL:
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_STAT_ERROR:
    default:
        dir = remote ? SyncFileItem::Down : SyncFileItem::Up;
        if (!remote && file->instruction == CSYNC_INSTRUCTION_SYNC) {
            // An upload of an existing file means that the file was left unchanged on the server
            // This count as a NONE for detecting if all the file on the server were changed
            _hasNoneFiles = true;
        }
        break;
    }

    item._direction = dir;
    // check for blacklisting of this item.
    // if the item is on blacklist, the instruction was set to IGNORE
    checkBlacklisting( &item );

    if (!item._isDirectory) {
        _progressInfo._totalFileCount++;
        if (Progress::isSizeDependent(file->instruction)) {
            _progressInfo._totalSize += file->size;
        }
    }
    _needsUpdate = true;

    item.log._etag          = file->etag;
    item.log._fileId        = file->file_id;
    item.log._instruction   = file->instruction;
    item.log._modtime       = file->modtime;
    item.log._size          = file->size;

    item.log._other_etag        = file->other.etag;
    item.log._other_fileId      = file->other.file_id;
    item.log._other_instruction = file->other.instruction;
    item.log._other_modtime     = file->other.modtime;
    item.log._other_size        = file->other.size;

    _syncItemMap.insert(fileUtf8, item);

    emit syncItemDiscovered(item);
    return re;
}

void SyncEngine::handleSyncError(CSYNC *ctx, const char *state) {
    CSYNC_STATUS err = csync_get_status( ctx );
    const char *errMsg = csync_get_status_string( ctx );
    QString errStr = csyncErrorToString(err);
    if( errMsg ) {
        if( !errStr.endsWith(" ")) {
            errStr.append(" ");
        }
        errStr += QString::fromUtf8(errMsg);
    }

    // if there is csyncs url modifier in the error message, replace it.
    if( errStr.contains("ownclouds://") ) errStr.replace("ownclouds://", "https://");
    if( errStr.contains("owncloud://") ) errStr.replace("owncloud://", "http://");

    qDebug() << " #### ERROR during "<< state << ": " << errStr;

    if( CSYNC_STATUS_IS_EQUAL( err, CSYNC_STATUS_ABORTED) ) {
        qDebug() << "Update phase was aborted by user!";
    } else if( CSYNC_STATUS_IS_EQUAL( err, CSYNC_STATUS_SERVICE_UNAVAILABLE ) ||
            CSYNC_STATUS_IS_EQUAL( err, CSYNC_STATUS_CONNECT_ERROR )) {
        emit csyncUnavailable();
    } else {
        emit csyncError(errStr);
    }
    finalize();
}

void SyncEngine::startSync()
{
    if (_journal->exists()) {
        QVector< SyncJournalDb::PollInfo > pollInfos = _journal->getPollInfos();
        if (!pollInfos.isEmpty()) {
            qDebug() << "Finish Poll jobs before starting a sync";
            CleanupPollsJob *job = new CleanupPollsJob(pollInfos, AccountManager::instance()->account(),
                                                       _journal, _localPath, this);
            connect(job, SIGNAL(finished()), this, SLOT(startSync()));
            connect(job, SIGNAL(aborted(QString)), this, SLOT(slotCleanPollsJobAborted(QString)));
            job->start();
            return;
        }
    }

    Q_ASSERT(!_syncRunning);
    _syncRunning = true;

    Q_ASSERT(_csync_ctx);

    if (!QDir(_localPath).exists()) {
        // No _tr, it should only occur in non-mirall
        emit csyncError("Unable to find local sync directory.");
        finalize();
        return;
    }

    _syncedItems.clear();
    _syncItemMap.clear();
    _needsUpdate = false;

    csync_resume(_csync_ctx);

    int fileRecordCount = -1;
    if (!_journal->exists()) {
        qDebug() << "=====sync looks new (no DB exists)";
    } else {
        qDebug() << "=====sync with existing DB";
    }

    qDebug() <<  "=====Using Qt" << qVersion();
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    qDebug() <<  "=====Using SSL library version"
             <<  QSslSocket::sslLibraryVersionString().toUtf8().data();
#endif
    // Note that this seems to output the OpenSSL build version not runtime version:
    qDebug() <<  "=====Using" << ne_version_string();

    fileRecordCount = _journal->getFileRecordCount(); // this creates the DB if it does not exist yet
    bool isUpdateFrom_1_5 = _journal->isUpdateFrom_1_5();

    if( fileRecordCount == -1 ) {
        qDebug() << "No way to create a sync journal!";
        emit csyncError(tr("Unable to initialize a sync journal."));
        finalize();
        return;
        // database creation error!
    }

    if (fileRecordCount >= 1 && isUpdateFrom_1_5) {
        qDebug() << "detected update from 1.5" << fileRecordCount << isUpdateFrom_1_5;
        // Disable the read from DB to be sure to re-read all the fileid and etags.
        csync_set_read_from_db(_csync_ctx, false);
    } else {
        csync_set_read_from_db(_csync_ctx, true);
    }

    bool usingSelectiveSync = (!_selectiveSyncBlackList.isEmpty());
    qDebug() << (usingSelectiveSync ? "====Using Selective Sync" : "====NOT Using Selective Sync");
    if (fileRecordCount >= 0 && fileRecordCount < 50 && !usingSelectiveSync) {
        qDebug() << "===== Activating recursive PROPFIND (currently" << fileRecordCount << "file records)";
        bool no_recursive_propfind = false;
        csync_set_module_property(_csync_ctx, "no_recursive_propfind", &no_recursive_propfind);
    } else {
        bool no_recursive_propfind = true;
        csync_set_module_property(_csync_ctx, "no_recursive_propfind", &no_recursive_propfind);
    }

    csync_set_userdata(_csync_ctx, this);
    // TODO: This should be a part of this method, but we don't have
    // any way to get "session_key" module property from csync. Had we
    // have it, then we could keep this code and remove it from
    // AbstractCredentials implementations.
    if (Account *account = AccountManager::instance()->account()) {
        account->credentials()->syncContextPreStart(_csync_ctx);
    } else {
        qDebug() << Q_FUNC_INFO << "No default Account object, huh?";
    }
    // if (_lastAuthCookies.length() > 0) {
    //     // Stuff cookies inside csync, then we can avoid the intermediate HTTP 401 reply
    //     // when https://github.com/owncloud/core/pull/4042 is merged.
    //     QString cookiesAsString;
    //     foreach(QNetworkCookie c, _lastAuthCookies) {
    //         cookiesAsString += c.name();
    //         cookiesAsString += '=';
    //         cookiesAsString += c.value();
    //         cookiesAsString += "; ";
    //     }
    //     csync_set_module_property(_csync_ctx, "session_key", cookiesAsString.to
    // }

    // csync_set_auth_callback( _csync_ctx, getauth );
    //csync_set_log_level( 11 ); don't set the loglevel here, it shall be done by folder.cpp or owncloudcmd.cpp
    int timeout = OwncloudPropagator::httpTimeout();
    csync_set_module_property(_csync_ctx, "timeout", &timeout);


    _stopWatch.start();

    qDebug() << "#### Discovery start #################################################### >>";

    DiscoveryJob *job = new DiscoveryJob(_csync_ctx);
    job->_selectiveSyncBlackList = _selectiveSyncBlackList;
    job->moveToThread(&_thread);
    connect(job, SIGNAL(finished(int)), this, SLOT(slotDiscoveryJobFinished(int)));
    connect(job, SIGNAL(folderDiscovered(bool,QString)),
            this, SIGNAL(folderDiscovered(bool,QString)));
    QMetaObject::invokeMethod(job, "start", Qt::QueuedConnection);
}

void SyncEngine::slotDiscoveryJobFinished(int discoveryResult)
{
    // To clean the progress info
    emit folderDiscovered(false, QString());

    if (discoveryResult < 0 ) {
        handleSyncError(_csync_ctx, "csync_update");
        return;
    }
    qDebug() << "<<#### Discovery end #################################################### " << _stopWatch.addLapTime(QLatin1String("Discovery Finished"));

    // Sanity check
    if (!_journal->isConnected()) {
        qDebug() << "Bailing out, DB failure";
        emit csyncError(tr("Cannot open the sync journal"));
        finalize();
        return;
    } else {
        // Commits a possibly existing (should not though) transaction and starts a new one for the propagate phase
        _journal->commitIfNeededAndStartNewTransaction("Post discovery");
    }

    if( csync_reconcile(_csync_ctx) < 0 ) {
        handleSyncError(_csync_ctx, "csync_reconcile");
        return;
    }

    _stopWatch.addLapTime(QLatin1String("Reconcile Finished"));

    _progressInfo = Progress::Info();

    _hasNoneFiles = false;
    _hasRemoveFile = false;
    bool walkOk = true;
    _seenFiles.clear();

    if( csync_walk_local_tree(_csync_ctx, &treewalkLocal, 0) < 0 ) {
        qDebug() << "Error in local treewalk.";
        walkOk = false;
    }
    if( walkOk && csync_walk_remote_tree(_csync_ctx, &treewalkRemote, 0) < 0 ) {
        qDebug() << "Error in remote treewalk.";
    }

    if (_csync_ctx->remote.root_perms) {
        _remotePerms[QLatin1String("")] = _csync_ctx->remote.root_perms;
        qDebug() << "Permissions of the root folder: " << _remotePerms[QLatin1String("")];
    }

    // The map was used for merging trees, convert it to a list:
    _syncedItems = _syncItemMap.values().toVector();

    // Adjust the paths for the renames.
    for (SyncFileItemVector::iterator it = _syncedItems.begin();
            it != _syncedItems.end(); ++it) {
        it->_file = adjustRenamedPath(it->_file);
    }

    // Sort items per destination
    std::sort(_syncedItems.begin(), _syncedItems.end());

    // make sure everything is allowed
    checkForPermission();

    // To announce the beginning of the sync
    emit aboutToPropagate(_syncedItems);
    _progressInfo._completedFileCount = ULLONG_MAX; // indicate the start with max
    emit transmissionProgress(_progressInfo);
    _progressInfo._completedFileCount = 0;

    if (!_hasNoneFiles && _hasRemoveFile) {
        qDebug() << Q_FUNC_INFO << "All the files are going to be changed, asking the user";
        bool cancel = false;
        emit aboutToRemoveAllFiles(_syncedItems.first()._direction, &cancel);
        if (cancel) {
            qDebug() << Q_FUNC_INFO << "Abort sync";
            finalize();
            return;
        }
    }

    ne_session_s *session = 0;
    // that call to set property actually is a get which will return the session
    csync_set_module_property(_csync_ctx, "get_dav_session", &session);
    Q_ASSERT(session);

    // post update phase script: allow to tweak stuff by a custom script in debug mode.
    if( !qgetenv("OWNCLOUD_POST_UPDATE_SCRIPT").isEmpty() ) {
#ifndef NDEBUG
        QString script = qgetenv("OWNCLOUD_POST_UPDATE_SCRIPT");

        qDebug() << "OOO => Post Update Script: " << script;
        QProcess::execute(script.toUtf8());
#else
    qDebug() << "**** Attention: POST_UPDATE_SCRIPT installed, but not executed because compiled with NDEBUG";
#endif
    }

    // do a database commit
    _journal->commit("post treewalk");

    _propagator = QSharedPointer<OwncloudPropagator>(
        new OwncloudPropagator (session, _localPath, _remoteUrl, _remotePath, _journal, &_thread));
    connect(_propagator.data(), SIGNAL(completed(SyncFileItem)),
            this, SLOT(slotJobCompleted(SyncFileItem)));
    connect(_propagator.data(), SIGNAL(progress(SyncFileItem,quint64)),
            this, SLOT(slotProgress(SyncFileItem,quint64)));
    connect(_propagator.data(), SIGNAL(adjustTotalTransmissionSize(qint64)), this, SLOT(slotAdjustTotalTransmissionSize(qint64)));
    connect(_propagator.data(), SIGNAL(finished()), this, SLOT(slotFinished()), Qt::QueuedConnection);

    // apply the network limits to the propagator
    setNetworkLimits(_uploadLimit, _downloadLimit);

    deleteStaleDownloadInfos();
    deleteStaleUploadInfos();
    deleteStaleBlacklistEntries();
    _journal->commit("post stale entry removal");

    // Emit the started signal only after the propagator has been set up.
    if (_needsUpdate)
        emit(started());

    _propagator->start(_syncedItems);
}

void SyncEngine::slotCleanPollsJobAborted(const QString &error)
{
    csyncError(error);
    finalize();
}

void SyncEngine::setNetworkLimits(int upload, int download)
{
    _uploadLimit = upload;
    _downloadLimit = download;

    if( !_propagator ) return;

    _propagator->_uploadLimit = upload;
    _propagator->_downloadLimit = download;

    int propDownloadLimit = _propagator->_downloadLimit
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
            .load()
#endif
            ;
    int propUploadLimit = _propagator->_uploadLimit
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
            .load()
#endif
            ;

    if( propDownloadLimit != 0 || propUploadLimit != 0 ) {
        qDebug() << " N------N Network Limits (down/up) " << propDownloadLimit << propUploadLimit;
    }
}

void SyncEngine::slotJobCompleted(const SyncFileItem &item)
{
    qDebug() << Q_FUNC_INFO << item._file << item._status << item._errorString;

    /* Update the _syncedItems vector */
    int idx = _syncedItems.indexOf(item);
    if (idx >= 0) {
        _syncedItems[idx]._instruction = item._instruction;
        _syncedItems[idx]._errorString = item._errorString;
        _syncedItems[idx]._status = item._status;

        _syncedItems[idx]._requestDuration = item._requestDuration;
        _syncedItems[idx]._responseTimeStamp = item._responseTimeStamp;
    } else {
        qWarning() << Q_FUNC_INFO << "Could not find index in synced items!";

    }

    _progressInfo.setProgressComplete(item);

    if (item._status == SyncFileItem::FatalError) {
        emit csyncError(item._errorString);
    }

    emit transmissionProgress(_progressInfo);
    emit jobCompleted(item);
}

void SyncEngine::slotFinished()
{
    _anotherSyncNeeded = _anotherSyncNeeded || _propagator->_anotherSyncNeeded;

    // emit the treewalk results.
    if( ! _journal->postSyncCleanup( _seenFiles ) ) {
        qDebug() << "Cleaning of synced ";
    }

    _journal->commit("All Finished.", false);
    emit treeWalkResult(_syncedItems);
    finalize();
}

void SyncEngine::finalize()
{
    _thread.quit();
    _thread.wait();
    csync_commit(_csync_ctx);

    qDebug() << "CSync run took " << _stopWatch.addLapTime(QLatin1String("Sync Finished"));
    _stopWatch.stop();

    _syncRunning = false;
    emit finished();

    // Delete the propagator only after emitting the signal.
    _propagator.clear();
}

void SyncEngine::slotProgress(const SyncFileItem& item, quint64 current)
{
    _progressInfo.setProgressItem(item, current);
    emit transmissionProgress(_progressInfo);
}


void SyncEngine::slotAdjustTotalTransmissionSize(qint64 change)
{
    _progressInfo._totalSize += change;
}

/* Given a path on the remote, give the path as it is when the rename is done */
QString SyncEngine::adjustRenamedPath(const QString& original)
{
    int slashPos = original.size();
    while ((slashPos = original.lastIndexOf('/' , slashPos - 1)) > 0) {
        QHash< QString, QString >::const_iterator it = _renamedFolders.constFind(original.left(slashPos));
        if (it != _renamedFolders.constEnd()) {
            return *it + original.mid(slashPos);
        }
    }
    return original;
}

/**
 *
 * Make sure that we are allowed to do what we do by checking the permissions and the selective sync list
 *
 */
void SyncEngine::checkForPermission()
{
    for (SyncFileItemVector::iterator it = _syncedItems.begin(); it != _syncedItems.end(); ++it) {

        if (it->_direction != SyncFileItem::Up) {
            // Currently we only check server-side permissions
            continue;
        }

        // Do not propagate anything in the server if it is in the selective sync blacklist
        const QString path = it->destination() + QLatin1Char('/');
        if (std::binary_search(_selectiveSyncBlackList.constBegin(), _selectiveSyncBlackList.constEnd(),
                                path)) {
            it->_instruction = CSYNC_INSTRUCTION_IGNORE;
            it->_status = SyncFileItem::FileIgnored;
            it->_errorString = tr("Ignored because of the \"choose what to sync\" blacklist");

            if (it->_isDirectory) {
                for (SyncFileItemVector::iterator it_next = it + 1; it_next != _syncedItems.end() && it_next->_file.startsWith(path); ++it_next) {
                    it = it_next;
                    it->_instruction = CSYNC_INSTRUCTION_IGNORE;
                    it->_status = SyncFileItem::FileIgnored;
                    it->_errorString = tr("Ignored because of the \"choose what to sync\" blacklist");
                }
            }
            continue;
        }

        switch(it->_instruction) {
            case CSYNC_INSTRUCTION_NEW: {
                int slashPos = it->_file.lastIndexOf('/');
                QString parentDir = slashPos <= 0 ? "" : it->_file.mid(0, slashPos);
                const QByteArray perms = getPermissions(parentDir);
                if (perms.isNull()) {
                    // No permissions set
                    break;
                } else if (it->_isDirectory && !perms.contains("K")) {
                    qDebug() << "checkForPermission: ERROR" << it->_file;
                    it->_instruction = CSYNC_INSTRUCTION_ERROR;
                    it->_status = SyncFileItem::NormalError;
                    it->_errorString = tr("Not allowed because you don't have permission to add sub-directories in that directory");

                    for (SyncFileItemVector::iterator it_next = it + 1; it_next != _syncedItems.end() && it_next->_file.startsWith(path); ++it_next) {
                        it = it_next;
                        it->_instruction = CSYNC_INSTRUCTION_ERROR;
                        it->_status = SyncFileItem::NormalError;
                        it->_errorString = tr("Not allowed because you don't have permission to add parent directory");
                    }

                } else if (!it->_isDirectory && !perms.contains("C")) {
                    qDebug() << "checkForPermission: ERROR" << it->_file;
                    it->_instruction = CSYNC_INSTRUCTION_ERROR;
                    it->_status = SyncFileItem::NormalError;
                    it->_errorString = tr("Not allowed because you don't have permission to add files in that directory");
                }
                break;
            }
            case CSYNC_INSTRUCTION_SYNC: {
                const QByteArray perms = getPermissions(it->_file);
                if (perms.isNull()) {
                    // No permissions set
                    break;
                } if (!it->_isDirectory && !perms.contains("W")) {
                    qDebug() << "checkForPermission: RESTORING" << it->_file;
                    it->_should_update_etag = true;
                    it->_instruction = CSYNC_INSTRUCTION_CONFLICT;
                    it->_direction = SyncFileItem::Down;
                    it->_isRestoration = true;
                    // take the things to write to the db from the "other" node (i.e: info from server)
                    // ^^ FIXME This might not be needed anymore since we merge the info in treewalkFile
                    it->_modtime = it->log._other_modtime;
                    it->_fileId = it->log._other_fileId;
                    it->_etag = it->log._other_etag;
                    it->_errorString = tr("Not allowed to upload this file because it is read-only on the server, restoring");
                    continue;
                }
                break;
            }
            case CSYNC_INSTRUCTION_REMOVE: {
                const QByteArray perms = getPermissions(it->_file);
                if (perms.isNull()) {
                    // No permissions set
                    break;
                }
                if (!perms.contains("D")) {
                    qDebug() << "checkForPermission: RESTORING" << it->_file;
                    it->_should_update_etag = true;
                    it->_instruction = CSYNC_INSTRUCTION_NEW;
                    it->_direction = SyncFileItem::Down;
                    it->_isRestoration = true;
                    it->_errorString = tr("Not allowed to remove, restoring");

                    if (it->_isDirectory) {
                        // restore all sub items
                        for (SyncFileItemVector::iterator it_next = it + 1;
                             it_next != _syncedItems.end() && it_next->_file.startsWith(path); ++it_next) {
                            it = it_next;

                            if (it->_instruction != CSYNC_INSTRUCTION_REMOVE) {
                                qWarning() << "non-removed job within a removed directory"
                                           << it->_file << it->_instruction;
                                continue;
                            }

                            qDebug() << "checkForPermission: RESTORING" << it->_file;
                            it->_should_update_etag = true;

                            it->_instruction = CSYNC_INSTRUCTION_NEW;
                            it->_direction = SyncFileItem::Down;
                            it->_isRestoration = true;
                            it->_errorString = tr("Not allowed to remove, restoring");
                        }
                    }
                } else if(perms.contains("S") && perms.contains("D")) {
                    // this is a top level shared dir which can be removed to unshare it,
                    // regardless if it is a read only share or not.
                    // To avoid that we try to restore files underneath this dir which have
                    // not delete permission we fast forward the iterator and leave the
                    // delete jobs intact. It is not physically tried to remove this files
                    // underneath, propagator sees that.
                    if( it->_isDirectory ) {
                        SyncFileItemVector::iterator it_prev = it - 1;

                        // put a more descriptive message if really a top level share dir is removed.
                        if( it_prev != _syncedItems.begin() && !(path.startsWith(it_prev->_file)) ) {
                            it->_errorString = tr("Local files and share folder removed.");
                        }

                        for (SyncFileItemVector::iterator it_next = it + 1;
                             it_next != _syncedItems.end() && it_next->_file.startsWith(path); ++it_next) {
                            it = it_next;
                        }
                    }
                }
                break;
            }

            case CSYNC_INSTRUCTION_RENAME: {

                int slashPos = it->_renameTarget.lastIndexOf('/');
                const QString parentDir = slashPos <= 0 ? "" : it->_renameTarget.mid(0, slashPos);
                const QByteArray destPerms = getPermissions(parentDir);
                const QByteArray filePerms = getPermissions(it->_file);

                //true when it is just a rename in the same directory. (not a move)
                bool isRename = it->_file.startsWith(parentDir) && it->_file.lastIndexOf('/') == slashPos;


                // Check if we are allowed to move to the destination.
                bool destinationOK = true;
                if (isRename || destPerms.isNull()) {
                    // no need to check for the destination dir permission
                    destinationOK = true;
                } else if (it->_isDirectory && !destPerms.contains("K")) {
                    destinationOK = false;
                } else if (!it->_isDirectory && !destPerms.contains("C")) {
                    destinationOK = false;
                }

                // check if we are allowed to move from the source
                bool sourceOK = true;
                if (!filePerms.isNull()
                    &&  ((isRename && !filePerms.contains("N"))
                         || (!isRename && !filePerms.contains("V")))) {

                    // We are not allowed to move or rename this file
                    sourceOK = false;

                    if (filePerms.contains("D") && destinationOK) {
                        // but we are allowed to delete it
                        // TODO!  simulate delete & upload
                    }
                }

#if 0 /* We don't like the idea of renaming behind user's back, as the user may be working with the files */

                if (!sourceOK && !destinationOK) {
                    // Both the source and the destination won't allow move.  Move back to the original
                    std::swap(it->_file, it->_renameTarget);
                    it->_direction = SyncFileItem::Down;
                    it->_errorString = tr("Move not allowed, item restored");
                    it->_isRestoration = true;
                    qDebug() << "checkForPermission: MOVING BACK" << it->_file;
                } else
#endif
                if (!sourceOK || !destinationOK) {
                    // One of them is not possible, just throw an error
                    it->_instruction = CSYNC_INSTRUCTION_ERROR;
                    it->_status = SyncFileItem::NormalError;
                    const QString errorString = tr("Move not allowed because %1 is read-only").arg(
                        sourceOK ? tr("the destination") : tr("the source"));
                    it->_errorString = errorString;

                    qDebug() << "checkForPermission: ERROR MOVING" << it->_file << errorString;

                    // Avoid a rename on next sync:
                    // TODO:  do the resolution now already so we don't need two sync
                    //  At this point we would need to go back to the propagate phase on both remote to take
                    //  the decision.
                    _journal->avoidRenamesOnNextSync(it->_file);
                    _anotherSyncNeeded = true;


                    if (it->_isDirectory) {
                        for (SyncFileItemVector::iterator it_next = it + 1;
                             it_next != _syncedItems.end() && it_next->destination().startsWith(path); ++it_next) {
                            it = it_next;
                            it->_instruction = CSYNC_INSTRUCTION_ERROR;
                            it->_status = SyncFileItem::NormalError;
                            it->_errorString = errorString;
                            qDebug() << "checkForPermission: ERROR MOVING" << it->_file;
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

QByteArray SyncEngine::getPermissions(const QString& file) const
{
    static bool isTest = qgetenv("OWNCLOUD_TEST_PERMISSIONS").toInt();
    if (isTest) {
        QRegExp rx("_PERM_([^_]*)_[^/]*$");
        if (rx.indexIn(file) != -1) {
            return rx.cap(1).toLatin1();
        }
    }
    return _remotePerms.value(file);
}

void SyncEngine::setSelectiveSyncBlackList(const QStringList& list)
{
    _selectiveSyncBlackList = list;
}

bool SyncEngine::estimateState(QString fn, csync_ftw_type_e t, SyncFileStatus* s)
{
    Q_UNUSED(t);
    Q_FOREACH(const SyncFileItem &item, _syncedItems) {
        //qDebug() << Q_FUNC_INFO << fn << item._file << fn.startsWith(item._file) << item._file.startsWith(fn);
        if (item._file.startsWith(fn)) {
            qDebug() << Q_FUNC_INFO << "Setting" << fn << " to STATUS_EVAL";
            s->set(SyncFileStatus::STATUS_EVAL);
            return true;
        }
    }
    return false;
}

qint64 SyncEngine::timeSinceFileTouched(const QString& fn) const
{
    // This copy is essential for thread safety.
    QSharedPointer<OwncloudPropagator> prop = _propagator;
    if (prop) {
        return prop->timeSinceFileTouched(fn);
    }
    return -1;
}

void SyncEngine::abort()
{
    csync_request_abort(_csync_ctx);
    if(_propagator)
        _propagator->abort();
}

} // namespace OCC
