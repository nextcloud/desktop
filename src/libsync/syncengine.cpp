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

    const QString dbFile = _journal->databaseFilePath();
    _csync_ctx.reset(new CSYNC(localPath.toUtf8().data(), dbFile.toUtf8().data()));

    _excludedFiles.reset(new ExcludedFiles(&_csync_ctx->excludes));
    _syncFileStatusTracker.reset(new SyncFileStatusTracker(this));

    _clearTouchedFilesTimer.setSingleShot(true);
    _clearTouchedFilesTimer.setInterval(30 * 1000);
    connect(&_clearTouchedFilesTimer, SIGNAL(timeout()), SLOT(slotClearTouchedFiles()));

    _thread.setObjectName("SyncEngine_Thread");
}

SyncEngine::~SyncEngine()
{
    abort();
    _thread.quit();
    _thread.wait();
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
        errStr = tr("CSync failed to load or create the journal file. "
                    "Make sure you have read and write permissions in the local sync folder.");
        break;
    case CSYNC_STATUS_STATEDB_CORRUPTED:
        errStr = tr("CSync failed to load the journal file. The journal file is corrupted.");
        break;
    case CSYNC_STATUS_NO_MODULE:
        errStr = tr("<p>The %1 plugin for csync could not be loaded.<br/>Please verify the installation!</p>").arg(qApp->applicationName());
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
    case CSYNC_STATUS_PROXY_AUTH_ERROR:
        errStr = tr("CSync could not authenticate at the proxy.");
        break;
    case CSYNC_STATUS_LOOKUP_ERROR:
        errStr = tr("CSync failed to lookup proxy or server.");
        break;
    case CSYNC_STATUS_SERVER_AUTH_ERROR:
        errStr = tr("CSync failed to authenticate at the %1 server.").arg(qApp->applicationName());
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
        errStr = tr("CSync failed due to unhandled permission denied.");
        break;
    case CSYNC_STATUS_NOT_FOUND:
        errStr = tr("CSync failed to access") + " "; // filename gets added.
        break;
    case CSYNC_STATUS_FILE_EXISTS:
        errStr = tr("CSync tried to create a folder that already exists.");
        break;
    case CSYNC_STATUS_OUT_OF_SPACE:
        errStr = tr("CSync: No space on %1 server available.").arg(qApp->applicationName());
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
    time_t now = Utility::qDateTimeToTime_t(QDateTime::currentDateTime());
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

void SyncEngine::deleteStaleDownloadInfos(const SyncFileItemVector &syncItems)
{
    // Find all downloadinfo paths that we want to preserve.
    QSet<QString> download_file_paths;
    foreach (const SyncFileItemPtr &it, syncItems) {
        if (it->_direction == SyncFileItem::Down
            && it->_type == SyncFileItem::File) {
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
            && it->_type == SyncFileItem::File) {
            upload_file_paths.insert(it->_file);
        }
    }

    // Delete from journal.
    auto ids = _journal->deleteStaleUploadInfos(upload_file_paths);

    // Delete the stales chunk on the server.
    if (account()->capabilities().chunkingNg()) {
        foreach (uint transferId, ids) {
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

int SyncEngine::treewalkLocal(csync_file_stat_t *file, csync_file_stat_t *other, void *data)
{
    return static_cast<SyncEngine *>(data)->treewalkFile(file, other, false);
}

int SyncEngine::treewalkRemote(csync_file_stat_t *file, csync_file_stat_t *other, void *data)
{
    return static_cast<SyncEngine *>(data)->treewalkFile(file, other, true);
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
int SyncEngine::treewalkFile(csync_file_stat_t *file, csync_file_stat_t *other, bool remote)
{
    if (!file)
        return -1;

    QTextCodec::ConverterState utf8State;
    static QTextCodec *codec = QTextCodec::codecForName("UTF-8");
    ASSERT(codec);
    QString fileUtf8 = codec->toUnicode(file->path, file->path.size(), &utf8State);
    QString renameTarget;
    QString key = fileUtf8;

    auto instruction = file->instruction;
    if (utf8State.invalidChars > 0 || utf8State.remainingChars > 0) {
        qCWarning(lcEngine) << "File ignored because of invalid utf-8 sequence: " << file->path;
        instruction = CSYNC_INSTRUCTION_IGNORE;
    } else {
        renameTarget = codec->toUnicode(file->rename_path, file->rename_path.size(), &utf8State);
        if (utf8State.invalidChars > 0 || utf8State.remainingChars > 0) {
            qCWarning(lcEngine) << "File ignored because of invalid utf-8 sequence in the rename_path: " << file->path << file->rename_path;
            instruction = CSYNC_INSTRUCTION_IGNORE;
        }
        if (instruction == CSYNC_INSTRUCTION_RENAME) {
            key = renameTarget;
        }
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
    if (!file->remotePerm.isEmpty()) {
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

    // record the seen files to be able to clean the journal later
    _seenFiles.insert(item->_file);
    if (!renameTarget.isEmpty()) {
        // Yes, this records both the rename renameTarget and the original so we keep both in case of a rename
        _seenFiles.insert(renameTarget);
    }

    switch (file->error_status) {
    case CSYNC_STATUS_OK:
        break;
    case CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK:
        item->_errorString = tr("Symbolic links are not supported in syncing.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_IGNORE_LIST:
        item->_errorString = tr("File is listed on the ignore list.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_IS_INVALID_CHARS:
        if (item->_file.endsWith('.')) {
            item->_errorString = tr("File names ending with a period are not supported on this file system.");
        } else {
            char invalid = '\0';
            foreach (char x, QByteArray("\\:?*\"<>|")) {
                if (item->_file.contains(x)) {
                    invalid = x;
                    break;
                }
            }
            if (invalid) {
                item->_errorString = tr("File names containing the character '%1' are not supported on this file system.")
                                         .arg(QLatin1Char(invalid));
            } else {
                item->_errorString = tr("The file name is a reserved name on this file system.");
            }
        }
        break;
    case CSYNC_STATUS_INDIVIDUAL_TRAILING_SPACE:
        item->_errorString = tr("Filename contains trailing spaces.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_EXCLUDE_LONG_FILENAME:
        item->_errorString = tr("Filename is too long.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_EXCLUDE_HIDDEN:
        item->_errorString = tr("File/Folder is ignored because it's hidden.");
        break;
    case CSYNC_STATUS_INDIVIDUAL_TOO_DEEP:
        item->_errorString = tr("Folder hierarchy is too deep");
        break;
    case CSYNC_STATUS_INDIVIDUAL_IS_CONFLICT_FILE:
        item->_status = SyncFileItem::Conflict;
        if (Utility::shouldUploadConflictFiles()) {
            // For uploaded conflict files, files with no action performed on them should
            // be displayed: but we mustn't overwrite the instruction if something happens
            // to the file!
            if (remote && item->_instruction == CSYNC_INSTRUCTION_NONE) {
                item->_errorString = tr("Unresolved conflict.");
                item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            }
        } else {
            item->_errorString = tr("Conflict: Server version downloaded, local copy renamed and not uploaded.");
        }
        break;
    case CYSNC_STATUS_FILE_LOCKED_OR_OPEN:
        item->_errorString = QLatin1String("File locked"); // don't translate, internal use!
        break;
    case CSYNC_STATUS_INDIVIDUAL_STAT_FAILED:
        item->_errorString = tr("Stat failed.");
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
    case CSYNC_STATUS_PERMISSION_DENIED:
        item->_errorString = QLatin1String("Directory not accessible on client, permission denied.");
        item->_status = SyncFileItem::SoftError;
        break;
    default:
        ASSERT(false, "Non handled error-status");
        /* No error string */
    }

    if (item->_instruction == CSYNC_INSTRUCTION_IGNORE && (utf8State.invalidChars > 0 || utf8State.remainingChars > 0)) {
        item->_status = SyncFileItem::NormalError;
        //item->_instruction = CSYNC_INSTRUCTION_ERROR;
        item->_errorString = tr("Filename encoding is not valid");
    }

    bool isDirectory = file->type == CSYNC_FTW_TYPE_DIR;

    if (!file->etag.isEmpty()) {
        item->_etag = file->etag;
    }


    if (!item->_inode) {
        item->_inode = file->inode;
    }

    switch (file->type) {
    case CSYNC_FTW_TYPE_DIR:
        item->_type = SyncFileItem::Directory;
        break;
    case CSYNC_FTW_TYPE_FILE:
        item->_type = SyncFileItem::File;
        break;
    case CSYNC_FTW_TYPE_SLINK:
        item->_type = SyncFileItem::SoftLink;
        break;
    default:
        item->_type = SyncFileItem::UnknownType;
    }

    SyncFileItem::Direction dir = SyncFileItem::None;

    int re = 0;
    switch (file->instruction) {
    case CSYNC_INSTRUCTION_NONE: {
        // Any files that are instruction NONE?
        if (!isDirectory && (!other || other->instruction == CSYNC_INSTRUCTION_NONE)) {
            _hasNoneFiles = true;
        }
        // Put none-instruction conflict files into the syncfileitem list
        if (Utility::shouldUploadConflictFiles()
            && file->error_status == CSYNC_STATUS_INDIVIDUAL_IS_CONFLICT_FILE
            && item->_instruction == CSYNC_INSTRUCTION_IGNORE) {
            break;
        }
        // No syncing or update to be done.
        return re;
    }
    case CSYNC_INSTRUCTION_UPDATE_METADATA:
        dir = SyncFileItem::None;
        // For directories, metadata-only updates will be done after all their files are propagated.
        if (!isDirectory) {
            emit syncItemDiscovered(*item);

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

            if (remote) {
                QString filePath = _localPath + item->_file;

                if (other) {
                    // Even if the mtime is different on the server, we always want to keep the mtime from
                    // the file system in the DB, this is to avoid spurious upload on the next sync
                    item->_modtime = other->modtime;
                    // same for the size
                    item->_size = other->size;
                }

                // If the 'W' remote permission changed, update the local filesystem
                SyncJournalFileRecord prev = _journal->getFileRecord(item->_file);
                if (prev.isValid() && prev._remotePerm.contains('W') != item->_remotePerm.contains('W')) {
                    const bool isReadOnly = !item->_remotePerm.contains('W');
                    FileSystem::setFileReadOnlyWeak(filePath, isReadOnly);
                }

                _journal->setFileRecordMetadata(item->toSyncJournalFileRecordWithInode(filePath));
            } else {
                // The local tree is walked first and doesn't have all the info from the server.
                // Update only outdated data from the disk.
                _journal->updateLocalMetadata(item->_file, item->_modtime, item->_size, item->_inode);
            }

            // Technically we're done with this item.
            return re;
        }
        break;
    case CSYNC_INSTRUCTION_RENAME:
        dir = !remote ? SyncFileItem::Down : SyncFileItem::Up;
        item->_renameTarget = renameTarget;
        if (isDirectory)
            _renamedFolders.insert(item->_file, item->_renameTarget);
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
    case CSYNC_INSTRUCTION_TYPE_CHANGE:
    case CSYNC_INSTRUCTION_SYNC:
        if (!remote) {
            // An upload of an existing file means that the file was left unchanged on the server
            // This counts as a NONE for detecting if all the files on the server were changed
            _hasNoneFiles = true;
        } else if (!isDirectory) {
            auto difftime = std::difftime(file->modtime, other ? other->modtime : 0);
            if (difftime < -3600 * 2) {
                // We are going back on time
                // We only increment if the difference is more than two hours to avoid clock skew
                // issues or DST changes. (We simply ignore files that goes in the past less than
                // two hours for the backup detection heuristics.)
                _backInTimeFiles++;
                qCWarning(lcEngine) << file->path << "has a timestamp earlier than the local file";
            } else if (difftime > 0) {
                _hasForwardInTimeFiles = true;
            }
        }
        dir = remote ? SyncFileItem::Down : SyncFileItem::Up;
        break;
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_EVAL:
    case CSYNC_INSTRUCTION_STAT_ERROR:
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

    _progressInfo->adjustTotalsForFile(*item);

    _needsUpdate = true;

    if (other) {
        item->_previousModtime = other->modtime;
        item->_previousSize = other->size;
    }

    _syncItemMap.insert(key, item);

    emit syncItemDiscovered(*item);
    return re;
}

void SyncEngine::handleSyncError(CSYNC *ctx, const char *state)
{
    CSYNC_STATUS err = csync_get_status(ctx);
    const char *errMsg = csync_get_status_string(ctx);
    QString errStr = csyncErrorToString(err);
    if (errMsg) {
        if (!errStr.endsWith(" ")) {
            errStr.append(" ");
        }
        errStr += QString::fromUtf8(errMsg);
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
    } else if (CSYNC_STATUS_IS_EQUAL(err, CSYNC_STATUS_SERVICE_UNAVAILABLE) || CSYNC_STATUS_IS_EQUAL(err, CSYNC_STATUS_CONNECT_ERROR)) {
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
            connect(job, SIGNAL(finished()), this, SLOT(startSync()));
            connect(job, SIGNAL(aborted(QString)), this, SLOT(slotCleanPollsJobAborted(QString)));
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
        qCInfo(lcEngine) << "There are" << freeBytes << "bytes available at" << _localPath
                         << "and at least" << minFree << "are required";
        if (freeBytes < minFree) {
            _anotherSyncNeeded = DelayedFollowUp;
            csyncError(tr("Only %1 are available, need at least %2 to start",
                "Placeholders are postfixed with file sizes using Utility::octetsToString()")
                           .arg(
                               Utility::octetsToString(freeBytes),
                               Utility::octetsToString(minFree)));
            finalize(false);
            return;
        }
    } else {
        qCWarning(lcEngine) << "Could not determine free space available at" << _localPath;
    }

    _syncItemMap.clear();
    _needsUpdate = false;

    csync_resume(_csync_ctx.data());

    int fileRecordCount = -1;
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

    fileRecordCount = _journal->getFileRecordCount(); // this creates the DB if it does not exist yet

    if (fileRecordCount == -1) {
        qCWarning(lcEngine) << "No way to create a sync journal!";
        csyncError(tr("Unable to open or create the local sync database. Make sure you have write access in the sync folder."));
        finalize(false);
        return;
        // database creation error!
    }

    _csync_ctx->read_remote_from_db = true;

    // This tells csync to never read from the DB if it is empty
    // thereby speeding up the initial discovery significantly.
    _csync_ctx->db_is_empty = (fileRecordCount == 0);

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
    _progressInfo->_status = ProgressInfo::Discovery;
    emit transmissionProgress(*_progressInfo);

    // Usually the discovery runs in the background: We want to avoid
    // stealing too much time from other processes that the user might
    // be interacting with at the time.
    _thread.start(QThread::LowPriority);

    _discoveryMainThread = new DiscoveryMainThread(account());
    _discoveryMainThread->setParent(this);
    connect(this, SIGNAL(finished(bool)), _discoveryMainThread, SLOT(deleteLater()));
    qCInfo(lcEngine) << "Server" << account()->serverVersion()
                     << (account()->isHttp2Supported() ? "Using HTTP/2" : "");
    if (account()->rootEtagChangesNotOnlySubFolderEtags()) {
        connect(_discoveryMainThread, SIGNAL(etag(QString)), this, SLOT(slotRootEtagReceived(QString)));
    } else {
        connect(_discoveryMainThread, SIGNAL(etagConcatenation(QString)), this, SLOT(slotRootEtagReceived(QString)));
    }

    DiscoveryJob *discoveryJob = new DiscoveryJob(_csync_ctx.data());
    discoveryJob->_selectiveSyncBlackList = selectiveSyncBlackList;
    discoveryJob->_selectiveSyncWhiteList =
        _journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, &ok);
    if (!ok) {
        delete discoveryJob;
        qCWarning(lcEngine) << "Unable to read selective sync list, aborting.";
        csyncError(tr("Unable to read from the sync journal."));
        finalize(false);
        return;
    }

    discoveryJob->_syncOptions = _syncOptions;
    discoveryJob->moveToThread(&_thread);
    connect(discoveryJob, SIGNAL(finished(int)), this, SLOT(slotDiscoveryJobFinished(int)));
    connect(discoveryJob, SIGNAL(folderDiscovered(bool, QString)),
        this, SLOT(slotFolderDiscovered(bool, QString)));

    connect(discoveryJob, SIGNAL(newBigFolder(QString, bool)),
        this, SIGNAL(newBigFolder(QString, bool)));


    // This is used for the DiscoveryJob to be able to request the main thread/
    // to read in directory contents.
    _discoveryMainThread->setupHooks(discoveryJob, _remotePath);

    // Starts the update in a seperate thread
    QMetaObject::invokeMethod(discoveryJob, "start", Qt::QueuedConnection);
}

void SyncEngine::slotFolderDiscovered(bool /*local*/, const QString &folder)
{
    _progressInfo->_currentDiscoveredFolder = folder;
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

void SyncEngine::slotDiscoveryJobFinished(int discoveryResult)
{
    if (discoveryResult < 0) {
        handleSyncError(_csync_ctx.data(), "csync_update");
        return;
    }
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

    _progressInfo->_currentDiscoveredFolder.clear();
    _progressInfo->_status = ProgressInfo::Reconcile;
    emit transmissionProgress(*_progressInfo);

    if (csync_reconcile(_csync_ctx.data()) < 0) {
        handleSyncError(_csync_ctx.data(), "csync_reconcile");
        return;
    }

    qCInfo(lcEngine) << "#### Reconcile end #################################################### " << _stopWatch.addLapTime(QLatin1String("Reconcile Finished")) << "ms";

    _hasNoneFiles = false;
    _hasRemoveFile = false;
    _hasForwardInTimeFiles = false;
    _backInTimeFiles = 0;
    bool walkOk = true;
    _seenFiles.clear();
    _temporarilyUnavailablePaths.clear();
    _renamedFolders.clear();

    if (csync_walk_local_tree(_csync_ctx.data(), &treewalkLocal, 0) < 0) {
        qCWarning(lcEngine) << "Error in local treewalk.";
        walkOk = false;
    }
    if (walkOk && csync_walk_remote_tree(_csync_ctx.data(), &treewalkRemote, 0) < 0) {
        qCWarning(lcEngine) << "Error in remote treewalk.";
    }

    qCInfo(lcEngine) << "Permissions of the root folder: " << _csync_ctx->remote.root_perms;

    // The map was used for merging trees, convert it to a list:
    SyncFileItemVector syncItems = _syncItemMap.values().toVector();
    _syncItemMap.clear(); // free memory

    // Adjust the paths for the renames.
    for (SyncFileItemVector::iterator it = syncItems.begin();
         it != syncItems.end(); ++it) {
        (*it)->_file = adjustRenamedPath((*it)->_file);
    }

    // Check for invalid character in old server version
    if (_account->serverVersionInt() < Account::makeServerVersion(8, 1, 0)) {
        // Server version older than 8.1 don't support these character in filename.
        static const QRegExp invalidCharRx("[\\\\:?*\"<>|]");
        for (auto it = syncItems.begin(); it != syncItems.end(); ++it) {
            if ((*it)->_direction == SyncFileItem::Up && (*it)->destination().contains(invalidCharRx)) {
                (*it)->_errorString = tr("File name contains at least one invalid character");
                (*it)->_instruction = CSYNC_INSTRUCTION_IGNORE;
            }
        }
    }

    if (!_hasNoneFiles && _hasRemoveFile) {
        qCInfo(lcEngine) << "All the files are going to be changed, asking the user";
        bool cancel = false;
        emit aboutToRemoveAllFiles(syncItems.first()->_direction, &cancel);
        if (cancel) {
            qCInfo(lcEngine) << "User aborted sync";
            finalize(false);
            return;
        }
    }

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

    // Sort items per destination
    std::sort(syncItems.begin(), syncItems.end());

    // make sure everything is allowed
    checkForPermission(syncItems);

    // Re-init the csync context to free memory
    _csync_ctx->reinitialize();

    // To announce the beginning of the sync
    emit aboutToPropagate(syncItems);

    // it's important to do this before ProgressInfo::start(), to announce start of new sync
    _progressInfo->_status = ProgressInfo::Propagation;
    emit transmissionProgress(*_progressInfo);
    _progressInfo->startEstimateUpdates();

    // post update phase script: allow to tweak stuff by a custom script in debug mode.
    if (!qgetenv("OWNCLOUD_POST_UPDATE_SCRIPT").isEmpty()) {
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
    connect(_propagator.data(), SIGNAL(itemCompleted(const SyncFileItemPtr &)),
        this, SLOT(slotItemCompleted(const SyncFileItemPtr &)));
    connect(_propagator.data(), SIGNAL(progress(const SyncFileItem &, quint64)),
        this, SLOT(slotProgress(const SyncFileItem &, quint64)));
    connect(_propagator.data(), SIGNAL(finished(bool)), this, SLOT(slotFinished(bool)), Qt::QueuedConnection);
    connect(_propagator.data(), SIGNAL(seenLockedFile(QString)), SIGNAL(seenLockedFile(QString)));
    connect(_propagator.data(), SIGNAL(touchedFile(QString)), SLOT(slotAddTouchedFile(QString)));
    connect(_propagator.data(), SIGNAL(insufficientLocalStorage()), SLOT(slotInsufficientLocalStorage()));
    connect(_propagator.data(), SIGNAL(insufficientRemoteStorage()), SLOT(slotInsufficientRemoteStorage()));

    // apply the network limits to the propagator
    setNetworkLimits(_uploadLimit, _downloadLimit);

    deleteStaleDownloadInfos(syncItems);
    deleteStaleUploadInfos(syncItems);
    deleteStaleErrorBlacklistEntries(syncItems);
    _journal->commit("post stale entry removal");

    // Emit the started signal only after the propagator has been set up.
    if (_needsUpdate)
        emit(started());

    _propagator->start(syncItems);

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

    if (success) {
        _journal->setDataFingerprint(_discoveryMainThread->_dataFingerprint);
    }

    // emit the treewalk results.
    if (!_journal->postSyncCleanup(_seenFiles, _temporarilyUnavailablePaths)) {
        qCDebug(lcEngine) << "Cleaning of synced ";
    }

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
    _thread.quit();
    _thread.wait();

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
    _renamedFolders.clear();
    _uniqueErrors.clear();

    _clearTouchedFilesTimer.start();
}

void SyncEngine::slotProgress(const SyncFileItem &item, quint64 current)
{
    _progressInfo->setProgressItem(item, current);
    emit transmissionProgress(*_progressInfo);
}


/* Given a path on the remote, give the path as it is when the rename is done */
QString SyncEngine::adjustRenamedPath(const QString &original)
{
    int slashPos = original.size();
    while ((slashPos = original.lastIndexOf('/', slashPos - 1)) > 0) {
        QHash<QString, QString>::const_iterator it = _renamedFolders.constFind(original.left(slashPos));
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
void SyncEngine::checkForPermission(SyncFileItemVector &syncItems)
{
    bool selectiveListOk;
    auto selectiveSyncBlackList = _journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &selectiveListOk);
    std::sort(selectiveSyncBlackList.begin(), selectiveSyncBlackList.end());
    SyncFileItemPtr needle;

    for (SyncFileItemVector::iterator it = syncItems.begin(); it != syncItems.end(); ++it) {
        if ((*it)->_direction != SyncFileItem::Up) {
            // Currently we only check server-side permissions
            continue;
        }

        // Do not propagate anything in the server if it is in the selective sync blacklist
        const QString path = (*it)->destination() + QLatin1Char('/');

        // if reading the selective sync list from db failed, lets ignore all rather than nothing.
        if (!selectiveListOk || std::binary_search(selectiveSyncBlackList.constBegin(), selectiveSyncBlackList.constEnd(),
                                    path)) {
            (*it)->_instruction = CSYNC_INSTRUCTION_IGNORE;
            (*it)->_status = SyncFileItem::FileIgnored;
            (*it)->_errorString = tr("Ignored because of the \"choose what to sync\" blacklist");

            if ((*it)->isDirectory()) {
                auto it_base = it;
                for (SyncFileItemVector::iterator it_next = it + 1; it_next != syncItems.end() && (*it_next)->_file.startsWith(path); ++it_next) {
                    it = it_next;
                    // We want to ignore almost all instructions for items inside selective-sync excluded folders.
                    //The exception are DOWN/REMOVE actions that remove local files and folders that are
                    //guaranteed to be up-to-date with their server copies.
                    if ((*it)->_direction == SyncFileItem::Down && (*it)->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                        // We need to keep the "delete" items. So we need to un-ignore parent directories
                        QString parentDir = (*it)->_file;
                        do {
                            parentDir = QFileInfo(parentDir).path();
                            if (parentDir.isEmpty() || !parentDir.startsWith((*it_base)->destination())) {
                                break;
                            }
                            // Find the parent directory in the syncItems vector. Since the vector
                            // is sorted we can use a lower_bound, but we need a fake
                            // SyncFileItemPtr needle to compare against
                            if (!needle)
                                needle = SyncFileItemPtr::create();
                            needle->_file = parentDir;
                            auto parent_it = std::lower_bound(it_base, it, needle);
                            if (parent_it == syncItems.end() || (*parent_it)->destination() != parentDir) {
                                break;
                            }
                            ASSERT((*parent_it)->isDirectory());
                            if ((*parent_it)->_instruction != CSYNC_INSTRUCTION_IGNORE) {
                                break; // already changed
                            }
                            (*parent_it)->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                            (*parent_it)->_status = SyncFileItem::NoStatus;
                            (*parent_it)->_errorString.clear();

                        } while (true);
                        continue;
                    }
                    (*it)->_instruction = CSYNC_INSTRUCTION_IGNORE;
                    (*it)->_status = SyncFileItem::FileIgnored;
                    (*it)->_errorString = tr("Ignored because of the \"choose what to sync\" blacklist");
                }
            }
            continue;
        }

        switch ((*it)->_instruction) {
        case CSYNC_INSTRUCTION_TYPE_CHANGE:
        case CSYNC_INSTRUCTION_NEW: {
            int slashPos = (*it)->_file.lastIndexOf('/');
            QString parentDir = slashPos <= 0 ? "" : (*it)->_file.mid(0, slashPos);
            const QByteArray perms = getPermissions(parentDir);
            if (perms.isNull()) {
                // No permissions set
                break;
            } else if ((*it)->isDirectory() && !perms.contains("K")) {
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

            } else if (!(*it)->isDirectory() && !perms.contains("C")) {
                qCWarning(lcEngine) << "checkForPermission: ERROR" << (*it)->_file;
                (*it)->_instruction = CSYNC_INSTRUCTION_ERROR;
                (*it)->_status = SyncFileItem::NormalError;
                (*it)->_errorString = tr("Not allowed because you don't have permission to add files in that folder");
            }
            break;
        }
        case CSYNC_INSTRUCTION_SYNC: {
            const QByteArray perms = getPermissions((*it)->_file);
            if (perms.isNull()) {
                // No permissions set
                break;
            }
            if (!perms.contains("W")) {
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
            const QByteArray perms = getPermissions((*it)->_file);
            if (perms.isNull()) {
                // No permissions set
                break;
            }
            if (!perms.contains("D")) {
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
            } else if (perms.contains("S") && perms.contains("D")) {
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
            const QByteArray destPerms = getPermissions(parentDir);
            const QByteArray filePerms = getPermissions((*it)->_file);

            //true when it is just a rename in the same directory. (not a move)
            bool isRename = (*it)->_file.startsWith(parentDir) && (*it)->_file.lastIndexOf('/') == slashPos;


            // Check if we are allowed to move to the destination.
            bool destinationOK = true;
            if (isRename || destPerms.isNull()) {
                // no need to check for the destination dir permission
                destinationOK = true;
            } else if ((*it)->isDirectory() && !destPerms.contains("K")) {
                destinationOK = false;
            } else if (!(*it)->isDirectory() && !destPerms.contains("C")) {
                destinationOK = false;
            }

            // check if we are allowed to move from the source
            bool sourceOK = true;
            if (!filePerms.isNull()
                && ((isRename && !filePerms.contains("N"))
                       || (!isRename && !filePerms.contains("V")))) {
                // We are not allowed to move or rename this file
                sourceOK = false;

                if (filePerms.contains("D") && destinationOK) {
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

QByteArray SyncEngine::getPermissions(const QString &file) const
{
    static bool isTest = qgetenv("OWNCLOUD_TEST_PERMISSIONS").toInt();
    if (isTest) {
        QRegExp rx("_PERM_([^_]*)_[^/]*$");
        if (rx.indexIn(file) != -1) {
            return rx.cap(1).toLatin1();
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
    return QByteArray();
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

void SyncEngine::abort()
{
    if (_propagator)
        qCInfo(lcEngine) << "Aborting sync";

    // Sets a flag for the update phase
    csync_request_abort(_csync_ctx.data());

    // Aborts the discovery phase job
    if (_discoveryMainThread) {
        _discoveryMainThread->abort();
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
