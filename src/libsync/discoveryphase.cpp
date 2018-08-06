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

#include <csync_exclude.h>

#include <QLoggingCategory>
#include <QUrl>
#include <QFileInfo>
#include <cstring>


namespace OCC {

Q_LOGGING_CATEGORY(lcDiscovery, "sync.discovery", QtInfoMsg)

/* Given a sorted list of paths ending with '/', return whether or not the given path is within one of the paths of the list*/
static bool findPathInList(const QStringList &list, const QString &path)
{
    Q_ASSERT(std::is_sorted(list.begin(), list.end()));

    if (list.size() == 1 && list.first() == QLatin1String("/")) {
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
    if (_selectiveSyncBlackList.isEmpty()) {
        // If there is no black list, everything is allowed
        return false;
    }

    // Block if it is in the black list
    if (findPathInList(_selectiveSyncBlackList, path)) {
        return true;
    }

    return false;
}

bool DiscoveryPhase::checkSelectiveSyncNewFolder(const QString &path, RemotePermissions remotePerm)
{
    if (_syncOptions._confirmExternalStorage
        && remotePerm.hasPermission(RemotePermissions::IsMounted)) {
        // external storage.

        /* Note: DiscoverySingleDirectoryJob::directoryListingIteratedSlot make sure that only the
         * root of a mounted storage has 'M', all sub entries have 'm' */

        // Only allow it if the white list contains exactly this path (not parents)
        // We want to ask confirmation for external storage even if the parents where selected
        if (_selectiveSyncWhiteList.contains(path + QLatin1Char('/'))) {
            return false;
        }

        emit newBigFolder(path, true);
        return true;
    }

    // If this path or the parent is in the white list, then we do not block this file
    if (findPathInList(_selectiveSyncWhiteList, path)) {
        return false;
    }

    auto limit = _syncOptions._newBigFolderSizeLimit;
    if (limit < 0) {
        // no limit, everything is allowed;
        return false;
    }

    // Go in the main thread to do a PROPFIND to know the size of this folder
    qint64 result = -1;

    /* FIXME TOTO
    {
        QMutexLocker locker(&_vioMutex);
        emit doGetSizeSignal(path, &result);
        _vioWaitCondition.wait(&_vioMutex);
    }*/

    if (result >= limit) {
        // we tell the UI there is a new folder
        emit newBigFolder(path, false);
        return true;
    } else {
        // it is not too big, put it in the white list (so we will not do more query for the children)
        // and and do not block.
        auto p = path;
        if (!p.endsWith(QLatin1Char('/'))) {
            p += QLatin1Char('/');
        }
        _selectiveSyncWhiteList.insert(std::upper_bound(_selectiveSyncWhiteList.begin(),
                                           _selectiveSyncWhiteList.end(), p),
            p);

        return false;
    }
}

/* Given a path on the remote, give the path as it is when the rename is done */
QString DiscoveryPhase::adjustRenamedPath(const QString &original) const
{
    int slashPos = original.size();
    while ((slashPos = original.lastIndexOf('/', slashPos - 1)) > 0) {
        auto it = _renamedItems.constFind(original.left(slashPos));
        if (it != _renamedItems.constEnd()) {
            return *it + original.mid(slashPos);
        }
    }
    return original;
}

void DiscoveryPhase::startJob(ProcessDirectoryJob *job)
{
    connect(job, &ProcessDirectoryJob::finished, this, [this, job] {
        if (job->_dirItem)
            emit itemDiscovered(job->_dirItem);
        job->deleteLater();
        if (!_queuedDeletedDirectories.isEmpty()) {
            auto nextJob = _queuedDeletedDirectories.take(_queuedDeletedDirectories.firstKey());
            startJob(nextJob);
        } else {
            emit finished();
        }
    });
    job->start();
}

DiscoverySingleDirectoryJob::DiscoverySingleDirectoryJob(const AccountPtr &account, const QString &path, QObject *parent)
    : QObject(parent)
    , _subPath(path)
    , _account(account)
    , _ignoredFirst(false)
    , _isRootPath(false)
    , _isExternalStorage(false)
{
}

void DiscoverySingleDirectoryJob::start()
{
    // Start the actual HTTP job
    LsColJob *lsColJob = new LsColJob(_account, _subPath, this);

    QList<QByteArray> props;
    props << "resourcetype"
          << "getlastmodified"
          << "getcontentlength"
          << "getetag"
          << "http://owncloud.org/ns:id"
          << "http://owncloud.org/ns:downloadURL"
          << "http://owncloud.org/ns:dDC"
          << "http://owncloud.org/ns:permissions"
          << "http://owncloud.org/ns:checksums";
    if (_isRootPath)
        props << "http://owncloud.org/ns:data-fingerprint";
    if (_account->serverVersionInt() >= Account::makeServerVersion(10, 0, 0)) {
        // Server older than 10.0 have performances issue if we ask for the share-types on every PROPFIND
        props << "http://owncloud.org/ns:share-types";
    }

    lsColJob->setProperties(props);

    QObject::connect(lsColJob, &LsColJob::directoryListingIterated,
        this, &DiscoverySingleDirectoryJob::directoryListingIteratedSlot);
    QObject::connect(lsColJob, &LsColJob::finishedWithError, this, &DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot);
    QObject::connect(lsColJob, &LsColJob::finishedWithoutError, this, &DiscoverySingleDirectoryJob::lsJobFinishedWithoutErrorSlot);
    lsColJob->start();

    _lsColJob = lsColJob;
}

void DiscoverySingleDirectoryJob::abort()
{
    if (_lsColJob && _lsColJob->reply()) {
        _lsColJob->reply()->abort();
    }
}

static void propertyMapToFileStat(const QMap<QString, QString> &map, RemoteInfo &result)
{
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        QString property = it.key();
        QString value = it.value();
        if (property == "resourcetype") {
            result.isDirectory = value.contains("collection");
        } else if (property == "getlastmodified") {
            result.modtime = oc_httpdate_parse(value.toUtf8());
        } else if (property == "getcontentlength") {
            // See #4573, sometimes negative size values are returned
            bool ok = false;
            qlonglong ll = value.toLongLong(&ok);
            if (ok && ll >= 0) {
                result.size = ll;
            } else {
                result.size = 0;
            }
        } else if (property == "getetag") {
            result.etag = Utility::normalizeEtag(value.toUtf8());
        } else if (property == "id") {
            result.fileId = value.toUtf8();
        } else if (property == "downloadURL") {
            result.directDownloadUrl = value;
        } else if (property == "dDC") {
            result.directDownloadCookies = value;
        } else if (property == "permissions") {
            result.remotePerm = RemotePermissions(value);
        } else if (property == "checksums") {
            result.checksumHeader = findBestChecksum(value.toUtf8());
        } else if (property == "share-types" && !value.isEmpty()) {
            // Since QMap is sorted, "share-types" is always after "permissions".
            if (result.remotePerm.isNull()) {
                qWarning() << "Server returned a share type, but no permissions?";
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

void DiscoverySingleDirectoryJob::directoryListingIteratedSlot(QString file, const QMap<QString, QString> &map)
{
    if (!_ignoredFirst) {
        // The first entry is for the folder itself, we should process it differently.
        _ignoredFirst = true;
        if (map.contains("permissions")) {
            RemotePermissions perm(map.value("permissions"));
            emit firstDirectoryPermissions(perm);
            _isExternalStorage = perm.hasPermission(RemotePermissions::IsMounted);
        }
        if (map.contains("data-fingerprint")) {
            _dataFingerprint = map.value("data-fingerprint").toUtf8();
        }
    } else {

        RemoteInfo result;
        int slash = file.lastIndexOf('/');
        result.name = file.mid(slash + 1);
        result.size = -1;
        result.modtime = -1;
        propertyMapToFileStat(map, result);
        if (result.isDirectory)
            result.size = 0;
        if (result.size == -1
            || result.modtime == -1
            || result.remotePerm.isNull()
            || result.etag.isEmpty()
            || result.fileId.isEmpty()) {
            _error = tr("The server file discovery reply is missing data.");
            qCWarning(lcDiscovery)
                << "Missing properties:" << file << result.isDirectory << result.size
                << result.modtime << result.remotePerm.toString()
                << result.etag << result.fileId;
        }

        if (_isExternalStorage && result.remotePerm.hasPermission(RemotePermissions::IsMounted)) {
            /* All the entries in a external storage have 'M' in their permission. However, for all
               purposes in the desktop client, we only need to know about the mount points.
               So replace the 'M' by a 'm' for every sub entries in an external storage */
            result.remotePerm.unsetPermission(RemotePermissions::IsMounted);
            result.remotePerm.setPermission(RemotePermissions::IsMountedSub);
        }

        QStringRef fileRef(&file);
        int slashPos = file.lastIndexOf(QLatin1Char('/'));
        if (slashPos > -1) {
            fileRef = file.midRef(slashPos + 1);
        }
        _results.push_back(std::move(result));
    }

    //This works in concerto with the RequestEtagJob and the Folder object to check if the remote folder changed.
    if (map.contains("getetag")) {
        _etagConcatenation += map.value("getetag");

        if (_firstEtag.isEmpty()) {
            _firstEtag = map.value("getetag"); // for directory itself
        }
    }
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithoutErrorSlot()
{
    if (!_ignoredFirst) {
        // This is a sanity check, if we haven't _ignoredFirst then it means we never received any directoryListingIteratedSlot
        // which means somehow the server XML was bogus
        emit finished({ 0, tr("Server error: PROPFIND reply is not XML formatted!") });
        deleteLater();
        return;
    } else if (!_error.isEmpty()) {
        emit finished({ 0, _error });
        deleteLater();
        return;
    }
    emit etag(_firstEtag);
    emit etagConcatenation(_etagConcatenation);
    emit finished(_results);
    deleteLater();
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot(QNetworkReply *r)
{
    QString contentType = r->header(QNetworkRequest::ContentTypeHeader).toString();
    int httpCode = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString httpReason = r->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    QString msg = r->errorString();
    qCWarning(lcDiscovery) << "LSCOL job error" << r->errorString() << httpCode << r->error();
    if (httpCode == 0 && r->error() == QNetworkReply::NoError
        && !contentType.contains("application/xml; charset=utf-8")) {
        msg = tr("Server error: PROPFIND reply is not XML formatted!");
    }
    emit finished({httpCode, msg});
    deleteLater();
}

/*
void DiscoveryMainThread::singleDirectoryJobFirstDirectoryPermissionsSlot(RemotePermissions p)
{
    // Should be thread safe since the sync thread is blocked
    if (_discoveryJob->_csync_ctx->remote.root_perms.isNull()) {
        qCDebug(lcDiscovery) << "Permissions for root dir:" << p.toString();
        _discoveryJob->_csync_ctx->remote.root_perms = p;
    }
}

void DiscoveryMainThread::doGetSizeSlot(const QString &path, qint64 *result)
{
    QString fullPath = _pathPrefix;
    if (!_pathPrefix.endsWith('/')) {
        fullPath += '/';
    }
    fullPath += path;
    // remove trailing slash
    while (fullPath.endsWith('/')) {
        fullPath.chop(1);
    }

    _currentGetSizeResult = result;

    // Schedule the DiscoverySingleDirectoryJob
    auto propfindJob = new PropfindJob(_account, fullPath, this);
    propfindJob->setProperties(QList<QByteArray>() << "resourcetype"
                                                   << "http://owncloud.org/ns:size");
    QObject::connect(propfindJob, &PropfindJob::finishedWithError,
        this, &DiscoveryMainThread::slotGetSizeFinishedWithError);
    QObject::connect(propfindJob, &PropfindJob::result,
        this, &DiscoveryMainThread::slotGetSizeResult);
    propfindJob->start();
}

void DiscoveryMainThread::slotGetSizeFinishedWithError()
{
    if (!_currentGetSizeResult) {
        return; // possibly aborted
    }

    qCWarning(lcDiscovery) << "Error getting the size of the directory";
    // just let let the discovery job continue then
    _currentGetSizeResult = 0;
    QMutexLocker locker(&_discoveryJob->_vioMutex);
    _discoveryJob->_vioWaitCondition.wakeAll();
}

void DiscoveryMainThread::slotGetSizeResult(const QVariantMap &map)
{
    if (!_currentGetSizeResult) {
        return; // possibly aborted
    }

    *_currentGetSizeResult = map.value(QLatin1String("size")).toLongLong();
    qCDebug(lcDiscovery) << "Size of folder:" << *_currentGetSizeResult;
    _currentGetSizeResult = 0;
    QMutexLocker locker(&_discoveryJob->_vioMutex);
    _discoveryJob->_vioWaitCondition.wakeAll();
}

*/
}
