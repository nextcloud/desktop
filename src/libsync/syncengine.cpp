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
#include "owncloudpropagator.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "discoveryphase.h"
#include "creds/abstractcredentials.h"
#include "syncfilestatus.h"
#include "csync_private.h"
#include "filesystem.h"
#include "propagateremotedelete.h"
#include "propagatedownload.h"
#include "common/asserts.h"
#include "discovery.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <climits>
#include <assert.h>

#include <QCoreApplication>
#include <QSslSocket>
#include <QDir>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QThread>
#include <QStringList>
#include <QTextStream>
#include <QTime>
#include <QUrl>
#include <QSslCertificate>
#include <QProcess>
#include <QElapsedTimer>
#include <qtextcodec.h>

namespace OCC {

Q_LOGGING_CATEGORY(lcEngine, "sync.engine", QtInfoMsg)

static const int s_touchedFilesMaxAgeMs = 15 * 1000;
bool SyncEngine::s_anySyncRunning = false;

qint64 SyncEngine::minimumFileAgeForUpload = 2000;

SyncEngine::SyncEngine(AccountPtr account, const QString &localPath,
    const QString &remotePath, OCC::SyncJournalDb *journal)
    : _account(account)
    , _needsUpdate(false)
    , _syncRunning(false)
    , _localPath(localPath)
    , _remotePath(remotePath)
    , _journal(journal)
    , _progressInfo(new ProgressInfo)
    , _hasNoneFiles(false)
    , _hasRemoveFile(false)
    , _hasForwardInTimeFiles(false)
    , _backInTimeFiles(0)
    , _uploadLimit(0)
    , _downloadLimit(0)
    , _anotherSyncNeeded(NoFollowUpSync)
{
    qRegisterMetaType<SyncFileItem>("SyncFileItem");
    qRegisterMetaType<SyncFileItemPtr>("SyncFileItemPtr");
    qRegisterMetaType<SyncFileItem::Status>("SyncFileItem::Status");
    qRegisterMetaType<SyncFileStatus>("SyncFileStatus");
    qRegisterMetaType<SyncFileItemVector>("SyncFileItemVector");
    qRegisterMetaType<SyncFileItem::Direction>("SyncFileItem::Direction");

    // Everything in the SyncEngine expects a trailing slash for the localPath.
    ASSERT(localPath.endsWith(QLatin1Char('/')));

    _csync_ctx.reset(new CSYNC(localPath.toUtf8().data(), journal));

    _excludedFiles.reset(new ExcludedFiles);
    _csync_ctx->exclude_traversal_fn = _excludedFiles->csyncTraversalMatchFun();

    _syncFileStatusTracker.reset(new SyncFileStatusTracker(this));

    _clearTouchedFilesTimer.setSingleShot(true);
    _clearTouchedFilesTimer.setInterval(30 * 1000);
    connect(&_clearTouchedFilesTimer, &QTimer::timeout, this, &SyncEngine::slotClearTouchedFiles);
}

SyncEngine::~SyncEngine()
{
    abort();
    _excludedFiles.reset();
}

//Convert an error code from csync to a user readable string.
// Keep that function thread safe as it can be called from the sync thread or the main thread
QString SyncEngine::csyncErrorToString(CSYNC_STATUS err)
{
    QString errStr;

    switch (err) {
    case CSYNC_STATUS_OK:
        errStr = tr("Success.");
        break;
    case CSYNC_STATUS_STATEDB_LOAD_ERROR:
        errStr = tr("Failed to load or create the journal file. "
                    "Make sure you have read and write permissions in the local sync folder.");
        break;
    case CSYNC_STATUS_UPDATE_ERROR:
        errStr = tr("Discovery step failed.");
        break;
    case CSYNC_STATUS_TIMEOUT:
        errStr = tr("A network connection timeout happened.");
        break;
    case CSYNC_STATUS_HTTP_ERROR:
        errStr = tr("A HTTP transmission error happened.");
        break;
    case CSYNC_STATUS_PERMISSION_DENIED:
        errStr = tr("Permission denied.");
        break;
    case CSYNC_STATUS_NOT_FOUND:
        errStr = tr("File or directory not found:") + " "; // filename gets added.
        break;
    case CSYNC_STATUS_FILE_EXISTS:
        errStr = tr("Tried to create a folder that already exists.");
        break;
    case CSYNC_STATUS_OUT_OF_SPACE:
        errStr = tr("No space on %1 server available.").arg(qApp->applicationName());
        break;
    case CSYNC_STATUS_UNSUCCESSFUL:
        errStr = tr("CSync unspecified error.");
        break;
    case CSYNC_STATUS_ABORTED:
        errStr = tr("Aborted by the user");
        break;
    case CSYNC_STATUS_SERVICE_UNAVAILABLE:
        errStr = tr("The service is temporarily unavailable");
        break;
    case CSYNC_STATUS_STORAGE_UNAVAILABLE:
        errStr = tr("The mounted folder is temporarily not available on the server");
        break;
    case CSYNC_STATUS_FORBIDDEN:
        errStr = tr("Access is forbidden");
        break;
    case CSYNC_STATUS_OPENDIR_ERROR:
        errStr = tr("An error occurred while opening a folder");
        break;
    case CSYNC_STATUS_READDIR_ERROR:
        errStr = tr("Error while reading folder.");
        break;
    case CSYNC_STATUS_INVALID_CHARACTERS:
    // Handled in callee
    default:
        errStr = tr("An internal error number %1 occurred.").arg((int)err);
    }

    return errStr;
}

/**
 * Check if the item is in the blacklist.
 * If it should not be sync'ed because of the blacklist, update the item with the error instruction
 * and proper error message, and return true.
 * If the item is not in the blacklist, or the blacklist is stale, return false.
 */
bool SyncEngine::checkErrorBlacklisting(SyncFileItem &item)
{
    if (!_journal) {
        qCCritical(lcEngine) << "Journal is undefined!";
        return false;
    }

    SyncJournalErrorBlacklistRecord entry = _journal->errorBlacklistEntry(item._file);
    item._hasBlacklistEntry = false;

    if (!entry.isValid()) {
        return false;
    }

    item._hasBlacklistEntry = true;

    // If duration has expired, it's not blacklisted anymore
    time_t now = Utility::qDateTimeToTime_t(QDateTime::currentDateTimeUtc());
    if (now >= entry._lastTryTime + entry._ignoreDuration) {
        qCInfo(lcEngine) << "blacklist entry for " << item._file << " has expired!";
        return false;
    }

    // If the file has changed locally or on the server, the blacklist
    // entry no longer applies
    if (item._direction == SyncFileItem::Up) { // check the modtime
        if (item._modtime == 0 || entry._lastTryModtime == 0) {
            return false;
        } else if (item._modtime != entry._lastTryModtime) {
            qCInfo(lcEngine) << item._file << " is blacklisted, but has changed mtime!";
            return false;
        } else if (item._renameTarget != entry._renameTarget) {
            qCInfo(lcEngine) << item._file << " is blacklisted, but rename target changed from" << entry._renameTarget;
            return false;
        }
    } else if (item._direction == SyncFileItem::Down) {
        // download, check the etag.
        if (item._etag.isEmpty() || entry._lastTryEtag.isEmpty()) {
            qCInfo(lcEngine) << item._file << "one ETag is empty, no blacklisting";
            return false;
        } else if (item._etag != entry._lastTryEtag) {
            qCInfo(lcEngine) << item._file << " is blacklisted, but has changed etag!";
            return false;
        }
    }

    int waitSeconds = entry._lastTryTime + entry._ignoreDuration - now;
    qCInfo(lcEngine) << "Item is on blacklist: " << entry._file
                     << "retries:" << entry._retryCount
                     << "for another" << waitSeconds << "s";

    // We need to indicate that we skip this file due to blacklisting
    // for reporting and for making sure we don't update the blacklist
    // entry yet.
    // Classification is this _instruction and _status
    item._instruction = CSYNC_INSTRUCTION_IGNORE;
    item._status = SyncFileItem::BlacklistedError;

    auto waitSecondsStr = Utility::durationToDescriptiveString1(1000 * waitSeconds);
    item._errorString = tr("%1 (skipped due to earlier error, trying again in %2)").arg(entry._errorString, waitSecondsStr);

    if (entry._errorCategory == SyncJournalErrorBlacklistRecord::InsufficientRemoteStorage) {
        slotInsufficientRemoteStorage();
    }

    return true;
}

static bool isFileTransferInstruction(csync_instructions_e instruction)
{
    return instruction == CSYNC_INSTRUCTION_CONFLICT
        || instruction == CSYNC_INSTRUCTION_NEW
        || instruction == CSYNC_INSTRUCTION_SYNC
        || instruction == CSYNC_INSTRUCTION_TYPE_CHANGE;
}

static bool isFileModifyingInstruction(csync_instructions_e instruction)
{
    return isFileTransferInstruction(instruction)
        || instruction == CSYNC_INSTRUCTION_RENAME
        || instruction == CSYNC_INSTRUCTION_REMOVE;
}

void SyncEngine::deleteStaleDownloadInfos(const SyncFileItemVector &syncItems)
{
    // Find all downloadinfo paths that we want to preserve.
    QSet<QString> download_file_paths;
    foreach (const SyncFileItemPtr &it, syncItems) {
        if (it->_direction == SyncFileItem::Down
            && it->_type == ItemTypeFile
            && isFileTransferInstruction(it->_instruction)) {
            download_file_paths.insert(it->_file);
        }
    }

    // Delete from journal and from filesystem.
    const QVector<SyncJournalDb::DownloadInfo> deleted_infos =
        _journal->getAndDeleteStaleDownloadInfos(download_file_paths);
    foreach (const SyncJournalDb::DownloadInfo &deleted_info, deleted_infos) {
        const QString tmppath = _propagator->getFilePath(deleted_info._tmpfile);
        qCInfo(lcEngine) << "Deleting stale temporary file: " << tmppath;
        FileSystem::remove(tmppath);
    }
}

void SyncEngine::deleteStaleUploadInfos(const SyncFileItemVector &syncItems)
{
    // Find all blacklisted paths that we want to preserve.
    QSet<QString> upload_file_paths;
    foreach (const SyncFileItemPtr &it, syncItems) {
        if (it->_direction == SyncFileItem::Up
            && it->_type == ItemTypeFile
            && isFileTransferInstruction(it->_instruction)) {
            upload_file_paths.insert(it->_file);
        }
    }

    // Delete from journal.
    auto ids = _journal->deleteStaleUploadInfos(upload_file_paths);

    // Delete the stales chunk on the server.
    if (account()->capabilities().chunkingNg()) {
        foreach (uint transferId, ids) {
            if (!transferId)
                continue; // Was not a chunked upload
            QUrl url = Utility::concatUrlPath(account()->url(), QLatin1String("remote.php/dav/uploads/") + account()->davUser() + QLatin1Char('/') + QString::number(transferId));
            (new DeleteJob(account(), url, this))->start();
        }
    }
}

void SyncEngine::deleteStaleErrorBlacklistEntries(const SyncFileItemVector &syncItems)
{
    // Find all blacklisted paths that we want to preserve.
    QSet<QString> blacklist_file_paths;
    foreach (const SyncFileItemPtr &it, syncItems) {
        if (it->_hasBlacklistEntry)
            blacklist_file_paths.insert(it->_file);
    }

    // Delete from journal.
    _journal->deleteStaleErrorBlacklistEntries(blacklist_file_paths);
}

void SyncEngine::conflictRecordMaintenance()
{
    // Remove stale conflict entries from the database
    // by checking which files still exist and removing the
    // missing ones.
    auto conflictRecordPaths = _journal->conflictRecordPaths();
    for (const auto &path : conflictRecordPaths) {
        auto fsPath = _propagator->getFilePath(QString::fromUtf8(path));
        if (!QFileInfo(fsPath).exists()) {
            _journal->deleteConflictRecord(path);
        }
    }

    // Did the sync see any conflict files that don't yet have records?
    // If so, add them now.
    //
    // This happens when the conflicts table is new or when conflict files
    // are downlaoded but the server doesn't send conflict headers.
    for (const auto &path : _seenFiles) {
        if (!Utility::isConflictFile(path))
            continue;

        auto bapath = path.toUtf8();
        if (!conflictRecordPaths.contains(bapath)) {
            ConflictRecord record;
            record.path = bapath;

            // Determine fileid of target file
            auto basePath = Utility::conflictFileBaseName(bapath);
            SyncJournalFileRecord baseRecord;
            if (_journal->getFileRecord(basePath, &baseRecord) && baseRecord.isValid()) {
                record.baseFileId = baseRecord._fileId;
            }

            _journal->setConflictRecord(record);
        }
    }
}


void OCC::SyncEngine::slotItemDiscovered(const OCC::SyncFileItemPtr &item)
{
    _seenFiles.insert(item->_file);
    if (!item->_renameTarget.isEmpty()) {
        // Yes, this records both the rename renameTarget and the original so we keep both in case of a rename
        _seenFiles.insert(item->_renameTarget);
    }
    if (item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA && !item->isDirectory()) {
        // For directories, metadata-only updates will be done after all their files are propagated.

        // Update the database now already:  New remote fileid or Etag or RemotePerm
        // Or for files that were detected as "resolved conflict".
        // Or a local inode/mtime change

        // In case of "resolved conflict": there should have been a conflict because they
        // both were new, or both had their local mtime or remote etag modified, but the
        // size and mtime is the same on the server.  This typically happens when the
        // database is removed. Nothing will be done for those files, but we still need
        // to update the database.

        // This metadata update *could* be a propagation job of its own, but since it's
        // quick to do and we don't want to create a potentially large number of
        // mini-jobs later on, we just update metadata right now.

        if (item->_direction == SyncFileItem::Down) {
            QString filePath = _localPath + item->_file;

            // If the 'W' remote permission changed, update the local filesystem
            SyncJournalFileRecord prev;
            if (_journal->getFileRecord(item->_file, &prev)
                && prev.isValid()
                && prev._remotePerm.hasPermission(RemotePermissions::CanWrite) != item->_remotePerm.hasPermission(RemotePermissions::CanWrite)) {
                const bool isReadOnly = !item->_remotePerm.isNull() && !item->_remotePerm.hasPermission(RemotePermissions::CanWrite);
                FileSystem::setFileReadOnlyWeak(filePath, isReadOnly);
            }

            _journal->setFileRecordMetadata(item->toSyncJournalFileRecordWithInode(filePath));

            // This might have changed the shared flag, so we must notify SyncFileStatusTracker for example
            emit itemCompleted(item);
        } else {
            // The local tree is walked first and doesn't have all the info from the server.
            // Update only outdated data from the disk.
            // FIXME!  I think this is no longer the case so a setFileRecordMetadata should work
            _journal->updateLocalMetadata(item->_file, item->_modtime, item->_size, item->_inode);
        }
        _hasNoneFiles = true;
        return;
    } else if (item->_instruction == CSYNC_INSTRUCTION_NONE) {
        _hasNoneFiles = true;
        return;
    } else if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
        _hasRemoveFile = true;
    } else if (item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE
        || item->_instruction == CSYNC_INSTRUCTION_SYNC) {
        if (item->_direction == SyncFileItem::Up) {
            // An upload of an existing file means that the file was left unchanged on the server
            // This counts as a NONE for detecting if all the files on the server were changed
            _hasNoneFiles = true;
        } else if (!item->isDirectory()) {
            auto difftime = std::difftime(item->_modtime, item->_previousModtime);
            if (difftime < -3600 * 2) {
                // We are going back on time
                // We only increment if the difference is more than two hours to avoid clock skew
                // issues or DST changes. (We simply ignore files that goes in the past less than
                // two hours for the backup detection heuristics.)
                _backInTimeFiles++;
                qCWarning(lcEngine) << item->_file << "has a timestamp earlier than the local file";
            } else if (difftime > 0) {
                _hasForwardInTimeFiles = true;
            }
        }
    }
    _syncItems.append(item);
    slotNewItem(item);
}


/**
 * The main function in the post-reconcile phase.
 *
 * Called on each entry in the local and remote trees by
 * csync_walk_local_tree()/csync_walk_remote_tree().
 *
 * It merges the two csync file trees into a single map of SyncFileItems.
 *
 * See doc/dev/sync-algorithm.md for an overview.
 */
int SyncEngine::treewalkFile(csync_file_stat_t * /*file*/, csync_file_stat_t * /*other*/, bool /*remote*/)
{
#if 0 // FIXME adapt
    if (!file)
        return -1;

    auto instruction = file->instruction;

    // Decode utf8 path and rename_path QByteArrays to QStrings
    QString fileUtf8;
    QString renameTarget;


    // key is the handle that the SyncFileItem will have in the map.
    QString key = fileUtf8;
    if (instruction == CSYNC_INSTRUCTION_RENAME) {
        key = renameTarget;
    }

    // Gets a default-constructed SyncFileItemPtr or the one from the first walk (=local walk)
    SyncFileItemPtr item = _syncItemMap.value(key);
    if (!item)
        item = SyncFileItemPtr(new SyncFileItem);

    if (item->_file.isEmpty() || instruction == CSYNC_INSTRUCTION_RENAME) {
        item->_file = fileUtf8;
    }
    item->_originalFile = item->_file;

    if (item->_instruction == CSYNC_INSTRUCTION_NONE
        || (item->_instruction == CSYNC_INSTRUCTION_IGNORE && instruction != CSYNC_INSTRUCTION_NONE)) {
        // Take values from side (local/remote) where instruction is not _NONE
        item->_instruction = instruction;
        item->_modtime = file->modtime;
        item->_size = file->size;
        item->_checksumHeader = file->checksumHeader;
        item->_type = file->type;
    } else {
        if (instruction != CSYNC_INSTRUCTION_NONE) {
            qCWarning(lcEngine) << "ERROR: Instruction" << item->_instruction << "vs" << instruction << "for" << fileUtf8;
            ASSERT(false);
            // Set instruction to NONE for safety.
            file->instruction = item->_instruction = instruction = CSYNC_INSTRUCTION_NONE;
            return -1; // should lead to treewalk error
        }
    }

    if (!file->file_id.isEmpty()) {
        item->_fileId = file->file_id;
    }
    if (!file->directDownloadUrl.isEmpty()) {
        item->_directDownloadUrl = QString::fromUtf8(file->directDownloadUrl);
    }
    if (!file->directDownloadCookies.isEmpty()) {
        item->_directDownloadCookies = QString::fromUtf8(file->directDownloadCookies);
    }
    if (!file->remotePerm.isNull()) {
        item->_remotePerm = file->remotePerm;
    }

    /* The flag "serverHasIgnoredFiles" is true if item in question is a directory
     * that has children which are ignored in sync, either because the files are
     * matched by an ignore pattern, or because they are hidden.
     *
     * Only the information about the server side ignored files is stored to the
     * database and thus written to the item here. For the local repository its
     * generated by the walk through the real file tree by discovery phase.
     *
     * It needs to go to the sync journal becasue the stat information about remote
     * files are often read from database rather than being pulled from remote.
     */
    if (remote) {
        item->_serverHasIgnoredFiles = file->has_ignored_files;
    }



    switch (file->error_status) {

    case CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK:
        item->_errorString = tr("Symbolic links are not supported in syncing.");
        break;

   case CSYNC_STATUS_INDIVIDUAL_TOO_DEEP:
        item->_errorString = tr("Folder hierarchy is too deep");
        break;
   case CSYNC_STATUS_SERVICE_UNAVAILABLE:
        item->_errorString = QLatin1String("Server temporarily unavailable.");
        break;
    case CSYNC_STATUS_STORAGE_UNAVAILABLE:
        item->_errorString = QLatin1String("Directory temporarily not available on server.");
        item->_status = SyncFileItem::SoftError;
        _temporarilyUnavailablePaths.insert(item->_file);
        break;
    case CSYNC_STATUS_FORBIDDEN:
        item->_errorString = QLatin1String("Access forbidden.");
        item->_status = SyncFileItem::SoftError;
        _temporarilyUnavailablePaths.insert(item->_file);
        break;

    }



    bool isDirectory = file->type == ItemTypeDirectory;

    if (!file->etag.isEmpty()) {
        item->_etag = file->etag;
    }


    if (!item->_inode) {
        item->_inode = file->inode;
    }

    SyncFileItem::Direction dir = SyncFileItem::None;

    int re = 0;
    switch (file->instruction) {
    case CSYNC_INSTRUCTION_NONE: {
       ... ported ....
    }
    case CSYNC_INSTRUCTION_UPDATE_METADATA:
        dir = SyncFileItem::None;

        ... ported ...

        break;
    case CSYNC_INSTRUCTION_RENAME:
        dir = !remote ? SyncFileItem::Down : SyncFileItem::Up;
        item->_renameTarget = renameTarget;
        if (isDirectory)
            _renamedFolders.insert(item->_file, item->_renameTarget);
        break;
    case CSYNC_INSTRUCTION_REMOVE:

        dir = !remote ? SyncFileItem::Down : SyncFileItem::Up;
        break;
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_ERROR:
        dir = SyncFileItem::None;
        break;
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_EVAL:
    case CSYNC_INSTRUCTION_STAT_ERROR:
    case CSYNC_INSTRUCTION_IGNORE:
    default:
        dir = remote ? SyncFileItem::Down : SyncFileItem::Up;
        break;
    }

    item->_direction = dir;
    if (instruction != CSYNC_INSTRUCTION_NONE) {
        // check for blacklisting of this item.
        // if the item is on blacklist, the instruction was set to ERROR
        checkErrorBlacklisting(*item);
    }

    _needsUpdate = true;



    slotNewItem(item);
    _syncItemMap.insert(key, item);
    return re;
#endif
    return 0;
}

void SyncEngine::handleSyncError(CSYNC *ctx, const char *state)
{
    CSYNC_STATUS err = csync_get_status(ctx);
    QString errMsg = ctx->error_string;
    QString errStr = csyncErrorToString(err);
    if (!errMsg.isEmpty()) {
        if (!errStr.endsWith(" ")) {
            errStr.append(" ");
        }
        errStr += errMsg;
    }
    // Special handling CSYNC_STATUS_INVALID_CHARACTERS
    if (err == CSYNC_STATUS_INVALID_CHARACTERS) {
        errStr = tr("Invalid characters, please rename \"%1\"").arg(errMsg);
    }

    // if there is csyncs url modifier in the error message, replace it.
    if (errStr.contains("ownclouds://"))
        errStr.replace("ownclouds://", "https://");
    if (errStr.contains("owncloud://"))
        errStr.replace("owncloud://", "http://");

    qCWarning(lcEngine) << "ERROR during " << state << ": " << errStr;

    if (CSYNC_STATUS_IS_EQUAL(err, CSYNC_STATUS_ABORTED)) {
        qCInfo(lcEngine) << "Update phase was aborted by user!";
    } else if (CSYNC_STATUS_IS_EQUAL(err, CSYNC_STATUS_SERVICE_UNAVAILABLE)) {
        emit csyncUnavailable();
    } else {
        csyncError(errStr);
    }
    finalize(false);
}

void SyncEngine::csyncError(const QString &message)
{
    emit syncError(message, ErrorCategory::Normal);
}


void SyncEngine::startSync()
{
    if (_journal->exists()) {
        QVector<SyncJournalDb::PollInfo> pollInfos = _journal->getPollInfos();
        if (!pollInfos.isEmpty()) {
            qCInfo(lcEngine) << "Finish Poll jobs before starting a sync";
            CleanupPollsJob *job = new CleanupPollsJob(pollInfos, _account,
                _journal, _localPath, this);
            connect(job, &CleanupPollsJob::finished, this, &SyncEngine::startSync);
            connect(job, &CleanupPollsJob::aborted, this, &SyncEngine::slotCleanPollsJobAborted);
            job->start();
            return;
        }
    }

    if (s_anySyncRunning || _syncRunning) {
        ASSERT(false);
        return;
    }

    s_anySyncRunning = true;
    _syncRunning = true;
    _anotherSyncNeeded = NoFollowUpSync;
    _clearTouchedFilesTimer.stop();

    _hasNoneFiles = false;
    _hasRemoveFile = false;
    _hasForwardInTimeFiles = false;
    _backInTimeFiles = 0;
    _seenFiles.clear();
    _temporarilyUnavailablePaths.clear();

    _progressInfo->reset();

    if (!QDir(_localPath).exists()) {
        _anotherSyncNeeded = DelayedFollowUp;
        // No _tr, it should only occur in non-mirall
        csyncError("Unable to find local sync folder.");
        finalize(false);
        return;
    }

    // Check free size on disk first.
    const qint64 minFree = criticalFreeSpaceLimit();
    const qint64 freeBytes = Utility::freeDiskSpace(_localPath);
    if (freeBytes >= 0) {
        if (freeBytes < minFree) {
            qCWarning(lcEngine()) << "Too little space available at" << _localPath << ". Have"
                                  << freeBytes << "bytes and require at least" << minFree << "bytes";
            _anotherSyncNeeded = DelayedFollowUp;
            csyncError(tr("Only %1 are available, need at least %2 to start",
                "Placeholders are postfixed with file sizes using Utility::octetsToString()")
                           .arg(
                               Utility::octetsToString(freeBytes),
                               Utility::octetsToString(minFree)));
            finalize(false);
            return;
        } else {
            qCInfo(lcEngine) << "There are" << freeBytes << "bytes available at" << _localPath;
        }
    } else {
        qCWarning(lcEngine) << "Could not determine free space available at" << _localPath;
    }

    _syncItems.clear();
    _needsUpdate = false;

    csync_resume(_csync_ctx.data());

    if (!_journal->exists()) {
        qCInfo(lcEngine) << "New sync (no sync journal exists)";
    } else {
        qCInfo(lcEngine) << "Sync with existing sync journal";
    }

    QString verStr("Using Qt ");
    verStr.append(qVersion());

    verStr.append(" SSL library ").append(QSslSocket::sslLibraryVersionString().toUtf8().data());
    verStr.append(" on ").append(Utility::platformName());
    qCInfo(lcEngine) << verStr;

    // This creates the DB if it does not exist yet.
    if (!_journal->isConnected()) {
        qCWarning(lcEngine) << "No way to create a sync journal!";
        csyncError(tr("Unable to open or create the local sync database. Make sure you have write access in the sync folder."));
        finalize(false);
        return;
        // database creation error!
    }

    // Functionality like selective sync might have set up etag storage
    // filtering via avoidReadFromDbOnNextSync(). This *is* the next sync, so
    // undo the filter to allow this sync to retrieve and store the correct etags.
    _journal->clearEtagStorageFilter();

    _csync_ctx->upload_conflict_files = _account->capabilities().uploadConflictFiles();
    _excludedFiles->setExcludeConflictFiles(!_account->capabilities().uploadConflictFiles());

    _csync_ctx->read_remote_from_db = true;

    _lastLocalDiscoveryStyle = _localDiscoveryStyle;
    _csync_ctx->should_discover_locally_fn = [this](const QByteArray &path) {
        return shouldDiscoverLocally(path);
    };

    _csync_ctx->new_files_are_virtual = _syncOptions._newFilesAreVirtual;
    _csync_ctx->virtual_file_suffix = _syncOptions._virtualFileSuffix.toUtf8();
    if (_csync_ctx->new_files_are_virtual && _csync_ctx->virtual_file_suffix.isEmpty()) {
        csyncError(tr("Using virtual files but suffix is not set"));
        finalize(false);
        return;
    }

    bool ok;
    auto selectiveSyncBlackList = _journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
    if (ok) {
        bool usingSelectiveSync = (!selectiveSyncBlackList.isEmpty());
        qCInfo(lcEngine) << (usingSelectiveSync ? "Using Selective Sync" : "NOT Using Selective Sync");
    } else {
        qCWarning(lcEngine) << "Could not retrieve selective sync list from DB";
        csyncError(tr("Unable to read the blacklist from the local database"));
        finalize(false);
        return;
    }
    csync_set_userdata(_csync_ctx.data(), this);

    // Set up checksumming hook
    _csync_ctx->callbacks.checksum_hook = &CSyncChecksumHook::hook;
    _csync_ctx->callbacks.checksum_userdata = &_checksum_hook;

    _stopWatch.start();
    _progressInfo->_status = ProgressInfo::Starting;
    emit transmissionProgress(*_progressInfo);

    qCInfo(lcEngine) << "#### Discovery start ####################################################";
    qCInfo(lcEngine) << "Server" << account()->serverVersion()
                     << (account()->isHttp2Supported() ? "Using HTTP/2" : "");
    _progressInfo->_status = ProgressInfo::Discovery;
    emit transmissionProgress(*_progressInfo);

    _discoveryPhase.reset(new DiscoveryPhase);
    _discoveryPhase->_account = _account;
    _discoveryPhase->_excludes = _excludedFiles.data();
    _discoveryPhase->_statedb = _journal;
    _discoveryPhase->_localDir = _localPath;
    _discoveryPhase->_remoteFolder = _remotePath;
    _discoveryPhase->_syncOptions = _syncOptions;
    _discoveryPhase->_selectiveSyncBlackList = selectiveSyncBlackList;
    _discoveryPhase->_selectiveSyncWhiteList = _journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok);
    if (!ok) {
        qCWarning(lcEngine) << "Unable to read selective sync list, aborting.";
        csyncError(tr("Unable to read from the sync journal."));
        finalize(false);
        return;
    }

    // Check for invalid character in old server version
    QString invalidFilenamePattern = _account->capabilities().invalidFilenameRegex();
    if (invalidFilenamePattern.isNull()
        && _account->serverVersionInt() < Account::makeServerVersion(8, 1, 0)) {
        // Server versions older than 8.1 don't support some characters in filenames.
        // If the capability is not set, default to a pattern that avoids uploading
        // files with names that contain these.
        // It's important to respect the capability also for older servers -- the
        // version check doesn't make sense for custom servers.
        invalidFilenamePattern = "[\\\\:?*\"<>|]";
    }
    _discoveryPhase->_invalidFilenamePattern = invalidFilenamePattern;
    _discoveryPhase->_ignoreHiddenFiles = ignoreHiddenFiles();

    connect(_discoveryPhase.data(), &DiscoveryPhase::folderDiscovered, this, &SyncEngine::slotFolderDiscovered);
    connect(_discoveryPhase.data(), &DiscoveryPhase::newBigFolder, this, &SyncEngine::newBigFolder);
    connect(_discoveryPhase.data(), &DiscoveryPhase::fatalError, this, [this](const QString &errorString) {
        csyncError(errorString);
        finalize(false);
    });

    _discoveryJob = new ProcessDirectoryJob(SyncFileItemPtr(), ProcessDirectoryJob::NormalQuery, ProcessDirectoryJob::NormalQuery,
        _discoveryPhase.data(), this);
    // FIXME! this sucks
    auto runQueuedJob = [this](ProcessDirectoryJob *job, const auto &runQueuedJob) -> void {
        connect(job, &ProcessDirectoryJob::finished, this, [this, job, runQueuedJob] {
            if (job->_dirItem)
                job->itemDiscovered(job->_dirItem);
            sender()->deleteLater();
            if (!_discoveryPhase->_queuedDeletedDirectories.isEmpty()) {
                auto job = qobject_cast<ProcessDirectoryJob *>(_discoveryPhase->_queuedDeletedDirectories.take(_discoveryPhase->_queuedDeletedDirectories.firstKey()).data());
                ASSERT(job);
                runQueuedJob(job, runQueuedJob);
            } else {
                slotDiscoveryJobFinished();
            }
        });
        connect(job, &ProcessDirectoryJob::itemDiscovered, this, &SyncEngine::slotItemDiscovered);
        job->start();
    };
    runQueuedJob(_discoveryJob.data(), runQueuedJob);

    /*
     * FIXME
    if (account()->rootEtagChangesNotOnlySubFolderEtags()) {
        connect(_discoveryMainThread.data(), &DiscoveryMainThread::etag, this, &SyncEngine::slotRootEtagReceived);
    } else {
        connect(_discoveryMainThread.data(), &DiscoveryMainThread::etagConcatenation, this, &SyncEngine::slotRootEtagReceived);
    }

    */
}

void SyncEngine::slotFolderDiscovered(bool local, const QString &folder)
{
    // Currently remote and local discovery never run in parallel
    // Note: Currently this slot is only called occasionally! See the throttling
    //       in DiscoveryJob::update_job_update_callback.
    if (local) {
        _progressInfo->_currentDiscoveredLocalFolder = folder;
        _progressInfo->_currentDiscoveredRemoteFolder.clear();
    } else {
        _progressInfo->_currentDiscoveredRemoteFolder = folder;
        _progressInfo->_currentDiscoveredLocalFolder.clear();
    }
    emit transmissionProgress(*_progressInfo);
}

void SyncEngine::slotRootEtagReceived(const QString &e)
{
    if (_remoteRootEtag.isEmpty()) {
        qCDebug(lcEngine) << "Root etag:" << e;
        _remoteRootEtag = e;
        emit rootEtag(_remoteRootEtag);
    }
}

void SyncEngine::slotNewItem(const SyncFileItemPtr &item)
{
    _progressInfo->adjustTotalsForFile(*item);
}

void SyncEngine::slotDiscoveryJobFinished()
{
    qCInfo(lcEngine) << "#### Discovery end #################################################### " << _stopWatch.addLapTime(QLatin1String("Discovery Finished")) << "ms";

    // Sanity check
    if (!_journal->isConnected()) {
        qCWarning(lcEngine) << "Bailing out, DB failure";
        csyncError(tr("Cannot open the sync journal"));
        finalize(false);
        return;
    } else {
        // Commits a possibly existing (should not though) transaction and starts a new one for the propagate phase
        _journal->commitIfNeededAndStartNewTransaction("Post discovery");
    }

    _progressInfo->_currentDiscoveredRemoteFolder.clear();
    _progressInfo->_currentDiscoveredLocalFolder.clear();
    _progressInfo->_status = ProgressInfo::Reconcile;
    emit transmissionProgress(*_progressInfo);

    //    qCInfo(lcEngine) << "Permissions of the root folder: " << _csync_ctx->remote.root_perms.toString();

    if (!_hasNoneFiles && _hasRemoveFile) {
        qCInfo(lcEngine) << "All the files are going to be changed, asking the user";
        bool cancel = false;
        emit aboutToRemoveAllFiles(_syncItems.first()->_direction, &cancel);
        if (cancel) {
            qCInfo(lcEngine) << "User aborted sync";
            finalize(false);
            return;
        }
    }

    /*
    auto databaseFingerprint = _journal->dataFingerprint();
    // If databaseFingerprint is null, this means that there was no information in the database
    // (for example, upgrading from a previous version, or first sync)
    // Note that an empty ("") fingerprint is valid and means it was empty on the server before.
    if (!databaseFingerprint.isNull()
        && _discoveryMainThread->_dataFingerprint != databaseFingerprint) {
        qCInfo(lcEngine) << "data fingerprint changed, assume restore from backup" << databaseFingerprint << _discoveryMainThread->_dataFingerprint;
        restoreOldFiles(syncItems);
    } else if (!_hasForwardInTimeFiles && _backInTimeFiles >= 2
        && _account->serverVersionInt() < Account::makeServerVersion(9, 1, 0)) {
        // The server before ownCloud 9.1 did not have the data-fingerprint property. So in that
        // case we use heuristics to detect restored backup.  This is disabled with newer version
        // because this causes troubles to the user and is not as reliable as the data-fingerprint.
        qCInfo(lcEngine) << "All the changes are bringing files in the past, asking the user";
        // this typically happen when a backup is restored on the server
        bool restore = false;
        emit aboutToRestoreBackup(&restore);
        if (restore) {
            restoreOldFiles(syncItems);
        }
    }
*/
    // Sort items per destination
    std::sort(_syncItems.begin(), _syncItems.end());

    // make sure everything is allowed
    // TODO checkForPermission(_syncItems);

    // Re-init the csync context to free memory
    _csync_ctx->reinitialize();
    _localDiscoveryPaths.clear();

    // To announce the beginning of the sync
    emit aboutToPropagate(_syncItems);

    // it's important to do this before ProgressInfo::start(), to announce start of new sync
    _progressInfo->_status = ProgressInfo::Propagation;
    emit transmissionProgress(*_progressInfo);
    _progressInfo->startEstimateUpdates();

    // post update phase script: allow to tweak stuff by a custom script in debug mode.
    if (!qEnvironmentVariableIsEmpty("OWNCLOUD_POST_UPDATE_SCRIPT")) {
#ifndef NDEBUG
        QString script = qgetenv("OWNCLOUD_POST_UPDATE_SCRIPT");

        qCDebug(lcEngine) << "Post Update Script: " << script;
        QProcess::execute(script.toUtf8());
#else
        qCWarning(lcEngine) << "**** Attention: POST_UPDATE_SCRIPT installed, but not executed because compiled with NDEBUG";
#endif
    }

    // do a database commit
    _journal->commit("post treewalk");

    _propagator = QSharedPointer<OwncloudPropagator>(
        new OwncloudPropagator(_account, _localPath, _remotePath, _journal));
    _propagator->setSyncOptions(_syncOptions);
    connect(_propagator.data(), &OwncloudPropagator::itemCompleted,
        this, &SyncEngine::slotItemCompleted);
    connect(_propagator.data(), &OwncloudPropagator::progress,
        this, &SyncEngine::slotProgress);
    connect(_propagator.data(), &OwncloudPropagator::finished, this, &SyncEngine::slotFinished, Qt::QueuedConnection);
    connect(_propagator.data(), &OwncloudPropagator::seenLockedFile, this, &SyncEngine::seenLockedFile);
    connect(_propagator.data(), &OwncloudPropagator::touchedFile, this, &SyncEngine::slotAddTouchedFile);
    connect(_propagator.data(), &OwncloudPropagator::insufficientLocalStorage, this, &SyncEngine::slotInsufficientLocalStorage);
    connect(_propagator.data(), &OwncloudPropagator::insufficientRemoteStorage, this, &SyncEngine::slotInsufficientRemoteStorage);
    connect(_propagator.data(), &OwncloudPropagator::newItem, this, &SyncEngine::slotNewItem);

    // apply the network limits to the propagator
    setNetworkLimits(_uploadLimit, _downloadLimit);

    deleteStaleDownloadInfos(_syncItems);
    deleteStaleUploadInfos(_syncItems);
    deleteStaleErrorBlacklistEntries(_syncItems);
    _journal->commit("post stale entry removal");

    // Emit the started signal only after the propagator has been set up.
    if (_needsUpdate)
        emit(started());

    _propagator->start(_syncItems);
    _syncItems.clear();

    qCInfo(lcEngine) << "#### Post-Reconcile end #################################################### " << _stopWatch.addLapTime(QLatin1String("Post-Reconcile Finished")) << "ms";
}

void SyncEngine::slotCleanPollsJobAborted(const QString &error)
{
    csyncError(error);
    finalize(false);
}

void SyncEngine::setNetworkLimits(int upload, int download)
{
    _uploadLimit = upload;
    _downloadLimit = download;

    if (!_propagator)
        return;

    _propagator->_uploadLimit = upload;
    _propagator->_downloadLimit = download;

    int propDownloadLimit = _propagator->_downloadLimit.load();
    int propUploadLimit = _propagator->_uploadLimit.load();

    if (propDownloadLimit != 0 || propUploadLimit != 0) {
        qCInfo(lcEngine) << "Network Limits (down/up) " << propDownloadLimit << propUploadLimit;
    }
}

void SyncEngine::slotItemCompleted(const SyncFileItemPtr &item)
{
    _progressInfo->setProgressComplete(*item);

    if (item->_status == SyncFileItem::FatalError) {
        csyncError(item->_errorString);
    }

    emit transmissionProgress(*_progressInfo);
    emit itemCompleted(item);
}

void SyncEngine::slotFinished(bool success)
{
    if (_propagator->_anotherSyncNeeded && _anotherSyncNeeded == NoFollowUpSync) {
        _anotherSyncNeeded = ImmediateFollowUp;
    }

#if 0
    FIXME
    if (success) {
        _journal->setDataFingerprint(_discoveryMainThread->_dataFingerprint);
    }

    if (!_journal->postSyncCleanup(_seenFiles, _temporarilyUnavailablePaths)) {
        qCDebug(lcEngine) << "Cleaning of synced ";
    }
#endif

    conflictRecordMaintenance();

    _journal->commit("All Finished.", false);

    // Send final progress information even if no
    // files needed propagation, but clear the lastCompletedItem
    // so we don't count this twice (like Recent Files)
    _progressInfo->_lastCompletedItem = SyncFileItem();
    _progressInfo->_status = ProgressInfo::Done;
    emit transmissionProgress(*_progressInfo);

    finalize(success);
}

void SyncEngine::finalize(bool success)
{
    _csync_ctx->reinitialize();
    _journal->close();

    qCInfo(lcEngine) << "CSync run took " << _stopWatch.addLapTime(QLatin1String("Sync Finished")) << "ms";
    _stopWatch.stop();

    s_anySyncRunning = false;
    _syncRunning = false;
    emit finished(success);

    // Delete the propagator only after emitting the signal.
    _propagator.clear();
    _seenFiles.clear();
    _temporarilyUnavailablePaths.clear();
    _uniqueErrors.clear();
    _localDiscoveryPaths.clear();
    _localDiscoveryStyle = LocalDiscoveryStyle::FilesystemOnly;

    _clearTouchedFilesTimer.start();
}

void SyncEngine::slotProgress(const SyncFileItem &item, quint64 current)
{
    _progressInfo->setProgressItem(item, current);
    emit transmissionProgress(*_progressInfo);
}


/**
 *
 * Make sure that we are allowed to do what we do by checking the permissions and the selective sync list
 *
 */
void SyncEngine::checkForPermission(SyncFileItemVector &syncItems)
{
    bool selectiveListOk;
    auto selectiveSyncBlackList = _journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &selectiveListOk);
    std::sort(selectiveSyncBlackList.begin(), selectiveSyncBlackList.end());
    SyncFileItemPtr needle;

    for (SyncFileItemVector::iterator it = syncItems.begin(); it != syncItems.end(); ++it) {
        if ((*it)->_direction != SyncFileItem::Up
            || !isFileModifyingInstruction((*it)->_instruction)) {
            // Currently we only check server-side permissions
            continue;
        }

        // Do not propagate anything in the server if it is in the selective sync blacklist
        const QString path = (*it)->destination() + QLatin1Char('/');

        switch ((*it)->_instruction) {
        case CSYNC_INSTRUCTION_TYPE_CHANGE:
        case CSYNC_INSTRUCTION_NEW: {
            int slashPos = (*it)->_file.lastIndexOf('/');
            QString parentDir = slashPos <= 0 ? "" : (*it)->_file.mid(0, slashPos);
            const auto perms = getPermissions(parentDir);
            if (perms.isNull()) {
                // No permissions set
                break;
            } else if ((*it)->isDirectory() && !perms.hasPermission(RemotePermissions::CanAddSubDirectories)) {
                qCWarning(lcEngine) << "checkForPermission: ERROR" << (*it)->_file;
                (*it)->_instruction = CSYNC_INSTRUCTION_ERROR;
                (*it)->_status = SyncFileItem::NormalError;
                (*it)->_errorString = tr("Not allowed because you don't have permission to add subfolders to that folder");

                for (SyncFileItemVector::iterator it_next = it + 1; it_next != syncItems.end() && (*it_next)->destination().startsWith(path); ++it_next) {
                    it = it_next;
                    if ((*it)->_instruction == CSYNC_INSTRUCTION_RENAME) {
                        // The file was most likely moved in this directory.
                        // If the file was read only or could not be moved or removed, it should
                        // be restored. Do that in the next sync by not considering as a rename
                        // but delete and upload. It will then be restored if needed.
                        _journal->avoidRenamesOnNextSync((*it)->_file);
                        _anotherSyncNeeded = ImmediateFollowUp;
                        qCWarning(lcEngine) << "Moving of " << (*it)->_file << " canceled because no permission to add parent folder";
                    }
                    (*it)->_instruction = CSYNC_INSTRUCTION_ERROR;
                    (*it)->_status = SyncFileItem::SoftError;
                    (*it)->_errorString = tr("Not allowed because you don't have permission to add parent folder");
                }

            } else if (!(*it)->isDirectory() && !perms.hasPermission(RemotePermissions::CanAddFile)) {
                qCWarning(lcEngine) << "checkForPermission: ERROR" << (*it)->_file;
                (*it)->_instruction = CSYNC_INSTRUCTION_ERROR;
                (*it)->_status = SyncFileItem::NormalError;
                (*it)->_errorString = tr("Not allowed because you don't have permission to add files in that folder");
            }
            break;
        }
        case CSYNC_INSTRUCTION_SYNC: {
            const auto perms = getPermissions((*it)->_file);
            if (perms.isNull()) {
                // No permissions set
                break;
            }
            if (!perms.hasPermission(RemotePermissions::CanWrite)) {
                qCWarning(lcEngine) << "checkForPermission: RESTORING" << (*it)->_file;
                (*it)->_instruction = CSYNC_INSTRUCTION_CONFLICT;
                (*it)->_direction = SyncFileItem::Down;
                (*it)->_isRestoration = true;
                // Take the things to write to the db from the "other" node (i.e: info from server).
                // Do a lookup into the csync remote tree to get the metadata we need to restore.
                ASSERT(_csync_ctx->status != CSYNC_STATUS_INIT);
                auto csyncIt = _csync_ctx->remote.files.find((*it)->_file.toUtf8());
                if (csyncIt != _csync_ctx->remote.files.end()) {
                    (*it)->_modtime = csyncIt->second->modtime;
                    (*it)->_size = csyncIt->second->size;
                    (*it)->_fileId = csyncIt->second->file_id;
                    (*it)->_etag = csyncIt->second->etag;
                }
                (*it)->_errorString = tr("Not allowed to upload this file because it is read-only on the server, restoring");
                continue;
            }
            break;
        }
        case CSYNC_INSTRUCTION_REMOVE: {
            const auto perms = getPermissions((*it)->_file);
            if (perms.isNull()) {
                // No permissions set
                break;
            }
            if (!perms.hasPermission(RemotePermissions::CanDelete)) {
                qCWarning(lcEngine) << "checkForPermission: RESTORING" << (*it)->_file;
                (*it)->_instruction = CSYNC_INSTRUCTION_NEW;
                (*it)->_direction = SyncFileItem::Down;
                (*it)->_isRestoration = true;
                (*it)->_errorString = tr("Not allowed to remove, restoring");

                if ((*it)->isDirectory()) {
                    // restore all sub items
                    for (SyncFileItemVector::iterator it_next = it + 1;
                         it_next != syncItems.end() && (*it_next)->_file.startsWith(path); ++it_next) {
                        it = it_next;

                        if ((*it)->_instruction != CSYNC_INSTRUCTION_REMOVE) {
                            qCWarning(lcEngine) << "non-removed job within a removed folder"
                                                << (*it)->_file << (*it)->_instruction;
                            continue;
                        }

                        qCWarning(lcEngine) << "checkForPermission: RESTORING" << (*it)->_file;

                        (*it)->_instruction = CSYNC_INSTRUCTION_NEW;
                        (*it)->_direction = SyncFileItem::Down;
                        (*it)->_isRestoration = true;
                        (*it)->_errorString = tr("Not allowed to remove, restoring");
                    }
                }
            } else if (perms.hasPermission(RemotePermissions::IsShared)
                && perms.hasPermission(RemotePermissions::CanDelete)) {
                // this is a top level shared dir which can be removed to unshare it,
                // regardless if it is a read only share or not.
                // To avoid that we try to restore files underneath this dir which have
                // not delete permission we fast forward the iterator and leave the
                // delete jobs intact. It is not physically tried to remove this files
                // underneath, propagator sees that.
                if ((*it)->isDirectory()) {
                    // put a more descriptive message if a top level share dir really is removed.
                    if (it == syncItems.begin() || !(path.startsWith((*(it - 1))->_file))) {
                        (*it)->_errorString = tr("Local files and share folder removed.");
                    }

                    for (SyncFileItemVector::iterator it_next = it + 1;
                         it_next != syncItems.end() && (*it_next)->_file.startsWith(path); ++it_next) {
                        it = it_next;
                    }
                }
            }
            break;
        }

        case CSYNC_INSTRUCTION_RENAME: {
            int slashPos = (*it)->_renameTarget.lastIndexOf('/');
            const QString parentDir = slashPos <= 0 ? "" : (*it)->_renameTarget.mid(0, slashPos);
            const auto destPerms = getPermissions(parentDir);
            const auto filePerms = getPermissions((*it)->_file);

            //true when it is just a rename in the same directory. (not a move)
            bool isRename = (*it)->_file.startsWith(parentDir) && (*it)->_file.lastIndexOf('/') == slashPos;


            // Check if we are allowed to move to the destination.
            bool destinationOK = true;
            if (isRename || destPerms.isNull()) {
                // no need to check for the destination dir permission
                destinationOK = true;
            } else if ((*it)->isDirectory() && !destPerms.hasPermission(RemotePermissions::CanAddSubDirectories)) {
                destinationOK = false;
            } else if (!(*it)->isDirectory() && !destPerms.hasPermission(RemotePermissions::CanAddFile)) {
                destinationOK = false;
            }

            // check if we are allowed to move from the source
            bool sourceOK = true;
            if (!filePerms.isNull()
                && ((isRename && !filePerms.hasPermission(RemotePermissions::CanRename))
                       || (!isRename && !filePerms.hasPermission(RemotePermissions::CanMove)))) {
                // We are not allowed to move or rename this file
                sourceOK = false;

                if (filePerms.hasPermission(RemotePermissions::CanDelete) && destinationOK) {
                    // but we are allowed to delete it
                    // TODO!  simulate delete & upload
                }
            }

#ifdef OWNCLOUD_RESTORE_RENAME /* We don't like the idea of renaming behind user's back, as the user may be working with the files */
            if (!sourceOK && (!destinationOK || isRename)
                // (not for directory because that's more complicated with the contents that needs to be adjusted)
                && !(*it)->isDirectory()) {
                // Both the source and the destination won't allow move.  Move back to the original
                std::swap((*it)->_file, (*it)->_renameTarget);
                (*it)->_direction = SyncFileItem::Down;
                (*it)->_errorString = tr("Move not allowed, item restored");
                (*it)->_isRestoration = true;
                qCWarning(lcEngine) << "checkForPermission: MOVING BACK" << (*it)->_file;
                // in case something does wrong, we will not do it next time
                _journal->avoidRenamesOnNextSync((*it)->_file);
            } else
#endif
                if (!sourceOK || !destinationOK) {
                // One of them is not possible, just throw an error
                (*it)->_instruction = CSYNC_INSTRUCTION_ERROR;
                (*it)->_status = SyncFileItem::NormalError;
                const QString errorString = tr("Move not allowed because %1 is read-only").arg(sourceOK ? tr("the destination") : tr("the source"));
                (*it)->_errorString = errorString;

                qCWarning(lcEngine) << "checkForPermission: ERROR MOVING" << (*it)->_file << errorString;

                // Avoid a rename on next sync:
                // TODO:  do the resolution now already so we don't need two sync
                //  At this point we would need to go back to the propagate phase on both remote to take
                //  the decision.
                _journal->avoidRenamesOnNextSync((*it)->_file);
                _anotherSyncNeeded = ImmediateFollowUp;


                if ((*it)->isDirectory()) {
                    for (SyncFileItemVector::iterator it_next = it + 1;
                         it_next != syncItems.end() && (*it_next)->destination().startsWith(path); ++it_next) {
                        it = it_next;
                        (*it)->_instruction = CSYNC_INSTRUCTION_ERROR;
                        (*it)->_status = SyncFileItem::NormalError;
                        (*it)->_errorString = errorString;
                        qCWarning(lcEngine) << "checkForPermission: ERROR MOVING" << (*it)->_file;
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

RemotePermissions SyncEngine::getPermissions(const QString &file) const
{
    static bool isTest = qEnvironmentVariableIntValue("OWNCLOUD_TEST_PERMISSIONS");
    if (isTest) {
        QRegExp rx("_PERM_([^_]*)_[^/]*$");
        if (rx.indexIn(file) != -1) {
            return RemotePermissions(rx.cap(1));
        }
    }

    // Fetch from the csync context while we still have it.
    ASSERT(_csync_ctx->status != CSYNC_STATUS_INIT);

    if (file == QLatin1String(""))
        return _csync_ctx->remote.root_perms;

    auto it = _csync_ctx->remote.files.find(file.toUtf8());
    if (it != _csync_ctx->remote.files.end()) {
        return it->second->remotePerm;
    }
    return RemotePermissions();
}

void SyncEngine::restoreOldFiles(SyncFileItemVector &syncItems)
{
    /* When the server is trying to send us lots of file in the past, this means that a backup
       was restored in the server.  In that case, we should not simply overwrite the newer file
       on the file system with the older file from the backup on the server. Instead, we will
       upload the client file. But we still downloaded the old file in a conflict file just in case
    */

    for (auto it = syncItems.begin(); it != syncItems.end(); ++it) {
        if ((*it)->_direction != SyncFileItem::Down)
            continue;

        switch ((*it)->_instruction) {
        case CSYNC_INSTRUCTION_SYNC:
            qCWarning(lcEngine) << "restoreOldFiles: RESTORING" << (*it)->_file;
            (*it)->_instruction = CSYNC_INSTRUCTION_CONFLICT;
            break;
        case CSYNC_INSTRUCTION_REMOVE:
            qCWarning(lcEngine) << "restoreOldFiles: RESTORING" << (*it)->_file;
            (*it)->_instruction = CSYNC_INSTRUCTION_NEW;
            (*it)->_direction = SyncFileItem::Up;
            break;
        case CSYNC_INSTRUCTION_RENAME:
        case CSYNC_INSTRUCTION_NEW:
            // Ideally we should try to revert the rename or remove, but this would be dangerous
            // without re-doing the reconcile phase.  So just let it happen.
            break;
        default:
            break;
        }
    }
}

void SyncEngine::slotAddTouchedFile(const QString &fn)
{
    QElapsedTimer now;
    now.start();
    QString file = QDir::cleanPath(fn);

    // Iterate from the oldest and remove anything older than 15 seconds.
    while (true) {
        auto first = _touchedFiles.begin();
        if (first == _touchedFiles.end())
            break;
        // Compare to our new QElapsedTimer instead of using elapsed().
        // This avoids querying the current time from the OS for every loop.
        if (now.msecsSinceReference() - first.key().msecsSinceReference() <= s_touchedFilesMaxAgeMs) {
            // We found the first path younger than 15 second, keep the rest.
            break;
        }

        _touchedFiles.erase(first);
    }

    // This should be the largest QElapsedTimer yet, use constEnd() as hint.
    _touchedFiles.insert(_touchedFiles.constEnd(), now, file);
}

void SyncEngine::slotClearTouchedFiles()
{
    _touchedFiles.clear();
}

bool SyncEngine::wasFileTouched(const QString &fn) const
{
    // Start from the end (most recent) and look for our path. Check the time just in case.
    auto begin = _touchedFiles.constBegin();
    for (auto it = _touchedFiles.constEnd(); it != begin; --it) {
        if ((it-1).value() == fn)
            return (it-1).key().elapsed() <= s_touchedFilesMaxAgeMs;
    }
    return false;
}

AccountPtr SyncEngine::account() const
{
    return _account;
}

void SyncEngine::setLocalDiscoveryOptions(LocalDiscoveryStyle style, std::set<QByteArray> paths)
{
    _localDiscoveryStyle = style;
    _localDiscoveryPaths = std::move(paths);
}

bool SyncEngine::shouldDiscoverLocally(const QByteArray &path) const
{
    if (_localDiscoveryStyle == LocalDiscoveryStyle::FilesystemOnly)
        return true;

    auto it = _localDiscoveryPaths.lower_bound(path);
    if (it == _localDiscoveryPaths.end() || !it->startsWith(path))
        return false;

    // maybe an exact match or an empty path?
    if (it->size() == path.size() || path.isEmpty())
        return true;

    // check for a prefix + / match
    forever {
        if (it->size() > path.size() && it->at(path.size()) == '/')
            return true;
        ++it;
        if (it == _localDiscoveryPaths.end() || !it->startsWith(path))
            return false;
    }
    return false;
}

void SyncEngine::abort()
{
    if (_propagator)
        qCInfo(lcEngine) << "Aborting sync";

    // Sets a flag for the update phase
    csync_request_abort(_csync_ctx.data());

    // Aborts the discovery phase job
    if (_discoveryJob) {
        _discoveryJob->abort();
    }
    // For the propagator
    if (_propagator) {
        _propagator->abort();
    }
}

void SyncEngine::slotSummaryError(const QString &message)
{
    if (_uniqueErrors.contains(message))
        return;

    _uniqueErrors.insert(message);
    emit syncError(message, ErrorCategory::Normal);
}

void SyncEngine::slotInsufficientLocalStorage()
{
    slotSummaryError(
        tr("Disk space is low: Downloads that would reduce free space "
           "below %1 were skipped.")
            .arg(Utility::octetsToString(freeSpaceLimit())));
}

void SyncEngine::slotInsufficientRemoteStorage()
{
    auto msg = tr("There is insufficient space available on the server for some uploads.");
    if (_uniqueErrors.contains(msg))
        return;

    _uniqueErrors.insert(msg);
    emit syncError(msg, ErrorCategory::InsufficientRemoteStorage);
}

} // namespace OCC
