/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "discovery.h"
#include "common/filesystembase.h"
#include "common/syncjournaldb.h"
#include "filesystem.h"
#include "syncfileitem.h"
#include "progressdispatcher.h"
#include <QDebug>
#include <algorithm>
#include <QEventLoop>
#include <QDir>
#include <set>
#include <QTextCodec>
#include "vio/csync_vio_local.h"
#include <QFileInfo>
#include <QFile>
#include <QThreadPool>
#include <common/checksums.h>
#include <common/constants.h>
#include "csync_exclude.h"
#include "csync.h"

namespace
{
constexpr const char *editorNamesForDelayedUpload[] = {"PowerPDF"};
constexpr const char *fileExtensionsToCheckIfOpenForSigning[] = {".pdf"};
constexpr auto delayIntervalForSyncRetryForOpenedForSigningFilesSeconds = 60;
constexpr auto delayIntervalForSyncRetryForFilesExceedQuotaSeconds = 60;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcDisco, "nextcloud.sync.discovery", QtInfoMsg)

ProcessDirectoryJob::ProcessDirectoryJob(DiscoveryPhase *data, PinState basePinState, qint64 lastSyncTimestamp, QObject *parent)
    : QObject(parent)
    , _lastSyncTimestamp(lastSyncTimestamp)
    , _discoveryData(data)
{
    qCDebug(lcDisco) << data;
    computePinState(basePinState);
}

ProcessDirectoryJob::ProcessDirectoryJob(const PathTuple &path, const SyncFileItemPtr &dirItem, QueryMode queryLocal, QueryMode queryServer, qint64 lastSyncTimestamp, ProcessDirectoryJob *parent)
    : QObject(parent)
    , _dirItem(dirItem)
    , _dirParentItem(parent->_dirItem)
    , _lastSyncTimestamp(lastSyncTimestamp)
    , _queryServer(queryServer)
    , _queryLocal(queryLocal)
    , _discoveryData(parent->_discoveryData)
    , _currentFolder(path)
{
    qCDebug(lcDisco) << "PREPARING" << _currentFolder._server << _queryServer << _currentFolder._local << _queryLocal;
    computePinState(parent->_pinState);
}

ProcessDirectoryJob::ProcessDirectoryJob(DiscoveryPhase *data, PinState basePinState, const PathTuple &path, const SyncFileItemPtr &dirItem, const SyncFileItemPtr &parentDirItem, QueryMode queryLocal, qint64 lastSyncTimestamp, QObject *parent)
        : QObject(parent)
        , _dirItem(dirItem)
        , _dirParentItem(parentDirItem)
        , _lastSyncTimestamp(lastSyncTimestamp)
        , _queryLocal(queryLocal)
        , _discoveryData(data)
        , _currentFolder(path)
{
    qCDebug(lcDisco) << "PREPARING" << _currentFolder._server << _queryServer << _currentFolder._local << _queryLocal;
    computePinState(basePinState);
}

void ProcessDirectoryJob::start()
{
    qCInfo(lcDisco) << "STARTING" << _currentFolder._server << _queryServer << _currentFolder._local << _queryLocal;

    _discoveryData->_noCaseConflictRecordsInDb = _discoveryData->_statedb->caseClashConflictRecordPaths().isEmpty();

    if (_queryServer == NormalQuery) {
        _serverJob = startAsyncServerQuery();
    } else {
        _serverQueryDone = true;
    }

    // Check whether a normal local query is even necessary
    if (_queryLocal == NormalQuery) {
        if (!_discoveryData->_shouldDiscoverLocaly(_currentFolder._local)
            && (_currentFolder._local == _currentFolder._original || !_discoveryData->_shouldDiscoverLocaly(_currentFolder._original))
            && !_discoveryData->isInSelectiveSyncBlackList(_currentFolder._original)) {
            _queryLocal = ParentNotChanged;
            qCDebug(lcDisco) << "adjusted discovery policy" << _currentFolder._server << _queryServer << _currentFolder._local << _queryLocal;
        }
    }

    if (_queryLocal == NormalQuery) {
        startAsyncLocalQuery();
    } else {
        _localQueryDone = true;
    }

    if (_localQueryDone && _serverQueryDone) {
        process();
    }
}

void ProcessDirectoryJob::process()
{
    ASSERT(_localQueryDone && _serverQueryDone);

    // Build lookup tables for local, remote and db entries.
    // For suffix-virtual files, the key will normally be the base file name
    // without the suffix.
    // However, if foo and foo.owncloud exists locally, there'll be "foo"
    // with local, db, server entries and "foo.owncloud" with only a local
    // entry.
    std::map<QString, Entries> entries;
    for (auto &e : _serverNormalQueryEntries) {
        entries[e.name].serverEntry = std::move(e);
    }
    _serverNormalQueryEntries.clear();

    // fetch all the name from the DB
    auto pathU8 = _currentFolder._original.toUtf8();
    if (!_discoveryData->_statedb->listFilesInPath(pathU8, [&](const SyncJournalFileRecord &rec) {
            auto name = pathU8.isEmpty() ? rec._path : QString::fromUtf8(rec._path.constData() + (pathU8.size() + 1));
            if (rec.isVirtualFile() && isVfsWithSuffix())
                chopVirtualFileSuffix(name);
            auto &dbEntry = entries[name].dbEntry;
            dbEntry = rec;
            setupDbPinStateActions(dbEntry);
        })) {
        dbError();
        return;
    }

    for (auto &e : _localNormalQueryEntries) {
        entries[e.name].localEntry = e;
    }
    if (isVfsWithSuffix()) {
        // For vfs-suffix the local data for suffixed files should usually be associated
        // with the non-suffixed name. Unless both names exist locally or there's
        // other data about the suffixed file.
        // This is done in a second path in order to not depend on the order of
        // _localNormalQueryEntries.
        for (auto &e : _localNormalQueryEntries) {
            if (!e.isVirtualFile)
                continue;
            auto &suffixedEntry = entries[e.name];
            bool hasOtherData = suffixedEntry.serverEntry.isValid() || suffixedEntry.dbEntry.isValid();

            auto nonvirtualName = e.name;
            chopVirtualFileSuffix(nonvirtualName);
            auto &nonvirtualEntry = entries[nonvirtualName];
            // If the non-suffixed entry has no data, move it
            if (!nonvirtualEntry.localEntry.isValid()) {
                std::swap(nonvirtualEntry.localEntry, suffixedEntry.localEntry);
                if (!hasOtherData)
                    entries.erase(e.name);
            } else if (!hasOtherData) {
                // Normally a lone local suffixed file would be processed under the
                // unsuffixed name. In this special case it's under the suffixed name.
                // To avoid lots of special casing, make sure PathTuple::addName()
                // will be called with the unsuffixed name anyway.
                suffixedEntry.nameOverride = nonvirtualName;
            }
        }
    }
    _localNormalQueryEntries.clear();

    //
    // Iterate over entries and process them
    //
    for (auto &f : entries) {
        auto &e = f.second;

        PathTuple path;
        path = _currentFolder.addName(e.nameOverride.isEmpty() ? f.first : e.nameOverride);

        if (!_discoveryData->_listExclusiveFiles.isEmpty() && !_discoveryData->_listExclusiveFiles.contains(path._server)) {
            qCInfo(lcDisco) << "Skipping a file:" << path._server << "as it is not listed in the _listExclusiveFiles";
            continue;
        }

        if (isVfsWithSuffix()) {
            // Without suffix vfs the paths would be good. But since the dbEntry and localEntry
            // can have different names from f.first when suffix vfs is on, make sure the
            // corresponding _original and _local paths are right.

            if (e.dbEntry.isValid()) {
                path._original = e.dbEntry._path;
            } else if (e.localEntry.isVirtualFile) {
                // We don't have a db entry - but it should be at this path
                path._original = PathTuple::pathAppend(_currentFolder._original,  e.localEntry.name);
            }
            if (e.localEntry.isValid()) {
                path._local = PathTuple::pathAppend(_currentFolder._local, e.localEntry.name);
            } else if (e.dbEntry.isVirtualFile()) {
                // We don't have a local entry - but it should be at this path
                addVirtualFileSuffix(path._local);
            }
        }

        // On the server the path is mangled in case of E2EE
        if (!e.serverEntry.e2eMangledName.isEmpty()) {
            Q_ASSERT(_discoveryData->_remoteFolder.startsWith('/'));
            Q_ASSERT(_discoveryData->_remoteFolder.endsWith('/'));

            const auto rootPath = _discoveryData->_remoteFolder.mid(1);
            Q_ASSERT(e.serverEntry.e2eMangledName.startsWith(rootPath));

            path._server = e.serverEntry.e2eMangledName.mid(rootPath.length());
        }

        // If the filename starts with a . we consider it a hidden file
        // For windows, the hidden state is also discovered within the vio
        // local stat function.
        // Recall file shall not be ignored (#4420)
        const auto isHidden = e.localEntry.isHidden || (!f.first.isEmpty() && f.first[0] == '.' && f.first != QLatin1String(".sys.admin#recall#"));

        const auto isEncryptedFolderButE2eIsNotSetup = e.serverEntry.isValid() && e.serverEntry.isE2eEncrypted() &&
            _discoveryData->_account->e2e() && !_discoveryData->_account->e2e()->isInitialized();

        if (isEncryptedFolderButE2eIsNotSetup) {
            checkAndUpdateSelectiveSyncListsForE2eeFolders(path._server + "/");
        }

        const auto isBlacklisted = _queryServer == InBlackList || _discoveryData->isInSelectiveSyncBlackList(path._original) || isEncryptedFolderButE2eIsNotSetup;

        const auto willBeExcluded = handleExcluded(path._target, e, entries, isHidden, isBlacklisted);

        if (willBeExcluded) {
            continue;
        }

        if (isBlacklisted) {
            processBlacklisted(path, e.localEntry, e.dbEntry);
            continue;
        }

        // HACK: Sometimes the serverEntry.etag does not correctly have its quotation marks amputated in the string.
        // We are once again making sure they are chopped off here, but we should really find the root cause for why
        // exactly they are not being lobbed off at any of the prior points of processing.

        e.serverEntry.etag = Utility::normalizeEtag(e.serverEntry.etag);

        processFile(std::move(path), e.localEntry, e.serverEntry, e.dbEntry);
    }
    _discoveryData->_listExclusiveFiles.clear();
    QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
}

bool ProcessDirectoryJob::handleExcluded(const QString &path, const Entries &entries, const std::map<QString, Entries> &allEntries, const bool isHidden, const bool isBlacklisted)
{
    const auto isDirectory = entries.localEntry.isDirectory || entries.serverEntry.isDirectory;

    auto excluded = _discoveryData->_excludes->traversalPatternMatch(path, isDirectory ? ItemTypeDirectory : ItemTypeFile);

    const auto fileName = path.mid(path.lastIndexOf('/') + 1);

    const auto isLocal = entries.localEntry.isValid();

    if (excluded == CSYNC_NOT_EXCLUDED) {
        const auto endsWithSpace = fileName.endsWith(QLatin1Char(' '));
        if (endsWithSpace) {
            excluded = CSYNC_FILE_EXCLUDE_TRAILING_SPACE;
        }
    }

    // we don't need to trigger a warning if trailing/leading space file is already on the server or has already been synced down
    // only if the OS supports trailing/leading spaces
    const auto wasSyncedAlready = entries.serverEntry.isValid() || entries.dbEntry.isValid();
    const auto hasLeadingOrTrailingSpaces = excluded == CSYNC_FILE_EXCLUDE_LEADING_SPACE
                                            || excluded == CSYNC_FILE_EXCLUDE_TRAILING_SPACE
                                            || excluded == CSYNC_FILE_EXCLUDE_LEADING_AND_TRAILING_SPACE;

    const auto leadingAndTrailingSpacesFilesAllowed = !_discoveryData->_shouldEnforceWindowsFileNameCompatibility || _discoveryData->_leadingAndTrailingSpacesFilesAllowed.contains(_discoveryData->_localDir + path);
#if defined Q_OS_WINDOWS
    if (hasLeadingOrTrailingSpaces && leadingAndTrailingSpacesFilesAllowed) {
#else
    if (hasLeadingOrTrailingSpaces && (wasSyncedAlready || leadingAndTrailingSpacesFilesAllowed)) {
#endif
        excluded = CSYNC_NOT_EXCLUDED;
    }

    // FIXME: move to ExcludedFiles 's regexp ?
    bool isInvalidPattern = false;
    if (excluded == CSYNC_NOT_EXCLUDED
        && !wasSyncedAlready
        && !_discoveryData->_invalidFilenameRx.pattern().isEmpty()
        && path.contains(_discoveryData->_invalidFilenameRx)) {

        excluded = CSYNC_FILE_EXCLUDE_INVALID_CHAR;
        isInvalidPattern = true;
    }
    if (excluded == CSYNC_NOT_EXCLUDED
        && !entries.dbEntry.isValid()
        && _discoveryData->_ignoreHiddenFiles && isHidden) {

        excluded = CSYNC_FILE_EXCLUDE_HIDDEN;
    }

    const auto &localName = entries.localEntry.name;
    const auto splitName = localName.split('.');
    const auto &baseName = splitName.first();
    const auto extension = splitName.size() > 1 ? splitName.last() : QString();
    const auto accountCaps = _discoveryData->_account->capabilities();
    const auto forbiddenFilenames = accountCaps.forbiddenFilenames();
    const auto forbiddenBasenames = accountCaps.forbiddenFilenameBasenames();
    const auto forbiddenExtensions = accountCaps.forbiddenFilenameExtensions();
    const auto forbiddenChars = accountCaps.forbiddenFilenameCharacters();

    const auto hasForbiddenFilename = forbiddenFilenames.contains(localName);
    const auto hasForbiddenBasename = forbiddenBasenames.contains(baseName);
    const auto hasForbiddenExtension = forbiddenExtensions.contains(extension);

    auto forbiddenCharMatch = QString{};
    const auto containsForbiddenCharacters =
        std::any_of(forbiddenChars.cbegin(),
                    forbiddenChars.cend(),
                    [&localName, &forbiddenCharMatch](const QString &charPattern) {
                        if (localName.contains(charPattern)) {
                            forbiddenCharMatch = charPattern;
                            return true;
                        }
                        return false;
                    });

    if (excluded == CSYNC_NOT_EXCLUDED && !localName.isEmpty()
        && !wasSyncedAlready
        &&(_discoveryData->_serverBlacklistedFiles.contains(localName)
            || hasForbiddenFilename
            || hasForbiddenBasename
            || hasForbiddenExtension
            || containsForbiddenCharacters)) {
        excluded = CSYNC_FILE_EXCLUDE_SERVER_BLACKLISTED;
        isInvalidPattern = true;
    }

    auto localCodec = QTextCodec::codecForLocale();
    if (!OCC::Utility::isWindows() && localCodec->mibEnum() != 106) {
        // If the locale codec is not UTF-8, we must check that the filename from the server can
        // be encoded in the local file system.
        // (Note: on windows, the FS is always UTF-16, so we don't need to check)        //
        // We cannot use QTextCodec::canEncode() since that can incorrectly return true, see
        // https://bugreports.qt.io/browse/QTBUG-6925.
        QTextEncoder encoder(localCodec, QTextCodec::ConvertInvalidToNull);
        if (encoder.fromUnicode(path).contains('\0')) {
            qCWarning(lcDisco) << "Cannot encode " << path << " to local encoding " << localCodec->name();
            excluded = CSYNC_FILE_EXCLUDE_CANNOT_ENCODE;
        }
    }

    if (excluded == CSYNC_NOT_EXCLUDED && !entries.localEntry.isSymLink) {
        return false;
    } else if (excluded == CSYNC_FILE_SILENTLY_EXCLUDED || excluded == CSYNC_FILE_EXCLUDE_AND_REMOVE) {
        emit _discoveryData->silentlyExcluded(path);
        return true;
    }

    auto item = SyncFileItemPtr::create();
    item->_file = path;
    item->_originalFile = path;
    item->_instruction = CSYNC_INSTRUCTION_IGNORE;

    if (excluded == CSYNC_FILE_EXCLUDE_CASE_CLASH_CONFLICT && canRemoveCaseClashConflictedCopy(path, allEntries)) {
        excluded = CSYNC_NOT_EXCLUDED;
        item->_instruction = CSYNC_INSTRUCTION_REMOVE;
        item->_direction = SyncFileItem::Down;
        emit _discoveryData->itemDiscovered(item);
        return true;
    }

    if (entries.localEntry.isSymLink) {
        /* Symbolic links are ignored. */
        item->_errorString = tr("Symbolic links are not supported in syncing.");
    } else {
        switch (excluded) {
        case CSYNC_NOT_EXCLUDED:
        case CSYNC_FILE_SILENTLY_EXCLUDED:
        case CSYNC_FILE_EXCLUDE_AND_REMOVE:
            qCFatal(lcDisco) << "These were handled earlier";
            break;
        case CSYNC_FILE_EXCLUDE_LIST:
            item->_errorString = tr("File is listed on the ignore list.");
            break;
        case CSYNC_FILE_EXCLUDE_INVALID_CHAR:
            if (item->_file.endsWith('.')) {
                item->_errorString = tr("File names ending with a period are not supported on this file system.");
            } else {
                char invalid = '\0';
                constexpr QByteArrayView invalidChars("\\:?*\"<>|");
                for (const auto x : invalidChars) {
                    if (item->_file.contains(x)) {
                        invalid = x;
                        break;
                    }
                }
                if (invalid) {
                    item->_errorString = isDirectory
                        ? tr("Folder names containing the character \"%1\" are not supported on this file system.", "%1: the invalid character").arg(QChar(invalid))
                        : tr("File names containing the character \"%1\" are not supported on this file system.", "%1: the invalid character").arg(QChar(invalid));
                } else if (isInvalidPattern) {
                    item->_errorString = isDirectory
                        ? tr("Folder name contains at least one invalid character")
                        : tr("File name contains at least one invalid character");
                } else {
                    item->_errorString = isDirectory
                        ? tr("Folder name is a reserved name on this file system.")
                        : tr("File name is a reserved name on this file system.");
                }
            }
            item->_status = SyncFileItem::FileNameInvalid;
            break;
        case CSYNC_FILE_EXCLUDE_TRAILING_SPACE:
            item->_errorString = tr("Filename contains trailing spaces.");
            item->_status = SyncFileItem::FileNameInvalid;
            if (isLocal && !maybeRenameForWindowsCompatibility(_discoveryData->_localDir + item->_file, excluded)) {
                item->_errorString += QStringLiteral(" %1").arg(tr("Cannot be renamed or uploaded."));
            }
            break;
        case CSYNC_FILE_EXCLUDE_LEADING_SPACE:
            item->_errorString = tr("Filename contains leading spaces.");
            item->_status = SyncFileItem::FileNameInvalid;
            if (isLocal && !maybeRenameForWindowsCompatibility(_discoveryData->_localDir + item->_file, excluded)) {
                item->_errorString += QStringLiteral(" %1").arg(tr("Cannot be renamed or uploaded."));
            }
            break;
        case CSYNC_FILE_EXCLUDE_LEADING_AND_TRAILING_SPACE:
            item->_errorString = tr("Filename contains leading and trailing spaces.");
            item->_status = SyncFileItem::FileNameInvalid;
            if (isLocal && !maybeRenameForWindowsCompatibility(_discoveryData->_localDir + item->_file, excluded)) {
                item->_errorString += QStringLiteral(" %1").arg(tr("Cannot be renamed or uploaded."));
            }
            break;
        case CSYNC_FILE_EXCLUDE_LONG_FILENAME:
            item->_errorString = tr("Filename is too long.");
            item->_status = SyncFileItem::FileNameInvalid;
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
        case CSYNC_FILE_EXCLUDE_CASE_CLASH_CONFLICT:
            item->_errorString = tr("Case Clash Conflict: Server file downloaded and renamed to avoid clash.");
            item->_status = SyncFileItem::FileNameClash;
            break;
        case CSYNC_FILE_EXCLUDE_CANNOT_ENCODE:
            item->_errorString = tr("The filename cannot be encoded on your file system.");
            break;
        case CSYNC_FILE_EXCLUDE_SERVER_BLACKLISTED:
            const auto errorString = tr("The filename is blacklisted on the server.");
            QString reasonString;
            if (hasForbiddenFilename) {
                reasonString = tr("Reason: the entire filename is forbidden.");
            }
            if (hasForbiddenBasename) {
                reasonString = tr("Reason: the filename has a forbidden base name (filename start).");
            }
            if (hasForbiddenExtension) {
                reasonString = tr("Reason: the file has a forbidden extension (.%1).").arg(extension);
            }
            if (containsForbiddenCharacters) {
                reasonString = tr("Reason: the filename contains a forbidden character (%1).").arg(forbiddenCharMatch);
            }
            item->_errorString = reasonString.isEmpty() ? errorString : QStringLiteral("%1 %2").arg(errorString, reasonString);
            item->_status = SyncFileItem::FileNameInvalidOnServer;
            if (isLocal && !maybeRenameForWindowsCompatibility(_discoveryData->_localDir + item->_file, excluded)) {
                item->_errorString += QStringLiteral(" %1").arg(tr("Cannot be renamed or uploaded."));
            }
            break;
        }
    }

    if (_dirItem) {
        _dirItem->_isAnyInvalidCharChild = _dirItem->_isAnyInvalidCharChild || item->_status == SyncFileItem::FileNameInvalid;
        _dirItem->_isAnyCaseClashChild = _dirItem->_isAnyCaseClashChild || item->_status == SyncFileItem::FileNameClash;
    }

    _childIgnored = true;

    if (isBlacklisted) {
        emit _discoveryData->silentlyExcluded(path);
    } else {
        emit _discoveryData->itemDiscovered(item);
    }

    return true;
}

bool ProcessDirectoryJob::canRemoveCaseClashConflictedCopy(const QString &path, const std::map<QString, Entries> &allEntries)
{
    const auto conflictRecord = _discoveryData->_statedb->caseConflictRecordByPath(path.toUtf8());
    const auto originalBaseFileName = QFileInfo(QString(_discoveryData->_localDir + "/" + conflictRecord.initialBasePath)).fileName();

    if (allEntries.find(originalBaseFileName) == allEntries.end()) {
        // original entry is no longer on the server, remove conflicted copy
        qCDebug(lcDisco) << "original entry:" << originalBaseFileName << "is no longer on the server, remove conflicted copy:" << path;
        return true;
    }

    auto numMatchingEntries = 0;
    for (auto it = allEntries.cbegin(); it != allEntries.cend(); ++it) {
        if (it->first.compare(originalBaseFileName, Qt::CaseInsensitive) == 0 && it->second.serverEntry.isValid()) {
            // only case-insensitive matching entries that are present on the server
            ++numMatchingEntries;
        }
        if (numMatchingEntries >= 2) {
            break;
        }
    }

    if (numMatchingEntries < 2) {
        // original entry is present on the server but there is no case-clash conflict anymore, remove conflicted copy (only 1 matching file found during case-insensitive search)
        qCDebug(lcDisco) << "original entry:" << originalBaseFileName << "is present on the server, but there is no case-clas conflict anymore, remove conflicted copy:" << path;
        _discoveryData->_anotherSyncNeeded = true;
        return true;
    }

    return false;
}

void ProcessDirectoryJob::checkAndUpdateSelectiveSyncListsForE2eeFolders(const QString &path)
{
    bool ok = false;

    const auto pathWithTrailingSlash = Utility::trailingSlashPath(path);

    const auto blackListList = _discoveryData->_statedb->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok);
    auto blackListSet = QSet<QString>{blackListList.begin(), blackListList.end()};
    blackListSet.insert(pathWithTrailingSlash);
    auto blackList = blackListSet.values();
    blackList.sort();
    _discoveryData->_statedb->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, blackList);

    const auto toRemoveFromBlacklistList = _discoveryData->_statedb->getSelectiveSyncList(SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, &ok);
    auto toRemoveFromBlacklistSet = QSet<QString>{toRemoveFromBlacklistList.begin(), toRemoveFromBlacklistList.end()};
    toRemoveFromBlacklistSet.insert(pathWithTrailingSlash);
    // record it into a separate list to automatically remove from blacklist once the e2EE gets set up
    auto toRemoveFromBlacklist = toRemoveFromBlacklistSet.values();
    toRemoveFromBlacklist.sort();
    _discoveryData->_statedb->setSelectiveSyncList(SyncJournalDb::SelectiveSyncE2eFoldersToRemoveFromBlacklist, toRemoveFromBlacklist);
}

void ProcessDirectoryJob::processFile(PathTuple path,
    const LocalInfo &localEntry, const RemoteInfo &serverEntry,
    const SyncJournalFileRecord &dbEntry)
{
    const auto hasServer = serverEntry.isValid() ? "true" : _queryServer == ParentNotChanged ? "db" : "false";
    const auto hasLocal = localEntry.isValid() ? "true" : _queryLocal == ParentNotChanged ? "db" : "false";
    const auto serverFileIsLocked = (serverEntry.isValid() ? (serverEntry.locked == SyncFileItem::LockStatus::LockedItem ? "locked" : "not locked")  : "");
    const auto localFileIsLocked = dbEntry._lockstate._locked ? "locked" : "not locked";
    const auto serverFileLockType = serverEntry.isValid() ? QString::number(static_cast<int>(serverEntry.lockOwnerType)) : QString{};
    const auto localFileLockType = dbEntry._lockstate._locked ? QString::number(static_cast<int>(dbEntry._lockstate._lockOwnerType)) : QString{};

    QString processingLog;
    QDebug deleteLogger{&processingLog};
    deleteLogger.nospace() << "Processing " << path._original
                           << " | (db/local/remote)"
                           << " | valid: " << dbEntry.isValid() << "/" << hasLocal << "/" << hasServer
                           << " | mtime: " << dbEntry._modtime << "/" << localEntry.modtime << "/" << serverEntry.modtime
                           << " | size: " << dbEntry._fileSize << "/" << localEntry.size << "/" << serverEntry.size
                           << " | etag: " << dbEntry._etag << "//" << serverEntry.etag
                           << " | checksum: " << dbEntry._checksumHeader << "//" << serverEntry.checksumHeader
                           << " | perm: " << dbEntry._remotePerm << "//" << serverEntry.remotePerm
                           << " | fileid: " << dbEntry._fileId << "//" << serverEntry.fileId
                           << " | inode: " << dbEntry._inode << "/" << localEntry.inode << "/"
                           << " | type: " << dbEntry._type << "/" << localEntry.type << "/" << (serverEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile)
                           << " | e2ee: " << dbEntry.isE2eEncrypted() << "/" << serverEntry.isE2eEncrypted()
                           << " | e2eeMangledName: " << dbEntry.e2eMangledName() << "/" << serverEntry.e2eMangledName
                           << " | file lock: " << localFileIsLocked << "//" << serverFileIsLocked
                           << " | file lock type: " << localFileLockType << "//" << serverFileLockType
                           << " | live photo: " << dbEntry._isLivePhoto << "//" << serverEntry.isLivePhoto
                           << " | metadata missing: /" << localEntry.isMetadataMissing << '/';

    if (_discoveryData->isRenamed(path._original)) {
        qCDebug(lcDisco) << "Ignoring renamed";
        return; // Ignore this.
    }

    const auto item = SyncFileItem::fromSyncJournalFileRecord(dbEntry);
    item->_file = path._target;
    item->_originalFile = path._original;
    item->_previousSize = dbEntry._fileSize;
    item->_previousModtime = dbEntry._modtime;
    item->_discoveryResult = std::move(processingLog);

    if (dbEntry._modtime == localEntry.modtime && dbEntry._type == ItemTypeVirtualFile && localEntry.type == ItemTypeFile) {
        item->_type = ItemTypeFile;
        qCInfo(lcDisco) << "Changing item type from virtual to normal file" << item->_file;
    }

    // The item shall only have this type if the db request for the virtual download
    // was successful (like: no conflicting remote remove etc). This decision is done
    // either in processFileAnalyzeRemoteInfo() or further down here.
    if (item->_type == ItemTypeVirtualFileDownload)
        item->_type = ItemTypeVirtualFile;
    // Similarly db entries with a dehydration request denote a regular file
    // until the request is processed.
    if (item->_type == ItemTypeVirtualFileDehydration) {
        item->_type = ItemTypeFile;
        qCInfo(lcDisco) << "Changing item type from virtual to normal file" << item->_file;
    }

    // VFS suffixed files on the server are ignored
    if (isVfsWithSuffix()) {
        if (hasVirtualFileSuffix(serverEntry.name)
            || (localEntry.isVirtualFile && !dbEntry.isVirtualFile() && hasVirtualFileSuffix(dbEntry._path))) {
            item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            item->_errorString = tr("File has extension reserved for virtual files.");
            _childIgnored = true;
            emit _discoveryData->itemDiscovered(item);
            return;
        }
    }

    if (serverEntry.isValid()) {
        processFileAnalyzeRemoteInfo(item, path, localEntry, serverEntry, dbEntry);
        return;
    }

    // Downloading a virtual file is like a server action and can happen even if
    // server-side nothing has changed
    // NOTE: Normally setting the VirtualFileDownload flag means that local and
    // remote will be rediscovered. This is just a fallback for a similar check
    // in processFileAnalyzeRemoteInfo().
    if (_queryServer == ParentNotChanged
        && dbEntry.isValid()
        && (dbEntry._type == ItemTypeVirtualFileDownload
            || localEntry.type == ItemTypeVirtualFileDownload)
        && (localEntry.isValid() || _queryLocal == ParentNotChanged)) {
        item->_direction = SyncFileItem::Down;
        item->_instruction = CSYNC_INSTRUCTION_SYNC;
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

void ProcessDirectoryJob::postProcessServerNew(const SyncFileItemPtr &item,
                                               PathTuple &path,
                                               const LocalInfo &localEntry,
                                               const RemoteInfo &serverEntry,
                                               const SyncJournalFileRecord &dbEntry)
{
    const auto opts = _discoveryData->_syncOptions;

    if (item->isDirectory()) {
        // Turn new remote folders into virtual folders if the option is enabled.
        if (!localEntry.isValid() &&
            opts._vfs->mode() == Vfs::WindowsCfApi &&
            _pinState != PinState::AlwaysLocal &&
            !FileSystem::isExcludeFile(item->_file)) {
            item->_type = ItemTypeVirtualDirectory;
        }

        _pendingAsyncJobs++;
        _discoveryData->checkSelectiveSyncNewFolder(path._server,
                                                    serverEntry.remotePerm,
                                                    [=, this](bool result) {
                                                        --_pendingAsyncJobs;
                                                        if (!result) {
                                                            processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
                                                        }
                                                        QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
                                                    });
        return;
    }

    // Turn new remote files into virtual files if the option is enabled.
    if (!localEntry.isValid() &&
        opts._vfs->mode() != Vfs::Off &&
        _pinState != PinState::AlwaysLocal &&
        !FileSystem::isExcludeFile(item->_file)) {

        if (item->_type == ItemTypeFile) {
            item->_type = ItemTypeVirtualFile;
        }
        if (isVfsWithSuffix()) {
            addVirtualFileSuffix(path._original);
        }
    }

    if (opts._vfs->mode() != Vfs::Off && !item->_encryptedFileName.isEmpty()) {
        // We are syncing a file for the first time (local entry is invalid) and it is encrypted file that will be virtual once synced
        // to avoid having error of "file has changed during sync" when trying to hydrate it explicitly - we must remove Constants::e2EeTagSize bytes from the end
        // as explicit hydration does not care if these bytes are present in the placeholder or not, but, the size must not change in the middle of the sync
        // this way it works for both implicit and explicit hydration by making a placeholder size that does not includes encryption tag Constants::e2EeTagSize bytes
        // another scenario - we are syncing a file which is on disk but not in the database (database was removed or file was not written there yet)
        item->_size = serverEntry.size - Constants::e2EeTagSize;
    }

    processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
}

void ProcessDirectoryJob::processFileAnalyzeRemoteInfo(const SyncFileItemPtr &item,
                                                       PathTuple path,
                                                       const LocalInfo &localEntry,
                                                       const RemoteInfo &serverEntry,
                                                       const SyncJournalFileRecord &dbEntry)
{
    item->_checksumHeader = serverEntry.checksumHeader;
    item->_fileId = serverEntry.fileId;
    item->_remotePerm = serverEntry.remotePerm;
    item->_isShared = serverEntry.remotePerm.hasPermission(RemotePermissions::IsShared) || serverEntry.sharedByMe;
    item->_sharedByMe = serverEntry.sharedByMe;
    item->_lastShareStateFetchedTimestamp = QDateTime::currentMSecsSinceEpoch();
    item->_type = serverEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile;
    item->_etag = serverEntry.etag;
    item->_directDownloadUrl = serverEntry.directDownloadUrl;
    item->_directDownloadCookies = serverEntry.directDownloadCookies;
    item->_e2eEncryptionStatus = serverEntry.isE2eEncrypted() ? SyncFileItem::EncryptionStatus::EncryptedMigratedV2_0 : SyncFileItem::EncryptionStatus::NotEncrypted;
    if (serverEntry.isE2eEncrypted()) {
        item->_e2eEncryptionServerCapability = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_discoveryData->_account->capabilities().clientSideEncryptionVersion());
    }
    Q_ASSERT(item->_e2eEncryptionStatus != SyncFileItem::EncryptionStatus::Encrypted);
    if (item->_e2eEncryptionStatusRemote != SyncFileItem::EncryptionStatus::NotEncrypted) {
        Q_ASSERT(item->_e2eEncryptionStatus != SyncFileItem::EncryptionStatus::NotEncrypted);
    }
    item->_encryptedFileName = [=, this] {
        if (serverEntry.e2eMangledName.isEmpty()) {
            return QString();
        }

        Q_ASSERT(_discoveryData->_remoteFolder.startsWith('/'));
        Q_ASSERT(_discoveryData->_remoteFolder.endsWith('/'));

        const auto rootPath = _discoveryData->_remoteFolder.mid(1);
        Q_ASSERT(serverEntry.e2eMangledName.startsWith(rootPath));
        return serverEntry.e2eMangledName.mid(rootPath.length());
    }();
    item->_locked = serverEntry.locked;
    item->_lockOwnerDisplayName = serverEntry.lockOwnerDisplayName;
    item->_lockOwnerId = serverEntry.lockOwnerId;
    item->_lockOwnerType = serverEntry.lockOwnerType;
    item->_lockEditorApp = serverEntry.lockEditorApp;
    item->_lockTime = serverEntry.lockTime;
    item->_lockTimeout = serverEntry.lockTimeout;
    item->_lockToken = serverEntry.lockToken;

    item->_isLivePhoto = serverEntry.isLivePhoto;
    item->_livePhotoFile = serverEntry.livePhotoFile;

    item->_folderQuota.bytesUsed = serverEntry.folderQuota.bytesUsed;
    item->_folderQuota.bytesAvailable = serverEntry.folderQuota.bytesAvailable;

    // Check for missing server data
    {
        if (serverEntry.size == -1 || serverEntry.remotePerm.isNull() || serverEntry.etag.isEmpty() || serverEntry.fileId.isEmpty()) {
            qCWarning(lcDisco) << "The response provided by the server is missing data:"
                               << "- serverEntry.name:" << serverEntry.name
                               << "- serverEntry.size:" << serverEntry.size
                               << "- serverEntry.remotePerm:" << serverEntry.remotePerm
                               << "- serverEntry.etag:" << serverEntry.etag
                               << "- serverEntry.fileId:" << serverEntry.fileId;

            item->_instruction = CSYNC_INSTRUCTION_ERROR;
            _childIgnored = true;
            item->_errorString = serverEntry.isDirectory ? tr("Folder is not accessible on the server.", "server error")
                                                         : tr("File is not accessible on the server.", "server error");
            emit _discoveryData->itemDiscovered(item);
            return;
        }
    }

    // We want to check the lock state of this file after the lock time has expired
    if(serverEntry.locked == SyncFileItem::LockStatus::LockedItem && serverEntry.lockTimeout > 0) {
        const auto lockExpirationTime = serverEntry.lockTime + serverEntry.lockTimeout;
        const auto timeRemaining = QDateTime::currentDateTime().secsTo(QDateTime::fromSecsSinceEpoch(lockExpirationTime));
        // Add on a second as a precaution, sometimes we catch the server before it has had a chance to update
        const auto lockExpirationTimeout = qMax(5LL, timeRemaining + 1);

        _discoveryData->_anotherSyncNeeded = true;
        _discoveryData->_filesNeedingScheduledSync.insert(path._original, lockExpirationTimeout);

    } else if (serverEntry.locked == SyncFileItem::LockStatus::UnlockedItem && dbEntry._lockstate._locked) {
        // We have received data that this file has been unlocked remotely, so let's notify the sync engine
        // that we no longer need a scheduled sync run for this file

        _discoveryData->_filesUnscheduleSync.append(path._original);
    }

    // The file is known in the db already
    if (dbEntry.isValid()) {
        const auto isDbEntryAnE2EePlaceholder = dbEntry.isVirtualFile() && !dbEntry.e2eMangledName().isEmpty();
        Q_ASSERT(!isDbEntryAnE2EePlaceholder || serverEntry.size >= Constants::e2EeTagSize);
        const auto isVirtualE2EePlaceholder = isDbEntryAnE2EePlaceholder && serverEntry.size >= Constants::e2EeTagSize;
        const auto sizeOnServer = isVirtualE2EePlaceholder ? serverEntry.size - Constants::e2EeTagSize : serverEntry.size;
        const auto metaDataSizeNeedsUpdateForE2EeFilePlaceholder = isVirtualE2EePlaceholder && dbEntry._fileSize == serverEntry.size;

        if (serverEntry.isDirectory) {
            // Even if over quota, continue syncing as normal for now
            _discoveryData->checkSelectiveSyncExistingFolder(path._server);
        }

        if (serverEntry.isDirectory != dbEntry.isDirectory()) {
            // If the type of the entity changed, it's like NEW, but
            // needs to delete the other entity first.
            item->_instruction = CSYNC_INSTRUCTION_TYPE_CHANGE;
            item->_direction = SyncFileItem::Down;
            item->_modtime = serverEntry.modtime;
            item->_size = sizeOnServer;
        } else if ((dbEntry._type == ItemTypeVirtualFileDownload || localEntry.type == ItemTypeVirtualFileDownload)
            && (localEntry.isValid() || _queryLocal == ParentNotChanged)) {
            // The above check for the localEntry existing is important. Otherwise it breaks
            // the case where a file is moved and simultaneously tagged for download in the db.
            item->_direction = SyncFileItem::Down;
            item->_instruction = CSYNC_INSTRUCTION_SYNC;
            item->_type = ItemTypeVirtualFileDownload;
        } else if (serverEntry.isValid() && !serverEntry.isDirectory && !serverEntry.remotePerm.isNull() && !serverEntry.remotePerm.hasPermission(RemotePermissions::CanRead)) {
            item->_instruction = CSYNC_INSTRUCTION_REMOVE;
            item->_direction = SyncFileItem::Down;
        } else if (dbEntry._etag != serverEntry.etag) {
            item->_direction = SyncFileItem::Down;
            item->_modtime = serverEntry.modtime;
            item->_size = sizeOnServer;

            if (serverEntry.isDirectory) {
                Q_ASSERT(dbEntry.isDirectory());
                item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            } else if (!localEntry.isValid() && _queryLocal != ParentNotChanged) {
                // Deleted locally, changed on server
                item->_instruction = CSYNC_INSTRUCTION_NEW;
            } else {
                item->_instruction = CSYNC_INSTRUCTION_SYNC;
                qCDebug(lcDisco) << "CSYNC_INSTRUCTION_SYNC: File" << item->_file << "if (dbEntry._etag != serverEntry.etag)"
                                 << "dbEntry._etag:" << dbEntry._etag
                                 << "serverEntry.etag:" << serverEntry.etag
                                 << "serverEntry.isDirectory:" << serverEntry.isDirectory
                                 << "dbEntry.isDirectory:" << dbEntry.isDirectory();
            }
        } else if (dbEntry._modtime != serverEntry.modtime && localEntry.size == serverEntry.size && dbEntry._fileSize == serverEntry.size
                   && dbEntry._etag == serverEntry.etag) {
            item->_direction = SyncFileItem::Down;
            item->_modtime = serverEntry.modtime;
            item->_size = sizeOnServer;
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
        } else if (dbEntry._remotePerm != serverEntry.remotePerm || dbEntry._fileId != serverEntry.fileId || metaDataSizeNeedsUpdateForE2EeFilePlaceholder) {
            if (metaDataSizeNeedsUpdateForE2EeFilePlaceholder) {
                // we are updating placeholder sizes after migrating from older versions with VFS + E2EE implicit hydration not supported
                qCDebug(lcDisco) << "Migrating the E2EE VFS placeholder " << dbEntry.path() << " from older version. The old size is " << item->_size << ". The new size is " << sizeOnServer;
                item->_size = sizeOnServer;
            }
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            item->_direction = SyncFileItem::Down;
        } else if (serverEntry.requestUpload && !serverEntry.isDirectory && (localEntry.isValid() || _queryLocal == ParentNotChanged)) {
            // Server requested re-upload of this file. Check if we have a local copy and trigger upload.
            qCInfo(lcDisco) << "Server requested re-upload for file:" << item->_file;
            item->_instruction = CSYNC_INSTRUCTION_SYNC;
            item->_direction = SyncFileItem::Up;
            if (localEntry.isValid()) {
                item->_size = localEntry.size;
                item->_modtime = localEntry.modtime;
            } else {
                // If we're in ParentNotChanged mode and don't have localEntry, use db values
                item->_size = dbEntry._fileSize;
                item->_modtime = dbEntry._modtime;
            }
            _childModified = true;
        } else {
            // if (is virtual mode enabled and folder is encrypted - check if the size is the same as on the server and then - trigger server query
            // to update a placeholder with corrected size (-16 Bytes)
            // or, maybe, add a flag to the database - vfsE2eeSizeCorrected? if it is not set - subtract it from the placeholder's size and re-create/update a placeholder?
            const QueryMode serverQueryMode = [this, &dbEntry, &serverEntry]() {
                const auto isVfsModeOn = _discoveryData && _discoveryData->_syncOptions._vfs && _discoveryData->_syncOptions._vfs->mode() != Vfs::Off;
                if (isVfsModeOn && dbEntry.isDirectory() && dbEntry.isE2eEncrypted()) {
                    qint64 localFolderSize = 0;
                    const auto listFilesCallback = [&localFolderSize](const OCC::SyncJournalFileRecord &record) {
                        if (record.isFile()) {
                            // add Constants::e2EeTagSize so we will know the size of E2EE file on the server
                            localFolderSize += record._fileSize + Constants::e2EeTagSize;
                        } else if (record.isVirtualFile()) {
                            // just a virtual file, so, the size must contain Constants::e2EeTagSize if it was not corrected already
                            localFolderSize += record._fileSize;
                        }
                    };

                    const auto listFilesSucceeded = _discoveryData->_statedb->listFilesInPath(dbEntry.path().toUtf8(), listFilesCallback);

                    if (listFilesSucceeded && localFolderSize != 0 && localFolderSize == serverEntry.sizeOfFolder) {
                        qCInfo(lcDisco) << "Migration of E2EE folder " << dbEntry.path() << " from older version to the one, supporting the implicit VFS hydration.";
                        return NormalQuery;
                    }
                }
                return ParentNotChanged;
            }();

            processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, serverQueryMode);
            return;
        }

        processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
        return;
    }

    // Unknown in db: new file on the server
    Q_ASSERT(!dbEntry.isValid());

    if (checkNewDeleteConflict(item)) {
        return;
    }

    item->_instruction = CSYNC_INSTRUCTION_NEW;
    item->_direction = SyncFileItem::Down;
    item->_modtime = serverEntry.modtime;
    item->_size = serverEntry.size;

    const auto conflictRecord = _discoveryData->_noCaseConflictRecordsInDb
        ? ConflictRecord{} :
        _discoveryData->_statedb->caseConflictRecordByBasePath(item->_file);
    if (conflictRecord.isValid() && QString::fromUtf8(conflictRecord.path).contains(QStringLiteral("(case clash from"))) {
        qCInfo(lcDisco) << "should ignore" << item->_file << "has already a case clash conflict record" << conflictRecord.path;

        item->_instruction = CSYNC_INSTRUCTION_IGNORE;

        return;
    }

    if (serverEntry.isValid() && !serverEntry.isDirectory && !serverEntry.remotePerm.isNull() && !serverEntry.remotePerm.hasPermission(RemotePermissions::CanRead)) {
        item->_instruction = CSYNC_INSTRUCTION_IGNORE;
        emit _discoveryData->itemDiscovered(item);

        return;
    }

    // Check if server requested re-upload and we have a local copy
    if (serverEntry.requestUpload && !serverEntry.isDirectory && localEntry.isValid()) {
        qCInfo(lcDisco) << "Server requested re-upload for new file:" << item->_file;
        item->_instruction = CSYNC_INSTRUCTION_SYNC;
        item->_direction = SyncFileItem::Up;
        item->_size = localEntry.size;
        item->_modtime = localEntry.modtime;
        processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
        return;
    }

    // Potential NEW/NEW conflict is handled in AnalyzeLocal
    if (localEntry.isValid()) {
        postProcessServerNew(item, path, localEntry, serverEntry, dbEntry);
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
        if (!serverEntry.isDirectory && (base._modtime != serverEntry.modtime || base._fileSize != serverEntry.size)) {
            qCInfo(lcDisco, "file metadata different, forcing a download of the new file");
            done = true;
            return;
        }

        // Now we know there is a sane rename candidate.
        const auto originalPath = base.path();

        if (_discoveryData->isRenamed(originalPath)) {
            qCInfo(lcDisco, "folder already has a rename entry, skipping");
            return;
        }

        /* A remote rename can also mean Encryption Mangled Name.
         * if we find one of those in the database, we ignore it.
         */
        if (!base._e2eMangledName.isEmpty()) {
            qCWarning(lcDisco, "Encrypted file can not rename");
            done = true;
            return;
        }

        const auto originalPathAdjusted = _discoveryData->adjustRenamedPath(originalPath, SyncFileItem::Up);

        if (!base.isDirectory()) {
            csync_file_stat_t buf;
            if (csync_vio_local_stat(_discoveryData->_localDir + originalPathAdjusted, &buf, true)) {
                qCInfo(lcDisco) << "Local file does not exist anymore." << originalPathAdjusted;
                return;
            }
            // NOTE: This prohibits some VFS renames from being detected since
            // suffix-file size is different from the db size. That's ok, they'll DELETE+NEW.
            if (buf.modtime != base._modtime || buf.size != base._fileSize || buf.type == ItemTypeDirectory) {
                qCInfo(lcDisco) << "File has changed locally, not a rename." << originalPath;
                return;
            }
        } else {
            if (!QFileInfo(_discoveryData->_localDir + originalPathAdjusted).isDir()) {
                qCInfo(lcDisco) << "Local directory does not exist anymore." << originalPathAdjusted;
                return;
            }
        }

        // Renames of virtuals are possible
        if (base.isVirtualFile()) {
            item->_type = ItemTypeVirtualFile;
        }

        const auto wasDeletedOnServer = _discoveryData->findAndCancelDeletedJob(originalPath).first;

        auto postProcessRename = [this, item, base, originalPath](PathTuple &path) {
            const auto adjustedOriginalPath = _discoveryData->adjustRenamedPath(originalPath, SyncFileItem::Up);
            _discoveryData->_renamedItemsRemote.insert(originalPath, path._target);
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
            const auto job = new RequestEtagJob(_discoveryData->_account, _discoveryData->_remoteFolder + originalPath, this);
            connect(job, &RequestEtagJob::finishedWithResult, this, [=, this](const HttpResult<QByteArray> &etag) mutable {
                _pendingAsyncJobs--;
                QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
                if (etag || etag.error().code != 404 ||
                    // Somehow another item claimed this original path, consider as if it existed
                    _discoveryData->isRenamed(originalPath)) {
                    // If the file exist or if there is another error, consider it is a new file.
                    postProcessServerNew(item, path, localEntry, serverEntry, dbEntry);
                    return;
                }

                // The file do not exist, it is a rename

                // In case the deleted item was discovered in parallel
                _discoveryData->findAndCancelDeletedJob(originalPath);

                postProcessRename(path);
                if (item->isDirectory() && serverEntry.isValid() && dbEntry.isValid() && serverEntry.etag == dbEntry._etag && serverEntry.remotePerm != dbEntry._remotePerm) {
                    _queryServer = ParentNotChanged;
                }
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
        postProcessServerNew(item, path, localEntry, serverEntry, dbEntry);
        return;
    }
    processFileAnalyzeLocalInfo(item, path, localEntry, serverEntry, dbEntry, _queryServer);
}

int64_t ProcessDirectoryJob::folderBytesAvailable(const SyncFileItemPtr &item, const FolderQuota::ServerEntry serverEntry) const
{
    const auto unlimitedFreeSpace = -3;
    const auto isTypeChange = item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE;
    const auto isUpdateMetadataOrRename = item->_instruction != CSYNC_INSTRUCTION_SYNC && item->_instruction != CSYNC_INSTRUCTION_NEW;
    const auto isFileDownloadOrDirectory = item->_direction != SyncFileItem::Up || item->isDirectory();

    qCDebug(lcDisco) << "Checking quota for item:" << item->_file
                     << "isTypeChange?" << isTypeChange
                     << "isUpdateMetadataOrRename?" << isUpdateMetadataOrRename
                     << "isFileDownloadOrDirectory?" << isFileDownloadOrDirectory
                     << "_dirItem?" << _dirItem
                     << "_dirParentItem?" << _dirParentItem
                     << "item->_size:" << item->_size
                     << "_folderQuota.bytesAvailable:" << _folderQuota.bytesAvailable
                     << "_folderQuota.bytesUsed:" << _folderQuota.bytesUsed;

    if (item->_size == 0 || isTypeChange || isFileDownloadOrDirectory || isUpdateMetadataOrRename) {
        qCDebug(lcDisco) << "Returning unlimited free space (-3) for item quota.";
        return unlimitedFreeSpace;
    }

    if (serverEntry == FolderQuota::ServerEntry::Valid) {
        qCDebug(lcDisco) << "Returning cached _folderQuota.bytesAvailable for item quota.";
        return _folderQuota.bytesAvailable;
    }

    if (!_dirItem) {
        qCDebug(lcDisco) << "Returning unlimited free space (-3) for item quota with no _dirItem.";
        return unlimitedFreeSpace;
    }

    qCDebug(lcDisco) << "_dirItem->_folderQuota.bytesAvailable:" << _dirItem->_folderQuota.bytesAvailable;

    SyncJournalFileRecord dirItemDbRecord;
    if (_discoveryData->_statedb->getFileRecord(_dirItem->_file, &dirItemDbRecord) && dirItemDbRecord.isValid()) {
        const auto dirDbBytesAvailable = dirItemDbRecord._folderQuota.bytesAvailable;
        qCDebug(lcDisco) << "Returning for item quota db value dirItemDbRecord._folderQuota.bytesAvailable" << dirDbBytesAvailable;
        return dirDbBytesAvailable;
    }

    qCDebug(lcDisco) << "Returning _dirItem->_folderQuota.bytesAvailable for item quota.";
    return _dirItem->_folderQuota.bytesAvailable;
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

    const auto isTypeChange = item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE;

    qCDebug(lcDisco) << "File" << item->_file << "- servermodified:" << serverModified
                     << "noServerEntry:" << noServerEntry
                     << "type:" << item->_type;

    if (serverEntry.isValid()) {
        item->_folderQuota.bytesUsed = serverEntry.folderQuota.bytesUsed;
        item->_folderQuota.bytesAvailable = serverEntry.folderQuota.bytesAvailable;
    } else {
        item->_folderQuota.bytesUsed = -1;
        item->_folderQuota.bytesAvailable = -1;
    }

    // Decay server modifications to UPDATE_METADATA if the local virtual exists
    bool hasLocalVirtual = localEntry.isVirtualFile || (_queryLocal == ParentNotChanged && dbEntry.isVirtualFile());
    bool virtualFileDownload = item->_type == ItemTypeVirtualFileDownload;
    if (serverModified && !isTypeChange && !virtualFileDownload && hasLocalVirtual) {
        item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
        serverModified = false;
        item->_type = ItemTypeVirtualFile;
    }

    if (dbEntry.isVirtualFile() && (!localEntry.isValid() || localEntry.isVirtualFile) && !virtualFileDownload && !isTypeChange) {
        item->_type = ItemTypeVirtualFile;
    }

    _childModified |= serverModified;

    auto finalize = [&] {
        bool recurse = item->isDirectory() || localEntry.isDirectory || serverEntry.isDirectory;
        // Even if we have a local directory: If the remote is a file that's propagated as a
        // conflict we don't need to recurse into it. (local c1.owncloud, c1/ ; remote: c1)
        if (item->_instruction == CSYNC_INSTRUCTION_CONFLICT && !item->isDirectory())
            recurse = false;
        if (_queryLocal != NormalQuery && _queryServer != NormalQuery)
            recurse = false;

        if (localEntry.isPermissionsInvalid) {
            recurse = true;
        }

        if ((item->_direction == SyncFileItem::Down || item->_instruction == CSYNC_INSTRUCTION_CONFLICT || item->_instruction == CSYNC_INSTRUCTION_NEW || item->_instruction == CSYNC_INSTRUCTION_SYNC) &&
                item->_direction != SyncFileItem::Up &&
                (item->_modtime <= 0 || item->_modtime >= 0xFFFFFFFF)) {
            item->_instruction = CSYNC_INSTRUCTION_ERROR;
            item->_errorString = tr("Cannot sync due to invalid modification time");
            item->_status = SyncFileItem::Status::NormalError;
        }

        if (const auto folderQuota = folderBytesAvailable(item,
                                                          serverEntry.isValid() ? FolderQuota::ServerEntry::Valid
                                                                                : FolderQuota::ServerEntry::Invalid);
            item->_size > folderQuota
            && folderQuota > -1) {

            qCInfo(lcDisco) << "Quota exceeded for item:" << item->_file
                             << "- item bytes used:" << item->_size
                             << "- folder bytes available: " << folderQuota;

            item->_instruction = CSYNC_INSTRUCTION_ERROR;
            if (_currentFolder._server.isEmpty()) {
                item->_errorString = tr("Upload of %1 exceeds %2 of space left in personal files.").arg(Utility::octetsToString(item->_size),
                                                                                                        Utility::octetsToString(folderQuota));
            } else {
                item->_errorString = tr("Upload of %1 exceeds %2 of space left in folder %3.").arg(Utility::octetsToString(item->_size),
                                                                                                   Utility::octetsToString(folderQuota),
                                                                                                   _currentFolder._server);
            }

            item->_status = SyncFileItem::Status::NormalError;
            _discoveryData->_anotherSyncNeeded = true;
            _discoveryData->_filesNeedingScheduledSync.insert(path._original, delayIntervalForSyncRetryForFilesExceedQuotaSeconds);
        }

        if (item->_type != CSyncEnums::ItemTypeVirtualFile) {
            const auto foundEditorsKeepingFileBusy = queryEditorsKeepingFileBusy(item, path);
            if (!foundEditorsKeepingFileBusy.isEmpty()) {
                item->_instruction = CSYNC_INSTRUCTION_ERROR;
                const auto editorsString = foundEditorsKeepingFileBusy.join(", ");
                qCInfo(lcDisco) << "Failed, because it is open in the editor." << item->_file << "direction" << item->_direction << editorsString;
                item->_errorString = tr("Could not upload file, because it is open in \"%1\".").arg(editorsString);
                item->_status = SyncFileItem::Status::SoftError;
                _discoveryData->_anotherSyncNeeded = true;
                _discoveryData->_filesNeedingScheduledSync.insert(path._original, delayIntervalForSyncRetryForOpenedForSigningFilesSeconds);
            }
        }

        if (dbEntry.isValid() && item->isDirectory()) {
            item->_e2eEncryptionStatus = EncryptionStatusEnums::fromDbEncryptionStatus(dbEntry._e2eEncryptionStatus);
            if (item->isEncrypted()) {
                item->_e2eEncryptionServerCapability = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_discoveryData->_account->capabilities().clientSideEncryptionVersion());
            }
            Q_ASSERT(item->_e2eEncryptionStatus != SyncFileItem::EncryptionStatus::Encrypted);
            if (item->_e2eEncryptionStatusRemote != SyncFileItem::EncryptionStatus::NotEncrypted) {
                Q_ASSERT(item->_e2eEncryptionStatus != SyncFileItem::EncryptionStatus::NotEncrypted);
            }
        }

        if (localEntry.isPermissionsInvalid && item->_instruction == CSyncEnums::CSYNC_INSTRUCTION_NONE) {
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            item->_direction = SyncFileItem::Down;
        }

        item->isPermissionsInvalid = localEntry.isPermissionsInvalid;

        auto recurseQueryLocal = _queryLocal == ParentNotChanged ? ParentNotChanged : localEntry.isDirectory || item->_instruction == CSYNC_INSTRUCTION_RENAME ? NormalQuery : ParentDontExist;
        if (item->isDirectory() && serverEntry.isValid() && dbEntry.isValid() && serverEntry.etag == dbEntry._etag && serverEntry.remotePerm != dbEntry._remotePerm) {
            recurseQueryServer = ParentNotChanged;
        }
        processFileFinalize(item, path, recurse, recurseQueryLocal, recurseQueryServer);
    };

    if (!localEntry.isValid()) {
        if (_queryLocal == ParentNotChanged && dbEntry.isValid()) {
            // Not modified locally (ParentNotChanged)
            if (noServerEntry) {
                // not on the server: Removed on the server, delete locally
                qCInfo(lcDisco) << "File" << item->_file << "is not anymore on server. Going to delete it locally.";
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else if (dbEntry._type == ItemTypeVirtualFileDehydration) {
                // dehydration requested
                item->_direction = SyncFileItem::Down;
                item->_instruction = CSYNC_INSTRUCTION_SYNC;
                item->_type = ItemTypeVirtualFileDehydration;
            }
        } else if (noServerEntry) {
            // Not locally, not on the server. The entry is stale!
            qCInfo(lcDisco).nospace() << "Stale DB entry (missing local/server entries)"
                << " path=" << path._original
                << " db_etag=" << dbEntry._etag
                << " db_inode=" << dbEntry._inode;
            if (!_discoveryData->_statedb->deleteFileRecord(path._original, true)) {
                emit _discoveryData->fatalError(tr("Error while deleting file record %1 from the database").arg(path._original), ErrorCategory::GenericError);
                qCWarning(lcDisco) << "Failed to delete a file record from the local DB" << path._original;
            }
            return;
        } else if (dbEntry._isLivePhoto && QMimeDatabase().mimeTypeForFile(item->_file).inherits(QStringLiteral("video/quicktime"))) {
            // This is a live photo's video file; the server won't allow deletion of this file
            // so we need to *not* propagate the .mov deletion to the server and redownload the file
            qCInfo(lcDisco) << "Live photo video file deletion detected, redownloading" << item->_file;
            item->_direction = SyncFileItem::Down;
            item->_instruction = CSYNC_INSTRUCTION_SYNC;
        } else if (!serverModified) {
            // Removed locally: also remove on the server.
            if (!dbEntry._serverHasIgnoredFiles) {
#if !defined QT_NO_DEBUG
                qCInfo(lcDisco) << "File" << item->_file << "was deleted locally. Going to delete it on the server.";
#endif
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
            } else if (!dbEntry.isVirtualFile() && isVfsWithSuffix()) {
                // If we find what looks to be a spurious "abc.owncloud" the base file "abc"
                // might have been renamed to that. Make sure that the base file is not
                // deleted from the server.
                if (dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) {
                    qCInfo(lcDisco) << "Base file was renamed to virtual file:" << item->_file;
                    item->_direction = SyncFileItem::Down;
                    item->_instruction = CSYNC_INSTRUCTION_SYNC;
                    item->_type = ItemTypeVirtualFileDehydration;
                    addVirtualFileSuffix(item->_file);
                    item->_renameTarget = item->_file;
                } else {
                    qCInfo(lcDisco) << "Virtual file with non-virtual db entry, ignoring:" << item->_file;
                    item->_instruction = CSYNC_INSTRUCTION_IGNORE;
                }
            }
        } else if (!typeChange && ((dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) || localEntry.isDirectory)) {
            // Local file unchanged.
            if (noServerEntry) {
#if !defined QT_NO_DEBUG
                qCInfo(lcDisco) << "File" << item->_file << "is not anymore on server. Going to delete it locally.";
#endif
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else if (dbEntry._type == ItemTypeVirtualFileDehydration || localEntry.type == ItemTypeVirtualFileDehydration) {
                item->_direction = SyncFileItem::Down;
                item->_instruction = CSYNC_INSTRUCTION_SYNC;
                const auto pinState = _discoveryData->_syncOptions._vfs->pinState(path._local);
                item->_type = ItemTypeVirtualFileDehydration;
            } else if (!serverModified
                && (dbEntry._inode != localEntry.inode
                    || (localEntry.isMetadataMissing && item->_type == ItemTypeFile && !FileSystem::isLnkFile(item->_file))
                    || _discoveryData->_syncOptions._vfs->needsMetadataUpdate(*item))) {
                item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
                item->_direction = SyncFileItem::Down;
            }
        } else if (!typeChange && isVfsWithSuffix()
            && dbEntry.isVirtualFile() && !localEntry.isVirtualFile
            && dbEntry._inode == localEntry.inode
            && dbEntry._modtime == localEntry.modtime
            && localEntry.size == 1) {
            // A suffix vfs file can be downloaded by renaming it to remove the suffix.
            // This check leaks some details of VfsSuffix, particularly the size of placeholders.
            item->_direction = SyncFileItem::Down;
            if (noServerEntry) {
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_type = ItemTypeFile;
            } else {
                item->_instruction = CSYNC_INSTRUCTION_SYNC;
                item->_type = ItemTypeVirtualFileDownload;
                item->_previousSize = 1;
            }
        } else if (serverModified
            || (isVfsWithSuffix() && dbEntry.isVirtualFile())) {
            // There's a local change and a server change: Conflict!
            // Alternatively, this might be a suffix-file that's virtual in the db but
            // not locally. These also become conflicts. For in-place placeholders that's
            // not necessary: they could be replaced by real files and should then trigger
            // a regular SYNC upwards when there's no server change.
            processFileConflict(item, path, localEntry, serverEntry, dbEntry);
        } else if (typeChange) {
            item->_instruction = CSYNC_INSTRUCTION_TYPE_CHANGE;
            item->_direction = SyncFileItem::Up;
            item->_checksumHeader.clear();
            item->_size = localEntry.size;
            item->_modtime = localEntry.modtime;
            item->_type = localEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile;
            _childModified = true;
        } else if (dbEntry._modtime > 0 && (localEntry.modtime <= 0 || localEntry.modtime >= 0xFFFFFFFF) && dbEntry._fileSize == localEntry.size) {
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            item->_direction = SyncFileItem::Down;
            item->_size = localEntry.size > 0 ? localEntry.size : dbEntry._fileSize;
            item->_modtime = dbEntry._modtime;
            item->_previousModtime = dbEntry._modtime;
            item->_type = localEntry.isDirectory ? ItemTypeDirectory : ItemTypeFile;
            qCDebug(lcDisco) << "CSYNC_INSTRUCTION_SYNC: File" << item->_file << "if (dbEntry._modtime > 0 && localEntry.modtime <= 0)"
                             << "dbEntry._modtime:" << dbEntry._modtime
                             << "localEntry.modtime:" << localEntry.modtime;
            _childModified = true;
        } else if (!serverModified &&
                   !noServerEntry &&
                   dbEntry._fileSize == localEntry.size &&
                   dbEntry._modtime == localEntry.modtime &&
                   dbEntry._inode == 0 &&
                   localEntry.inode > 0) {
            item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
            item->_direction = SyncFileItem::Down;
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

            qCDebug(lcDisco) << "Local file was changed: File" << item->_file
                             << "item->_instruction:" << item->_instruction
                             << "noServerEntry:" << noServerEntry
                             << "item->_direction:" << item->_direction
                             << "item->_size:" << item->_size
                             << "item->_modtime:" << item->_modtime;

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

    if (checkNewDeleteConflict(item)) {
        return;
    }

    // New local file or rename
    item->_instruction = CSYNC_INSTRUCTION_NEW;
    item->_direction = SyncFileItem::Up;
    item->_checksumHeader.clear();
    item->_size = localEntry.size;
    item->_modtime = localEntry.modtime;
    item->_type = localEntry.isDirectory && !localEntry.isVirtualFile ? ItemTypeDirectory : localEntry.isDirectory ? ItemTypeVirtualDirectory : localEntry.isVirtualFile ? ItemTypeVirtualFile : ItemTypeFile;
    _childModified = true;

    if (!localEntry.caseClashConflictingName.isEmpty()) {
        qCInfo(lcDisco) << item->_file << "case clash conflict" << localEntry.caseClashConflictingName;
        item->_instruction = CSYNC_INSTRUCTION_CONFLICT;
    }

    auto conflictRecord = _discoveryData->_statedb->caseConflictRecordByBasePath(item->_file);
    if (conflictRecord.isValid() && QString::fromUtf8(conflictRecord.path).contains(QStringLiteral("(case clash from"))) {
        qCInfo(lcDisco) << "should ignore" << item->_file << "has already a case clash conflict record" << conflictRecord.path;

        item->_instruction = CSYNC_INSTRUCTION_IGNORE;

        return;
    }

    auto postProcessLocalNew = [item, localEntry, path, this]() {
        if (localEntry.isVirtualFile) {
            const bool isPlaceHolder = _discoveryData->_syncOptions._vfs->isDehydratedPlaceholder(_discoveryData->_localDir + path._local);
            if (isPlaceHolder) {
                qCWarning(lcDisco) << "Wiping virtual file without db entry for" << path._local;
                item->_instruction = CSYNC_INSTRUCTION_REMOVE;
                item->_direction = SyncFileItem::Down;
            } else {
                qCWarning(lcDisco) << "Virtual file without db entry for" << path._local
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
    const auto originalPath = base.path();

    // Function to gradually check conditions for accepting a move-candidate
    auto moveCheck = [&]() {
        if (!base.isValid()) {
            qCInfo(lcDisco) << "Not a move, no item in db with inode" << localEntry.inode;
            return false;
        }

        if (base.isDirectory() != item->isDirectory()) {
            qCInfo(lcDisco) << "Not a move, types don't match" << base._type << item->_type << localEntry.type;
            return false;
        }
        // Directories and virtual files don't need size/mtime equality
        if (!localEntry.isDirectory && !base.isVirtualFile()
            && (base._modtime != localEntry.modtime || base._fileSize != localEntry.size)) {
            qCInfo(lcDisco) << "Not a move, mtime or size differs, "
                            << "modtime:" << base._modtime << localEntry.modtime << ", "
                            << "size:" << base._fileSize << localEntry.size;
            return false;
        }

        // The old file must have been deleted.
        if (QFile::exists(_discoveryData->_localDir + originalPath)
            // Exception: If the rename changes case only (like "foo" -> "Foo") the
            // old filename might still point to the same file.
            && !(Utility::fsCasePreserving()
                 && originalPath.compare(path._local, Qt::CaseInsensitive) == 0
                 && originalPath != path._local)) {
            qCInfo(lcDisco) << "Not a move, base file still exists at" << originalPath;
            return false;
        }

        // Verify the checksum where possible
        if (!base._checksumHeader.isEmpty() && item->_type == ItemTypeFile && base._type == ItemTypeFile) {
            if (computeLocalChecksum(base._checksumHeader, _discoveryData->_localDir + path._original, item)) {
                qCInfo(lcDisco) << "checking checksum of potential rename " << path._original << item->_checksumHeader << base._checksumHeader;
                if (item->_checksumHeader != base._checksumHeader) {
                    qCInfo(lcDisco) << "Not a move, checksums differ";
                    return false;
                }
            }
        }

        if (_discoveryData->isRenamed(originalPath)) {
            qCInfo(lcDisco) << "Not a move, base path already renamed";
            return false;
        }

        return true;
    };
    const auto isMove = moveCheck();
    const auto isE2eeMove = isMove && (base.isE2eEncrypted() || isInsideEncryptedTree());
    const auto isCfApiVfsMode = _discoveryData->_syncOptions._vfs && _discoveryData->_syncOptions._vfs->mode() == Vfs::WindowsCfApi;
    const bool isOnlineOnlyItem = isCfApiVfsMode && (localEntry.isDirectory || _discoveryData->_syncOptions._vfs->isDehydratedPlaceholder(_discoveryData->_localDir + path._local));
    const auto isE2eeMoveOnlineOnlyItemWithCfApi = isE2eeMove && isOnlineOnlyItem;

    if (isE2eeMove) {
        qCDebug(lcDisco) << "requesting permanent deletion for" << originalPath;
        _discoveryData->_permanentDeletionRequests.insert(originalPath);
    }

    if (isE2eeMoveOnlineOnlyItemWithCfApi) {
        item->_instruction = CSYNC_INSTRUCTION_NEW;
        item->_direction = SyncFileItem::Down;
        item->_isRestoration = true;
        item->_errorString = tr("Moved to invalid target, restoring");
    }

    // If it's not a move it's just a local-NEW
    if (!isMove || (isE2eeMove && !isE2eeMoveOnlineOnlyItemWithCfApi)) {
        if (base.isE2eEncrypted()) {
            // renaming the encrypted folder is done via remove + re-upload hence we need to mark the newly created folder as encrypted
            // base is a record in the SyncJournal database that contains the data about the being-renamed folder with it's old name and encryption information
            item->_e2eEncryptionStatus = EncryptionStatusEnums::fromDbEncryptionStatus(base._e2eEncryptionStatus);
            item->_e2eEncryptionServerCapability = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(_discoveryData->_account->capabilities().clientSideEncryptionVersion());
            if (item->_e2eEncryptionStatus != item->_e2eEncryptionServerCapability) {
                item->_e2eEncryptionStatus = item->_e2eEncryptionServerCapability;
                if (base._e2eEncryptionStatus != SyncJournalFileRecord::EncryptionStatus::NotEncrypted) {
                    Q_ASSERT(item->_e2eEncryptionStatus != SyncFileItem::EncryptionStatus::NotEncrypted);
                }
            }
            Q_ASSERT(item->_e2eEncryptionStatus != SyncFileItem::EncryptionStatus::NotEncrypted);
        }
        postProcessLocalNew();
        finalize();
        return;
    }

    // Check local permission if we are allowed to put move the file here
    // Technically we should use the permissions from the server, but we'll assume it is the same
    const auto serverHasMountRootProperty = _discoveryData->_account->serverHasMountRootProperty();
    const auto isExternalStorage = base._remotePerm.hasPermission(RemotePermissions::IsMounted) && base.isDirectory();
    const auto movePerms = checkMovePermissions(base._remotePerm, originalPath, item->isDirectory());
    if (!movePerms.sourceOk || !movePerms.destinationOk || (serverHasMountRootProperty && isExternalStorage) || isE2eeMoveOnlineOnlyItemWithCfApi) {
        qCInfo(lcDisco) << "Move without permission to rename base file, "
                        << "source:" << movePerms.sourceOk
                        << ", target:" << movePerms.destinationOk
                        << ", targetNew:" << movePerms.destinationNewOk
                        << ", isExternalStorage:" << isExternalStorage
                        << ", serverHasMountRootProperty:" << serverHasMountRootProperty
                        << ", base._remotePerm:" << base._remotePerm.toString()
                        << ", base.path():" << base.path();

        // If we can create the destination, do that.
        // Permission errors on the destination will be handled by checkPermissions later.
        postProcessLocalNew();
        finalize();

        // If the destination upload will work, we're fine with the source deletion.
        // If the source deletion can't work, checkPermissions will error.
        // In case of external storage mounted folders we are never allowed to move/delete them
        if (movePerms.destinationNewOk && !isExternalStorage && !isE2eeMoveOnlineOnlyItemWithCfApi) {
            return;
        }

        // Here we know the new location can't be uploaded: must prevent the source delete.
        // Two cases: either the source item was already processed or not.
        auto wasDeletedOnClient = _discoveryData->findAndCancelDeletedJob(originalPath);
        if (wasDeletedOnClient.first) {
            // More complicated. The REMOVE is canceled. Restore will happen next sync.
            qCInfo(lcDisco) << "Undid remove instruction on source" << originalPath;
            if (!_discoveryData->_statedb->deleteFileRecord(originalPath, true)) {
                qCWarning(lcDisco) << "Failed to delete a file record from the local DB" << originalPath;
            }
            _discoveryData->_statedb->schedulePathForRemoteDiscovery(originalPath);
            _discoveryData->_anotherSyncNeeded = true;
        } else {
            // Signal to future checkPermissions() to forbid the REMOVE and set to restore instead
            qCInfo(lcDisco) << "Preventing future remove on source" << originalPath;
            _discoveryData->_forbiddenDeletes[originalPath + '/'] = true;
        }
        return;
    }

    auto wasDeletedOnClient = _discoveryData->findAndCancelDeletedJob(originalPath);

    auto processRename = [item, originalPath, base, this](PathTuple &path) {
        auto adjustedOriginalPath = _discoveryData->adjustRenamedPath(originalPath, SyncFileItem::Down);
        _discoveryData->_renamedItemsLocal.insert(originalPath, path._target);
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
        item->_isShared = base._isShared;
        item->_sharedByMe = base._sharedByMe;
        item->_lastShareStateFetchedTimestamp = base._lastShareStateFetchedTimestamp;
        item->_etag = base._etag;
        item->_type = base._type;

        // Discard any download/dehydrate tags on the base file.
        // They could be preserved and honored in a follow-up sync,
        // but it complicates handling a lot and will happen rarely.
        if (item->_type == ItemTypeVirtualFileDownload)
            item->_type = ItemTypeVirtualFile;
        if (item->_type == ItemTypeVirtualFileDehydration) {
            item->_type = ItemTypeFile;
            qCInfo(lcDisco) << "Changing item type from virtual to normal file" << item->_file;
        }

        qCInfo(lcDisco) << "Rename detected (up) " << item->_file << " -> " << item->_renameTarget;
    };
    if (wasDeletedOnClient.first) {
        recurseQueryServer = wasDeletedOnClient.second == base._etag ? ParentNotChanged : NormalQuery;
        processRename(path);
    } else {
        // We must query the server to know if the etag has not changed
        _pendingAsyncJobs++;
        QString serverOriginalPath = _discoveryData->_remoteFolder + _discoveryData->adjustRenamedPath(originalPath, SyncFileItem::Down);
        if (base.isVirtualFile() && isVfsWithSuffix())
            chopVirtualFileSuffix(serverOriginalPath);
        auto job = new RequestEtagJob(_discoveryData->_account, serverOriginalPath, this);
        connect(job, &RequestEtagJob::finishedWithResult, this, [=, this](const HttpResult<QByteArray> &etag) mutable {


            if (!etag || (etag.get() != base._etag && !item->isDirectory()) || _discoveryData->isRenamed(originalPath)
                || (isAnyParentBeingRestored(originalPath) && !isRename(originalPath))) {
                qCInfo(lcDisco) << "Can't rename because the etag has changed or the directory is gone or we are restoring one of the file's parents." << originalPath;
                // Can't be a rename, leave it as a new.
                postProcessLocalNew();
            } else {
                // In case the deleted item was discovered in parallel
                _discoveryData->findAndCancelDeletedJob(originalPath);
                processRename(path);
                recurseQueryServer = etag.get() == base._etag ? ParentNotChanged : NormalQuery;
            }
            if (item->isDirectory() && serverEntry.isValid() && dbEntry.isValid() && serverEntry.etag == dbEntry._etag && serverEntry.remotePerm != dbEntry._remotePerm) {
                recurseQueryServer = ParentNotChanged;
            }
            processFileFinalize(item, path, item->isDirectory(), NormalQuery, recurseQueryServer);
            _pendingAsyncJobs--;
            QTimer::singleShot(0, _discoveryData, &DiscoveryPhase::scheduleMoreJobs);
        });
        job->start();
        return;
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
        bool isConflict = (serverEntry.size != localEntry.size) || (serverEntry.modtime != localEntry.modtime) || (dbEntry.isValid() && dbEntry._modtime != localEntry.modtime && serverEntry.modtime == localEntry.modtime);

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
        qCDebug(lcDisco) << "CSYNC_INSTRUCTION_CONFLICT: File" << item->_file << "if (serverEntry.checksumHeader.isEmpty())";
        qCDebug(lcDisco) << "CSYNC_INSTRUCTION_CONFLICT: serverEntry.size:" << serverEntry.size
                         << "localEntry.size:" << localEntry.size
                         << "serverEntry.modtime:" << serverEntry.modtime
                         << "localEntry.modtime:" << localEntry.modtime;
        return;
    }

    if (!serverEntry.checksumHeader.isEmpty()) {
        qCDebug(lcDisco) << "CSYNC_INSTRUCTION_CONFLICT: File" << item->_file << "if (!serverEntry.checksumHeader.isEmpty())";
        qCDebug(lcDisco) << "CSYNC_INSTRUCTION_CONFLICT: serverEntry.size:" << serverEntry.size
                         << "localEntry.size:" << localEntry.size
                         << "serverEntry.modtime:" << serverEntry.modtime
                         << "localEntry.modtime:" << localEntry.modtime;
    }

    // Do we have an UploadInfo for this?
    // Maybe the Upload was completed, but the connection was broken just before
    // we received the etag (Issue #5106)
    auto up = _discoveryData->_statedb->getUploadInfo(path._original);
    if (up._valid && up._contentChecksum == serverEntry.checksumHeader) {
        // Solve the conflict into an upload, or nothing
        item->_instruction = up._modtime == localEntry.modtime && up._size == localEntry.size
            ? CSYNC_INSTRUCTION_NONE : CSYNC_INSTRUCTION_SYNC;
        item->_direction = SyncFileItem::Up;
        qCDebug(lcDisco) << "CSYNC_INSTRUCTION_SYNC: File" << item->_file << "if (up._valid && up._contentChecksum == serverEntry.checksumHeader)";
        qCDebug(lcDisco) << "CSYNC_INSTRUCTION_SYNC: up._valid:" << up._valid
                         << "up._contentChecksum:" << up._contentChecksum
                         << "serverEntry.checksumHeader:" << serverEntry.checksumHeader;

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
            rec._isShared = serverEntry.remotePerm.hasPermission(RemotePermissions::IsShared) || serverEntry.sharedByMe;
            rec._sharedByMe = serverEntry.sharedByMe;
            rec._lastShareStateFetchedTimestamp = QDateTime::currentMSecsSinceEpoch();
            rec._checksumHeader = serverEntry.checksumHeader;
            const auto result = _discoveryData->_statedb->setFileRecord(rec);
            if (!result) {
                qCWarning(lcDisco) << "Error when setting the file record to the database" << rec._path << result.error();
            }
        }
        return;
    }

    if (!up._valid || up._contentChecksum != serverEntry.checksumHeader) {
        qCDebug(lcDisco) << "CSYNC_INSTRUCTION_SYNC: File" << item->_file << "if (!up._valid && up._contentChecksum != serverEntry.checksumHeader)";
        qCDebug(lcDisco) << "CSYNC_INSTRUCTION_SYNC: up._valid:" << up._valid
                         << "up._contentChecksum:" << up._contentChecksum
                         << "serverEntry.checksumHeader:" << serverEntry.checksumHeader;
    }

    // Rely on content hash comparisons to optimize away non-conflicts inside the job
    item->_instruction = CSYNC_INSTRUCTION_CONFLICT;
    item->_direction = SyncFileItem::None;
}

void ProcessDirectoryJob::processFileFinalize(
    const SyncFileItemPtr &item, PathTuple path, bool recurse,
    QueryMode recurseQueryLocal, QueryMode recurseQueryServer)
{
    if (item->isEncrypted() && !_discoveryData->_account->capabilities().clientSideEncryptionAvailable()) {
        item->_instruction = CSyncEnums::CSYNC_INSTRUCTION_IGNORE;
        item->_direction = SyncFileItem::None;
        emit _discoveryData->itemDiscovered(item);
        return;
    }

    // Adjust target path for virtual-suffix files
    if (isVfsWithSuffix()) {
        if (item->_type == ItemTypeVirtualFile) {
            addVirtualFileSuffix(path._target);
            if (item->_instruction == CSYNC_INSTRUCTION_RENAME) {
                addVirtualFileSuffix(item->_renameTarget);
            } else {
                addVirtualFileSuffix(item->_file);
            }
        }
        if (item->_type == ItemTypeVirtualFileDehydration
            && item->_instruction == CSYNC_INSTRUCTION_SYNC) {
            if (item->_renameTarget.isEmpty()) {
                item->_renameTarget = item->_file;
                addVirtualFileSuffix(item->_renameTarget);
            }
        }
    }

    if (_discoveryData->_syncOptions._vfs && _discoveryData->_syncOptions._vfs->mode() != OCC::Vfs::Off &&
        (item->_type == CSyncEnums::ItemTypeFile || item->_type == CSyncEnums::ItemTypeDirectory) &&
        item->_instruction == CSyncEnums::CSYNC_INSTRUCTION_NONE &&
        !_discoveryData->_syncOptions._vfs->isPlaceHolderInSync(_discoveryData->_localDir + path._local)) {
        item->_instruction = CSyncEnums::CSYNC_INSTRUCTION_UPDATE_VFS_METADATA;
    }

    if (_discoveryData->_syncOptions._vfs && _discoveryData->_syncOptions._vfs->mode() != OCC::Vfs::Off &&
        (item->_type == CSyncEnums::ItemTypeFile || item->_type == CSyncEnums::ItemTypeDirectory) &&
        item->_instruction == CSyncEnums::CSYNC_INSTRUCTION_NONE &&
        FileSystem::isLnkFile((_discoveryData->_localDir + path._local)) &&
        !_discoveryData->_syncOptions._vfs->isPlaceHolderInSync(_discoveryData->_localDir + path._local)) {
        item->_instruction = CSyncEnums::CSYNC_INSTRUCTION_SYNC;
        item->_direction = SyncFileItem::Down;
        item->_type = CSyncEnums::ItemTypeVirtualFileDehydration;
    }

    if (item->_instruction == CSyncEnums::CSYNC_INSTRUCTION_NONE &&
        !item->isDirectory() &&
        _discoveryData->_syncOptions._vfs &&
        _discoveryData->_syncOptions._vfs->mode() == OCC::Vfs::Off &&
        (item->_type == CSyncEnums::ItemTypeVirtualFile ||
         item->_type == CSyncEnums::ItemTypeVirtualFileDownload ||
         item->_type == CSyncEnums::ItemTypeVirtualFileDehydration)) {
        item->_instruction = CSyncEnums::CSYNC_INSTRUCTION_UPDATE_METADATA;
        item->_direction = SyncFileItem::Down;
        item->_type = CSyncEnums::ItemTypeFile;
    }

    if (path._original != path._target && (item->_instruction == CSYNC_INSTRUCTION_UPDATE_VFS_METADATA || item->_instruction == CSYNC_INSTRUCTION_UPDATE_METADATA || item->_instruction == CSYNC_INSTRUCTION_NONE)) {
        ASSERT(_dirItem && _dirItem->_instruction == CSYNC_INSTRUCTION_RENAME);
        // This is because otherwise subitems are not updated!  (ideally renaming a directory could
        // update the database for all items!  See PropagateDirectory::slotSubJobsFinished)
        if (_dirItem->_direction == SyncFileItem::Direction::Down) {
            _discoveryData->_renamedItemsRemote.insert(path._original, path._target);
        } else {
            _discoveryData->_renamedItemsLocal.insert(path._original, path._target);
        }
        item->_instruction = CSYNC_INSTRUCTION_RENAME;
        item->_renameTarget = path._target;
        item->_direction = _dirItem->_direction;
    }

    {
        const auto isImportantInstruction = item->_instruction != CSYNC_INSTRUCTION_NONE;
        if (isImportantInstruction) {
            qCInfo(lcDisco).noquote() << item->_discoveryResult;
            qCInfo(lcDisco) << "discovered" << item->_file << item->_instruction << item->_direction << item->_type;
        } else {
            qCDebug(lcDisco).noquote() << item->_discoveryResult;
            qCDebug(lcDisco) << "discovered" << item->_file << item->_instruction << item->_direction << item->_type;
        }
    }

    if (item->_direction == SyncFileItem::Up && item->isEncrypted() && !_discoveryData->_account->e2e()->canEncrypt()) {
        item->_instruction = CSYNC_INSTRUCTION_ERROR;
        item->_errorString = tr("Cannot modify encrypted item because the selected certificate is not valid.");
        item->_status = SyncFileItem::Status::NormalError;
    }

    if (item->isDirectory() && item->_instruction == CSYNC_INSTRUCTION_SYNC)
        item->_instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
    bool removed = item->_instruction == CSYNC_INSTRUCTION_REMOVE;
    if (checkPermissions(item)) {
        if (item->_isRestoration && item->isDirectory())
            recurse = true;
    } else {
        recurse = false;
    }

    if (item->_type == ItemTypeVirtualDirectory) {
        qCDebug(lcDisco()) << "do not recurse inside a virtual folder" << item->_file;
        recurse = false;
    }

    if (!(item->isDirectory() ||
          (!_discoveryData->_syncOptions._vfs || _discoveryData->_syncOptions._vfs->mode() != OCC::Vfs::Off) ||
          item->_type != CSyncEnums::ItemTypeVirtualFile ||
          item->_type != CSyncEnums::ItemTypeVirtualFileDownload ||
          item->_type != CSyncEnums::ItemTypeVirtualFileDehydration)) {
        qCCritical(lcDisco()) << "wong item type for" << item->_file << item->_type;
        Q_ASSERT(false);
    }

    if (recurse) {
        auto job = new ProcessDirectoryJob(path, item, recurseQueryLocal, recurseQueryServer,
            _lastSyncTimestamp, this);
        job->setInsideEncryptedTree(isInsideEncryptedTree() || item->isEncrypted());
        if (removed) {
            job->setParent(_discoveryData);
            _discoveryData->_deletedItem[path._original] = item;
            _discoveryData->enqueueDirectoryToDelete(path._original, job);
        } else {
            connect(job, &ProcessDirectoryJob::finished, this, &ProcessDirectoryJob::subJobFinished);
            _queuedJobs.push_back(job);
        }
    } else {
        if (removed
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
    item->_isSelectiveSync = true;
    if (dbEntry.isValid() && ((dbEntry._modtime == localEntry.modtime && dbEntry._fileSize == localEntry.size) || (localEntry.isDirectory && dbEntry.isDirectory()))) {
        item->_instruction = CSYNC_INSTRUCTION_REMOVE;
        item->_direction = SyncFileItem::Down;
    } else {
        item->_instruction = CSYNC_INSTRUCTION_IGNORE;
        item->_status = SyncFileItem::FileIgnored;
        item->_errorString = tr("Ignored because of the \"choose what to sync\" blacklist");
        qCInfo(lcDisco) << "Ignored because of the \"choose what to sync\" blacklist" << item->_file << "direction" << item->_direction;
        _childIgnored = true;
    }

    qCInfo(lcDisco) << "Discovered (blacklisted) " << item->_file << item->_instruction << item->_direction << item->isDirectory();

    if (item->isDirectory() && item->_instruction != CSYNC_INSTRUCTION_IGNORE) {
        auto job = new ProcessDirectoryJob(path, item, NormalQuery, InBlackList, _lastSyncTimestamp, this);
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
            qCWarning(lcDisco) << "checkForPermission: Not allowed because you don't have permission to add subfolders to that folder:" << item->_file;
            item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            item->_errorString = tr("Not allowed because you don't have permission to add subfolders to that folder");
            emit _discoveryData->remnantReadOnlyFolderDiscovered(item);
            return false;
        } else if (!item->isDirectory() && !perms.hasPermission(RemotePermissions::CanAddFile)) {
            qCWarning(lcDisco) << "checkForPermission: Not allowed because you don't have permission to add files in that folder:" << item->_file;
            item->_instruction = CSYNC_INSTRUCTION_IGNORE;
            item->_errorString = tr("Not allowed because you don't have permission to add files in that folder");
            emit _discoveryData->remnantReadOnlyFolderDiscovered(item);
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
            item->_instruction = CSYNC_INSTRUCTION_CONFLICT;
            item->_errorString = tr("Not allowed to upload this file because it is read-only on the server, restoring");
            item->_direction = SyncFileItem::Down;
            item->_isRestoration = true;
            qCWarning(lcDisco) << "checkForPermission: RESTORING" << item->_file << item->_errorString;
            // Take the things to write to the db from the "other" node (i.e: info from server).
            // Do a lookup into the csync remote tree to get the metadata we need to restore.
            qSwap(item->_size, item->_previousSize);
            qSwap(item->_modtime, item->_previousModtime);
            return false;
        }
        break;
    }
    case CSYNC_INSTRUCTION_REMOVE: {
        QString fileSlash = item->_file + '/';
        auto forbiddenIt = _discoveryData->_forbiddenDeletes.upperBound(fileSlash);
        if (forbiddenIt != _discoveryData->_forbiddenDeletes.begin())
            forbiddenIt = std::prev(forbiddenIt);
        if (forbiddenIt != _discoveryData->_forbiddenDeletes.end()
            && fileSlash.startsWith(forbiddenIt.key())) {
            item->_instruction = CSYNC_INSTRUCTION_NEW;
            item->_direction = SyncFileItem::Down;
            item->_isRestoration = true;
            item->_errorString = tr("Moved to invalid target, restoring");
            qCWarning(lcDisco) << "checkForPermission: RESTORING" << item->_file << item->_errorString;
            return true; // restore sub items
        }
        const auto perms = item->_remotePerm;
        if (perms.isNull()) {
            // No permissions set
            return true;
        }
        if (!perms.hasPermission(RemotePermissions::CanDelete) || isAnyParentBeingRestored(item->_file))
        {
            item->_instruction = CSYNC_INSTRUCTION_NEW;
            item->_direction = SyncFileItem::Down;
            item->_isRestoration = true;
            item->_errorString = tr("Not allowed to remove, restoring");
            qCWarning(lcDisco) << "checkForPermission: RESTORING" << item->_file << item->_errorString;
            return true; // (we need to recurse to restore sub items)
        }
        break;
    }
    default:
        break;
    }
    return true;
}

bool ProcessDirectoryJob::isAnyParentBeingRestored(const QString &file) const
{
    for (const auto &directoryNameToRestore : std::as_const(_discoveryData->_directoryNamesToRestoreOnPropagation)) {
        if (file.startsWith(QString(directoryNameToRestore + QLatin1Char('/')))) {
            qCWarning(lcDisco) << "File" << file << " is within the tree that's being restored" << directoryNameToRestore;
            return true;
        }
    }
    return false;
}

bool ProcessDirectoryJob::isRename(const QString &originalPath) const
{
    return (originalPath.startsWith(_currentFolder._original)
        && originalPath.lastIndexOf('/') == _currentFolder._original.size());

    /* TODO: This was needed at some point to cover an edge case which I am no longer to reproduce and it might no longer be the case.
    *  Still, leaving this here just in case the edge case is caught at some point in future.
    *
    OCC::SyncJournalFileRecord base;
    // are we allowed to rename?
    if (!_discoveryData || !_discoveryData->_statedb || !_discoveryData->_statedb->getFileRecord(originalPath, &base)) {
        return false;
    }
    qCWarning(lcDisco) << "isRename from" << originalPath << " to" << targetPath << " :"
                       << base._remotePerm.hasPermission(RemotePermissions::CanRename);
    return base._remotePerm.hasPermission(RemotePermissions::CanRename);
    */
}

QStringList ProcessDirectoryJob::queryEditorsKeepingFileBusy(const SyncFileItemPtr &item, const PathTuple &path) const
{
    QStringList matchingEditorsKeepingFileBusy;

    if (item->isDirectory() || item->_direction != SyncFileItem::Up) {
        return matchingEditorsKeepingFileBusy;
    }

    const auto isMatchingFileExtension = std::find_if(std::cbegin(fileExtensionsToCheckIfOpenForSigning), std::cend(fileExtensionsToCheckIfOpenForSigning),
        [path](const auto &matchingExtension) {
            return path._local.endsWith(matchingExtension, Qt::CaseInsensitive);
        }) != std::cend(fileExtensionsToCheckIfOpenForSigning);

    if (!isMatchingFileExtension) {
        return matchingEditorsKeepingFileBusy;
    }

    const QString fullLocalPath(_discoveryData->_localDir + path._local);
    const auto editorsKeepingFileBusy = Utility::queryProcessInfosKeepingFileOpen(fullLocalPath);

    for (const auto &detectedEditorName : editorsKeepingFileBusy) {
        const auto isMatchingEditorFound = std::find_if(std::cbegin(editorNamesForDelayedUpload), std::cend(editorNamesForDelayedUpload),
            [detectedEditorName](const auto &matchingEditorName) {
                return detectedEditorName.processName.startsWith(matchingEditorName, Qt::CaseInsensitive);
            }) != std::cend(editorNamesForDelayedUpload);
        if (isMatchingEditorFound) {
            matchingEditorsKeepingFileBusy.push_back(detectedEditorName.processName);
        }
    }

    if (!matchingEditorsKeepingFileBusy.isEmpty()) {
        matchingEditorsKeepingFileBusy.push_back("PowerPDF.exe");
    }

    return matchingEditorsKeepingFileBusy;
}

auto ProcessDirectoryJob::checkMovePermissions(RemotePermissions srcPerm, const QString &srcPath,
                                               bool isDirectory)
    -> MovePermissionResult
{
    auto destPerms = !_rootPermissions.isNull() ? _rootPermissions
                                                : _dirItem ? _dirItem->_remotePerm : _rootPermissions;
    auto filePerms = srcPerm;
    //true when it is just a rename in the same directory. (not a move)
    bool isRename = srcPath.startsWith(_currentFolder._original)
        && srcPath.lastIndexOf('/') == _currentFolder._original.size();
    // Check if we are allowed to move to the destination.
    bool destinationOK = true;
    bool destinationNewOK = true;
    if (destPerms.isNull()) {
    } else if ((isDirectory && !destPerms.hasPermission(RemotePermissions::CanAddSubDirectories)) ||
              (!isDirectory && !destPerms.hasPermission(RemotePermissions::CanAddFile))) {
        destinationNewOK = false;
    }
    if (!isRename && !destinationNewOK) {
        // no need to check for the destination dir permission for renames
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
    return MovePermissionResult{sourceOK, destinationOK, destinationNewOK};
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
                qCInfo(lcDisco) << "Child ignored for a folder to remove" << _dirItem->_file << "direction" << _dirItem->_direction;
                _dirItem->_instruction = CSYNC_INSTRUCTION_NONE;
            }
        }
        emit finished();
    }

    int started = 0;
    for (const auto rj : std::as_const(_runningJobs)) {
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
    emit _discoveryData->fatalError(tr("Error while reading the database"), ErrorCategory::GenericError);
}

void ProcessDirectoryJob::addVirtualFileSuffix(QString &str) const
{
    str.append(_discoveryData->_syncOptions._vfs->fileSuffix());
}

bool ProcessDirectoryJob::hasVirtualFileSuffix(const QString &str) const
{
    if (!isVfsWithSuffix())
        return false;
    return str.endsWith(_discoveryData->_syncOptions._vfs->fileSuffix());
}

void ProcessDirectoryJob::chopVirtualFileSuffix(QString &str) const
{
    if (!isVfsWithSuffix()) {
        return;
    }

    const auto hasSuffix = hasVirtualFileSuffix(str);
    if (!hasSuffix) {
        qCDebug(lcDisco()) << "has no suffix" << str;
        Q_ASSERT(hasSuffix);
    }

    if (hasSuffix) {
        str.chop(_discoveryData->_syncOptions._vfs->fileSuffix().size());
    }
}

DiscoverySingleDirectoryJob *ProcessDirectoryJob::startAsyncServerQuery()
{
    if (_dirItem && _dirItem->isEncrypted() && _dirItem->_encryptedFileName.isEmpty()) {
        _discoveryData->_topLevelE2eeFolderPaths.insert(_discoveryData->_remoteFolder + _dirItem->_file);
    }
    auto serverJob = new DiscoverySingleDirectoryJob(_discoveryData->_account,
                                                     _currentFolder._server,
                                                     _discoveryData->_remoteFolder,
                                                     _discoveryData->_topLevelE2eeFolderPaths,
                                                     _dirItem ? _dirItem->_e2eEncryptionStatus : SyncFileItem::EncryptionStatus::NotEncrypted,
                                                     this);
    if (!_dirItem) {
        serverJob->setIsRootPath(); // query the fingerprint on the root
    }

    connect(serverJob, &DiscoverySingleDirectoryJob::etag, this, &ProcessDirectoryJob::etag);
    connect(serverJob, &DiscoverySingleDirectoryJob::firstDirectoryFileId, this, &ProcessDirectoryJob::rootFileIdReceived);
    connect(serverJob, &DiscoverySingleDirectoryJob::setfolderQuota, this, &ProcessDirectoryJob::setFolderQuota);
    _discoveryData->_currentlyActiveJobs++;
    _pendingAsyncJobs++;
    connect(serverJob, &DiscoverySingleDirectoryJob::finished, this, [this, serverJob](const auto &results) {
        if (_dirItem) {
            if (_dirItem->isEncrypted()) {
                _dirItem->_isFileDropDetected = serverJob->isFileDropDetected();

                SyncJournalFileRecord record;
                const auto alreadyDownloaded = _discoveryData->_statedb->getFileRecord(_dirItem->_file, &record) && record.isValid();
                // we need to make sure we first download all e2ee files/folders before migrating
                _dirItem->_isEncryptedMetadataNeedUpdate = alreadyDownloaded && serverJob->encryptedMetadataNeedUpdate();
                _dirItem->_e2eEncryptionStatus = SyncFileItem::EncryptionStatus::EncryptedMigratedV2_0;
                _dirItem->_e2eEncryptionStatusRemote = serverJob->currentEncryptionStatus();
                _dirItem->_e2eEncryptionServerCapability = serverJob->requiredEncryptionStatus();
                qCDebug(lcDisco()) << _dirItem->_e2eEncryptionStatus << _dirItem->_e2eEncryptionServerCapability;
                Q_ASSERT(_dirItem->_e2eEncryptionStatus != SyncFileItem::EncryptionStatus::Encrypted);
                Q_ASSERT(_dirItem->_e2eEncryptionStatus != SyncFileItem::EncryptionStatus::NotEncrypted);
                _discoveryData->_anotherSyncNeeded = !alreadyDownloaded && serverJob->encryptedMetadataNeedUpdate();
            }
            qCDebug(lcDisco) << "serverJob has finished for folder:" << _dirItem->_file << " and it has _isFileDropDetected:" << _dirItem->_isFileDropDetected
                             << "with quota bytesUsed:" << _dirItem->_folderQuota.bytesUsed
                             << "bytesAvailable:" << _dirItem->_folderQuota.bytesAvailable;
        }
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
            auto code = results.error().code;
            qCWarning(lcDisco) << "Server error in directory" << _currentFolder._server << code;
            if (_dirItem && code >= 403) {
                // In case of an HTTP error, we ignore that directory
                // 403 Forbidden can be sent by the server if the file firewall is active.
                // A file or directory should be ignored and sync must continue. See #3490
                // The server usually replies with the custom "503 Storage not available"
                // if some path is temporarily unavailable. But in some cases a standard 503
                // is returned too. Thus we can't distinguish the two and will treat any
                // 503 as request to ignore the folder. See #3113 #2884.
                // Similarly, the server might also return 404 or 50x in case of bugs. #7199 #7586
                _dirItem->_instruction = CSYNC_INSTRUCTION_IGNORE;
                _dirItem->_errorString = results.error().message;
                emit this->finished();
            } else {
                qCWarning(lcDisco) << "Error:" << results.error().message;
                // Fatal for the root job since it has no SyncFileItem, or for the network errors
                emit _discoveryData->fatalError(results.error().message, ErrorCategory::NetworkError);
            }
        }
    });
    connect(serverJob, &DiscoverySingleDirectoryJob::firstDirectoryPermissions, this,
        [this](const RemotePermissions &perms) { _rootPermissions = perms; });
    serverJob->start();
    return serverJob;
}

void ProcessDirectoryJob::setFolderQuota(const FolderQuota &folderQuota)
{
    qCDebug(lcDisco) << "Setting quota for folder" << _discoveryData->_localDir + _currentFolder._local
                     << "bytes used:" << folderQuota.bytesUsed
                     << "bytes available:" << folderQuota.bytesAvailable;
    _folderQuota.bytesUsed = folderQuota.bytesUsed;
    _folderQuota.bytesAvailable = folderQuota.bytesAvailable;

    if (_currentFolder._original.isEmpty()) {
        emit updatedRootFolderQuota(_folderQuota.bytesUsed, _folderQuota.bytesAvailable);
    }
}

void ProcessDirectoryJob::startAsyncLocalQuery()
{
    QString localPath = _discoveryData->_localDir + _currentFolder._local;
    auto localJob = new DiscoverySingleLocalDirectoryJob(_discoveryData->_account, localPath, _discoveryData->_syncOptions._vfs.data(), _discoveryData->_fileSystemReliablePermissions);

    _discoveryData->_currentlyActiveJobs++;
    _pendingAsyncJobs++;

    connect(localJob, &DiscoverySingleLocalDirectoryJob::itemDiscovered, _discoveryData, &DiscoveryPhase::itemDiscovered);

    connect(localJob, &DiscoverySingleLocalDirectoryJob::childIgnored, this, [this](bool b) {
        _childIgnored = b;
    });

    connect(localJob, &DiscoverySingleLocalDirectoryJob::finishedFatalError, this, [this](const QString &msg) {
        _discoveryData->_currentlyActiveJobs--;
        _pendingAsyncJobs--;
        if (_serverJob)
            _serverJob->abort();

        emit _discoveryData->fatalError(msg, ErrorCategory::NetworkError);
    });

    connect(localJob, &DiscoverySingleLocalDirectoryJob::finishedNonFatalError, this, [this](const QString &msg) {
        _discoveryData->_currentlyActiveJobs--;
        _pendingAsyncJobs--;

        if (_dirItem) {
            _dirItem->_instruction = CSYNC_INSTRUCTION_IGNORE;
            _dirItem->_errorString = msg;
            emit this->finished();
        } else {
            // Fatal for the root job since it has no SyncFileItem
            emit _discoveryData->fatalError(msg, ErrorCategory::GenericError);
        }
    });

    connect(localJob, &DiscoverySingleLocalDirectoryJob::finished, this, [this](const auto &results) {
        _discoveryData->_currentlyActiveJobs--;
        _pendingAsyncJobs--;

        _localNormalQueryEntries = results;
        _localQueryDone = true;

        if (_serverQueryDone)
            this->process();
    });

    QThreadPool *pool = QThreadPool::globalInstance();
    pool->start(localJob); // QThreadPool takes ownership
}


bool ProcessDirectoryJob::isVfsWithSuffix() const
{
    return _discoveryData->_syncOptions._vfs->mode() == Vfs::WithSuffix;
}

void ProcessDirectoryJob::computePinState(PinState parentState)
{
    _pinState = parentState;
    if (_queryLocal != ParentDontExist && FileSystem::fileExists(_discoveryData->_localDir + _currentFolder._local)) {
        if (auto state = _discoveryData->_syncOptions._vfs->pinState(_currentFolder._local)) // ouch! pin local or original?
            _pinState = *state;
    }
}

void ProcessDirectoryJob::setupDbPinStateActions(SyncJournalFileRecord &record)
{
    // Only suffix-vfs uses the db for pin states.
    // Other plugins will set localEntry._type according to the file's pin state.
    if (!isVfsWithSuffix()) {
        return;
    }

    auto pin = _discoveryData->_statedb->internalPinStates().rawForPath(record._path);
    if (!pin || *pin == PinState::Inherited) {
        pin = _pinState;
    }

    // OnlineOnly hydrated files want to be dehydrated
    if (record._type == ItemTypeFile && *pin == PinState::OnlineOnly) {
        record._type = ItemTypeVirtualFileDehydration;
    }

    // AlwaysLocal dehydrated files want to be hydrated
    if (record._type == ItemTypeVirtualFile && *pin == PinState::AlwaysLocal) {
        record._type = ItemTypeVirtualFileDownload;
    }
}

bool ProcessDirectoryJob::maybeRenameForWindowsCompatibility(const QString &absoluteFileName,
                                                             CSYNC_EXCLUDE_TYPE excludeReason)
{
    auto result = true;

    const auto leadingAndTrailingSpacesFilesAllowed = !_discoveryData->_shouldEnforceWindowsFileNameCompatibility || _discoveryData->_leadingAndTrailingSpacesFilesAllowed.contains(absoluteFileName);
    if (leadingAndTrailingSpacesFilesAllowed) {
        return result;
    }

    const auto fileInfo = QFileInfo{absoluteFileName};
    switch (excludeReason)
    {
    case CSYNC_NOT_EXCLUDED:
    case CSYNC_FILE_EXCLUDE_CASE_CLASH_CONFLICT:
    case CSYNC_FILE_EXCLUDE_AND_REMOVE:
    case CSYNC_FILE_EXCLUDE_CANNOT_ENCODE:
    case CSYNC_FILE_EXCLUDE_INVALID_CHAR:
    case CSYNC_FILE_EXCLUDE_SERVER_BLACKLISTED:
    case CSYNC_FILE_EXCLUDE_HIDDEN:
    case CSYNC_FILE_SILENTLY_EXCLUDED:
    case CSYNC_FILE_EXCLUDE_STAT_FAILED:
    case CSYNC_FILE_EXCLUDE_LONG_FILENAME:
    case CSYNC_FILE_EXCLUDE_LIST:
    case CSYNC_FILE_EXCLUDE_CONFLICT:
        break;
    case CSYNC_FILE_EXCLUDE_LEADING_AND_TRAILING_SPACE:
    case CSYNC_FILE_EXCLUDE_LEADING_SPACE:
    case CSYNC_FILE_EXCLUDE_TRAILING_SPACE:
    {
        const auto removeTrailingSpaces = [] (QString string) -> QString {
            for (int n = string.size() - 1; n >= 0; -- n) {
                if (!string.at(n).isSpace()) {
                    string.truncate(n + 1);
                    break;
                }
            }

            return string;
        };

        auto fileNameWithoutTrailingSpace = removeTrailingSpaces(fileInfo.fileName());
        const auto renameTarget = QString{fileInfo.absolutePath() + QStringLiteral("/") + fileNameWithoutTrailingSpace};
        qDebug() << fileInfo.fileName() << fileNameWithoutTrailingSpace << renameTarget;
        result = FileSystem::rename(absoluteFileName, renameTarget);
        break;
    }
    }
    return result;
}

bool ProcessDirectoryJob::checkNewDeleteConflict(const SyncFileItemPtr &item) const
{
    if (_discoveryData->recursiveCheckForDeletedParents(item->_file)) {
        qCWarning(lcDisco) << "Removing local file inside a remotely deleted folder" << item->_file;
        item->_instruction = CSYNC_INSTRUCTION_REMOVE;
        item->_direction = SyncFileItem::Down;
        item->_wantsSpecificActions = SyncFileItem::SynchronizationOptions::MoveToClientTrashBin;
        emit _discoveryData->itemDiscovered(item);
        return true;
    }

    return false;
}

}
