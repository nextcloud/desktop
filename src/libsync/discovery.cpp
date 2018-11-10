/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include "discovery.h"
#include "common/syncjournaldb.h"
#include "syncfileitem.h"
#include <QDebug>
#include <algorithm>
#include <set>
#include <QDirIterator>
#include <QTextCodec>
#include "vio/csync_vio_local.h"
#include "common/checksums.h"
#include "csync_exclude.h"
#include "csync_util.h"


namespace OCC {

Q_LOGGING_CATEGORY(lcDisco, "sync.discovery", QtInfoMsg)

void ProcessDirectoryJob::start()
{
    qCInfo(lcDisco) << "STARTING" << _currentFolder._server << _queryServer << _currentFolder._local << _queryLocal;

    DiscoverySingleDirectoryJob *serverJob = nullptr;
    if (_queryServer == NormalQuery) {
        serverJob = startAsyncServerQuery();
    } else {
        _serverQueryDone = true;
    }

    // Check whether a normal local query is even necessary
    if (_queryLocal == NormalQuery) {
        if (!_discoveryData->_shouldDiscoverLocaly(_currentFolder._local)
            && (_currentFolder._local == _currentFolder._original || !_discoveryData->_shouldDiscoverLocaly(_currentFolder._original))) {
            _queryLocal = ParentNotChanged;
        }
    }

    if (_queryLocal == NormalQuery) {
        if (!runLocalQuery() && serverJob)
            serverJob->abort();
    }
    _localQueryDone = true;

    // Process is being called when both local and server entries are fetched.
    if (_serverQueryDone)
        process();
}

void ProcessDirectoryJob::process()
{
    ASSERT(_localQueryDone && _serverQueryDone);

    QString localDir;

    //
    // Build lookup tables for local, remote and db entries.
    // For suffix-virtual files, the key will always be the base file name
    // without the suffix.
    //
    struct Entries {
        SyncJournalFileRecord dbEntry;
        RemoteInfo serverEntry;
        LocalInfo localEntry;
    };
    std::map<QString, Entries> entries;
    for (auto &e : _serverNormalQueryEntries) {
        entries[e.name].serverEntry = std::move(e);
    }
    _serverNormalQueryEntries.clear();

    for (auto &e : _localNormalQueryEntries) {
        // Remove the virtual file suffix
        auto name = e.name;
        if (e.isVirtualFile && isVfsWithSuffix()) {
            chopVirtualFileSuffix(name);
            auto &entry = entries[name];
            // If there is both a virtual file and a real file, we must keep the real file
            if (!entry.localEntry.isValid())
                entry.localEntry = std::move(e);
        } else {
            entries[name].localEntry = std::move(e);
        }
    }
    _localNormalQueryEntries.clear();

    // fetch all the name from the DB
    auto pathU8 = _currentFolder._original.toUtf8();
    if (!_discoveryData->_statedb->listFilesInPath(pathU8, [&](const SyncJournalFileRecord &rec) {
            auto name = pathU8.isEmpty() ? rec._path : QString::fromUtf8(rec._path.constData() + (pathU8.size() + 1));
            if (rec.isVirtualFile() && isVfsWithSuffix())
                chopVirtualFileSuffix(name);
            entries[name].dbEntry = rec;
        })) {
        dbError();
        return;
    }

    //
    // Iterate over entries and process them
    //
    for (const auto &f : entries) {
        const auto &e = f.second;

        PathTuple path;
        path = _currentFolder.addName(f.first);

        if (isVfsWithSuffix()) {
            // If the file is virtual in the db, adjust path._original
            if (e.dbEntry.isVirtualFile()) {
                ASSERT(hasVirtualFileSuffix(e.dbEntry._path));
                addVirtualFileSuffix(path._original);
            } else if (e.localEntry.isVirtualFile) {
                // We don't have a db entry - but it should be at this path
                addVirtualFileSuffix(path._original);
            }

            // If the file is virtual locally, adjust path._local
            if (e.localEntry.isVirtualFile) {
                ASSERT(hasVirtualFileSuffix(e.localEntry.name));
                addVirtualFileSuffix(path._local);
            } else if (e.dbEntry.isVirtualFile() && _queryLocal == ParentNotChanged) {
                addVirtualFileSuffix(path._local);
            }
        }

        // If the filename starts with a . we consider it a hidden file
        // For windows, the hidden state is also discovered within the vio
        // local stat function.
        // Recall file shall not be ignored (#4420)
        bool isHidden = e.localEntry.isHidden || (f.first[0] == '.' && f.first != QLatin1String(".sys.admin#recall#"));
        if (handleExcluded(path._target, e.localEntry.isDirectory || e.serverEntry.isDirectory, isHidden, e.localEntry.isSymLink))
            continue;

        if (_queryServer == InBlackList || _discoveryData->isInSelectiveSyncBlackList(path._original)) {
            processBlacklisted(path, e.localEntry, e.dbEntry);
            continue;
        }
        processFile(std::move(path), e.localEntry, e.serverEntry, e.dbEntry);
    }
    QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
}

bool ProcessDirectoryJob::handleExcluded(const QString &path, bool isDirectory, bool isHidden, bool isSymlink)
{
    auto excluded = _discoveryData->_excludes->traversalPatternMatch(path, isDirectory ? ItemTypeDirectory : ItemTypeFile);

    // FIXME: move to ExcludedFiles 's regexp ?
    bool isInvalidPattern = false;
    if (excluded == CSYNC_NOT_EXCLUDED && !_discoveryData->_invalidFilenameRx.isEmpty()) {
        if (path.contains(_discoveryData->_invalidFilenameRx)) {
            excluded = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
            isInvalidPattern = true;
        }
    }
    if (excluded == CSYNC_NOT_EXCLUDED && _discoveryData->_ignoreHiddenFiles && isHidden) {
        excluded = CSYNC_FILE_EXCLUDE_HIDDEN;
    }

    auto localCodec = QTextCodec::codecForLocale();
    if (!OCC::Utility::isWindows() && localCodec->mibEnum() != 106) {
        // If the locale codec is not UTF-8, we must check that the filename from the server can
        // be encoded in the local file system.
        // (Note: on windows, the FS is always UTF-16, so we don't need to check)
        //
        // We cannot use QTextCodec::canEncode() since that can incorrectly return true, see
        // https://bugreports.qt.io/browse/QTBUG-6925.
        QTextEncoder encoder(localCodec, QTextCodec::ConvertInvalidToNull);
        if (encoder.fromUnicode(path).contains('\0')) {
            qCWarning(lcDisco) << "Cannot encode " << path << " to local encoding " << localCodec->name();
            excluded = CSYNC_FILE_EXCLUDE_CANNOT_ENCODE;
        }
    }

    if (excluded == CSYNC_NOT_EXCLUDED && !isSymlink) {
        return false;
    } else if (excluded == CSYNC_FILE_SILENTLY_EXCLUDED || excluded == CSYNC_FILE_EXCLUDE_AND_REMOVE) {
        return true;
    }

    auto item = SyncFileItemPtr::create();
    item->_file = path;
    item->_originalFile = path;
    item->_instruction = CSYNC_INSTRUCTION_IGNORE;

    if (isSymlink) {
        /* Symbolic links are ignored. */
        item->_errorString = tr("Symbolic links are not supported in syncing.");
    } else {
        switch (excluded) {
        case CSYNC_NOT_EXCLUDED:
        case CSYNC_FILE_SILENTLY_EXCLUDED:
        case CSYNC_FILE_EXCLUDE_AND_REMOVE:
            qFatal("These were handled earlier");
        case CSYNC_FILE_EXCLUDE_LIST:
            item->_errorString = tr("File is listed on the ignore list.");
            break;
        case CSYNC_FILE_EXCLUDE_INVALID_CHAR:
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
                }
                if (isInvalidPattern) {
                    item->_errorString = tr("File name contains at least one invalid character");
                } else {
                    item->_errorString = tr("The file name is a reserved name on this file system.");
                }
            }
            break;
        case CSYNC_FILE_EXCLUDE_TRAILING_SPACE:
            item->_errorString = tr("Filename contains trailing spaces.");
            break;
        case CSYNC_FILE_EXCLUDE_LONG_FILENAME:
            item->_errorString = tr("Filename is too long.");
            break;
        case CSYNC_FILE_EXCLUDE_HIDDEN:
            item->_errorString = tr("File/Folder is ignored because it's hidden.");
            break;
        case CSYNC_FILE_EXCLUDE_STAT_FAILED:
            item->_errorString = tr("Stat failed.");
            break;
        case CSYNC_FILE_EXCLUDE_CONFLICT:
            item->_errorString = tr("Conflict: Server version downloaded, local copy renamed and not uploaded.");
            item->_status = SyncFileItem::Conflict;
        break;
        case CSYNC_FILE_EXCLUDE_CANNOT_ENCODE:
            item->_errorString = tr("The filename cannot be encoded on your file system.");
            break;
        }
    }

    _childIgnored = true;
    emit _discoveryData->itemDiscovered(item);
    return true;
}

void ProcessDirectoryJob::processFile(PathTuple path,
    const LocalInfo &localEntry, const RemoteInfo &serverEntry,
    const SyncJournalFileRecord &dbEntry)
{
    const char *hasServer = serverEntry.isValid() ? "true" : _queryServer == ParentNotChanged ? "db" : "false";
    const char *hasLocal = localEntry.isValid() ? "true" : _queryLocal == ParentNotChanged ? "db" : "false";
    qCInfo(lcDisco).nospace() << "Processing " << path._original
                              << " | valid: " << dbEntry.isValid() << "/" << hasLocal << "/" << hasServer
                              << " | mtime: " << dbEntry._modtime << "/" << localEntry.modtime << "/" << serverEntry.modtime
                              << " | size: " << dbEntry._fileSize << "/" << localEntry.size << "/" << serverEntry.size
                              << " | etag: " << dbEntry._etag << "//" << serverEntry.etag
                              << " | checksum: " << dbEntry._checksumHeader << "//" << serverEntry.checksumHeader
                              << " | perm: " << dbEntry._remotePerm << "//" << serverEntry.remotePerm
                              << " | fileid: " << dbEntry._fileId << "//" << serverEntry.fileId
                              << " | inode: " << dbEntry._inode << "/" << localEntry.inode << "/";

    if (_discoveryData->_renamedItems.contains(path._original)) {
        qCDebug(lcDisco) << "Ignoring renamed";
        return; // Ignore this.
    }

    auto item = SyncFileItem::fromSyncJournalFileRecord(dbEntry);
    item->_file = path._target;
    item->_originalFile = path._original;
    item->_previousSize = dbEntry._fileSize;
    item->_previousModtime = dbEntry._modtime;

    // The item shall only have this type if the db request for the virtual download
    // was successful (like: no conflicting remote remove etc). This decision is done
    // either in processFileAnalyzeRemoteInfo() or further down here.
    if (item->_type == ItemTypeVirtualFileDownload)
        item->_type = ItemTypeVirtualFile;
    // Similarly db entries with a dehydration request denote a regular file
    // until the request is processed.
    if (item->_type == ItemTypeVirtualFileDehydration)
        item->_type = ItemTypeFile;

    if (serverEntry.isValid()) {
        processFileAnalyzeRemoteInfo(item, path, localEntry, serverEntry, dbEntry);
        return;
    }

    // Downloading a virtual file is like a server action and can happen even if
    // server-side nothing has changed
    // NOTE: Normally setting the VirtualFileDownload flag means that local and
    // remote will be rediscovered. This is just a fallback.
    if (_queryServer == ParentNotChanged && dbEntry._type == ItemTypeVirtualFileDownload) {
        item->_direction = SyncFileItem::Down;
        item->_instruction = CSYNC_INSTRUCTION_NEW;
        item->_type = ItemTypeVirtualFileDownload;
    }

    processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
}

// Compute the checksum of the given file and assign the result in item->_checksumHeader
// Returns true if the checksum was successfully computed
static bool computeLocalChecksum(const QByteArray &header, const QString &path, const SyncFileItemPtr &item)
{
    auto type = parseChecksumHeaderType(header);
    if (!type.isEmpty()) {
        // TODO: compute async?
        QByteArray checksum = ComputeChecksum::computeNowOnFile(path, type);
        if (!checksum.isEmpty()) {
            item->_checksumHeader = makeChecksumHeader(type, checksum);
            return true;
        }
    }
    return false;
}

void ProcessDirectoryJob::processFileAnalyzeRemoteInfo(
    const SyncFileItemPtr &item, PathTuple path, const LocalInfo &localEntry,
    const RemoteInfo &serverEntry, const SyncJournalFileRecord &dbEntry)
{
    item->_checksumHeader = serverEntry.checksumHeader;
    item->_fileId = serverEntry.fileId;
    item->_remotePerm = serverEntry.remotePerm;
    item->_type = serverEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile;
    item->_etag = serverEntry.etag;
    item->_directDownloadUrl = serverEntry.directDownloadUrl;
    item->_directDownloadCookies = serverEntry.directDownloadCookies;

    // The file is known in the db already
    if (dbEntry.isValid()) {
        if (serverEntry.isDirectory != dbEntry.isDirectory()) {
            // If the type of the entity changed, it's like NEW, but
            // needs to delete the other entity first.
            item->_instruction = CSYNC_INSTRUCTION_TYPE_CHANGE;
            item->_direction = SyncFileItem::Down;
            item->_modtime = serverEntry.modtime;
            item->_size = serverEntry.size;
        } else if (dbEntry._type == ItemTypeVirtualFileDownload) {
            item->_direction = SyncFileItem::Down;
            item->_instruction = CSYNC_INSTRUCTION_NEW;
            item->_type = ItemTypeVirtualFileDownload;
        } else if (dbEntry._etag != serverEntry.etag) {
            item->_direction = SyncFileItem::Down;
            item->_modtime = serverEntry.modtime;
            item->_size = serverEntry.size;
            if (serverEntry.isDirectory) {
                ENFORCE(dbEntry.isDirectory());
                item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            } else if (!localEntry.isValid() && _queryLocal != ParentNotChanged) {
                // Deleted locally, changed on server
                item->_instruction = CSYNC_INSTRUCTION_NEW;
            } else {
                item->_instruction = CSYNC_INSTRUCTION_SYNC;
            }
        } else if (dbEntry._remotePerm != serverEntry.remotePerm || dbEntry._fileId != serverEntry.fileId) {
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            item->_direction = SyncFileItem::Down;
        } else {
            processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, ParentNotChanged);
            return;
        }

        processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
        return;
    }

    // Unknown in db: new file on the server
    Q_ASSERT(!dbEntry.isValid());

    item->_instruction = CSYNC_INSTRUCTION_NEW;
    item->_direction = SyncFileItem::Down;
    item->_modtime = serverEntry.modtime;
    item->_size = serverEntry.size;

    auto postProcessServerNew = [item, this, path, serverEntry, localEntry, dbEntry] () mutable {
        if (item->isDirectory()) {
            _pendingAsyncJobs++;
            _discoveryData->checkSelectiveSyncNewFolder(path._server, serverEntry.remotePerm,
                [=](bool result) {
                    --_pendingAsyncJobs;
                    if (!result) {
                        processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
                    }
                    QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
                });
            return;
        }
        // Turn new remote files into virtual files if the option is enabled.
        auto vfs = _discoveryData->_syncOptions._vfs;
        if (!localEntry.isValid() && vfs && item->_type == ItemTypeFile) {
            item->_type = ItemTypeVirtualFile;
            if (isVfsWithSuffix())
                addVirtualFileSuffix(path._original);
        }
        processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
    };

    // Potential NEW/NEW conflict is handled in AnalyzeLocal
    if (localEntry.isValid()) {
        postProcessServerNew();
        return;
    }

    // Not in db or locally: either new or a rename
    Q_ASSERT(!dbEntry.isValid() && !localEntry.isValid());

    // Check for renames (if there is a file with the same file id)
    bool done = false;
    bool async = false;
    // This function will be executed for every candidate
    auto renameCandidateProcessing = [&](const OCC::SyncJournalFileRecord &base) {
        if (done)
            return;
        if (!base.isValid())
            return;

        // Remote rename of a virtual file we have locally scheduled for download.
        if (base._type == ItemTypeVirtualFileDownload) {
            // We just consider this NEW but mark it for download.
            item->_type = ItemTypeVirtualFileDownload;
            done = true;
            return;
        }

        // Remote rename targets a file that shall be locally dehydrated.
        if (base._type == ItemTypeVirtualFileDehydration) {
            // Don't worry about the rename, just consider it DELETE + NEW(virtual)
            done = true;
            return;
        }

        // Some things prohibit rename detection entirely.
        // Since we don't do the same checks again in reconcile, we can't
        // just skip the candidate, but have to give up completely.
        if (base.isDirectory() != item->isDirectory()) {
            qCInfo(lcDisco, "file types different, not a rename");
            done = true;
            return;
        }
        if (!serverEntry.isDirectory && base._etag != serverEntry.etag) {
            /* File with different etag, don't do a rename, but download the file again */
            qCInfo(lcDisco, "file etag different, not a rename");
            done = true;
            return;
        }

        // Now we know there is a sane rename candidate.
        QString originalPath = QString::fromUtf8(base._path);

        // Rename of a virtual file
        if (base.isVirtualFile() && item->_type == ItemTypeFile) {
            // Ignore if the base is a virtual files
            return;
        }

        if (_discoveryData->_renamedItems.contains(originalPath)) {
            qCInfo(lcDisco, "folder already has a rename entry, skipping");
            return;
        }

        if (!item->isDirectory()) {
            csync_file_stat_t buf;
            if (csync_vio_local_stat((_discoveryData->_localDir + originalPath).toUtf8(), &buf)) {
                qCInfo(lcDisco) << "Local file does not exist anymore." << originalPath;
                return;
            }
            if (buf.modtime != base._modtime || buf.size != base._fileSize || buf.type == ItemTypeDirectory) {
                qCInfo(lcDisco) << "File has changed locally, not a rename." << originalPath;
                return;
            }
        } else {
            if (!QFileInfo(_discoveryData->_localDir + originalPath).isDir()) {
                qCInfo(lcDisco) << "Local directory does not exist anymore." << originalPath;
                return;
            }
        }

        bool wasDeletedOnServer = _discoveryData->findAndCancelDeletedJob(originalPath).first;

        auto postProcessRename = [this, item, base, originalPath](PathTuple &path) {
            auto adjustedOriginalPath = _discoveryData->adjustRenamedPath(originalPath);
            _discoveryData->_renamedItems.insert(originalPath, path._target);
            item->_modtime = base._modtime;
            item->_inode = base._inode;
            item->_instruction = CSYNC_INSTRUCTION_RENAME;
            item->_direction = SyncFileItem::Down;
            item->_renameTarget = path._target;
            item->_file = adjustedOriginalPath;
            item->_originalFile = originalPath;
            path._original = originalPath;
            path._local = adjustedOriginalPath;
            qCInfo(lcDisco) << "Rename detected (down) " << item->_file << " -> " << item->_renameTarget;
        };

        if (wasDeletedOnServer) {
            postProcessRename(path);
            done = true;
        } else {
            // we need to make a request to the server to know that the original file is deleted on the server
            _pendingAsyncJobs++;
            auto job = new RequestEtagJob(_discoveryData->_account, originalPath, this);
            connect(job, &RequestEtagJob::finishedWithResult, this, [=](const Result<QString> &etag) mutable {
                _pendingAsyncJobs--;
                QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
                if (etag.errorCode() != 404 ||
                    // Somehow another item claimed this original path, consider as if it existed
                    _discoveryData->_renamedItems.contains(originalPath)) {
                    // If the file exist or if there is another error, consider it is a new file.
                    postProcessServerNew();
                    return;
                }

                // The file do not exist, it is a rename

                // In case the deleted item was discovered in parallel
                _discoveryData->findAndCancelDeletedJob(originalPath);

                postProcessRename(path);
                processFileFinalize(item, path, item->isDirectory(), item->_instruction == CSYNC_INSTRUCTION_RENAME ? NormalQuery : ParentDontExist, _queryServer);
            });
            job->start();
            done = true; // Ideally, if the origin still exist on the server, we should continue searching...  but that'd be difficult
            async = true;
        }
    };
    if (!_discoveryData->_statedb->getFileRecordsByFileId(serverEntry.fileId, renameCandidateProcessing)) {
        dbError();
        return;
    }
    if (async) {
        return; // We went async
    }

    if (item->_instruction == CSYNC_INSTRUCTION_NEW) {
        postProcessServerNew();
        return;
    }
    processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
}

void ProcessDirectoryJob::processFileAnalyzeLocalInfo(
    const SyncFileItemPtr &item, PathTuple path, const LocalInfo &localEntry,
    const RemoteInfo &serverEntry, const SyncJournalFileRecord &dbEntry, QueryMode recurseQueryServer)
{
    bool noServerEntry = (_queryServer != ParentNotChanged && !serverEntry.isValid())
        || (_queryServer == ParentNotChanged && !dbEntry.isValid());

    if (noServerEntry)
        recurseQueryServer = ParentDontExist;

    bool serverModified = item->_instruction == CSYNC_INSTRUCTION_NEW || item->_instruction == CSYNC_INSTRUCTION_SYNC
        || item->_instruction == CSYNC_INSTRUCTION_RENAME || item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE;

    // Decay server modifications to UPDATE_METADATA if the local virtual exists
    bool hasLocalVirtual = localEntry.isVirtualFile || (_queryLocal == ParentNotChanged && dbEntry.isVirtualFile());
    bool virtualFileDownload = item->_type == ItemTypeVirtualFileDownload;
    if (serverModified && !virtualFileDownload && hasLocalVirtual) {
        item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
        serverModified = false;
        item->_type = ItemTypeVirtualFile;
    }

    if (dbEntry.isVirtualFile() && !virtualFileDownload)
        item->_type = ItemTypeVirtualFile;

    _childModified |= serverModified;

    auto finalize = [&] {
        bool recurse = item->isDirectory() || localEntry.isDirectory || serverEntry.isDirectory;
        // Even if we have a local directory: If the remote is a file that's propagated as a
        // conflict we don't need to recurse into it. (local c1.owncloud, c1/ ; remote: c1)
        if (item->_instruction == CSYNC_INSTRUCTION_CONFLICT && !item->isDirectory())
            recurse = false;
        if (_queryLocal != NormalQuery && _queryServer != NormalQuery && !item->_isRestoration)
            recurse = false;

        auto recurseQueryLocal = _queryLocal == ParentNotChanged ? ParentNotChanged : localEntry.isDirectory || item->_instruction == CSYNC_INSTRUCTION_RENAME ? NormalQuery : ParentDontExist;
        processFileFinalize(item, path, recurse, recurseQueryLocal, recurseQueryServer);
    };

    if (!localEntry.isValid()) {
        if (_queryLocal == ParentNotChanged && dbEntry.isValid()) {
            // Not modified locally (ParentNotChanged)
            if (noServerEntry) {
                // not on the server: Removed on the server, delete locally
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else if (dbEntry._type == ItemTypeVirtualFileDehydration) {
                // dehydration requested
                item->_direction = SyncFileItem::Down;
                item->_instruction = CSYNC_INSTRUCTION_NEW;
                item->_type = ItemTypeVirtualFileDehydration;
            }
        } else if (noServerEntry) {
            // Not locally, not on the server. The entry is stale!
            qCInfo(lcDisco) << "Stale DB entry";
            _discoveryData->_statedb->deleteFileRecord(path._original, true);
            return;
        } else if (dbEntry._type == ItemTypeVirtualFile) {
            // If the virtual file is removed, recreate it.
            item->_instruction = CSYNC_INSTRUCTION_NEW;
            item->_direction = SyncFileItem::Down;
            item->_type = ItemTypeVirtualFile;
        } else if (!serverModified) {
            // Removed locally: also remove on the server.
            if (_dirItem && _dirItem->_isRestoration && _dirItem->_instruction == CSYNC_INSTRUCTION_NEW) {
                // Also restore everything
                item->_instruction = CSYNC_INSTRUCTION_NEW;
                item->_direction = SyncFileItem::Down;
                item->_isRestoration = true;
                item->_errorString = tr("Not allowed to remove, restoring");
            } else if (!dbEntry._serverHasIgnoredFiles) {
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Up;
            }
        }

        finalize();
        return;
    }

    Q_ASSERT(localEntry.isValid());

    item->_inode = localEntry.inode;

    if (dbEntry.isValid()) {
        bool typeChange = localEntry.isDirectory != dbEntry.isDirectory();
        if (!typeChange && localEntry.isVirtualFile) {
            if (noServerEntry) {
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else if (!dbEntry.isVirtualFile()) {
                // If we find what looks to be a spurious "abc.owncloud" the base file "abc"
                // might have been renamed to that. Make sure that the base file is not
                // deleted from the server.
                if (dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) {
                    qCInfo(lcDisco) << "Base file was renamed to virtual file:" << item->_file;
                    item->_direction = SyncFileItem::Down;
                    item->_instruction = CSYNC_INSTRUCTION_NEW;
                    item->_type = ItemTypeVirtualFile;
                }
            }
        } else if (!typeChange && ((dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) || localEntry.isDirectory)) {
            // Local file unchanged.
            if (noServerEntry) {
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else if (dbEntry._type == ItemTypeVirtualFileDehydration) {
                item->_direction = SyncFileItem::Down;
                item->_instruction = CSYNC_INSTRUCTION_NEW;
                item->_type = ItemTypeVirtualFileDehydration;
            } else if (!serverModified && dbEntry._inode != localEntry.inode) {
                item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                item->_direction = SyncFileItem::Down; // Does not matter
            }
        } else if (serverModified || dbEntry.isVirtualFile()) {
            processFileConflict(item, path, localEntry, serverEntry, dbEntry);
        } else if (typeChange) {
            item->_instruction = CSYNC_INSTRUCTION_TYPE_CHANGE;
            item->_direction = SyncFileItem::Up;
            item->_checksumHeader.clear();
            item->_size = localEntry.size;
            item->_modtime = localEntry.modtime;
            item->_type = localEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile;
            _childModified = true;
        } else {
            // Local file was changed
            item->_instruction = CSYNC_INSTRUCTION_SYNC;
            if (noServerEntry) {
                // Special case! deleted on server, modified on client, the instruction is then NEW
                item->_instruction = CSYNC_INSTRUCTION_NEW;
            }
            item->_direction = SyncFileItem::Up;
            item->_checksumHeader.clear();
            item->_size = localEntry.size;
            item->_modtime = localEntry.modtime;
            _childModified = true;

            // Checksum comparison at this stage is only enabled for .eml files,
            // check #4754 #4755
            bool isEmlFile = path._original.endsWith(QLatin1String(".eml"), Qt::CaseInsensitive);
            if (isEmlFile && dbEntry._fileSize == localEntry.size && !dbEntry._checksumHeader.isEmpty()) {
                if (computeLocalChecksum(dbEntry._checksumHeader, _discoveryData->_localDir + path._local, item)
                        && item->_checksumHeader == dbEntry._checksumHeader) {
                    qCInfo(lcDisco) << "NOTE: Checksums are identical, file did not actually change: " << path._local;
                    item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                }
            }
        }

        finalize();
        return;
    }

    Q_ASSERT(!dbEntry.isValid());

    if (localEntry.isVirtualFile && !noServerEntry) {
        // Somehow there is a missing DB entry while the virtual file already exists.
        // The instruction should already be set correctly.
        ASSERT(item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA);
        ASSERT(item->_type == ItemTypeVirtualFile);
        finalize();
        return;
    } else if (serverModified) {
        processFileConflict(item, path, localEntry, serverEntry, dbEntry);
        finalize();
        return;
    }

    // New local file or rename
    item->_instruction = CSYNC_INSTRUCTION_NEW;
    item->_direction = SyncFileItem::Up;
    item->_checksumHeader.clear();
    item->_size = localEntry.size;
    item->_modtime = localEntry.modtime;
    item->_type = localEntry.isDirectory ? ItemTypeDirectory : localEntry.isVirtualFile ? ItemTypeVirtualFile : ItemTypeFile;
    _childModified = true;

    auto postProcessLocalNew = [item, localEntry, this]() {
        if (localEntry.isVirtualFile) {
            // Remove the spurious file if it looks like a placeholder file
            // (we know placeholder files contain " ")
            if (localEntry.size <= 1) {
                qCWarning(lcDisco) << "Wiping virtual file without db entry for" << _currentFolder._local + "/" + localEntry.name;
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else {
                qCWarning(lcDisco) << "Virtual file without db entry for" << _currentFolder._local << localEntry.name
                                   << "but looks odd, keeping";
                item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            }
        }
    };

    // Check if it is a move
    OCC::SyncJournalFileRecord base;
    if (!_discoveryData->_statedb->getFileRecordByInode(localEntry.inode, &base)) {
        dbError();
        return;
    }
    bool isMove = base.isValid() && base._type == item->_type
        && ((base._modtime == localEntry.modtime && base._fileSize == localEntry.size)
               // Directories and virtual files don't need size/mtime equality
               || localEntry.isDirectory || localEntry.isVirtualFile);

    if (isMove) {
        //  The old file must have been deleted.
        isMove = !QFile::exists(_discoveryData->_localDir + base._path);
    }

    // Verify the checksum where possible
    if (isMove && !base._checksumHeader.isEmpty() && item->_type == ItemTypeFile) {
        if (computeLocalChecksum(base._checksumHeader, _discoveryData->_localDir + path._original, item)) {
            qCInfo(lcDisco) << "checking checksum of potential rename " << path._original << item->_checksumHeader << base._checksumHeader;
            isMove = item->_checksumHeader == base._checksumHeader;
        }
    }
    auto originalPath = QString::fromUtf8(base._path);
    if (isMove && _discoveryData->_renamedItems.contains(originalPath))
        isMove = false;

    //Check local permission if we are allowed to put move the file here
    // Technically we should use the one from the server, but we'll assume it is the same
    if (isMove && !checkMovePermissions(base._remotePerm, originalPath, item->isDirectory()))
        isMove = false;

    // Finally make it a NEW or a RENAME
    if (!isMove) {
       postProcessLocalNew();
    } else {
        auto wasDeletedOnClient = _discoveryData->findAndCancelDeletedJob(originalPath);

        auto processRename = [item, originalPath, base, this](PathTuple &path) {
            auto adjustedOriginalPath = _discoveryData->adjustRenamedPath(originalPath);
            _discoveryData->_renamedItems.insert(originalPath, path._target);
            item->_renameTarget = path._target;
            path._server = adjustedOriginalPath;
            item->_file = path._server;
            path._original = originalPath;
            item->_originalFile = path._original;
            item->_modtime = base._modtime;
            item->_inode = base._inode;
            item->_instruction = CSYNC_INSTRUCTION_RENAME;
            item->_direction = SyncFileItem::Up;
            item->_fileId = base._fileId;
            item->_remotePerm = base._remotePerm;
            item->_etag = base._etag;
            item->_type = base._type;
            qCInfo(lcDisco) << "Rename detected (up) " << item->_file << " -> " << item->_renameTarget;
        };
        if (wasDeletedOnClient.first) {
            recurseQueryServer = wasDeletedOnClient.second == base._etag ? ParentNotChanged : NormalQuery;
            processRename(path);
        } else {
            // We must query the server to know if the etag has not changed
            _pendingAsyncJobs++;
            QString serverOriginalPath = originalPath;
            if (base.isVirtualFile() && isVfsWithSuffix())
                chopVirtualFileSuffix(serverOriginalPath);
            auto job = new RequestEtagJob(_discoveryData->_account, serverOriginalPath, this);
            connect(job, &RequestEtagJob::finishedWithResult, this, [=](const Result<QString> &etag) mutable {
                if (!etag || (*etag != base._etag && !item->isDirectory()) || _discoveryData->_renamedItems.contains(originalPath)) {
                    qCInfo(lcDisco) << "Can't rename because the etag has changed or the directory is gone" << originalPath;
                    // Can't be a rename, leave it as a new.
                    postProcessLocalNew();
                } else {
                    // In case the deleted item was discovered in parallel
                    _discoveryData->findAndCancelDeletedJob(originalPath);
                    processRename(path);
                    recurseQueryServer = *etag == base._etag ? ParentNotChanged : NormalQuery;
                }
                processFileFinalize(item, path, item->isDirectory(), NormalQuery, recurseQueryServer);
                _pendingAsyncJobs--;
                QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
            });
            job->start();
            return;
        }
    }

    finalize();
}

void ProcessDirectoryJob::processFileConflict(const SyncFileItemPtr &item, ProcessDirectoryJob::PathTuple path, const LocalInfo &localEntry, const RemoteInfo &serverEntry, const SyncJournalFileRecord &dbEntry)
{
    item->_previousSize = localEntry.size;
    item->_previousModtime = localEntry.modtime;

    if (serverEntry.isDirectory && localEntry.isDirectory) {
        // Folders of the same path are always considered equals
        item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
        return;
    }

    // A conflict with a virtual should lead to virtual file download
    if (dbEntry.isVirtualFile() || localEntry.isVirtualFile)
        item->_type = ItemTypeVirtualFileDownload;

    // If there's no content hash, use heuristics
    if (serverEntry.checksumHeader.isEmpty()) {
        // If the size or mtime is different, it's definitely a conflict.
        bool isConflict = (serverEntry.size != localEntry.size) || (serverEntry.modtime != localEntry.modtime);

        // It could be a conflict even if size and mtime match!
        //
        // In older client versions we always treated these cases as a
        // non-conflict. This behavior is preserved in case the server
        // doesn't provide a content checksum.
        // SO: If there is no checksum, we can have !isConflict here
        // even though the files might have different content! This is an
        // intentional tradeoff. Downloading and comparing files would
        // be technically correct in this situation but leads to too
        // much waste.
        // In particular this kind of NEW/NEW situation with identical
        // sizes and mtimes pops up when the local database is lost for
        // whatever reason.
        item->_instruction = isConflict ? CSYNC_INSTRUCTION_CONFLICT : CSYNC_INSTRUCTION_UPDATE_METADATA;
        item->_direction = isConflict ? SyncFileItem::None : SyncFileItem::Down;
        return;
    }

    // Do we have an UploadInfo for this?
    // Maybe the Upload was completed, but the connection was broken just before
    // we recieved the etag (Issue #5106)
    auto up = _discoveryData->_statedb->getUploadInfo(path._original);
    if (up._valid && up._contentChecksum == serverEntry.checksumHeader) {
        // Solve the conflict into an upload, or nothing
        item->_instruction = up._modtime == localEntry.modtime && up._size == localEntry.size
            ? CSYNC_INSTRUCTION_NONE : CSYNC_INSTRUCTION_SYNC;
        item->_direction = SyncFileItem::Up;

        // Update the etag and other server metadata in the journal already
        // (We can't use a typical CSYNC_INSTRUCTION_UPDATE_METADATA because
        // we must not store the size/modtime from the file system)
        OCC::SyncJournalFileRecord rec;
        if (_discoveryData->_statedb->getFileRecord(path._original, &rec)) {
            rec._path = path._original.toUtf8();
            rec._etag = serverEntry.etag;
            rec._fileId = serverEntry.fileId;
            rec._modtime = serverEntry.modtime;
            rec._type = item->_type;
            rec._fileSize = serverEntry.size;
            rec._remotePerm = serverEntry.remotePerm;
            rec._checksumHeader = serverEntry.checksumHeader;
            _discoveryData->_statedb->setFileRecord(rec);
        }
        return;
    }

    // Rely on content hash comparisons to optimize away non-conflicts inside the job
    item->_instruction = CSYNC_INSTRUCTION_CONFLICT;
    item->_direction = SyncFileItem::None;
}

void ProcessDirectoryJob::processFileFinalize(
    const SyncFileItemPtr &item, PathTuple path, bool recurse,
    QueryMode recurseQueryLocal, QueryMode recurseQueryServer)
{
    // Adjust target path for virtual-suffix files
    if (item->_type == ItemTypeVirtualFile && isVfsWithSuffix()) {
        addVirtualFileSuffix(path._target);
        if (item->_instruction == CSYNC_INSTRUCTION_RENAME)
            addVirtualFileSuffix(item->_renameTarget);
        else
            addVirtualFileSuffix(item->_file);
    }

    if (path._original != path._target && (item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA || item->_instruction == CSYNC_INSTRUCTION_NONE)) {
        ASSERT(_dirItem && _dirItem->_instruction == CSYNC_INSTRUCTION_RENAME);
        // This is because otherwise subitems are not updated!  (ideally renaming a directory could
        // update the database for all items!  See PropagateDirectory::slotSubJobsFinished)
        item->_instruction = CSYNC_INSTRUCTION_RENAME;
        item->_renameTarget = path._target;
        item->_direction = _dirItem->_direction;
    }

    qCInfo(lcDisco) << "Discovered" << item->_file << csync_instruction_str(item->_instruction) << item->_direction << item->_type;

    if (item->isDirectory() && item->_instruction == CSYNC_INSTRUCTION_SYNC)
        item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
    if (!checkPermissions(item))
        recurse = false;
    if (recurse) {
        auto job = new ProcessDirectoryJob(item, recurseQueryLocal, recurseQueryServer, _discoveryData, this);
        job->_currentFolder = path;
        if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
            job->setParent(_discoveryData);
            _discoveryData->_queuedDeletedDirectories[path._original] = job;
        } else {
            connect(job, &ProcessDirectoryJob::finished, this, &ProcessDirectoryJob::subJobFinished);
            _queuedJobs.push_back(job);
        }
    } else {
        if (item->_instruction == CSYNC_INSTRUCTION_REMOVE
            // For the purpose of rename deletion, restored deleted placeholder is as if it was deleted
            || (item->_type == ItemTypeVirtualFile && item->_instruction == CSYNC_INSTRUCTION_NEW)) {
            _discoveryData->_deletedItem[path._original] = item;
        }
        emit _discoveryData->itemDiscovered(item);
    }
}

void ProcessDirectoryJob::processBlacklisted(const PathTuple &path, const OCC::LocalInfo &localEntry,
    const SyncJournalFileRecord &dbEntry)
{
    if (!localEntry.isValid())
        return;

    auto item = SyncFileItem::fromSyncJournalFileRecord(dbEntry);
    item->_file = path._target;
    item->_originalFile = path._original;
    item->_inode = localEntry.inode;
    if (dbEntry.isValid() && ((dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) || (localEntry.isDirectory && dbEntry.isDirectory()))) {
        item->_instruction = CSYNC_INSTRUCTION_REMOVE;
        item->_direction = SyncFileItem::Down;
    } else {
        item->_instruction = CSYNC_INSTRUCTION_IGNORE;
        item->_status = SyncFileItem::FileIgnored;
        item->_errorString = tr("Ignored because of the \"choose what to sync\" blacklist");
        _childIgnored = true;
    }

    qCInfo(lcDisco) << "Discovered (blacklisted) " << item->_file << item->_instruction << item->_direction << item->isDirectory();

    if (item->isDirectory() && item->_instruction != CSYNC_INSTRUCTION_IGNORE) {
        auto job = new ProcessDirectoryJob(item, NormalQuery, InBlackList, _discoveryData, this);
        job->_currentFolder = path;
        connect(job, &ProcessDirectoryJob::finished, this, &ProcessDirectoryJob::subJobFinished);
        _queuedJobs.push_back(job);
    } else {
        emit _discoveryData->itemDiscovered(item);
    }
}

bool ProcessDirectoryJob::checkPermissions(const OCC::SyncFileItemPtr &item)
{
    if (item->_direction != SyncFileItem::Up) {
        // Currently we only check server-side permissions
        return true;
    }

    switch (item->_instruction) {
    case CSYNC_INSTRUCTION_TYPE_CHANGE:
    case CSYNC_INSTRUCTION_NEW: {
        const auto perms = !_rootPermissions.isNull() ? _rootPermissions
                                                      : _dirItem ? _dirItem->_remotePerm : _rootPermissions;
        if (perms.isNull()) {
            // No permissions set
            return true;
        } else if (item->isDirectory() && !perms.hasPermission(RemotePermissions::CanAddSubDirectories)) {
            qCWarning(lcDisco) << "checkForPermission: ERROR" << item->_file;
            item->_instruction = CSYNC_INSTRUCTION_ERROR;
            item->_status = SyncFileItem::NormalError;
            item->_errorString = tr("Not allowed because you don't have permission to add subfolders to that folder");
            return false;
        } else if (!item->isDirectory() && !perms.hasPermission(RemotePermissions::CanAddFile)) {
            qCWarning(lcDisco) << "checkForPermission: ERROR" << item->_file;
            item->_instruction = CSYNC_INSTRUCTION_ERROR;
            item->_status = SyncFileItem::NormalError;
            item->_errorString = tr("Not allowed because you don't have permission to add files in that folder");
            return false;
        }
        break;
    }
    case CSYNC_INSTRUCTION_SYNC: {
        const auto perms = item->_remotePerm;
        if (perms.isNull()) {
            // No permissions set
            return true;
        }
        if (!perms.hasPermission(RemotePermissions::CanWrite)) {
            qCWarning(lcDisco) << "checkForPermission: RESTORING" << item->_file;
            item->_instruction = CSYNC_INSTRUCTION_CONFLICT;
            item->_errorString = tr("Not allowed to upload this file because it is read-only on the server, restoring");
            item->_direction = SyncFileItem::Down;
            item->_isRestoration = true;
            // Take the things to write to the db from the "other" node (i.e: info from server).
            // Do a lookup into the csync remote tree to get the metadata we need to restore.
            qSwap(item->_size, item->_previousSize);
            qSwap(item->_modtime, item->_previousModtime);
            return false;
        }
        break;
    }
    case CSYNC_INSTRUCTION_REMOVE: {
        const auto perms = item->_remotePerm;
        if (perms.isNull()) {
            // No permissions set
            return true;
        }
        if (!perms.hasPermission(RemotePermissions::CanDelete)) {
            qCWarning(lcDisco) << "checkForPermission: RESTORING" << item->_file;
            item->_instruction = CSYNC_INSTRUCTION_NEW;
            item->_direction = SyncFileItem::Down;
            item->_isRestoration = true;
            item->_errorString = tr("Not allowed to remove, restoring");
            return true; // (we need to recurse to restore sub items)
        }
        break;
    }
    default:
        break;
    }
    return true;
}


bool ProcessDirectoryJob::checkMovePermissions(RemotePermissions srcPerm, const QString &srcPath,
                                               bool isDirectory)
{
    auto destPerms = !_rootPermissions.isNull() ? _rootPermissions
                                                : _dirItem ? _dirItem->_remotePerm : _rootPermissions;
    auto filePerms = srcPerm;
    //true when it is just a rename in the same directory. (not a move)
    bool isRename = srcPath.startsWith(_currentFolder._original)
        && srcPath.lastIndexOf('/') == _currentFolder._original.size();
    // Check if we are allowed to move to the destination.
    bool destinationOK = true;
    if (isRename || destPerms.isNull()) {
        // no need to check for the destination dir permission
        destinationOK = true;
    } else if (isDirectory && !destPerms.hasPermission(RemotePermissions::CanAddSubDirectories)) {
        destinationOK = false;
    } else if (!isDirectory && !destPerms.hasPermission(RemotePermissions::CanAddFile)) {
        destinationOK = false;
    }

    // check if we are allowed to move from the source
    bool sourceOK = true;
    if (!filePerms.isNull()
        && ((isRename && !filePerms.hasPermission(RemotePermissions::CanRename))
                || (!isRename && !filePerms.hasPermission(RemotePermissions::CanMove)))) {
        // We are not allowed to move or rename this file
        sourceOK = false;
    }
    if (!sourceOK || !destinationOK) {
        qCInfo(lcDisco) << "Not a move because permission does not allow it." << sourceOK << destinationOK;
        if (!sourceOK) {
            // This is the behavior that we had in the client <= 2.5.
            // but that might not be needed anymore
            _discoveryData->_statedb->avoidRenamesOnNextSync(srcPath);
        }
        return false;
    }
    return true;
}

void ProcessDirectoryJob::subJobFinished()
{
    auto job = qobject_cast<ProcessDirectoryJob *>(sender());
    ASSERT(job);

    _childIgnored |= job->_childIgnored;
    _childModified |= job->_childModified;

    if (job->_dirItem)
        emit _discoveryData->itemDiscovered(job->_dirItem);

    int count = _runningJobs.removeAll(job);
    ASSERT(count == 1);
    job->deleteLater();
    QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
}

int ProcessDirectoryJob::processSubJobs(int nbJobs)
{
    if (_queuedJobs.empty() && _runningJobs.empty() && _pendingAsyncJobs == 0) {
        _pendingAsyncJobs = -1; // We're finished, we don't want to emit finished again
        if (_dirItem) {
            if (_childModified && _dirItem->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // re-create directory that has modified contents
                _dirItem->_instruction = CSYNC_INSTRUCTION_NEW;
            }
            if (_childModified && _dirItem->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE && !_dirItem->isDirectory()) {
                // Replacing a directory by a file is a conflict, if the directory had modified children
                _dirItem->_instruction = CSYNC_INSTRUCTION_CONFLICT;
                if (_dirItem->_direction == SyncFileItem::Up) {
                    _dirItem->_type = ItemTypeDirectory;
                    _dirItem->_direction = SyncFileItem::Down;
                }
            }
            if (_childIgnored && _dirItem->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // Do not remove a directory that has ignored files
                _dirItem->_instruction = CSYNC_INSTRUCTION_NONE;
            }
        }
        emit finished();
    }

    int started = 0;
    foreach (auto *rj, _runningJobs) {
        started += rj->processSubJobs(nbJobs - started);
        if (started >= nbJobs)
            return started;
    }

    while (started < nbJobs && !_queuedJobs.empty()) {
        auto f = _queuedJobs.front();
        _queuedJobs.pop_front();
        _runningJobs.push_back(f);
        f->start();
        started++;
    }
    return started;
}

void ProcessDirectoryJob::dbError()
{
    _discoveryData->fatalError(tr("Error while reading the database"));
}

void ProcessDirectoryJob::addVirtualFileSuffix(QString &str) const
{
    if (auto vfs = _discoveryData->_syncOptions._vfs)
        str.append(vfs->fileSuffix());
}

bool ProcessDirectoryJob::hasVirtualFileSuffix(const QString &str) const
{
    if (!isVfsWithSuffix())
        return false;
    return str.endsWith(_discoveryData->_syncOptions._vfs->fileSuffix());
}

void ProcessDirectoryJob::chopVirtualFileSuffix(QString &str) const
{
    if (!isVfsWithSuffix())
        return;
    bool hasSuffix = hasVirtualFileSuffix(str);
    ASSERT(hasSuffix);
    if (hasSuffix)
        str.chop(_discoveryData->_syncOptions._vfs->fileSuffix().size());
}

DiscoverySingleDirectoryJob *ProcessDirectoryJob::startAsyncServerQuery()
{
    auto serverJob = new DiscoverySingleDirectoryJob(_discoveryData->_account,
        _discoveryData->_remoteFolder + _currentFolder._server, this);
    connect(serverJob, &DiscoverySingleDirectoryJob::etag, this, &ProcessDirectoryJob::etag);
    _discoveryData->_currentlyActiveJobs++;
    _pendingAsyncJobs++;
    connect(serverJob, &DiscoverySingleDirectoryJob::finished, this, [this, serverJob](const auto &results) {
        _discoveryData->_currentlyActiveJobs--;
        _pendingAsyncJobs--;
        if (results) {
            _serverNormalQueryEntries = *results;
            _serverQueryDone = true;
            if (!serverJob->_dataFingerprint.isEmpty() && _discoveryData->_dataFingerprint.isEmpty())
                _discoveryData->_dataFingerprint = serverJob->_dataFingerprint;
            if (_localQueryDone)
                this->process();
        } else {
            if (results.errorCode() == 403) {
                // 403 Forbidden can be sent by the server if the file firewall is active.
                // A file or directory should be ignored and sync must continue. See #3490
                qCWarning(lcDisco, "Directory access Forbidden (File Firewall?)");
                if (_dirItem) {
                    _dirItem->_instruction = CSYNC_INSTRUCTION_IGNORE;
                    _dirItem->_errorString = results.errorMessage();
                    emit this->finished();
                    return;
                }
            } else if (results.errorCode() == 503) {
                // The server usually replies with the custom "503 Storage not available"
                // if some path is temporarily unavailable. But in some cases a standard 503
                // is returned too. Thus we can't distinguish the two and will treat any
                // 503 as request to ignore the folder. See #3113 #2884.
                qCWarning(lcDisco(), "Storage was not available!");
                if (_dirItem) {
                    _dirItem->_instruction = CSYNC_INSTRUCTION_IGNORE;
                    _dirItem->_errorString = results.errorMessage();
                    emit this->finished();
                    return;
                }
            }
            emit _discoveryData->fatalError(tr("Server replied with an error while reading directory '%1' : %2")
                .arg(_currentFolder._server, results.errorMessage()));
        }
    });
    connect(serverJob, &DiscoverySingleDirectoryJob::firstDirectoryPermissions, this,
        [this](const RemotePermissions &perms) { _rootPermissions = perms; });
    serverJob->start();
    return serverJob;
}

bool ProcessDirectoryJob::runLocalQuery()
{
    QString localPath = _discoveryData->_localDir + _currentFolder._local;
    if (localPath.endsWith('/')) // Happens if _currentFolder._local.isEmpty()
        localPath.chop(1);
    auto dh = csync_vio_local_opendir(localPath);
    if (!dh) {
        qCInfo(lcDisco) << "Error while opening directory" << (localPath) << errno;
        QString errorString = tr("Error while opening directory %1").arg(localPath);
        if (errno == EACCES) {
            errorString = tr("Directory not accessible on client, permission denied");
            if (_dirItem) {
                _dirItem->_instruction = CSYNC_INSTRUCTION_IGNORE;
                _dirItem->_errorString = errorString;
                emit finished();
                return false;
            }
        } else if (errno == ENOENT) {
            errorString = tr("Directory not found: %1").arg(localPath);
        } else if (errno == ENOTDIR) {
            // Not a directory..
            // Just consider it is empty
            return true;
        }
        emit _discoveryData->fatalError(errorString);
        return false;
    }
    errno = 0;
    while (auto dirent = csync_vio_local_readdir(dh, _discoveryData->_syncOptions._vfs)) {
        if (dirent->type == ItemTypeSkip)
            continue;
        LocalInfo i;
        static QTextCodec *codec = QTextCodec::codecForName("UTF-8");
        ASSERT(codec);
        QTextCodec::ConverterState state;
        i.name = codec->toUnicode(dirent->path, dirent->path.size(), &state);
        if (state.invalidChars > 0 || state.remainingChars > 0) {
            _childIgnored = true;
            auto item = SyncFileItemPtr::create();
            item->_file = _currentFolder._target + i.name;
            item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            item->_status = SyncFileItem::NormalError;
            item->_errorString = tr("Filename encoding is not valid");
            emit _discoveryData->itemDiscovered(item);
            continue;
        }
        i.modtime = dirent->modtime;
        i.size = dirent->size;
        i.inode = dirent->inode;
        i.isDirectory = dirent->type == ItemTypeDirectory;
        i.isHidden = dirent->is_hidden;
        i.isSymLink = dirent->type == ItemTypeSoftLink;
        i.isVirtualFile = dirent->type == ItemTypeVirtualFile;
        _localNormalQueryEntries.push_back(i);
    }
    csync_vio_local_closedir(dh);
    if (errno != 0) {
        // Note: Windows vio converts any error into EACCES
        qCWarning(lcDisco) << "readdir failed for file in " << _currentFolder._local << " - errno: " << errno;
        emit _discoveryData->fatalError(tr("Error while reading directory %1").arg(localPath));
        return false;
    }
    return true;
}

bool ProcessDirectoryJob::isVfsWithSuffix() const
{
    auto vfs = _discoveryData->_syncOptions._vfs;
    return vfs && vfs->mode() == Vfs::WithSuffix;
}

}
