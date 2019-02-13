/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

extern "C" {
#include "libzsync/zsync.h"
}

#include "config.h"
#include "propagateupload.h"
#include "owncloudpropagator_p.h"
#include "networkjobs.h"
#include "account.h"
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "common/utility.h"
#include "filesystem.h"
#include "propagatorjobs.h"
#include "syncengine.h"
#include "propagateremotemove.h"
#include "propagateremotedelete.h"
#include "common/asserts.h"

#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>

#include <cmath>
#include <cstring>
#include <memory>

namespace OCC {

QUrl PropagateUploadFileNG::chunkUrl(qint64 chunkOffset)
{
    QString path = QLatin1String("remote.php/dav/uploads/")
        + propagator()->account()->davUser()
        + QLatin1Char('/') + QString::number(_transferId);
    if (chunkOffset != -1) {
        // We need to do add leading 0 because the server orders the chunk alphabetically
        path += QLatin1Char('/') + QString::number(chunkOffset).rightJustified(16, '0'); // 1e16 is 10 petabyte
    }
    return Utility::concatUrlPath(propagator()->account()->url(), path);
}

void PropagateUploadFileNG::slotZsyncSeedFinished(void *_zs)
{
    zsync_unique_ptr<struct zsync_state> zs(static_cast<struct zsync_state *>(_zs), [](struct zsync_state *zs) {
        zsync_end(zs);
    });
    { /* And print how far we've progressed towards the target file */
        long long done, total;

        zsync_progress(zs.get(), &done, &total);
        qCInfo(lcZsyncPut).nospace() << "Done reading: "
                                     << _item->_file << " " << fixed << qSetRealNumberPrecision(1) << (100.0f * done) / total
                                     << "% of target seeded.";
    }

    /* Get a set of byte ranges that we need to complete the target */
    int _nrange = 0;
    zsync_unique_ptr<off_t> zbyterange(zsync_needed_byte_ranges(zs.get(), &_nrange, 0), [](off_t *zbr) {
        free(zbr);
    });
    if (!zbyterange) {
        abortWithError(SyncFileItem::NormalError, tr("Failed to get zsync byte ranges."));
        return;
    }

    qCDebug(lcZsyncPut) << "Number of ranges:" << _nrange;

    // The remote file size according to zsync metadata
    auto remoteSize = static_cast<qint64>(zsync_file_length(zs.get()));

    /* If we have no ranges then we have equal files and we are done */
    if (_nrange == 0 && _item->_size == remoteSize) {
        propagator()->reportFileTotal(*_item, 0);
        finalize();
        return;
    }

    // Size of combined uploads
    int totalBytes = 0;

    /* The rightmost range returned by zsync can be larger than the local or remote
     * file size. That happens because zsync only considers whole blocks.
     *
     * There are two symmetric cases:
     *
     *   local smaller than remote (block size 6 bytes)
     *     local:   abcdefg
     *     remote:  abcdefaaa
     *     blocks:        000000
     *     desired:       0
     *
     *   remote smaller than local (block size 6 bytes)
     *     local:   abcdefaaa
     *     remote:  abcdefg
     *     blocks:        000000
     *     desired:       011     (so two blocks, the second one will be added below)
     *
     * To address this we limit the zsync blocks to min(localsize, remotesize). That
     * way we only attempt to upload data that's locally available (local smaller)
     * and can easily handle the case of a locally grown file below (remote smaller).
     */
    qint64 minSize = qMin(_item->_size, remoteSize);
    for (int i = 0; i < _nrange; i++) {
        UploadRangeInfo rangeinfo = { qint64(zbyterange.get()[(2 * i)]), qint64(zbyterange.get()[(2 * i) + 1]) - qint64(zbyterange.get()[(2 * i)]) + 1 };
        if (rangeinfo.start < minSize) {
            if (rangeinfo.end() > minSize)
                rangeinfo.size = minSize - rangeinfo.start;
            _rangesToUpload.append(rangeinfo);
            totalBytes += rangeinfo.size;
        }
    }

    // If the local file has grown, upload the new local data
    if (_item->_size > remoteSize) {
        qint64 appendedBytes = _item->_size - remoteSize;
        // Append to the last range if possible
        if (!_rangesToUpload.isEmpty() && _rangesToUpload.last().end() == remoteSize) {
            _rangesToUpload.last().size += appendedBytes;
        } else {
            UploadRangeInfo rangeinfo = { remoteSize, appendedBytes };
            _rangesToUpload.append(rangeinfo);
        }
        totalBytes += appendedBytes;
    }

    for (const auto &range : _rangesToUpload)
        qCDebug(lcZsyncPut) << "Upload range:" << range.start << range.size;
    qCDebug(lcZsyncPut) << "Total bytes:" << totalBytes << "of file size" << _item->_size;

    propagator()->reportFileTotal(*_item, totalBytes);
    _bytesToUpload = totalBytes;

    doStartUploadNext();
}

void PropagateUploadFileNG::slotZsyncSeedFailed(const QString &errorString)
{
    qCCritical(lcZsyncPut) << errorString;

    /* delete remote zsync file */
    QUrl zsyncUrl = zsyncMetadataUrl(propagator(), _item->_file);
    (new DeleteJob(propagator()->account(), zsyncUrl, this))->start();

    abortWithError(SyncFileItem::NormalError, errorString);
}

/*
State machine:

  +---> doStartUpload()
        isZsyncPropagationEnabled()?  +--+ yes +---> Download and seed zsync metadata and set-up new _rangesToUpload
           +                                                               +
           |no                                                             |
           |                                                               |
           |                                                               |
           +^--------------------------------------------------------------+
           v
        doStartUploadNext()
        isZsyncPropagationEnabled()?  +--+ yes +---> Generate new zsync metadata file +--------------------+
           +                                                       +                                       |
           |no                                                     |                                       |
           |                                                       |                             Upload .zsync chunk
           v                                                       |                                       |
        Check the db: is there an entry? <-------------------------+                                       |
           +                           +                                                                   |
           |no                         |yes                                                                |
           |                           v                                                                   |
           v                        PROPFIND                                                               |
           startNewUpload() <-+        +-------------------------------------+                             |
              +               |        +                                     |                             |
             MKCOL            + slotPropfindFinishedWithError()     slotPropfindFinished()                 |
              +                                                       Is there stale files to remove?      |
          slotMkColFinished()                                         +                      +             |
              +                                                       no                    yes            |
              |                                                       +                      +             |
              |                                                       |                  DeleteJob         |
              |                                                       |                      +             |
        +-----+^------------------------------------------------------+^--+  slotDeleteJobFinished()       |
        |                                                                                                  |
        |                             +--------------------------------------------------------------------+
        |                             v
        +---->  startNextChunk() +-> finished?  +-
                      ^               +          |
                      +---------------+          |
                                                 |
        +----------------------------------------+
        |
        +-> MOVE +-----> moveJobFinished() +--> finalize()
 */

void PropagateUploadFileNG::doStartUpload()
{
    propagator()->_activeJobList.append(this);

    _zsyncSupported = isZsyncPropagationEnabled(propagator(), _item);
    if (_zsyncSupported && _item->_remotePerm.hasPermission(RemotePermissions::HasZSyncMetadata)) {
        // Retrieve zsync metadata file from the server
        qCInfo(lcZsyncPut) << "Retrieving zsync metadata for:" << _item->_file;
        QNetworkRequest req;
        req.setPriority(QNetworkRequest::LowPriority);
        QUrl zsyncUrl = zsyncMetadataUrl(propagator(), _item->_file);
        auto job = propagator()->account()->sendRequest("GET", zsyncUrl, req);
        connect(job, &SimpleNetworkJob::finishedSignal, this, &PropagateUploadFileNG::slotZsyncGetMetaFinished);
        return;
    }

    UploadRangeInfo rangeinfo = { 0, _item->_size };
    _rangesToUpload.append(rangeinfo);
    _bytesToUpload = _item->_size;
    doStartUploadNext();
}

void PropagateUploadFileNG::slotZsyncGetMetaFinished(QNetworkReply *reply)
{
    int httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatusCode / 100 != 2) {
        /* Fall back to full upload */
        qCWarning(lcZsyncPut) << "Failed to retrieve zsync metadata for:" << _item->_file;
        _rangesToUpload.clear();
        UploadRangeInfo rangeinfo = { 0, _item->_size };
        _rangesToUpload.append(rangeinfo);
        _bytesToUpload = _item->_size;
        doStartUploadNext();
        return;
    }

    QByteArray zsyncData = reply->readAll();

    qCInfo(lcZsyncPut) << "Retrieved zsync metadata for:" << _item->_file << "size:" << zsyncData.size();

    ZsyncSeedRunnable *run = new ZsyncSeedRunnable(zsyncData, propagator()->getFilePath(_item->_file), ZsyncMode::upload);
    connect(run, &ZsyncSeedRunnable::finishedSignal, this, &PropagateUploadFileNG::slotZsyncSeedFinished);
    connect(run, &ZsyncSeedRunnable::failedSignal, this, &PropagateUploadFileNG::slotZsyncSeedFailed);

    // Starts in a seperate thread
    QThreadPool::globalInstance()->start(run);
}

void PropagateUploadFileNG::doStartUploadNext()
{
    if (_zsyncSupported) {
        _isZsyncMetadataUploadRunning = true;

        ZsyncGenerateRunnable *run = new ZsyncGenerateRunnable(propagator()->getFilePath(_item->_file));
        connect(run, &ZsyncGenerateRunnable::finishedSignal, this, &PropagateUploadFileNG::slotZsyncGenerationFinished);
        connect(run, &ZsyncGenerateRunnable::failedSignal, this, &PropagateUploadFileNG::slotZsyncGenerationFailed);

        // Starts in a seperate thread
        QThreadPool::globalInstance()->start(run);
    }

    const SyncJournalDb::UploadInfo progressInfo = propagator()->_journal->getUploadInfo(_item->_file);
    if (progressInfo._valid && progressInfo.isChunked() && progressInfo._modtime == _item->_modtime
            && progressInfo._size == _item->_size) {
        _transferId = progressInfo._transferid;
        auto url = chunkUrl();
        auto job = new LsColJob(propagator()->account(), url, this);
        _jobs.append(job);
        job->setProperties(QList<QByteArray>() << "resourcetype"
                                               << "getcontentlength");
        connect(job, &LsColJob::finishedWithoutError, this, &PropagateUploadFileNG::slotPropfindFinished);
        connect(job, &LsColJob::finishedWithError,
            this, &PropagateUploadFileNG::slotPropfindFinishedWithError);
        connect(job, &QObject::destroyed, this, &PropagateUploadFileCommon::slotJobDestroyed);
        connect(job, &LsColJob::directoryListingIterated,
            this, &PropagateUploadFileNG::slotPropfindIterate);
        job->start();
        return;
    } else if (progressInfo._valid && progressInfo.isChunked()) {
        // The upload info is stale. remove the stale chunks on the server
        _transferId = progressInfo._transferid;
        // Fire and forget. Any error will be ignored.
        (new DeleteJob(propagator()->account(), chunkUrl(), this))->start();
        // startNewUpload will reset the _transferId and the UploadInfo in the db.
    }

    startNewUpload();
}

void PropagateUploadFileNG::slotPropfindIterate(const QString &name, const QMap<QString, QString> &properties)
{
    if (name == chunkUrl().path()) {
        return; // skip the info about the path itself
    }
    bool ok = false;
    QString chunkName = name.mid(name.lastIndexOf('/') + 1);
    qint64 chunkOffset = chunkName.toLongLong(&ok);
    if (ok) {
        ServerChunkInfo chunkinfo = { properties["getcontentlength"].toLongLong(), chunkName };
        _serverChunks[chunkOffset] = chunkinfo;
    }
}


bool PropagateUploadFileNG::markRangeAsDone(qint64 start, qint64 size)
{
    bool found = false;
    for (auto iter = _rangesToUpload.begin(); iter != _rangesToUpload.end(); ++iter) {
        /* Only remove if they start at exactly the same chunk */
        if (iter->start == start && iter->size >= size) {
            found = true;
            iter->start += size;
            iter->size -= size;
            if (iter->size <= 0) {
                _rangesToUpload.erase(iter);
                break;
            }
        }
    }

    return found;
}

void PropagateUploadFileNG::slotPropfindFinished()
{
    auto job = qobject_cast<LsColJob *>(sender());
    slotJobDestroyed(job); // remove it from the _jobs list
    propagator()->_activeJobList.removeOne(this);

    _currentChunkOffset = 0;
    _sent = 0;

    for (auto chunkOffset : _serverChunks.keys()) {
        auto chunkSize = _serverChunks[chunkOffset].size;
        if (markRangeAsDone(chunkOffset, chunkSize)) {
            qCDebug(lcPropagateUpload) << "Reusing existing data:" << chunkOffset << chunkSize;
            _sent += chunkSize;
            _serverChunks.remove(chunkOffset);
        } else {
            qCDebug(lcPropagateUpload) << "Discarding existing data:" << chunkOffset << chunkSize;
        }
    }

    if (_sent > _bytesToUpload) {
        // Normally this can't happen because the size is xor'ed with the transfer id, and it is
        // therefore impossible that there is more data on the server than on the file.
        qCCritical(lcPropagateUpload) << "Inconsistency while resuming " << _item->_file
                                      << ": the size on the server (" << _sent << ") is bigger than the size of the file ("
                                      << _item->_size << ")";

        // Wipe the old chunking data.
        // Fire and forget. Any error will be ignored.
        (new DeleteJob(propagator()->account(), chunkUrl(), this))->start();

        propagator()->_activeJobList.append(this);
        startNewUpload();
        return;
    }

    qCInfo(lcPropagateUpload) << "Resuming " << _item->_file << "; sent =" << _sent << "; total=" << _bytesToUpload;

    if (!_serverChunks.isEmpty()) {
        qCInfo(lcPropagateUpload) << "To Delete" << _serverChunks.keys();
        propagator()->_activeJobList.append(this);
        _removeJobError = false;

        // Make sure that if there is a "hole" and then a few more chunks, on the server
        // we should remove the later chunks. Otherwise when we do dynamic chunk sizing, we may end up
        // with corruptions if there are too many chunks, or if we abort and there are still stale chunks.
        for (auto it = _serverChunks.begin(); it != _serverChunks.end(); ++it) {
            auto job = new DeleteJob(propagator()->account(), Utility::concatUrlPath(chunkUrl(), it->originalName), this);
            QObject::connect(job, &DeleteJob::finishedSignal, this, &PropagateUploadFileNG::slotDeleteJobFinished);
            _jobs.append(job);
            job->start();
        }
        _serverChunks.clear();
        return;
    }

    startNextChunk();
}

void PropagateUploadFileNG::slotPropfindFinishedWithError()
{
    auto job = qobject_cast<LsColJob *>(sender());
    slotJobDestroyed(job); // remove it from the _jobs list
    QNetworkReply::NetworkError err = job->reply()->error();
    auto httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    auto status = classifyError(err, httpErrorCode, &propagator()->_anotherSyncNeeded);
    if (status == SyncFileItem::FatalError) {
        _item->_requestId = job->requestId();
        propagator()->_activeJobList.removeOne(this);
        abortWithError(status, job->errorStringParsingBody());
        return;
    }
    startNewUpload();
}

void PropagateUploadFileNG::slotDeleteJobFinished()
{
    auto job = qobject_cast<DeleteJob *>(sender());
    ASSERT(job);
    _jobs.remove(_jobs.indexOf(job));

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        const int httpStatus = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        SyncFileItem::Status status = classifyError(err, httpStatus);
        if (status == SyncFileItem::FatalError) {
            _item->_requestId = job->requestId();
            abortWithError(status, job->errorString());
            return;
        } else {
            qCWarning(lcPropagateUpload) << "DeleteJob errored out" << job->errorString() << job->reply()->url();
            _removeJobError = true;
            // Let the other jobs finish
        }
    }

    // If no more Delete jobs are running, we can continue
    // (the zsync metadata upload might run in parallel)
    bool runningDeleteJobs = false;
    for (auto otherJob : _jobs) {
        if (qobject_cast<DeleteJob *>(otherJob))
            runningDeleteJobs = true;
    }
    if (!runningDeleteJobs) {
        propagator()->_activeJobList.removeOne(this);
        if (_removeJobError) {
            // There was an error removing some files, just start over
            startNewUpload();
        } else {
            startNextChunk();
        }
    }
}


void PropagateUploadFileNG::startNewUpload()
{
    ASSERT(propagator()->_activeJobList.count(this) == 1);
    _transferId = uint(qrand()) ^ uint(_item->_modtime) ^ (uint(_item->_size) << 16) ^ qHash(_item->_file);
    _sent = 0;

    propagator()->reportProgress(*_item, 0);

    SyncJournalDb::UploadInfo pi;
    pi._valid = true;
    pi._transferid = _transferId;
    pi._modtime = _item->_modtime;
    pi._contentChecksum = _item->_checksumHeader;
    pi._size = _item->_size;
    propagator()->_journal->setUploadInfo(_item->_file, pi);
    propagator()->_journal->commit("Upload info");
    QMap<QByteArray, QByteArray> headers;
    headers["OC-Total-Length"] = QByteArray::number(_item->_size);
    auto job = new MkColJob(propagator()->account(), chunkUrl(), headers, this);
    connect(job, SIGNAL(finished(QNetworkReply::NetworkError)),
        this, SLOT(slotMkColFinished(QNetworkReply::NetworkError)));
    connect(job, &QObject::destroyed, this, &PropagateUploadFileCommon::slotJobDestroyed);
    job->start();
}

void PropagateUploadFileNG::slotMkColFinished(QNetworkReply::NetworkError)
{
    propagator()->_activeJobList.removeOne(this);
    auto job = qobject_cast<MkColJob *>(sender());
    slotJobDestroyed(job); // remove it from the _jobs list
    QNetworkReply::NetworkError err = job->reply()->error();
    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (err != QNetworkReply::NoError || _item->_httpErrorCode != 201) {
        _item->_requestId = job->requestId();
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
            &propagator()->_anotherSyncNeeded);
        abortWithError(status, job->errorStringParsingBody());
        return;
    }

    startNextChunk();
}

void PropagateUploadFileNG::doFinalMove()
{
    // Still not finished metadata upload.
    if (_isZsyncMetadataUploadRunning)
        return;

    // Still not finished all ranges.
    if (!_rangesToUpload.isEmpty())
        return;

    ENFORCE(_jobs.isEmpty(), "MOVE for upload even though jobs are still running");

    _finished = true;

    // Finish with a MOVE
    QString destination = QDir::cleanPath(propagator()->account()->davUrl().path()
        + propagator()->_remoteFolder + _item->_file);
    auto headers = PropagateUploadFileCommon::headers();

    // "If-Match applies to the source, but we are interested in comparing the etag of the destination
    auto ifMatch = headers.take(QByteArrayLiteral("If-Match"));
    if (!ifMatch.isEmpty()) {
        headers[QByteArrayLiteral("If")] = "<" + destination.toUtf8() + "> ([" + ifMatch + "])";
    }
    if (!_transmissionChecksumHeader.isEmpty()) {
        headers[checkSumHeaderC] = _transmissionChecksumHeader;
    }
    headers[QByteArrayLiteral("OC-Total-Length")] = QByteArray::number(_bytesToUpload);
    headers[QByteArrayLiteral("OC-Total-File-Length")] = QByteArray::number(_item->_size);

    QUrl source = _zsyncSupported ? Utility::concatUrlPath(chunkUrl(), QStringLiteral("/.file.zsync")) : Utility::concatUrlPath(chunkUrl(), QStringLiteral("/.file"));

    auto job = new MoveJob(propagator()->account(), source, destination, headers, this);
    _jobs.append(job);
    connect(job, &MoveJob::finishedSignal, this, &PropagateUploadFileNG::slotMoveJobFinished);
    connect(job, &QObject::destroyed, this, &PropagateUploadFileCommon::slotJobDestroyed);
    propagator()->_activeJobList.append(this);
    adjustLastJobTimeout(job, _item->_size);
    job->start();
    return;
}

void PropagateUploadFileNG::startNextChunk()
{
    if (propagator()->_abortRequested.fetchAndAddRelaxed(0))
        return;

    ENFORCE(_bytesToUpload >= _sent, "Sent data exceeds file size");

    // All ranges complete!
    if (_rangesToUpload.isEmpty()) {
        doFinalMove();
        return;
    }

    _currentChunkOffset = _rangesToUpload.first().start;
    _currentChunkSize = qMin(propagator()->_chunkSize, _rangesToUpload.first().size);

    auto device = std::unique_ptr<UploadDevice>(new UploadDevice(&propagator()->_bandwidthManager));
    const QString fileName = propagator()->getFilePath(_item->_file);

    if (!device->prepareAndOpen(fileName, _currentChunkOffset, _currentChunkSize)) {
        qCWarning(lcPropagateUpload) << "Could not prepare upload device: " << device->errorString();

        // If the file is currently locked, we want to retry the sync
        // when it becomes available again.
        if (FileSystem::isFileLocked(fileName)) {
            emit propagator()->seenLockedFile(fileName);
        }
        // Soft error because this is likely caused by the user modifying his files while syncing
        abortWithError(SyncFileItem::SoftError, device->errorString());
        return;
    }

    QMap<QByteArray, QByteArray> headers;
    headers["OC-Chunk-Offset"] = QByteArray::number(_currentChunkOffset);

    QUrl url = chunkUrl(_currentChunkOffset);

    // job takes ownership of device via a QScopedPointer. Job deletes itself when finishing
    auto devicePtr = device.get(); // for connections later
    PUTFileJob *job = new PUTFileJob(propagator()->account(), url, std::move(device), headers, 0, this);
    _jobs.append(job);
    connect(job, &PUTFileJob::finishedSignal, this, &PropagateUploadFileNG::slotPutFinished);
    connect(job, &PUTFileJob::uploadProgress,
        this, &PropagateUploadFileNG::slotUploadProgress);
    connect(job, &PUTFileJob::uploadProgress,
        devicePtr, &UploadDevice::slotJobUploadProgress);
    connect(job, &QObject::destroyed, this, &PropagateUploadFileCommon::slotJobDestroyed);
    job->start();
    propagator()->_activeJobList.append(this);
}

void PropagateUploadFileNG::slotZsyncGenerationFinished(const QString &generatedFileName)
{
    qCDebug(lcPropagateUpload)
        << "Finished generation of:" << generatedFileName
        << "size:" << FileSystem::getSize(generatedFileName);

    auto device = std::unique_ptr<UploadDevice>(new UploadDevice(&propagator()->_bandwidthManager));

    if (!device->prepareAndOpen(generatedFileName, 0, FileSystem::getSize(generatedFileName))) {
        qCWarning(lcPropagateUpload) << "Could not prepare generated file: " << generatedFileName << device->errorString();
        abortWithError(SyncFileItem::SoftError, device->errorString());
        return;
    }

    QMap<QByteArray, QByteArray> headers;
    QUrl url = Utility::concatUrlPath(chunkUrl(), ".zsync");

    _sent += FileSystem::getSize(generatedFileName);
    _bytesToUpload += FileSystem::getSize(generatedFileName);

    qCDebug(lcPropagateUpload) << "Starting upload of .zsync";

    // job takes ownership of device via a QScopedPointer. Job deletes itself when finishing
    auto devicePtr = device.get(); // for connections later
    PUTFileJob *job = new PUTFileJob(propagator()->account(), url, std::move(device), headers, 0, this);
    _jobs.append(job);
    connect(job, &PUTFileJob::finishedSignal, this, &PropagateUploadFileNG::slotZsyncMetadataUploadFinished);
    connect(job, &PUTFileJob::uploadProgress,
        this, &PropagateUploadFileNG::slotUploadProgress);
    connect(job, &PUTFileJob::uploadProgress,
        devicePtr, &UploadDevice::slotJobUploadProgress);
    job->start();
    propagator()->_activeJobList.append(this);

    FileSystem::remove(generatedFileName);
}

void PropagateUploadFileNG::slotZsyncMetadataUploadFinished()
{
    qCDebug(lcPropagateUpload) << "Uploading of .zsync complete";

    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    ASSERT(job);
    slotJobDestroyed(job);

    _isZsyncMetadataUploadRunning = false;
    doFinalMove();
}

void PropagateUploadFileNG::slotZsyncGenerationFailed(const QString &errorString)
{
    qCWarning(lcZsyncPut) << "Failed to generate zsync metadata file:" << errorString;

    abortWithError(SyncFileItem::SoftError, tr("Failed to generate zsync file."));
}

void PropagateUploadFileNG::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    ASSERT(job);

    slotJobDestroyed(job); // remove it from the _jobs list

    propagator()->_activeJobList.removeOne(this);

    if (_finished) {
        // We have sent the finished signal already. We don't need to handle any remaining jobs
        return;
    }

    QNetworkReply::NetworkError err = job->reply()->error();

    if (err != QNetworkReply::NoError) {
        _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        _item->_requestId = job->requestId();
        commonErrorHandling(job);
        return;
    }

    // Mark the range as uploaded
    markRangeAsDone(_currentChunkOffset, _currentChunkSize);
    _sent += _currentChunkSize;

    ENFORCE(_sent <= _bytesToUpload, "can't send more than size");

    // Adjust the chunk size for the time taken.
    //
    // Dynamic chunk sizing is enabled if the server configured a
    // target duration for each chunk upload.
    auto targetDuration = propagator()->syncOptions()._targetChunkUploadDuration;
    if (targetDuration.count() > 0) {
        auto uploadTime = ++job->msSinceStart(); // add one to avoid div-by-zero
        qint64 predictedGoodSize = (_currentChunkSize * targetDuration) / uploadTime;

        // The whole targeting is heuristic. The predictedGoodSize will fluctuate
        // quite a bit because of external factors (like available bandwidth)
        // and internal factors (like number of parallel uploads).
        //
        // We use an exponential moving average here as a cheap way of smoothing
        // the chunk sizes a bit.
        qint64 targetSize = propagator()->_chunkSize / 2 + predictedGoodSize / 2;

        // Adjust the dynamic chunk size _chunkSize used for sizing of the item's chunks to be send
        propagator()->_chunkSize = qBound(
            propagator()->syncOptions()._minChunkSize,
            targetSize,
            propagator()->syncOptions()._maxChunkSize);

        qCInfo(lcPropagateUpload) << "Chunked upload of" << _currentChunkSize << "bytes took" << uploadTime.count()
                                  << "ms, desired is" << targetDuration.count() << "ms, expected good chunk size is"
                                  << predictedGoodSize << "bytes and nudged next chunk size to "
                                  << propagator()->_chunkSize << "bytes";
    }

    _finished = _sent == _bytesToUpload;

    // Check if the file still exists
    const QString fullFilePath(propagator()->getFilePath(_item->_file));
    if (!FileSystem::fileExists(fullFilePath)) {
        if (!_finished) {
            abortWithError(SyncFileItem::SoftError, tr("The local file was removed during sync."));
            return;
        } else {
            propagator()->_anotherSyncNeeded = true;
        }
    }

    // Check whether the file changed since discovery.
    if (!FileSystem::verifyFileUnchanged(fullFilePath, _item->_size, _item->_modtime)) {
        propagator()->_anotherSyncNeeded = true;
        if (!_finished) {
            abortWithError(SyncFileItem::SoftError, tr("Local file changed during sync."));
            return;
        }
    }

    if (!_finished) {
        // Deletes an existing blacklist entry on successful chunk upload
        if (_item->_hasBlacklistEntry) {
            propagator()->_journal->wipeErrorBlacklistEntry(_item->_file);
            _item->_hasBlacklistEntry = false;
        }

        // Reset the error count on successful chunk upload
        auto uploadInfo = propagator()->_journal->getUploadInfo(_item->_file);
        uploadInfo._errorCount = 0;
        propagator()->_journal->setUploadInfo(_item->_file, uploadInfo);
        propagator()->_journal->commit("Upload info");
    }
    startNextChunk();
}

void PropagateUploadFileNG::slotMoveJobFinished()
{
    propagator()->_activeJobList.removeOne(this);
    auto job = qobject_cast<MoveJob *>(sender());
    slotJobDestroyed(job); // remove it from the _jobs list
    QNetworkReply::NetworkError err = job->reply()->error();
    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = job->responseTimestamp();
    _item->_requestId = job->requestId();

    if (err != QNetworkReply::NoError) {
        commonErrorHandling(job);
        return;
    }
    if (_item->_httpErrorCode != 201 && _item->_httpErrorCode != 204) {
        abortWithError(SyncFileItem::NormalError, tr("Unexpected return code from server (%1)").arg(_item->_httpErrorCode));
        return;
    }

    QByteArray fid = job->reply()->rawHeader("OC-FileID");
    if (fid.isEmpty()) {
        qCWarning(lcPropagateUpload) << "Server did not return a OC-FileID" << _item->_file;
        abortWithError(SyncFileItem::NormalError, tr("Missing File ID from server"));
        return;
    } else {
        // the old file id should only be empty for new files uploaded
        if (!_item->_fileId.isEmpty() && _item->_fileId != fid) {
            qCWarning(lcPropagateUpload) << "File ID changed!" << _item->_fileId << fid;
        }
        _item->_fileId = fid;
    }

    _item->_etag = getEtagFromReply(job->reply());
    ;
    if (_item->_etag.isEmpty()) {
        qCWarning(lcPropagateUpload) << "Server did not return an ETAG" << _item->_file;
        abortWithError(SyncFileItem::NormalError, tr("Missing ETag from server"));
        return;
    }
    finalize();
}

void PropagateUploadFileNG::slotUploadProgress(qint64 sent, qint64 total)
{
    // Completion is signaled with sent=0, total=0; avoid accidentally
    // resetting progress due to the sent being zero by ignoring it.
    // finishedSignal() is bound to be emitted soon anyway.
    // See https://bugreports.qt.io/browse/QTBUG-44782.
    if (sent == 0 && total == 0) {
        return;
    }
    propagator()->reportProgress(*_item, _sent + sent);
}

void PropagateUploadFileNG::abort(PropagatorJob::AbortType abortType)
{
    abortNetworkJobs(
        abortType,
        [abortType](AbstractNetworkJob *job) {
            return abortType != AbortType::Asynchronous || !qobject_cast<MoveJob *>(job);
        });
}

}
