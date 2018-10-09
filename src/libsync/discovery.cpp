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


namespace OCC {

Q_LOGGING_CATEGORY(lcDisco, "sync.discovery", QtInfoMsg)

void ProcessDirectoryJob::start()
{
    qCInfo(lcDisco) << "STARTING" << _currentFolder._server << _queryServer << _currentFolder._local << _queryLocal;

    DiscoverySingleDirectoryJob *serverJob = nullptr;
    if (_queryServer == NormalQuery) {
        serverJob = new DiscoverySingleDirectoryJob(_discoveryData->_account,
            _discoveryData->_remoteFolder + _currentFolder._server, this);
        connect(serverJob, &DiscoverySingleDirectoryJob::finished, this, [this, serverJob](const auto &results) {
            if (results) {
                _serverEntries = *results;
                _hasServerEntries = true;
                if (!serverJob->_dataFingerprint.isEmpty() && _discoveryData->_dataFingerprint.isEmpty())
                    _discoveryData->_dataFingerprint = serverJob->_dataFingerprint;
                if (_hasLocalEntries)
                    process();
            } else {
                if (results.errorCode() == 403) {
                    // 403 Forbidden can be sent by the server if the file firewall is active.
                    // A file or directory should be ignored and sync must continue. See #3490
                    qCWarning(lcDisco, "Directory access Forbidden (File Firewall?)");
                    if (_dirItem) {
                        _dirItem->_instruction = CSYNC_INSTRUCTION_IGNORE;
                        _dirItem->_errorString = results.errorMessage();
                        emit finished();
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
                        emit finished();
                        return;
                    }
                }
                emit _discoveryData->fatalError(tr("Server replied with an error while reading directory '%1' : %2")
                    .arg(_currentFolder._server, results.errorMessage()));
                emit finished();
            }
        });
        connect(serverJob, &DiscoverySingleDirectoryJob::firstDirectoryPermissions, this,
            [this](const RemotePermissions &perms) { _rootPermissions = perms; });
        serverJob->start();
    } else {
        _hasServerEntries = true;
    }

    if (_queryLocal == NormalQuery) {
        if (!_discoveryData->_shouldDiscoverLocaly(_currentFolder._local)
            && (_currentFolder._local == _currentFolder._original || !_discoveryData->_shouldDiscoverLocaly(_currentFolder._original))) {
            _queryLocal = ParentNotChanged;
        }
    }

    if (_queryLocal == NormalQuery) {
        /*QDirIterator dirIt(_propagator->_localDir + _currentFolder);
        while (dirIt.hasNext()) {
            auto x = dirIt.next();
            LocalInfo i;
            i.name = dirIt.fileName();

        }*/
        auto dh = csync_vio_local_opendir((_discoveryData->_localDir + _currentFolder._local).toUtf8());
        if (!dh) {
            qCInfo(lcDisco) << "Error while opening directory" << (_discoveryData->_localDir + _currentFolder._local) << errno;
            if (serverJob) {
                serverJob->abort();
            }
            QString errorString = tr("Error while opening directory %1").arg(_discoveryData->_localDir + _currentFolder._local);
            if (errno == EACCES) {
                errorString = tr("Directory not accessible on client, permission denied");
                if (_dirItem) {
                    _dirItem->_instruction = CSYNC_INSTRUCTION_IGNORE;
                    _dirItem->_errorString = errorString;
                    emit finished();
                    return;
                }
            } else if (errno == ENOENT) {
                errorString = tr("Directory not found: %1").arg(_discoveryData->_localDir + _currentFolder._local);
            } else if (errno == ENOTDIR) {
                // Not a directory..
                // Just consider it is empty
                _hasLocalEntries = true;
                if (_hasServerEntries)
                    process();
                return;
            }
            emit _discoveryData->fatalError(errorString);
            emit finished();
            return;
        }
        errno = 0;
        while (auto dirent = csync_vio_local_readdir(dh)) {
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
            _localEntries.push_back(i);
        }
        if (errno != 0) {
            // Note: Windows vio converts any error into EACCES
            qCWarning(lcDisco) << "readdir failed for file in " << _currentFolder._local << " - errno: " << errno;
            emit _discoveryData->fatalError(tr("Error while reading directory %1").arg(_discoveryData->_localDir + _currentFolder._local));
            emit finished();
        }
        csync_vio_local_closedir(dh);
    }
    _hasLocalEntries = true;
    // Process is being called when both local and server entries are fetched.
    if (_hasServerEntries)
        process();
}

void ProcessDirectoryJob::process()
{
    ASSERT(_hasLocalEntries && _hasServerEntries);

    QString localDir;

    std::set<QString> entriesNames; // sorted
    QHash<QString, RemoteInfo> serverEntriesHash;
    QHash<QString, LocalInfo> localEntriesHash;
    QHash<QString, SyncJournalFileRecord> dbEntriesHash;
    for (auto &e : _serverEntries) {
        entriesNames.insert(e.name);
        serverEntriesHash[e.name] = std::move(e);
    }
    _serverEntries.clear();
    for (auto &e : _localEntries) {
        // Remove the virtual file suffix
        auto name = e.name;
        if (e.name.endsWith(_discoveryData->_syncOptions._virtualFileSuffix)) {
            e.isVirtualFile = true;
            name = e.name.left(e.name.size() - _discoveryData->_syncOptions._virtualFileSuffix.size());
            if (localEntriesHash.contains(name))
                continue; // If there is both a virtual file and a real file, we must keep the real file
        }
        entriesNames.insert(name);
        localEntriesHash[name] = std::move(e);
    }
    _localEntries.clear();

    if (_queryServer == ParentNotChanged || _queryLocal == ParentNotChanged) {
        // fetch all the name from the DB
        auto pathU8 = _currentFolder._original.toUtf8();
        // FIXME do that better (a query that do not get stuff recursively ?)
        if (!_discoveryData->_statedb->getFilesBelowPath(pathU8, [&](const SyncJournalFileRecord &rec) {
                if (rec._path.indexOf("/", pathU8.size() + 1) > 0)
                    return;
                auto name = pathU8.isEmpty() ? rec._path : QString::fromUtf8(rec._path.mid(pathU8.size() + 1));
                if (rec._type == ItemTypeVirtualFile || rec._type == ItemTypeVirtualFileDownload) {
                    name.chop(_discoveryData->_syncOptions._virtualFileSuffix.size());
                }
                entriesNames.insert(name);
                dbEntriesHash[name] = rec;
            })) {
            dbError();
            return;
        }
    }


    for (const auto &f : entriesNames) {
        auto localEntry = localEntriesHash.value(f);
        auto serverEntry = serverEntriesHash.value(f);
        SyncJournalFileRecord record = dbEntriesHash.value(f);
        PathTuple path;

        if ((localEntry.isValid() && localEntry.isVirtualFile)) {
            Q_ASSERT(localEntry.name.endsWith(_discoveryData->_syncOptions._virtualFileSuffix));
            path = _currentFolder.addName(localEntry.name);
            path._server.chop(_discoveryData->_syncOptions._virtualFileSuffix.size());
        } else if (_queryLocal == ParentNotChanged && record.isValid() && record._type == ItemTypeVirtualFile) {
            QString name = f + _discoveryData->_syncOptions._virtualFileSuffix;
            path = _currentFolder.addName(name);
            path._server.chop(_discoveryData->_syncOptions._virtualFileSuffix.size());
        } else {
            path = _currentFolder.addName(f);
        }

        // If the filename starts with a . we consider it a hidden file
        // For windows, the hidden state is also discovered within the vio
        // local stat function.
        // Recall file shall not be ignored (#4420)
        bool isHidden = localEntry.isHidden || (f[0] == '.' && f != QLatin1String(".sys.admin#recall#"));
        if (handleExcluded(path._target, localEntry.isDirectory || serverEntry.isDirectory, isHidden, localEntry.isSymLink))
            continue;

        if (_queryServer != ParentNotChanged && _queryLocal != ParentNotChanged && !_discoveryData->_statedb->getFileRecord(path._original, &record)) {
            dbError();
            return;
        }
        if (_queryServer == InBlackList || _discoveryData->isInSelectiveSyncBlackList(path._original)) {
            processBlacklisted(path, localEntry, record);
            continue;
        }
        processFile(std::move(path), localEntry, serverEntry, record);
    }

    progress();
}

bool ProcessDirectoryJob::handleExcluded(const QString &path, bool isDirectory, bool isHidden, bool isSymlink)
{
    // FIXME! call directly, without char* conversion
    auto excluded = _discoveryData->_excludes->csyncTraversalMatchFun()(path.toUtf8(), isDirectory ? ItemTypeDirectory : ItemTypeFile);

    // FIXME: move to ExcludedFiles 's regexp ?
    bool isInvalidPattern = false;
    if (excluded == CSYNC_NOT_EXCLUDED && !_discoveryData->_invalidFilenamePattern.isEmpty()) {
        const QRegExp invalidFilenameRx(_discoveryData->_invalidFilenamePattern);
        if (path.contains(invalidFilenameRx)) {
            excluded = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
            isInvalidPattern = true;
        }
    }
    if (excluded == CSYNC_NOT_EXCLUDED && _discoveryData->_ignoreHiddenFiles && isHidden) {
        excluded = CSYNC_FILE_EXCLUDE_HIDDEN;
    }

    auto localCodec = QTextCodec::codecForLocale();
    if (localCodec->mibEnum() != 106) {
        // If the locale codec is not UTF-8, we must check that the filename from the server can
        // be encoded in the local file system.
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

    if (_queryServer == NormalQuery && serverEntry.isValid()) {
        processFileAnalyzeRemoteInfo(item, path, localEntry, serverEntry, dbEntry);
        return;
    } else if (_queryServer == ParentNotChanged && dbEntry._type == ItemTypeVirtualFileDownload) {
        // download virtual file
        item->_direction = SyncFileItem::Down;
        item->_instruction = CSYNC_INSTRUCTION_NEW;
        Q_ASSERT(item->_file.endsWith(_discoveryData->_syncOptions._virtualFileSuffix));
        item->_file.chop(_discoveryData->_syncOptions._virtualFileSuffix.size());
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
        QByteArray checksum = ComputeChecksum::computeNow(path, type);
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
    item->_previousSize = localEntry.size;
    item->_previousModtime = localEntry.modtime;
    item->_directDownloadUrl = serverEntry.directDownloadUrl;
    item->_directDownloadCookies = serverEntry.directDownloadCookies;
    if (!dbEntry.isValid()) { // New file on the server
        item->_instruction = CSYNC_INSTRUCTION_NEW;
        item->_direction = SyncFileItem::Down;
        item->_modtime = serverEntry.modtime;
        item->_size = serverEntry.size;

        auto postProcessNew = [item, this, path, serverEntry] {
            if (item->isDirectory()) {
                if (_discoveryData->checkSelectiveSyncNewFolder(path._server, serverEntry.remotePerm)) {
                    return;
                }
            }
            // Turn new remote files into virtual files if the option is enabled.
            if (_discoveryData->_syncOptions._newFilesAreVirtual && item->_type == ItemTypeFile) {
                item->_type = ItemTypeVirtualFile;
                item->_file.append(_discoveryData->_syncOptions._virtualFileSuffix);
            }
        };

        if (!localEntry.isValid()) {
            // Check for renames (if there is a file with the same file id)
            bool done = false;
            bool async = false;
            // This function will be executed for every candidate
            auto renameCandidateProcessing = [&](const OCC::SyncJournalFileRecord &base) {
                if (done)
                    return;
                if (!base.isValid())
                    return;

                if (base._type == ItemTypeVirtualFileDownload) {
                    // Remote rename of a virtual file we have locally scheduled
                    // for download. We just consider this NEW but mark it for download.
                    item->_type = ItemTypeVirtualFileDownload;
                    done = true;
                    return;
                }

                // Some things prohibit rename detection entirely.
                // Since we don't do the same checks again in reconcile, we can't
                // just skip the candidate, but have to give up completely.
                if (base._type != item->_type && base._type != ItemTypeVirtualFile) {
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
                if (base._type == ItemTypeVirtualFile && item->_type == ItemTypeFile) {
                    // Ignore if the base is a virtual files
                    return;
                }

                if (_discoveryData->_renamedItems.contains(originalPath)) {
                    qCInfo(lcDisco, "folder already has a rename entry, skipping");
                    return;
                }

                if (item->_type == ItemTypeFile) {
                    csync_file_stat_t buf;
                    if (csync_vio_local_stat((_discoveryData->_localDir + originalPath).toUtf8(), &buf)) {
                        qCInfo(lcDisco) << "Local file does not exist anymore." << originalPath;
                        return;
                    }
                    if (buf.modtime != base._modtime || buf.size != base._fileSize || buf.type != ItemTypeFile) {
                        qCInfo(lcDisco) << "File has changed locally, not a rename." << originalPath;
                        return;
                    }
                } else {
                    if (!QFileInfo(_discoveryData->_localDir + originalPath).isDir()) {
                        qCInfo(lcDisco) << "Local directory does not exist anymore." << originalPath;
                        return;
                    }
                }

                bool wasDeletedOnServer = false;
                auto it = _discoveryData->_deletedItem.find(originalPath);
                if (it != _discoveryData->_deletedItem.end()) {
                    ASSERT((*it)->_instruction == CSYNC_INSTRUCTION_REMOVE);
                    (*it)->_instruction = CSYNC_INSTRUCTION_NONE;
                    wasDeletedOnServer = true;
                }
                auto otherJob = _discoveryData->_queuedDeletedDirectories.take(originalPath);
                if (otherJob) {
                    delete otherJob;
                    wasDeletedOnServer = true;
                }

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
                        if (etag.errorCode() != 404 ||
                            // Somehow another item claimed this original path, consider as if it existed
                            _discoveryData->_renamedItems.contains(originalPath)) {
                            // If the file exist or if there is another error, consider it is a new file.
                            postProcessNew();
                        } else {
                            // The file do not exist, it is a rename

                            // In case the deleted item was discovered in parallel
                            auto it = _discoveryData->_deletedItem.find(originalPath);
                            if (it != _discoveryData->_deletedItem.end()) {
                                ASSERT((*it)->_instruction == CSYNC_INSTRUCTION_REMOVE);
                                (*it)->_instruction = CSYNC_INSTRUCTION_NONE;
                            }
                            delete _discoveryData->_queuedDeletedDirectories.take(originalPath);

                            postProcessRename(path);
                        }
                        processFileFinalize(item, path, item->isDirectory(), item->_instruction == CSYNC_INSTRUCTION_RENAME ? NormalQuery : ParentDontExist, _queryServer);
                        _pendingAsyncJobs--;
                        progress();
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
        }

        if (item->_instruction == CSYNC_INSTRUCTION_NEW) {
            postProcessNew();
        }
    } else if (serverEntry.isDirectory != (dbEntry._type == ItemTypeDirectory)) {
        // If the type of the entity changed, it's like NEW, but
        // needs to delete the other entity first.
        item->_instruction = CSYNC_INSTRUCTION_TYPE_CHANGE;
        item->_direction = SyncFileItem::Down;
        item->_modtime = serverEntry.modtime;
        item->_size = serverEntry.size;
    } else if (dbEntry._type == ItemTypeVirtualFileDownload) {
        item->_direction = SyncFileItem::Down;
        item->_instruction = CSYNC_INSTRUCTION_NEW;
        item->_file = _currentFolder._target + QLatin1Char('/') + serverEntry.name;
        item->_type = ItemTypeVirtualFileDownload;
    } else if (dbEntry._etag != serverEntry.etag) {
        item->_direction = SyncFileItem::Down;
        item->_modtime = serverEntry.modtime;
        item->_size = serverEntry.size;
        if (serverEntry.isDirectory && dbEntry._type == ItemTypeDirectory) {
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

    if ((dbEntry.isValid() && dbEntry._type == ItemTypeVirtualFile) || (localEntry.isValid() && localEntry.isVirtualFile && item->_type != ItemTypeVirtualFileDownload)) {
        // Do not download virtual files
        if (serverModified || dbEntry._type != ItemTypeVirtualFile)
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
        serverModified = false;
        item->_type = ItemTypeVirtualFile;
    }
    if (!_dirItem || _dirItem->_direction == SyncFileItem::Up) {
        _childModified |= serverModified;
    }
    if (localEntry.isValid()) {
        item->_inode = localEntry.inode;
        bool typeChange = dbEntry.isValid() && localEntry.isDirectory != (dbEntry._type == ItemTypeDirectory);
        if (dbEntry.isValid() && localEntry.isVirtualFile) {
            if (dbEntry._type == ItemTypeFile) {
                // If we find what looks to be a spurious "abc.owncloud" the base file "abc"
                // might have been renamed to that. Make sure that the base file is not
                // deleted from the server.
                if (dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) {
                    qCInfo(lcDisco) << "Base file was renamed to virtual file:" << item->_file;
                    Q_ASSERT(item->_file.endsWith(_discoveryData->_syncOptions._virtualFileSuffix));
                    item->_direction = SyncFileItem::Down;
                    item->_instruction = CSYNC_INSTRUCTION_NEW;
                    item->_type = ItemTypeVirtualFile;
                }
            } else if (item->_type != ItemTypeVirtualFileDownload) {
                item->_type = ItemTypeVirtualFile;
            }

            if (noServerEntry) {
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            }
        } else if (!dbEntry.isValid() && localEntry.isVirtualFile && !noServerEntry) {
            // Somehow there is a missing DB entry while the virtual file already exists.
            // The instruction should already be set correctly.
            ASSERT(item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA);
            ASSERT(item->_type == ItemTypeVirtualFile)
            ASSERT(item->_file.endsWith(_discoveryData->_syncOptions._virtualFileSuffix));
            item->_file.chop(_discoveryData->_syncOptions._virtualFileSuffix.size());
        } else if (dbEntry.isValid() && !typeChange && ((dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) || (localEntry.isDirectory && dbEntry._type == ItemTypeDirectory))) {
            // Local file unchanged.
            if (noServerEntry) {
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else if (!serverModified && dbEntry._inode != localEntry.inode) {
                item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                item->_direction = SyncFileItem::Down; // Does not matter
            }
        } else if (serverModified || dbEntry._type == ItemTypeVirtualFile) {
            if (serverEntry.isDirectory && localEntry.isDirectory) {
                // Folders of the same path are always considered equals
                item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            } else {
                // It could be a conflict even if size and mtime match!
                //
                // In older client versions we always treated these cases as a
                // non-conflict. This behavior is preserved in case the server
                // doesn't provide a content checksum.
                //
                // When it does have one, however, we do create a job, but the job
                // will compare hashes and avoid the download if possible.
                QByteArray remoteChecksumHeader = serverEntry.checksumHeader;
                if (!remoteChecksumHeader.isEmpty()) {
                    // Do we have an UploadInfo for this?
                    // Maybe the Upload was completed, but the connection was broken just before
                    // we recieved the etag (Issue #5106)
                    auto up = _discoveryData->_statedb->getUploadInfo(path._original);
                    if (up._valid && up._contentChecksum == remoteChecksumHeader) {
                        // Solve the conflict into an upload, or nothing
                        item->_instruction = up._modtime == localEntry.modtime && up._size == localEntry.size
                            ? CSYNC_INSTRUCTION_UPDATE_METADATA : CSYNC_INSTRUCTION_SYNC;
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
                            _discoveryData->_statedb->setFileRecordMetadata(rec);
                        }
                    } else {
                        item->_instruction = CSYNC_INSTRUCTION_CONFLICT;
                        item->_direction = SyncFileItem::None;
                    }
                } else {
                    // If the size or mtime is different, it's definitely a conflict.
                    bool isConflict = (serverEntry.size != localEntry.size) || (serverEntry.modtime != localEntry.modtime);

                    // SO: If there is no checksum, we can have !is_conflict here
                    // even though the files have different content! This is an
                    // intentional tradeoff. Downloading and comparing files would
                    // be technically correct in this situation but leads to too
                    // much waste.
                    // In particular this kind of NEW/NEW situation with identical
                    // sizes and mtimes pops up when the local database is lost for
                    // whatever reason.
                    item->_instruction = isConflict ? CSYNC_INSTRUCTION_CONFLICT : CSYNC_INSTRUCTION_UPDATE_METADATA;
                    item->_direction = isConflict ? SyncFileItem::None : SyncFileItem::Down;
                }
            }
            if (dbEntry._type == ItemTypeVirtualFile)
                item->_type = ItemTypeVirtualFileDownload;
            if (item->_file.endsWith(_discoveryData->_syncOptions._virtualFileSuffix)) {
                item->_file.chop(_discoveryData->_syncOptions._virtualFileSuffix.size());
                item->_type = ItemTypeVirtualFileDownload;
            }
            item->_previousSize = localEntry.size;
            item->_previousModtime = localEntry.modtime;
        } else if (typeChange) {
            item->_instruction = CSYNC_INSTRUCTION_TYPE_CHANGE;
            item->_direction = SyncFileItem::Up;
            item->_checksumHeader.clear();
            item->_size = localEntry.size;
            item->_modtime = localEntry.modtime;
            item->_type = localEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile;
            if (!_dirItem || _dirItem->_direction == SyncFileItem::Down) {
                _childModified = true;
            }
        } else if (!dbEntry.isValid()) { // New local file
            item->_instruction = CSYNC_INSTRUCTION_NEW;
            item->_direction = SyncFileItem::Up;
            item->_checksumHeader.clear();
            item->_size = localEntry.size;
            item->_modtime = localEntry.modtime;
            item->_type = localEntry.isDirectory ? ItemTypeDirectory : localEntry.isVirtualFile ? ItemTypeVirtualFile : ItemTypeFile;
            if (!_dirItem || _dirItem->_direction == SyncFileItem::Down) {
                _childModified = true;
            }

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
                       || item->_type == ItemTypeDirectory || localEntry.isVirtualFile);

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
            if (isMove) {
                auto destPerms = !_rootPermissions.isNull() ? _rootPermissions
                                                            : _dirItem ? _dirItem->_remotePerm : _rootPermissions;
                auto filePerms = base._remotePerm; // Technicly we should use the one from the server, but we'll assume it is the same
                //true when it is just a rename in the same directory. (not a move)
                bool isRename = originalPath.startsWith(_currentFolder._original)
                    && originalPath.lastIndexOf('/') == _currentFolder._original.size();
                // Check if we are allowed to move to the destination.
                bool destinationOK = true;
                if (isRename || destPerms.isNull()) {
                    // no need to check for the destination dir permission
                    destinationOK = true;
                } else if (item->isDirectory() && !destPerms.hasPermission(RemotePermissions::CanAddSubDirectories)) {
                    destinationOK = false;
                } else if (!item->isDirectory() && !destPerms.hasPermission(RemotePermissions::CanAddFile)) {
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
                        _discoveryData->_statedb->avoidRenamesOnNextSync(base._path);
                    }
                    isMove = false;
                }
            }

            if (isMove) {
                QByteArray oldEtag;
                auto it = _discoveryData->_deletedItem.find(originalPath);
                if (it != _discoveryData->_deletedItem.end()) {
                    if ((*it)->_instruction != CSYNC_INSTRUCTION_REMOVE && (*it)->_type != ItemTypeVirtualFile)
                        isMove = false;
                    else
                        (*it)->_instruction = CSYNC_INSTRUCTION_NONE;
                    oldEtag = (*it)->_etag;
                    if (!item->isDirectory() && oldEtag != base._etag) {
                        isMove = false;
                    }
                }
                if (auto deleteJob = _discoveryData->_queuedDeletedDirectories.value(originalPath)) {
                    oldEtag = deleteJob->_dirItem->_etag;
                    delete _discoveryData->_queuedDeletedDirectories.take(originalPath);
                }

                auto processRename = [item, originalPath, base, this](PathTuple &path) {
                    auto adjustedOriginalPath = _discoveryData->adjustRenamedPath(originalPath);
                    _discoveryData->_renamedItems.insert(originalPath, path._target);
                    item->_modtime = base._modtime;
                    item->_inode = base._inode;
                    item->_instruction = CSYNC_INSTRUCTION_RENAME;
                    item->_direction = SyncFileItem::Up;
                    item->_renameTarget = path._target;
                    item->_file = adjustedOriginalPath;
                    item->_originalFile = originalPath;
                    item->_fileId = base._fileId;
                    item->_remotePerm = base._remotePerm;
                    item->_etag = base._etag;
                    item->_type = base._type;
                    path._original = originalPath;
                    path._server = adjustedOriginalPath;
                    qCInfo(lcDisco) << "Rename detected (up) " << item->_file << " -> " << item->_renameTarget;
                };
                if (isMove && !oldEtag.isEmpty()) {
                    recurseQueryServer = oldEtag == base._etag ? ParentNotChanged : NormalQuery;
                    processRename(path);
                } else if (isMove) {
                    // We must query the server to know if the etag has not changed
                    _pendingAsyncJobs++;
                    QString serverOriginalPath = originalPath;
                    if (localEntry.isVirtualFile)
                        serverOriginalPath.chop(_discoveryData->_syncOptions._virtualFileSuffix.size());
                    auto job = new RequestEtagJob(_discoveryData->_account, serverOriginalPath, this);
                    connect(job, &RequestEtagJob::finishedWithResult, this, [=](const Result<QString> &etag) mutable {
                        if (!etag || (*etag != base._etag && !item->isDirectory()) || _discoveryData->_renamedItems.contains(originalPath)) {
                            qCInfo(lcDisco) << "Can't rename because the etag has changed or the directory is gone" << originalPath;
                            // Can't be a rename, leave it as a new.
                            postProcessLocalNew();
                        } else {
                            // In case the deleted item was discovered in parallel
                            auto it = _discoveryData->_deletedItem.find(originalPath);
                            if (it != _discoveryData->_deletedItem.end()) {
                                ASSERT((*it)->_instruction == CSYNC_INSTRUCTION_REMOVE);
                                (*it)->_instruction = CSYNC_INSTRUCTION_NONE;
                            }
                            delete _discoveryData->_queuedDeletedDirectories.take(originalPath);

                            processRename(path);
                            recurseQueryServer = *etag == base._etag ? ParentNotChanged : NormalQuery;
                        }
                        processFileFinalize(item, path, item->isDirectory(), NormalQuery, recurseQueryServer);
                        _pendingAsyncJobs--;
                        progress();
                    });
                    job->start();
                    return;
                } else {
                    postProcessLocalNew();
                }
            } else {
                postProcessLocalNew();
            }
        } else {
            item->_instruction = CSYNC_INSTRUCTION_SYNC;
            if (noServerEntry) {
                // Special case! deleted on server, modified on client, the instruction is then NEW
                item->_instruction = CSYNC_INSTRUCTION_NEW;
            }
            item->_direction = SyncFileItem::Up;
            item->_checksumHeader.clear();
            item->_size = localEntry.size;
            item->_modtime = localEntry.modtime;
            item->_previousSize = dbEntry._fileSize;
            item->_previousModtime = dbEntry._modtime;
            if (!_dirItem || _dirItem->_direction == SyncFileItem::Down) {
                _childModified = true;
            }

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
    } else if (_queryLocal == ParentNotChanged && dbEntry.isValid()) {
        if (noServerEntry) {
            // Not modified locally (ParentNotChanged), bit not on the server:  Removed on the server.
            item->_instruction = CSYNC_INSTRUCTION_REMOVE;
            item->_direction = SyncFileItem::Down;
        }
    } else if (noServerEntry) {
        // Not locally, not on the server. The entry is stale!
        qCInfo(lcDisco) << "Stale DB entry";
        return;
    } else if (dbEntry._type == ItemTypeVirtualFile) {
        // If the virtual file is removed, recreate it.
        item->_instruction = CSYNC_INSTRUCTION_NEW;
        item->_direction = SyncFileItem::Down;
        item->_type = ItemTypeVirtualFile;
        item->_file.append(_discoveryData->_syncOptions._virtualFileSuffix);
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

    bool recurse = item->isDirectory() || localEntry.isDirectory || serverEntry.isDirectory;
    if (_queryLocal != NormalQuery && _queryServer != NormalQuery && !item->_isRestoration)
        recurse = false;

    auto recurseQueryLocal = _queryLocal == ParentNotChanged ? ParentNotChanged : localEntry.isDirectory || item->_instruction == CSYNC_INSTRUCTION_RENAME ? NormalQuery : ParentDontExist;
    processFileFinalize(item, path, recurse, recurseQueryLocal, recurseQueryServer);
}

void ProcessDirectoryJob::processFileFinalize(
    const SyncFileItemPtr &item, PathTuple path, bool recurse,
    QueryMode recurseQueryLocal, QueryMode recurseQueryServer)
{
    if (path._original != path._target && (item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA || item->_instruction == CSYNC_INSTRUCTION_NONE)) {
        ASSERT(_dirItem && _dirItem->_instruction == CSYNC_INSTRUCTION_RENAME);
        // This is because otherwise subitems are not updated!  (ideally renaming a directory could
        // update the database for all items!  See PropagateDirectory::slotSubJobsFinished)
        item->_instruction = CSYNC_INSTRUCTION_RENAME;
        item->_renameTarget = path._target;
        item->_direction = _dirItem->_direction;
    }

    qCInfo(lcDisco) << "Discovered" << item->_file << item->_instruction << item->_direction << item->_type;

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
            if (item->_type == ItemTypeVirtualFile && !path._original.endsWith(_discoveryData->_syncOptions._virtualFileSuffix))
                path._original.append(_discoveryData->_syncOptions._virtualFileSuffix);
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
    item->_inode = localEntry.inode;
    if (dbEntry.isValid() && ((dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) || (localEntry.isDirectory && dbEntry._type == ItemTypeDirectory))) {
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
    progress();
}

void ProcessDirectoryJob::progress()
{
    int maxRunning = 3; // FIXME
    if (_pendingAsyncJobs + _runningJobs.size() > maxRunning)
        return;

    if (!_queuedJobs.empty()) {
        auto f = _queuedJobs.front();
        _queuedJobs.pop_front();
        _runningJobs.push_back(f);
        f->start();
        return;
    }
    if (_runningJobs.empty() && _pendingAsyncJobs == 0) {
        if (_dirItem) {
            if (_childModified && _dirItem->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // re-create directory that has modified contents
                _dirItem->_instruction = CSYNC_INSTRUCTION_NEW;
            }
            if (_childModified && _dirItem->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE) {
                if (_dirItem->_direction == SyncFileItem::Up) {
                    _dirItem->_type = ItemTypeDirectory;
                    _dirItem->_direction = SyncFileItem::Down;
                }
                _dirItem->_instruction = CSYNC_INSTRUCTION_CONFLICT;
            }
            if (_childIgnored && _dirItem->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // Do not remove a directory that has ignored files
                _dirItem->_instruction = CSYNC_INSTRUCTION_NONE;
            }
        }
        emit finished();
    }
}

void ProcessDirectoryJob::dbError()
{
    _discoveryData->fatalError(tr("Error while reading the database"));
    emit finished();
}
}
