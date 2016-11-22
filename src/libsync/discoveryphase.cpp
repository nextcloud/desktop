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
#include <csync_private.h>
#include <csync_rename.h>
#include <qdebug.h>

#include <QUrl>
#include "account.h"
#include <QFileInfo>

namespace OCC {


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

bool DiscoveryJob::isInSelectiveSyncBlackList(const char *path) const
{
    if (_selectiveSyncBlackList.isEmpty()) {
        // If there is no black list, everything is allowed
        return false;
    }

    // Block if it is in the black list
    if (findPathInList(_selectiveSyncBlackList, QString::fromUtf8(path))) {
        return true;
    }

    // Also try to adjust the path if there was renames
    if (csync_rename_count(_csync_ctx)) {
        QScopedPointer<char, QScopedPointerPodDeleter> adjusted(
            csync_rename_adjust_path_source(_csync_ctx, path));
        if (strcmp(adjusted.data(), path) != 0) {
            return findPathInList(_selectiveSyncBlackList, QString::fromUtf8(adjusted.data()));
        }
    }

    return false;
}

int DiscoveryJob::isInSelectiveSyncBlackListCallback(void *data, const char *path)
{
    return static_cast<DiscoveryJob*>(data)->isInSelectiveSyncBlackList(path);
}

bool DiscoveryJob::checkSelectiveSyncNewFolder(const QString& path)
{
    // If this path or the parent is in the white list, then we do not block this file
    if (findPathInList(_selectiveSyncWhiteList, path)) {
        return false;
    }

    if (_newBigFolderSizeLimit < 0) {
        // no limit, everything is allowed;
        return false;
    }

    // Go in the main thread to do a PROPFIND to know the size of this folder
    qint64 result = -1;

    {
        QMutexLocker locker(&_vioMutex);
        emit doGetSizeSignal(path, &result);
        _vioWaitCondition.wait(&_vioMutex);
    }

    auto limit = _newBigFolderSizeLimit;
    if (result >= limit) {
        // we tell the UI there is a new folder
        emit newBigFolder(path);
        return true;
    } else {
        // it is not too big, put it in the white list (so we will not do more query for the children)
        // and and do not block.
        auto p = path;
        if (!p.endsWith(QLatin1Char('/'))) { p += QLatin1Char('/'); }
        _selectiveSyncWhiteList.insert(std::upper_bound(_selectiveSyncWhiteList.begin(),
                                                        _selectiveSyncWhiteList.end(), p), p);

        return false;
    }
}

int DiscoveryJob::checkSelectiveSyncNewFolderCallback(void *data, const char *path)
{
    return static_cast<DiscoveryJob*>(data)->checkSelectiveSyncNewFolder(QString::fromUtf8(path));
}


void DiscoveryJob::update_job_update_callback (bool local,
                                    const char *dirUrl,
                                    void *userdata)
{
    DiscoveryJob *updateJob = static_cast<DiscoveryJob*>(userdata);
    if (updateJob) {
        // Don't wanna overload the UI
        if (!updateJob->_lastUpdateProgressCallbackCall.isValid()) {
            updateJob->_lastUpdateProgressCallbackCall.restart(); // first call
        } else if (updateJob->_lastUpdateProgressCallbackCall.elapsed() < 200) {
            return;
        } else {
            updateJob->_lastUpdateProgressCallbackCall.restart();
        }

        QByteArray pPath(dirUrl);
        int indx = pPath.lastIndexOf('/');
        if(indx>-1) {
            const QString path = QUrl::fromPercentEncoding( pPath.mid(indx+1));
            emit updateJob->folderDiscovered(local, path);
        }
    }
}

// Only use for error cases! It will always set an error errno
int get_errno_from_http_errcode( int err, const QString & reason ) {
    int new_errno = EIO;

    switch(err) {
    case 401:           /* Unauthorized */
    case 402:           /* Payment Required */
    case 407:           /* Proxy Authentication Required */
    case 405:
        new_errno = EPERM;
        break;
    case 301:           /* Moved Permanently */
    case 303:           /* See Other */
    case 404:           /* Not Found */
    case 410:           /* Gone */
        new_errno = ENOENT;
        break;
    case 408:           /* Request Timeout */
    case 504:           /* Gateway Timeout */
        new_errno = EAGAIN;
        break;
    case 423:           /* Locked */
        new_errno = EACCES;
        break;
    case 403:           /* Forbidden */
        new_errno = ERRNO_FORBIDDEN;
        break;
    case 400:           /* Bad Request */
    case 409:           /* Conflict */
    case 411:           /* Length Required */
    case 412:           /* Precondition Failed */
    case 414:           /* Request-URI Too Long */
    case 415:           /* Unsupported Media Type */
    case 424:           /* Failed Dependency */
    case 501:           /* Not Implemented */
        new_errno = EINVAL;
        break;
    case 507:           /* Insufficient Storage */
        new_errno = ENOSPC;
        break;
    case 206:           /* Partial Content */
    case 300:           /* Multiple Choices */
    case 302:           /* Found */
    case 305:           /* Use Proxy */
    case 306:           /* (Unused) */
    case 307:           /* Temporary Redirect */
    case 406:           /* Not Acceptable */
    case 416:           /* Requested Range Not Satisfiable */
    case 417:           /* Expectation Failed */
    case 422:           /* Unprocessable Entity */
    case 500:           /* Internal Server Error */
    case 502:           /* Bad Gateway */
    case 505:           /* HTTP Version Not Supported */
        new_errno = EIO;
        break;
    case 503:           /* Service Unavailable */
        // https://github.com/owncloud/core/pull/26145/files
        if (reason == "Storage not available" || reason == "Storage is temporarily not available") {
            new_errno = ERRNO_STORAGE_UNAVAILABLE;
        } else {
            new_errno = ERRNO_SERVICE_UNAVAILABLE;
        }
        break;
    case 413:           /* Request Entity too Large */
        new_errno = EFBIG;
        break;
    default:
        new_errno = EIO;
    }
    return new_errno;
}



DiscoverySingleDirectoryJob::DiscoverySingleDirectoryJob(const AccountPtr &account, const QString &path, QObject *parent)
    : QObject(parent), _subPath(path), _account(account), _ignoredFirst(false), _isRootPath(false)
{
}

void DiscoverySingleDirectoryJob::start()
{
    // Start the actual HTTP job
    LsColJob *lsColJob = new LsColJob(_account, _subPath, this);

    QList<QByteArray> props;
    props << "resourcetype" << "getlastmodified" << "getcontentlength" << "getetag"
          << "http://owncloud.org/ns:id" << "http://owncloud.org/ns:downloadURL"
          << "http://owncloud.org/ns:dDC" << "http://owncloud.org/ns:permissions";
    if (_isRootPath)
        props << "http://owncloud.org/ns:data-fingerprint";

    lsColJob->setProperties(props);

    QObject::connect(lsColJob, SIGNAL(directoryListingIterated(QString,QMap<QString,QString>)),
                     this, SLOT(directoryListingIteratedSlot(QString,QMap<QString,QString>)));
    QObject::connect(lsColJob, SIGNAL(finishedWithError(QNetworkReply*)), this, SLOT(lsJobFinishedWithErrorSlot(QNetworkReply*)));
    QObject::connect(lsColJob, SIGNAL(finishedWithoutError()), this, SLOT(lsJobFinishedWithoutErrorSlot()));
    lsColJob->start();

    _lsColJob = lsColJob;
}

void DiscoverySingleDirectoryJob::abort()
{
    if (_lsColJob && _lsColJob->reply()) {
        _lsColJob->reply()->abort();
    }
}

static csync_vio_file_stat_t* propertyMapToFileStat(const QMap<QString,QString> &map)
{
    csync_vio_file_stat_t* file_stat = csync_vio_file_stat_new();

    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        //qDebug() << it.key() << it.value();
        QString property = it.key();
        QString value = it.value();
        if (property == "resourcetype") {
            if (value.contains("collection")) {
                file_stat->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
            } else {
                file_stat->type = CSYNC_VIO_FILE_TYPE_REGULAR;
            }
            file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
        } else if  (property == "getlastmodified") {
            file_stat->mtime = oc_httpdate_parse(value.toUtf8());
            file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;
        } else if (property == "getcontentlength") {
            bool ok = false;
            qlonglong ll = value.toLongLong(&ok);
            if (ok && ll >= 0) {
                file_stat->size = ll;
                file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;
            }
        } else if (property == "getetag") {
            file_stat->etag = csync_normalize_etag(value.toUtf8());
            file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ETAG;
        } else if (property == "id") {
            csync_vio_file_stat_set_file_id(file_stat, value.toUtf8());
        } else if (property == "downloadURL") {
            file_stat->directDownloadUrl = strdup(value.toUtf8());
            file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADURL;
        } else if (property == "dDC") {
            file_stat->directDownloadCookies = strdup(value.toUtf8());
            file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_DIRECTDOWNLOADCOOKIES;
        } else if (property == "permissions") {
            auto v = value.toUtf8();
            if (value.isEmpty()) {
                // special meaning for our code: server returned permissions but are empty
                // meaning only reading is allowed for this resource
                file_stat->remotePerm[0] = ' ';
                // see _csync_detect_update()
                file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERM;
            } else if (v.length() < int(sizeof(file_stat->remotePerm))) {
                strcpy(file_stat->remotePerm, v.constData());
                file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERM;
            } else {
                qWarning() << "permissions too large" << v;
            }
        }
    }

    return file_stat;
}

void DiscoverySingleDirectoryJob::directoryListingIteratedSlot(QString file, const QMap<QString,QString> &map)
{
    //qDebug() << Q_FUNC_INFO << _subPath << file << map.count() << map.keys() << _account->davPath() << _lsColJob->reply()->request().url().path();
    if (!_ignoredFirst) {
        // The first entry is for the folder itself, we should process it differently.
        _ignoredFirst = true;
        if (map.contains("permissions")) {
            emit firstDirectoryPermissions(map.value("permissions"));
        }
        if (map.contains("data-fingerprint")) {
            _dataFingerprint = map.value("data-fingerprint").toUtf8();
        }
    } else {
        // Remove <webDAV-Url>/folder/ from <webDAV-Url>/folder/subfile.txt
        file.remove(0, _lsColJob->reply()->request().url().path().length());
        // remove trailing slash
        while (file.endsWith('/')) {
            file.chop(1);
        }
        // remove leading slash
        while (file.startsWith('/')) {
            file = file.remove(0, 1);
        }


        FileStatPointer file_stat(propertyMapToFileStat(map));
        file_stat->name = strdup(file.toUtf8());
        if (!file_stat->etag || strlen(file_stat->etag) == 0) {
            qDebug() << "WARNING: etag of" << file_stat->name << "is" << file_stat->etag << " This must not happen.";
        }

        QStringRef fileRef(&file);
        int slashPos = file.lastIndexOf(QLatin1Char('/'));
        if( slashPos > -1 ) {
            fileRef = file.midRef(slashPos+1);
        }
        //qDebug() << "!!!!" << file_stat << file_stat->name << file_stat->file_id << map.count();
        _results.append(file_stat);
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
        emit finishedWithError(ERRNO_WRONG_CONTENT, QLatin1String("Server error: PROPFIND reply is not XML formatted!"));
        deleteLater();
        return;
    }
    emit etag(_firstEtag);
    emit etagConcatenation(_etagConcatenation);
    emit finishedWithResult(_results);
    deleteLater();
}

void DiscoverySingleDirectoryJob::lsJobFinishedWithErrorSlot(QNetworkReply *r)
{
    QString contentType = r->header(QNetworkRequest::ContentTypeHeader).toString();
    int httpCode = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString httpReason = r->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    QString msg = r->errorString();
    int errnoCode = EIO; // Something went wrong
    qDebug() << Q_FUNC_INFO << r->errorString() << httpCode << r->error();
    if (httpCode != 0 && httpCode != 207) {
        errnoCode = get_errno_from_http_errcode(httpCode, httpReason);
    } else if (r->error() != QNetworkReply::NoError) {
        errnoCode = EIO;
    } else if (!contentType.contains("application/xml; charset=utf-8")) {
        msg = QLatin1String("Server error: PROPFIND reply is not XML formatted!");
        errnoCode = ERRNO_WRONG_CONTENT;
    } else {
        // Default keep at EIO, see above
    }

    emit finishedWithError(errnoCode == 0 ? EIO : errnoCode, msg);
    deleteLater();
}

void DiscoveryMainThread::setupHooks(DiscoveryJob *discoveryJob, const QString &pathPrefix)
{
    _discoveryJob = discoveryJob;
    _pathPrefix = pathPrefix;

    connect(discoveryJob, SIGNAL(doOpendirSignal(QString,DiscoveryDirectoryResult*)),
            this, SLOT(doOpendirSlot(QString,DiscoveryDirectoryResult*)),
            Qt::QueuedConnection);
    connect(discoveryJob, SIGNAL(doGetSizeSignal(QString,qint64*)),
            this, SLOT(doGetSizeSlot(QString,qint64*)),
            Qt::QueuedConnection);
}

// Coming from owncloud_opendir -> DiscoveryJob::vio_opendir_hook -> doOpendirSignal
void DiscoveryMainThread::doOpendirSlot(const QString &subPath, DiscoveryDirectoryResult *r)
{
    QString fullPath = _pathPrefix;
    if (!_pathPrefix.endsWith('/')) {
        fullPath += '/';
    }
    fullPath += subPath;
    // remove trailing slash
    while (fullPath.endsWith('/')) {
        fullPath.chop(1);
    }

    // emit _discoveryJob->folderDiscovered(false, subPath);
    _discoveryJob->update_job_update_callback (false, subPath.toUtf8(), _discoveryJob);

    // Result gets written in there
    _currentDiscoveryDirectoryResult = r;
    _currentDiscoveryDirectoryResult->path = fullPath;

    // Schedule the DiscoverySingleDirectoryJob
    _singleDirJob = new DiscoverySingleDirectoryJob(_account, fullPath, this);
    QObject::connect(_singleDirJob, SIGNAL(finishedWithResult(const QList<FileStatPointer> &)),
                     this, SLOT(singleDirectoryJobResultSlot(const QList<FileStatPointer> &)));
    QObject::connect(_singleDirJob, SIGNAL(finishedWithError(int,QString)),
                     this, SLOT(singleDirectoryJobFinishedWithErrorSlot(int,QString)));
    QObject::connect(_singleDirJob, SIGNAL(firstDirectoryPermissions(QString)),
                     this, SLOT(singleDirectoryJobFirstDirectoryPermissionsSlot(QString)));
    QObject::connect(_singleDirJob, SIGNAL(etagConcatenation(QString)),
                     this, SIGNAL(etagConcatenation(QString)));
    QObject::connect(_singleDirJob, SIGNAL(etag(QString)),
                     this, SIGNAL(etag(QString)));

    if (!_firstFolderProcessed) {
        _singleDirJob->setIsRootPath();
    }

    _singleDirJob->start();
}


void DiscoveryMainThread::singleDirectoryJobResultSlot(const QList<FileStatPointer> & result)
{
    if (!_currentDiscoveryDirectoryResult) {
        return; // possibly aborted
    }
    qDebug() << Q_FUNC_INFO << "Have" << result.count() << "results for " << _currentDiscoveryDirectoryResult->path;


    _currentDiscoveryDirectoryResult->list = result;
    _currentDiscoveryDirectoryResult->code = 0;
    _currentDiscoveryDirectoryResult->listIndex = 0;
    _currentDiscoveryDirectoryResult = 0; // the sync thread owns it now

    if (!_firstFolderProcessed) {
        _firstFolderProcessed = true;
        _dataFingerprint = _singleDirJob->_dataFingerprint;
    }

    _discoveryJob->_vioMutex.lock();
    _discoveryJob->_vioWaitCondition.wakeAll();
    _discoveryJob->_vioMutex.unlock();
}

void DiscoveryMainThread::singleDirectoryJobFinishedWithErrorSlot(int csyncErrnoCode, const QString &msg)
{
    if (!_currentDiscoveryDirectoryResult) {
        return; // possibly aborted
    }
    qDebug() << Q_FUNC_INFO << csyncErrnoCode << msg;

     _currentDiscoveryDirectoryResult->code = csyncErrnoCode;
     _currentDiscoveryDirectoryResult->msg = msg;
     _currentDiscoveryDirectoryResult = 0; // the sync thread owns it now

    _discoveryJob->_vioMutex.lock();
    _discoveryJob->_vioWaitCondition.wakeAll();
    _discoveryJob->_vioMutex.unlock();
}

void DiscoveryMainThread::singleDirectoryJobFirstDirectoryPermissionsSlot(const QString &p)
{
    // Should be thread safe since the sync thread is blocked
    if (!_discoveryJob->_csync_ctx->remote.root_perms) {
        qDebug() << "Permissions for root dir:" << p;
        _discoveryJob->_csync_ctx->remote.root_perms = strdup(p.toUtf8());
    }
}

void DiscoveryMainThread::doGetSizeSlot(const QString& path, qint64* result)
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
    propfindJob->setProperties(QList<QByteArray>() << "resourcetype" << "http://owncloud.org/ns:size");
    QObject::connect(propfindJob, SIGNAL(finishedWithError()),
                     this, SLOT(slotGetSizeFinishedWithError()));
    QObject::connect(propfindJob, SIGNAL(result(QVariantMap)),
                     this, SLOT(slotGetSizeResult(QVariantMap)));
    propfindJob->start();
}

void DiscoveryMainThread::slotGetSizeFinishedWithError()
{
    if (! _currentGetSizeResult) {
        return; // possibly aborted
    }

    qWarning() << "Error getting the size of the directory";
    // just let let the discovery job continue then
    _currentGetSizeResult = 0;
    QMutexLocker locker(&_discoveryJob->_vioMutex);
    _discoveryJob->_vioWaitCondition.wakeAll();

}

void DiscoveryMainThread::slotGetSizeResult(const QVariantMap &map)
{
    if (! _currentGetSizeResult) {
        return; // possibly aborted
    }

    *_currentGetSizeResult = map.value(QLatin1String("size")).toLongLong();
    qDebug() << "Size of folder:" << *_currentGetSizeResult;
    _currentGetSizeResult = 0;
    QMutexLocker locker(&_discoveryJob->_vioMutex);
    _discoveryJob->_vioWaitCondition.wakeAll();
}




// called from SyncEngine
void DiscoveryMainThread::abort() {
    if (_singleDirJob) {
        _singleDirJob->disconnect(SIGNAL(finishedWithError(int,QString)), this);
        _singleDirJob->disconnect(SIGNAL(firstDirectoryPermissions(QString)), this);
        _singleDirJob->disconnect(SIGNAL(finishedWithResult(const QList<FileStatPointer> &)), this);
        _singleDirJob->abort();
    }
    if (_currentDiscoveryDirectoryResult) {
        if (_discoveryJob->_vioMutex.tryLock()) {
            _currentDiscoveryDirectoryResult->msg = tr("Aborted by the user"); // Actually also created somewhere else by sync engine
            _currentDiscoveryDirectoryResult->code = EIO;
            _currentDiscoveryDirectoryResult = 0;
            _discoveryJob->_vioWaitCondition.wakeAll();
            _discoveryJob->_vioMutex.unlock();
        }
    }
    if (_currentGetSizeResult) {
        _currentGetSizeResult = 0;
        QMutexLocker locker(&_discoveryJob->_vioMutex);
        _discoveryJob->_vioWaitCondition.wakeAll();
    }
}

csync_vio_handle_t* DiscoveryJob::remote_vio_opendir_hook (const char *url,
                                    void *userdata)
{
    DiscoveryJob *discoveryJob = static_cast<DiscoveryJob*>(userdata);
    if (discoveryJob) {
        qDebug() << discoveryJob << url << "Calling into main thread...";

        QScopedPointer<DiscoveryDirectoryResult> directoryResult(new DiscoveryDirectoryResult());
        directoryResult->code = EIO;

        discoveryJob->_vioMutex.lock();
        const QString qurl = QString::fromUtf8(url);
        emit discoveryJob->doOpendirSignal(qurl, directoryResult.data());
        discoveryJob->_vioWaitCondition.wait(&discoveryJob->_vioMutex, ULONG_MAX); // FIXME timeout?
        discoveryJob->_vioMutex.unlock();

        qDebug() << discoveryJob << url << "...Returned from main thread";

        // Upon awakening from the _vioWaitCondition, iterator should be a valid iterator.
        if (directoryResult->code != 0) {
            qDebug() << directoryResult->code << "when opening" << url << "msg=" << directoryResult->msg;
            errno = directoryResult->code;
            // save the error string to the context
            discoveryJob->_csync_ctx->error_string = qstrdup( directoryResult->msg.toUtf8().constData() );
            return NULL;
        }

        return directoryResult.take();
    }
    return NULL;
}


csync_vio_file_stat_t* DiscoveryJob::remote_vio_readdir_hook (csync_vio_handle_t *dhandle,
                                                              void *userdata)
{
    DiscoveryJob *discoveryJob = static_cast<DiscoveryJob*>(userdata);
    if (discoveryJob) {
        DiscoveryDirectoryResult *directoryResult = static_cast<DiscoveryDirectoryResult*>(dhandle);
        if (directoryResult->listIndex < directoryResult->list.size()) {
            csync_vio_file_stat_t *file_stat = directoryResult->list.at(directoryResult->listIndex++).data();
            // Make a copy, csync_update will delete the copy
            return csync_vio_file_stat_copy(file_stat);
        }
    }
    return NULL;
}

void DiscoveryJob::remote_vio_closedir_hook (csync_vio_handle_t *dhandle,  void *userdata)
{
    DiscoveryJob *discoveryJob = static_cast<DiscoveryJob*>(userdata);
    if (discoveryJob) {
        DiscoveryDirectoryResult *directoryResult = static_cast<DiscoveryDirectoryResult*> (dhandle);
        QString path = directoryResult->path;
        qDebug() << Q_FUNC_INFO << discoveryJob << path;
        delete directoryResult; // just deletes the struct and the iterator, the data itself is owned by the SyncEngine/DiscoveryMainThread
    }
}

void DiscoveryJob::start() {
    _selectiveSyncBlackList.sort();
    _selectiveSyncWhiteList.sort();
    _csync_ctx->callbacks.update_callback_userdata = this;
    _csync_ctx->callbacks.update_callback = update_job_update_callback;
    _csync_ctx->callbacks.checkSelectiveSyncBlackListHook = isInSelectiveSyncBlackListCallback;
    _csync_ctx->callbacks.checkSelectiveSyncNewFolderHook = checkSelectiveSyncNewFolderCallback;

    _csync_ctx->callbacks.remote_opendir_hook = remote_vio_opendir_hook;
    _csync_ctx->callbacks.remote_readdir_hook = remote_vio_readdir_hook;
    _csync_ctx->callbacks.remote_closedir_hook = remote_vio_closedir_hook;
    _csync_ctx->callbacks.vio_userdata = this;

    csync_set_log_callback(_log_callback);
    csync_set_log_level(_log_level);
    csync_set_log_userdata(_log_userdata);
    _lastUpdateProgressCallbackCall.invalidate();
    int ret = csync_update(_csync_ctx);

    _csync_ctx->callbacks.checkSelectiveSyncNewFolderHook = 0;
    _csync_ctx->callbacks.checkSelectiveSyncBlackListHook = 0;
    _csync_ctx->callbacks.update_callback = 0;
    _csync_ctx->callbacks.update_callback_userdata = 0;

    emit finished(ret);
    deleteLater();
}

}
