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

#include "discoveryphase.h"
#include "discovery.h"

#include "account.h"
#include "common/asserts.h"
#include "common/checksums.h"

#include "vio/csync_vio_local.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QUrl>
#include <cstring>


namespace OCC {

Q_LOGGING_CATEGORY(lcDiscovery, "sync.discovery", QtInfoMsg)

/* Given a sorted list of paths ending with '/', return whether or not the given path is within one of the paths of the list*/
static bool findPathInList(const std::set<QString> &list, const QString &path)
{
    if (list.size() == 1 && *list.cbegin() == QLatin1String("/")) {
        // Special case for the case "/" is there, it matches everything
        return true;
    }

    QString pathSlash = path + QLatin1Char('/');

    // Since the list is sorted, we can do a binary search.
    // If the path is a prefix of another item or right after in the lexical order.
    auto it = std::lower_bound(list.begin(), list.end(), pathSlash);

    if (it != list.end() && *it == pathSlash) {
        return true;
    }

    if (it == list.begin()) {
        return false;
    }
    --it;
    Q_ASSERT(it->endsWith(QLatin1Char('/'))); // Folder::setSelectiveSyncBlackList makes sure of that
    return pathSlash.startsWith(*it);
}

bool DiscoveryPhase::isInSelectiveSyncBlackList(const QString &path) const
{
    if (_selectiveSyncBlackList.empty()) {
        // If there is no black list, everything is allowed
        return false;
    }

    // Block if it is in the black list
    if (findPathInList(_selectiveSyncBlackList, path)) {
        return true;
    }

    return false;
}

/* Given a path on the remote, give the path as it is when the rename is done */
QString DiscoveryPhase::adjustRenamedPath(const QString &original, SyncFileItem::Direction d) const
{
    return OCC::adjustRenamedPath(d == SyncFileItem::Down ? _renamedItemsRemote : _renamedItemsLocal, original);
}

QString adjustRenamedPath(const QHash<QString, QString> &renamedItems, const QString &original)
{
    int slashPos = original.size();
    while ((slashPos = original.lastIndexOf(QLatin1Char('/'), slashPos - 1)) > 0) {
        auto it = renamedItems.constFind(original.left(slashPos));
        if (it != renamedItems.constEnd()) {
            return *it + original.mid(slashPos);
        }
    }
    return original;
}

QPair<bool, QString> DiscoveryPhase::findAndCancelDeletedJob(const QString &originalPath)
{
    bool result = false;
    QString oldEtag;
    auto it = _deletedItem.constFind(originalPath);
    if (it != _deletedItem.cend()) {
        const auto &item = *it;
        const SyncInstructions instruction = item->instruction();
        if (instruction == CSYNC_INSTRUCTION_IGNORE && item->_type == ItemTypeVirtualFile) {
            // re-creation of virtual files count as a delete
            // restoration after a prohibited move
            // a file might be in an error state and thus gets marked as CSYNC_INSTRUCTION_IGNORE
            // after it was initially marked as CSYNC_INSTRUCTION_REMOVE
            // return true, to not trigger any additional actions on that file that could elad to dataloss
            result = true;
            oldEtag = item->_etag;
        } else {
            if (!(instruction == CSYNC_INSTRUCTION_REMOVE
                    // re-creation of virtual files count as a delete
                    || (item->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW)
                    || (item->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW)
                    // we encountered an ignored error
                    || (item->_hasBlacklistEntry && instruction == CSYNC_INSTRUCTION_IGNORE))) {
                qCWarning(lcDiscovery) << "OC_ENFORCE(FAILING)" << originalPath;
                qCWarning(lcDiscovery) << "instruction == CSYNC_INSTRUCTION_REMOVE" << (instruction == CSYNC_INSTRUCTION_REMOVE);
                qCWarning(lcDiscovery) << "(item->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW)"
                                       << (item->_type == ItemTypeVirtualFile && instruction == CSYNC_INSTRUCTION_NEW);
                qCWarning(lcDiscovery) << "(item->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW)"
                                       << (item->_isRestoration && instruction == CSYNC_INSTRUCTION_NEW);
                qCWarning(lcDiscovery) << "(item->_hasBlacklistEntry && instruction == CSYNC_INSTRUCTION_IGNORE)"
                                       << (item->_hasBlacklistEntry && instruction == CSYNC_INSTRUCTION_IGNORE);
                qCWarning(lcDiscovery) << "instruction" << instruction;
                qCWarning(lcDiscovery) << "item->_type" << item->_type;
                qCWarning(lcDiscovery) << "item->_isRestoration " << item->_isRestoration;
                qCWarning(lcDiscovery) << "item->_remotePerm" << item->_remotePerm;
                OC_ENFORCE(false);
            }
            item->setInstruction(CSYNC_INSTRUCTION_NONE);
            result = true;
            oldEtag = item->_etag;
        }
        _deletedItem.erase(it);
    }
    if (auto *otherJob = _queuedDeletedDirectories.take(originalPath)) {
        oldEtag = otherJob->_dirItem->_etag;
        delete otherJob;
        result = true;
    }
    return { result, oldEtag };
}

void DiscoveryPhase::startJob(ProcessDirectoryJob *job)
{
    OC_ENFORCE(!_currentRootJob);
    connect(job, &ProcessDirectoryJob::finished, this, [this, job] {
        OC_ENFORCE(_currentRootJob == sender());
        _currentRootJob = nullptr;
        if (job->_dirItem)
            Q_EMIT itemDiscovered(job->_dirItem);
        job->deleteLater();

        // Once the main job has finished recurse here to execute the remaining
        // jobs for queued deleted directories.
        if (!_queuedDeletedDirectories.isEmpty()) {
            auto nextJob = _queuedDeletedDirectories.take(_queuedDeletedDirectories.firstKey());
            startJob(nextJob);
        } else {
            Q_EMIT finished();
        }
    });
    _currentRootJob = job;
    job->start();
}

void DiscoveryPhase::setSelectiveSyncBlackList(const QSet<QString> &list)
{
    _selectiveSyncBlackList = {list.cbegin(), list.cend()};
}

void DiscoveryPhase::setSelectiveSyncWhiteList(const QSet<QString> &list)
{
    _selectiveSyncWhiteList = {list.cbegin(), list.cend()};
}

void DiscoveryPhase::scheduleMoreJobs()
{
    auto limit = qMax(1, _syncOptions._parallelNetworkJobs);
    if (_currentRootJob && _currentlyActiveJobs < limit) {
        _currentRootJob->processSubJobs(limit - _currentlyActiveJobs);
    }
}

bool DiscoveryPhase::isSpace() const
{
    return !(Utility::urlEqual(_account->davUrl(), _baseUrl) || _account->davUrl().isParentOf(_baseUrl));
}

DiscoverySingleLocalDirectoryJob::DiscoverySingleLocalDirectoryJob(const AccountPtr &account, const QString &localPath, OCC::Vfs *vfs, QObject *parent)
    : QObject(parent)
    , QRunnable()
    , _localPath(localPath)
    , _account(account)
    , _vfs(vfs)
{
    qRegisterMetaType<QVector<LocalInfo> >("QVector<LocalInfo>");
}

// Use as QRunnable
void DiscoverySingleLocalDirectoryJob::run() {
    QString localPath = _localPath;
    if (localPath.endsWith(QLatin1Char('/'))) // Happens if _currentFolder._local.isEmpty()
        localPath.chop(1);

    auto dh = csync_vio_local_opendir(localPath);
    if (!dh) {
        qCCritical(lcDiscovery) << "Error while opening directory" << (localPath) << errno;
        QString errorString = tr("Error while opening directory %1").arg(localPath);
        if (errno == EACCES) {
            errorString = tr("Directory not accessible on client, permission denied");
            Q_EMIT finishedNonFatalError(errorString);
            return;
        } else if (errno == ENOENT) {
            errorString = tr("Directory not found: %1").arg(localPath);
        } else if (errno == ENOTDIR) {
            // Not a directory..
            // Just consider it is empty
            return;
        }
        Q_EMIT finishedFatalError(errorString);
        return;
    }

    QVector<LocalInfo> results;
    while (true) {
        errno = 0;
        auto dirent = csync_vio_local_readdir(dh, _vfs);
        if (!dirent)
            break;
        if (dirent->type == ItemTypeSkip)
            continue;
        LocalInfo i;
        i.name = dirent->path;
        i.modtime = dirent->modtime;
        i.size = dirent->size;
        i.inode = dirent->inode;
        i.isDirectory = dirent->type == ItemTypeDirectory;
        i.isHidden = dirent->is_hidden;
        i.isSymLink = dirent->type == ItemTypeSoftLink;
        i.isVirtualFile = dirent->type == ItemTypeVirtualFile || dirent->type == ItemTypeVirtualFileDownload;
        i.type = dirent->type;
        results.push_back(i);
    }
    if (errno != 0) {
        csync_vio_local_closedir(dh);

        // Note: Windows vio converts any error into EACCES
        qCWarning(lcDiscovery) << "readdir failed for file in " << localPath << " - errno: " << errno;
        Q_EMIT finishedFatalError(tr("Error while reading directory %1").arg(localPath));
        return;
    }

    errno = 0;
    csync_vio_local_closedir(dh);
    if (errno != 0) {
        qCWarning(lcDiscovery) << "closedir failed for file in " << localPath << " - errno: " << errno;
    }

    Q_EMIT finished(results);
}

DiscoverySingleDirectoryJob::DiscoverySingleDirectoryJob(const AccountPtr &account, const QUrl &baseUrl, const QString &path, QObject *parent)
    : QObject(parent)
    , _subPath(path)
    , _account(account)
    , _baseUrl(baseUrl)
    , _ignoredFirst(false)
    , _isRootPath(false)
    , _isExternalStorage(false)
{
}

void DiscoverySingleDirectoryJob::start()
{
    // Start the actual HTTP job
    _proFindJob = new PropfindJob(_account, _baseUrl, _subPath, PropfindJob::Depth::One, this);

    QList<QByteArray> props {
        "resourcetype",
        "getlastmodified",
        "getcontentlength",
        "getetag",
        "http://owncloud.org/ns:id",
        "http://owncloud.org/ns:downloadURL",
        "http://owncloud.org/ns:dDC",
        "http://owncloud.org/ns:permissions",
        "http://owncloud.org/ns:checksums",
        "http://owncloud.org/ns:share-types"
    };
    if (_isRootPath) {
        props << "http://owncloud.org/ns:data-fingerprint";
    }


    _proFindJob->setProperties(props);

    QObject::connect(_proFindJob, &PropfindJob::directoryListingIterated,
        this, &DiscoverySingleDirectoryJob::directoryListingIteratedSlot);
    QObject::connect(_proFindJob, &PropfindJob::finishedWithError, this, &DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot);
    QObject::connect(_proFindJob, &PropfindJob::finishedWithoutError, this, &DiscoverySingleDirectoryJob::lsJobFinishedWithoutErrorSlot);
    _proFindJob->start();
}

void DiscoverySingleDirectoryJob::abort()
{
    if (_proFindJob) {
        _proFindJob->abort();
    }
}

static void propertyMapToRemoteInfo(const QMap<QString, QString> &map, RemoteInfo &result)
{
    result.directDownloadUrl = map.value(QStringLiteral("downloadURL"));
    result.directDownloadCookies = map.value(QStringLiteral("dDC"));

    if (auto it = Utility::optionalFind(map, QStringLiteral("resourcetype"))) {
        result.isDirectory = it->value().contains(QStringLiteral("collection"));
    }
    if (auto it = Utility::optionalFind(map, QStringLiteral("getlastmodified"))) {
        const auto date = Utility::parseRFC1123Date(**it);
        Q_ASSERT(date.isValid());
        result.modtime = date.toSecsSinceEpoch();
    }
    if (auto it = Utility::optionalFind(map, QStringLiteral("getcontentlength"))) {
        // See #4573, sometimes negative size values are returned
        result.size = std::max<int64_t>(0, it->value().toLongLong());
    }
    if (auto it = Utility::optionalFind(map, QStringLiteral("getetag"))) {
        result.etag = Utility::normalizeEtag(it->value());
    }
    if (auto it = Utility::optionalFind(map, QStringLiteral("id"))) {
        result.fileId = it->value().toUtf8();
    }
    if (auto it = Utility::optionalFind(map, QStringLiteral("checksums"))) {
        result.checksumHeader = findBestChecksum(it->value().toUtf8());
    }
    if (auto it = Utility::optionalFind(map, QStringLiteral("permissions"))) {
        result.remotePerm = RemotePermissions::fromServerString(it->value());
    }
    if (auto it = Utility::optionalFind(map, QStringLiteral("share-types"))) {
        const QString &value = it->value();
        if (!value.isEmpty()) {
            if (!map.contains(QStringLiteral("permissions"))) {
                qWarning() << "Server returned a share type, but no permissions?";
                // Empty permissions will cause a sync failure
            } else {
                // S means shared with me.
                // But for our purpose, we want to know if the file is shared. It does not matter
                // if we are the owner or not.
                // Piggy back on the persmission field
                result.remotePerm.setPermission(RemotePermissions::IsShared);
            }
        }
    }
}

void DiscoverySingleDirectoryJob::directoryListingIteratedSlot(const QString &file, const QMap<QString, QString> &map)
{
    if (!_ignoredFirst) {
        // The first entry is for the folder itself, we should process it differently.
        _ignoredFirst = true;
        if (auto it = Utility::optionalFind(map, QStringLiteral("permissions"))) {
            auto perm = RemotePermissions::fromServerString(it->value());
            Q_EMIT firstDirectoryPermissions(perm);
            _isExternalStorage = perm.hasPermission(RemotePermissions::IsMounted);
        }
        if (auto it = Utility::optionalFind(map, QStringLiteral("data-fingerprint"))) {
            _dataFingerprint = it->value().toUtf8();
            if (_dataFingerprint.isEmpty()) {
                // Placeholder that means that the server supports the feature even if it did not set one.
                _dataFingerprint = "[empty]";
            }
        }
    } else {

        RemoteInfo result;
        int slash = file.lastIndexOf(QLatin1Char('/'));
        result.name = file.mid(slash + 1);
        result.size = -1;
        propertyMapToRemoteInfo(map, result);
        if (result.isDirectory)
            result.size = 0;

        if (_isExternalStorage && result.remotePerm.hasPermission(RemotePermissions::IsMounted)) {
            /* All the entries in a external storage have 'M' in their permission. However, for all
               purposes in the desktop client, we only need to know about the mount points.
               So replace the 'M' by a 'm' for every sub entries in an external storage */
            result.remotePerm.unsetPermission(RemotePermissions::IsMounted);
            result.remotePerm.setPermission(RemotePermissions::IsMountedSub);
        }
        _results.push_back(std::move(result));
    }

    //This works in concerto with the RequestEtagJob and the Folder object to check if the remote folder changed.
    if (_firstEtag.isEmpty()) {
        if (auto it = Utility::optionalFind(map, QStringLiteral("getetag"))) {
            _firstEtag = Utility::normalizeEtag(it->value()); // for directory itself
        }
    }
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithoutErrorSlot()
{
    if (!_ignoredFirst) {
        // This is a sanity check, if we haven't _ignoredFirst then it means we never received any directoryListingIteratedSlot
        // which means somehow the server XML was bogus
        Q_EMIT finished(HttpError{0, tr("Server error: PROPFIND reply is not XML formatted!")});
        deleteLater();
        return;
    } else if (!_error.isEmpty()) {
        Q_EMIT finished(HttpError{0, _error});
        deleteLater();
        return;
    }
    Q_EMIT etag(_firstEtag, _proFindJob->responseQTimeStamp());
    Q_EMIT finished(_results);
    deleteLater();
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot(QNetworkReply *r)
{
    QString contentType = r->header(QNetworkRequest::ContentTypeHeader).toString();
    int httpCode = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString msg = r->errorString();
    qCWarning(lcDiscovery) << "LSCOL job error" << r->errorString() << httpCode << r->error();
    if (r->error() == QNetworkReply::NoError
        && !contentType.contains(QLatin1String("application/xml; charset=utf-8"))) {
        msg = tr("Server error: PROPFIND reply is not XML formatted!");

    } else if (_proFindJob->timedOut()) {
        msg = tr("Connection timed out");
    }
    Q_EMIT finished(HttpError{httpCode, msg});
    deleteLater();
}
}
