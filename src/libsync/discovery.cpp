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

static RemoteInfo remoteInfoFromCSync(const csync_file_stat_t &x)
{
    RemoteInfo ri;
    ri.name = QFileInfo(QString::fromUtf8(x.path)).fileName();
    ri.etag = x.etag;
    ri.fileId = x.file_id;
    ri.checksumHeader = x.checksumHeader;
    ri.modtime = x.modtime;
    ri.size = x.size;
    ri.isDirectory = x.type == ItemTypeDirectory;
    ri.remotePerm = x.remotePerm;
    return ri;
}

DiscoverServerJob::DiscoverServerJob(const AccountPtr &account, const QString &path, QObject *parent)
    : DiscoverySingleDirectoryJob(account, path, parent)
{
    connect(this, &DiscoverySingleDirectoryJob::finishedWithResult, this, [this] {
        auto csync_results = takeResults();
        QVector<RemoteInfo> results;
        std::transform(csync_results.begin(), csync_results.end(), std::back_inserter(results),
            [](const auto &x) { return remoteInfoFromCSync(*x); });
        emit this->finished(results);
    });

    connect(this, &DiscoverySingleDirectoryJob::finishedWithError, this,
        [this](int code, const QString &msg) {
            emit this->finished({ code, msg });
        });
}

void ProcessDirectoryJob::start()
{
    qCInfo(lcDisco) << "STARTING" << _currentFolder._server << _queryServer << _currentFolder._local << _queryLocal;

    DiscoverServerJob *serverJob = nullptr;
    if (_queryServer == NormalQuery) {
        serverJob = new DiscoverServerJob(_discoveryData->_account, _discoveryData->_remoteFolder + _currentFolder._server, this);
        connect(serverJob, &DiscoverServerJob::finished, this, [this](const auto &results) {
            if (results) {
                _serverEntries = *results;
                _hasServerEntries = true;
                if (_hasLocalEntries)
                    process();
            } else {
                qWarning() << results.errorMessage();
                qFatal("TODO: ERROR HANDLING");
            }
        });
        serverJob->start();
    } else {
        _hasServerEntries = true;
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
            serverJob->abort();
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
                if (_dirItem) {
                    _dirItem->_instruction = CSYNC_INSTRUCTION_IGNORE;
                    _dirItem->_errorString = errorString;
                    emit finished();
                    return;
                }
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
        while (auto dirent = csync_vio_local_readdir(dh)) {
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
                emit itemDiscovered(item);
                continue;
            }
            i.modtime = dirent->modtime;
            i.size = dirent->size;
            i.inode = dirent->inode;
            i.isDirectory = dirent->type == ItemTypeDirectory;
            i.isHidden = dirent->is_hidden;
            if (dirent->type != ItemTypeDirectory && dirent->type != ItemTypeFile)
                qFatal("FIXME:  NEED TO CARE ABOUT THE OTHER STUFF ");
            _localEntries.push_back(i);
        }
        csync_vio_local_closedir(dh);
    }
    _hasLocalEntries = true;
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
    for (auto &e : _serverEntries) {
        entriesNames.insert(e.name);
        serverEntriesHash[e.name] = std::move(e);
    }
    _serverEntries.clear();
    for (auto &e : _localEntries) {
        entriesNames.insert(e.name);
        localEntriesHash[e.name] = std::move(e);
    }
    _localEntries.clear();

    if (_queryServer == ParentNotChanged) {
        // fetch all the name from the DB
        auto pathU8 = _currentFolder._original.toUtf8();
        // FIXME cache, and do that better (a query that do not get stuff recursively)
        if (!_discoveryData->_statedb->getFilesBelowPath(pathU8, [&](const SyncJournalFileRecord &rec) {
                if (rec._path.indexOf("/", pathU8.size() + 1) > 0)
                    return;
                entriesNames.insert(QString::fromUtf8(rec._path.mid(pathU8.size() + 1)));
            })) {
            qFatal("TODO: DB ERROR HANDLING");
        }
    }

    for (const auto &f : entriesNames) {
        auto path = _currentFolder.addName(f);
        auto localEntry = localEntriesHash.value(f);
        auto serverEntry = serverEntriesHash.value(f);

        // If the filename starts with a . we consider it a hidden file
        // For windows, the hidden state is also discovered within the vio
        // local stat function.
        // Recall file shall not be ignored (#4420)
        bool isHidden = localEntry.isHidden || (f[0] == '.' && f != QLatin1String(".sys.admin#recall#"));
        if (handleExcluded(path._target, localEntry.isDirectory || serverEntry.isDirectory, isHidden))
            continue;

        SyncJournalFileRecord record;
        if (!_discoveryData->_statedb->getFileRecord(path._original, &record)) {
            qFatal("TODO: DB ERROR HANDLING");
        }
        if (_queryServer == InBlackList || _discoveryData->isInSelectiveSyncBlackList(path._original)) {
            processBlacklisted(path, localEntry, record);
            continue;
        }
        processFile(std::move(path), localEntry, serverEntry, record);
    }

    progress();
}

bool ProcessDirectoryJob::handleExcluded(const QString &path, bool isDirectory, bool isHidden)
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

    if (excluded == CSYNC_NOT_EXCLUDED /* FIXME && item->_type != ItemTypeSoftLink */) {
        return false;
    } else if (excluded == CSYNC_FILE_SILENTLY_EXCLUDED || excluded == CSYNC_FILE_EXCLUDE_AND_REMOVE) {
        return true;
    }

    auto item = SyncFileItemPtr::create();
    item->_file = path;
    item->_instruction = CSYNC_INSTRUCTION_IGNORE;

#if 0
    FIXME: soft links
    if( fs->type == ItemTypeSoftLink ) {
        fs->error_status = CSYNC_STATUS_INDIVIDUAL_IS_SYMLINK; /* Symbolic links are ignored. */
#endif
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
        qFatal("TODO: conflicts");
#if 0
        item->_status = SyncFileItem::Conflict;
        if (_propagator->account()->capabilities().uploadConflictFiles()) {
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
#endif
        break;
    case CSYNC_FILE_EXCLUDE_CANNOT_ENCODE: // FIXME!
        item->_errorString = tr("The filename cannot be encoded on your file system.");
        break;
    }

    _childIgnored = true;
    emit itemDiscovered(item);
    return true;
}

void ProcessDirectoryJob::processFile(PathTuple path,
    const LocalInfo &localEntry, const RemoteInfo &serverEntry,
    const SyncJournalFileRecord &dbEntry)
{
    const char *hasServer = serverEntry.isValid() ? "true" : _queryServer == ParentNotChanged ? "db" : "false";
    qCInfo(lcDisco).nospace() << "Processing " << path._original
                              << " | valid: " << dbEntry.isValid() << "/" << localEntry.isValid() << "/" << hasServer
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

    auto computeLocalChecksum = [&](const QByteArray &type, const QString &path) {
        if (!type.isEmpty()) {
            // TODO: compute async?
            QByteArray checksum = ComputeChecksum::computeNow(_discoveryData->_localDir + path, type);
            if (!checksum.isEmpty()) {
                item->_checksumHeader = makeChecksumHeader(type, checksum);
                return true;
            }
        }
        return false;
    };

    auto recurseQueryServer = _queryServer;
    if (recurseQueryServer != ParentNotChanged && !serverEntry.isValid())
        recurseQueryServer = ParentDontExist;

    if (_queryServer == NormalQuery && serverEntry.isValid()) {
        item->_checksumHeader = serverEntry.checksumHeader;
        item->_fileId = serverEntry.fileId;
        item->_remotePerm = serverEntry.remotePerm;
        item->_type = serverEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile;
        item->_etag = serverEntry.etag;
        item->_previousSize = localEntry.size;
        item->_previousModtime = localEntry.modtime;
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
                        item->_type = ItemTypeVirtualFile;
                        item->_file.append(_discoveryData->_syncOptions._virtualFileSuffix);
                        qFatal("FIXME rename virtual file"); // Need to be tested, i'm not sure about it now
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

                            qCInfo(lcDisco) << "Discovered" << item->_file << item->_instruction << item->_direction << item->isDirectory();

                            if (item->isDirectory()) {
                                auto job = new ProcessDirectoryJob(item, recurseQueryServer,
                                    item->_instruction == CSYNC_INSTRUCTION_RENAME ? NormalQuery : ParentDontExist,
                                    _discoveryData, this);
                                job->_currentFolder = path;
                                connect(job, &ProcessDirectoryJob::itemDiscovered, this, &ProcessDirectoryJob::itemDiscovered);
                                connect(job, &ProcessDirectoryJob::finished, this, &ProcessDirectoryJob::subJobFinished);
                                _queuedJobs.push_back(job);
                            } else {
                                emit itemDiscovered(item);
                            }
                            _pendingAsyncJobs--;
                            progress();
                        });
                        job->start();
                        done = true; // Ideally, if the origin still exist on the server, we should continue searching...  but that'd be difficult
                        item = nullptr;
                    }
                };
                if (!_discoveryData->_statedb->getFileRecordsByFileId(serverEntry.fileId, renameCandidateProcessing)) {
                    qFatal("TODO: Handle DB ERROR");
                }
                if (!item) {
                    return; // We wend async
                }
            }

            if (item->_instruction == CSYNC_INSTRUCTION_NEW) {
                postProcessNew();
            }

        } else if (dbEntry._etag != serverEntry.etag) {
            item->_direction = SyncFileItem::Down;
            item->_modtime = serverEntry.modtime;
            item->_size = serverEntry.size;
            if (serverEntry.isDirectory && dbEntry._type == ItemTypeDirectory) {
                item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            } else if (!localEntry.isValid()) {
                // Deleted locally, changed on server
                item->_instruction = CSYNC_INSTRUCTION_NEW;
            } else {
                item->_instruction = CSYNC_INSTRUCTION_SYNC;
            }
        } else if (dbEntry._remotePerm != serverEntry.remotePerm || dbEntry._fileId != serverEntry.fileId) {
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            item->_direction = SyncFileItem::Down;
        } else {
            recurseQueryServer = ParentNotChanged;
        }
    }
    bool serverModified = item->_instruction == CSYNC_INSTRUCTION_NEW || item->_instruction == CSYNC_INSTRUCTION_SYNC || item->_instruction == CSYNC_INSTRUCTION_RENAME;
    _childModified |= serverModified;
    if (localEntry.isValid()) {
        item->_inode = localEntry.inode;
        if (dbEntry.isValid() && ((dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) || (localEntry.isDirectory && dbEntry._type == ItemTypeDirectory))) {
            if (_queryServer != ParentNotChanged && !serverEntry.isValid()) {
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else if (!serverModified && dbEntry._inode != localEntry.inode) {
                item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                item->_direction = SyncFileItem::Down; // Does not matter
            }
        } else if (serverModified) {
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
                        item->_instruction = up._modtime == localEntry.modtime ? CSYNC_INSTRUCTION_UPDATE_METADATA : CSYNC_INSTRUCTION_SYNC;

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
                }
            }
            item->_direction = item->_instruction == CSYNC_INSTRUCTION_CONFLICT ? SyncFileItem::None : SyncFileItem::Down;
        } else if (!dbEntry.isValid()) { // New local file
            item->_instruction = CSYNC_INSTRUCTION_NEW;
            item->_direction = SyncFileItem::Up;
            item->_checksumHeader.clear();
            item->_size = localEntry.size;
            item->_modtime = localEntry.modtime;
            item->_type = localEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile;
            _childModified = true;

            // Check if it is a rename
            OCC::SyncJournalFileRecord base;
            if (!_discoveryData->_statedb->getFileRecordByInode(localEntry.inode, &base)) {
                qFatal("TODO: handle DB Errors");
            }
            bool isRename = base.isValid() && base._type == item->_type
                && ((base._modtime == localEntry.modtime && base._fileSize == localEntry.size) || item->_type == ItemTypeDirectory);

            if (isRename) {
                //  The old file must have been deleted.
                isRename = !QFile::exists(_discoveryData->_localDir + base._path);
            }

            // Verify the checksum where possible
            if (isRename && !base._checksumHeader.isEmpty() && item->_type == ItemTypeFile) {
                if (computeLocalChecksum(parseChecksumHeaderType(base._checksumHeader), path._original)) {
                    qCInfo(lcDisco) << "checking checksum of potential rename " << path._original << item->_checksumHeader << base._checksumHeader;
                    isRename = item->_checksumHeader == base._checksumHeader;
                }
            }
            auto originalPath = QString::fromUtf8(base._path);
            if (isRename && _discoveryData->_renamedItems.contains(originalPath))
                isRename = false;
            if (isRename) {
                QByteArray oldEtag;
                auto it = _discoveryData->_deletedItem.find(originalPath);
                if (it != _discoveryData->_deletedItem.end()) {
                    if ((*it)->_instruction != CSYNC_INSTRUCTION_REMOVE)
                        isRename = false;
                    else
                        (*it)->_instruction = CSYNC_INSTRUCTION_NONE;
                    oldEtag = (*it)->_etag;
                    if (!item->isDirectory() && oldEtag != base._etag) {
                        isRename = false;
                    }
                }
                if (auto deleteJob = static_cast<ProcessDirectoryJob *>(_discoveryData->_queuedDeletedDirectories.value(originalPath).data())) {
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
                    path._original = originalPath;
                    path._server = adjustedOriginalPath;
                    qCInfo(lcDisco) << "Rename detected (up) " << item->_file << " -> " << item->_renameTarget;
                };
                if (isRename && !oldEtag.isEmpty()) {
                    recurseQueryServer = oldEtag == base._etag ? ParentNotChanged : NormalQuery;
                    processRename(path);
                } else if (isRename) {
                    // We must query the server to know if the etag has not changed
                    _pendingAsyncJobs++;
                    auto job = new RequestEtagJob(_discoveryData->_account, originalPath, this);
                    connect(job, &RequestEtagJob::finishedWithResult, this, [=](const Result<QString> &etag) mutable {
                        if (!etag || (*etag != base._etag && !item->isDirectory()) || _discoveryData->_renamedItems.contains(originalPath)) {
                            qCInfo(lcDisco) << "Can't rename because the etag has changed or the directory is gone" << originalPath;
                            // Can't be a rename, leave it as a new.
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

                        qCInfo(lcDisco) << "Discovered" << item->_file << item->_instruction << item->_direction << item->isDirectory();
                        if (item->isDirectory()) {
                            auto job = new ProcessDirectoryJob(item, recurseQueryServer, NormalQuery, _discoveryData, this);
                            job->_currentFolder = path;
                            connect(job, &ProcessDirectoryJob::itemDiscovered, this, &ProcessDirectoryJob::itemDiscovered);
                            connect(job, &ProcessDirectoryJob::finished, this, &ProcessDirectoryJob::subJobFinished);
                            _queuedJobs.push_back(job);
                        } else {
                            emit itemDiscovered(item);
                        }
                        _pendingAsyncJobs--;
                        progress();
                    });
                    job->start();
                    return;
                }
            }
        } else {
            item->_instruction = CSYNC_INSTRUCTION_SYNC;
            if (_queryServer != ParentNotChanged && !serverEntry.isValid()) {
                // Special case! deleted on server, modified on client, the instruction is then NEW
                item->_instruction = CSYNC_INSTRUCTION_NEW;
            }
            item->_direction = SyncFileItem::Up;
            item->_checksumHeader.clear();
            item->_size = localEntry.size;
            item->_modtime = localEntry.modtime;
            item->_previousSize = dbEntry._fileSize;
            item->_previousModtime = dbEntry._modtime;
            _childModified = true;

            // Checksum comparison at this stage is only enabled for .eml files,
            // check #4754 #4755
            bool isEmlFile = path._original.endsWith(QLatin1String(".eml"), Qt::CaseInsensitive);
            if (isEmlFile && dbEntry._fileSize == localEntry.size && !dbEntry._checksumHeader.isEmpty()) {
                if (computeLocalChecksum(parseChecksumHeaderType(dbEntry._checksumHeader), path._local) && item->_checksumHeader == dbEntry._checksumHeader) {
                    qCInfo(lcDisco) << "NOTE: Checksums are identical, file did not actually change: " << path._local;
                    item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                }
            }
        }
    } else if (_queryServer != ParentNotChanged && !serverEntry.isValid()) {
        // Not locally, not on the server. The entry is stale!
        qCInfo(lcDisco) << "Stale DB entry";
        return;
    } else if (!serverModified) {
        if (!dbEntry._serverHasIgnoredFiles) {
            item->_instruction = CSYNC_INSTRUCTION_REMOVE;
            item->_direction = SyncFileItem::Up;
        }
    }

    if (path._original != path._target && (item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA || item->_instruction == CSYNC_INSTRUCTION_NONE)) {
        ASSERT(_dirItem && _dirItem->_instruction == CSYNC_INSTRUCTION_RENAME);
        // This is because otherwise subitems are not updated!  (ideally renaming a directory could
        // update the database for all items!  See PropagateDirectory::slotSubJobsFinished)
        item->_instruction = CSYNC_INSTRUCTION_RENAME;
        item->_renameTarget = path._target;
        item->_direction = _dirItem->_direction;
    }

    qCInfo(lcDisco) << "Discovered" << item->_file << item->_instruction << item->_direction << item->isDirectory();

    if (item->isDirectory()) {
        if (item->_instruction == CSYNC_INSTRUCTION_SYNC) {
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
        }
        auto job = new ProcessDirectoryJob(item, recurseQueryServer,
            localEntry.isValid() || item->_instruction == CSYNC_INSTRUCTION_RENAME ? NormalQuery : ParentDontExist,
            _discoveryData, this);
        job->_currentFolder = path;
        if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
            job->setParent(_discoveryData);
            _discoveryData->_queuedDeletedDirectories[path._original] = job;
        } else {
            connect(job, &ProcessDirectoryJob::itemDiscovered, this, &ProcessDirectoryJob::itemDiscovered);
            connect(job, &ProcessDirectoryJob::finished, this, &ProcessDirectoryJob::subJobFinished);
            _queuedJobs.push_back(job);
        }
    } else {
        if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
            _discoveryData->_deletedItem[path._original] = item;
        }
        emit itemDiscovered(item);
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
        auto job = new ProcessDirectoryJob(item, InBlackList, NormalQuery, _discoveryData, this);
        job->_currentFolder = path;
        connect(job, &ProcessDirectoryJob::itemDiscovered, this, &ProcessDirectoryJob::itemDiscovered);
        connect(job, &ProcessDirectoryJob::finished, this, &ProcessDirectoryJob::subJobFinished);
        _queuedJobs.push_back(job);
    } else {
        emit itemDiscovered(item);
    }
}


void ProcessDirectoryJob::subJobFinished()
{
    auto job = qobject_cast<ProcessDirectoryJob *>(sender());
    ASSERT(job);

    _childIgnored |= job->_childIgnored;
    _childModified |= job->_childModified;

    if (job->_dirItem)
        emit itemDiscovered(job->_dirItem);

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
            if (_childIgnored && _dirItem->_instruction == CSYNC_INSTRUCTION_REMOVE) {
                // Do not remove a directory that has ignored files
                _dirItem->_instruction = CSYNC_INSTRUCTION_NONE;
            }
        }
        emit finished();
    }
}
void ProcessDirectoryJob::abort()
{
    // This should delete all the sub jobs, and so abort everything
    deleteLater();
}
}
