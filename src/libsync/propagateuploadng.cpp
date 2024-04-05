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

#include "account.h"
#include "common/asserts.h"
#include "common/syncjournaldb.h"
#include "common/utility.h"
#include "filesystem.h"
#include "networkjobs.h"
#include "owncloudpropagator_p.h"
#include "propagateremotedelete.h"
#include "propagateremotemove.h"
#include "propagateupload.h"
#include "propagatorjobs.h"
#include "syncengine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QRandomGenerator>

#include <memory>

namespace OCC {

QString PropagateUploadFileNG::chunkPath(qint64 chunkOffset)
{
    QString path = QLatin1String("remote.php/dav/uploads/")
        + propagator()->account()->davUser()
        + QLatin1Char('/') + QString::number(_transferId);
    if (chunkOffset != -1) {
        // We need to do add leading 0 because the server orders the chunk alphabetically
        path += QLatin1Char('/') + QString::number(chunkOffset).rightJustified(16, QLatin1Char('0')); // 1e16 is 10 petabyte
    }
    return path;
}


/*
State machine:

  +---> doStartUpload()
        doStartUploadNext()
        Check the db: is there an entry?
           +                           +
           |no                         |yes
           |                           v
           v                        PROPFIND
           startNewUpload() <-+        +-------------------------------------+
              +               |        +                                     |
             MKCOL            + slotPropfindFinishedWithError()     slotPropfindFinished()
              +                                                       Is there stale files to remove?
          slotMkColFinished()                                         +                      +
              +                                                       no                    yes
              |                                                       +                      +
              |                                                       |                  DeleteJob
              |                                                       |                      +
        +-----+^------------------------------------------------------+^--+  slotDeleteJobFinished()
        |
        |
        |
        +---->  startNextChunk() +-> finished?  +-
                      ^               +          |
                      +---------------+          |
                                                 |
        +----------------------------------------+
        |
        +-> MOVE +-----> moveJobFinished() +--> finalize()
 */

PropagateUploadFileNG::PropagateUploadFileNG(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
    : PropagateUploadFileCommon(propagator, item)
    , _bytesToUpload(item->_size)
{
}
void PropagateUploadFileNG::doStartUpload()
{
    const QString fileName = propagator()->fullLocalPath(_item->_file);
    // If the file is currently locked, we want to retry the sync
    // when it becomes available again.
    if (FileSystem::isFileLocked(fileName, FileSystem::LockMode::SharedRead)) {
        Q_EMIT propagator()->seenLockedFile(fileName, FileSystem::LockMode::SharedRead);
        abortWithError(SyncFileItem::SoftError, tr("%1 the file is currently in use").arg(QDir::toNativeSeparators(fileName)));
        return;
    }

    propagator()->_activeJobList.append(this);

    UploadRangeInfo rangeinfo = { 0, _item->_size };
    _rangesToUpload.append(rangeinfo);
    _bytesToUpload = _item->_size;
    doStartUploadNext();
}


void PropagateUploadFileNG::doStartUploadNext()
{
    const SyncJournalDb::UploadInfo progressInfo = propagator()->_journal->getUploadInfo(_item->_file);
    if (progressInfo.isChunked() && progressInfo.validate(_item->_size, _item->_modtime, _item->_checksumHeader)) {
        _transferId = progressInfo._transferid;
        auto job = new PropfindJob(propagator()->account(), propagator()->account()->url(), chunkPath(), PropfindJob::Depth::One, this);
        addChildJob(job);
        job->setProperties({ QByteArrayLiteral("resourcetype"), QByteArrayLiteral("getcontentlength") });
        connect(job, &PropfindJob::finishedWithoutError, this, &PropagateUploadFileNG::slotPropfindFinished);
        connect(job, &PropfindJob::finishedWithError,
            this, &PropagateUploadFileNG::slotPropfindFinishedWithError);
        connect(job, &PropfindJob::directoryListingIterated,
            this, &PropagateUploadFileNG::slotPropfindIterate);
        job->start();
        return;
    } else if (progressInfo._valid && progressInfo.isChunked()) {
        // The upload info is stale. remove the stale chunks on the server
        _transferId = progressInfo._transferid;
        // Fire and forget. Any error will be ignored.
        (new DeleteJob(propagator()->account(), propagator()->account()->url(), chunkPath(), this))->start();
        // startNewUpload will reset the _transferId and the UploadInfo in the db.
    }

    startNewUpload();
}

void PropagateUploadFileNG::slotPropfindIterate(const QString &name, const QMap<QString, QString> &properties)
{
    if (name.endsWith(chunkPath())) {
        return; // skip the info about the path itself
    }
    bool ok = false;
    QString chunkName = name.mid(name.lastIndexOf(QLatin1Char('/')) + 1);
    qint64 chunkOffset = chunkName.toLongLong(&ok);
    if (ok) {
        ServerChunkInfo chunkinfo = { properties[QStringLiteral("getcontentlength")].toLongLong(), chunkName };
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
    propagator()->_activeJobList.removeOne(this);

    _currentChunkOffset = 0;
    _sent = 0;

    // here is a copy because we might need to remove item(s) during iteration
    const auto serverChunks = _serverChunks;
    for (auto it = serverChunks.cbegin(); it != serverChunks.cend(); ++it) {
        const auto &chunkOffset = it.key();
        const auto &chunkSize = it.value().size;
        if (markRangeAsDone(chunkOffset, chunkSize)) {
            qCDebug(lcPropagateUploadNG) << "Reusing existing data:" << chunkOffset << chunkSize;
            _sent += chunkSize;
            _serverChunks.remove(chunkOffset);
        } else {
            qCDebug(lcPropagateUploadNG) << "Discarding existing data:" << chunkOffset << chunkSize;
        }
    }

    if (_sent > _bytesToUpload) {
        // Normally this can't happen because the size is xor'ed with the transfer id, and it is
        // therefore impossible that there is more data on the server than on the file.
        qCCritical(lcPropagateUploadNG) << "Inconsistency while resuming " << _item->_file
                                      << ": the size on the server (" << _sent << ") is bigger than the size of the file ("
                                      << _item->_size << ")";

        // Wipe the old chunking data.
        // Fire and forget. Any error will be ignored.
        (new DeleteJob(propagator()->account(), propagator()->account()->url(), chunkPath(), this))->start();

        propagator()->_activeJobList.append(this);
        startNewUpload();
        return;
    }

    qCInfo(lcPropagateUploadNG) << "Resuming " << _item->_file << "; sent =" << _sent << "; total=" << _bytesToUpload;

    if (!_serverChunks.isEmpty()) {
        qCInfo(lcPropagateUploadNG) << "To Delete" << _serverChunks.keys();
        propagator()->_activeJobList.append(this);
        _removeJobError = false;

        // Make sure that if there is a "hole" and then a few more chunks, on the server
        // we should remove the later chunks. Otherwise when we do dynamic chunk sizing, we may end up
        // with corruptions if there are too many chunks, or if we abort and there are still stale chunks.
        for (auto it = _serverChunks.begin(); it != _serverChunks.end(); ++it) {
            auto job = new DeleteJob(propagator()->account(), propagator()->account()->url(), chunkPath() + QLatin1Char('/') + it->originalName, this);
            addChildJob(job);
            connect(job, &DeleteJob::finishedSignal, this, &PropagateUploadFileNG::slotDeleteJobFinished);
            job->start();
        }
        _serverChunks.clear();
        return;
    }

    startNextChunk();
}

void PropagateUploadFileNG::slotPropfindFinishedWithError()
{
    auto job = qobject_cast<PropfindJob *>(sender());
    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = job->responseTimestamp();
    _item->_requestId = job->requestId();

    QNetworkReply::NetworkError err = job->reply()->error();
    auto status = classifyError(err, _item->_httpErrorCode, &propagator()->_anotherSyncNeeded);
    if (status == SyncFileItem::FatalError) {
        propagator()->_activeJobList.removeOne(this);
        abortWithError(status, job->errorStringParsingBody());
        return;
    }
    startNewUpload();
}

void PropagateUploadFileNG::slotDeleteJobFinished()
{
    auto job = qobject_cast<DeleteJob *>(sender());
    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = job->responseTimestamp();
    _item->_requestId = job->requestId();

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        const int httpStatus = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        SyncFileItem::Status status = classifyError(err, httpStatus);
        if (status == SyncFileItem::FatalError) {
            abortWithError(status, job->errorString());
            return;
        } else {
            qCWarning(lcPropagateUploadNG) << "DeleteJob errored out" << job->errorString() << job->reply()->url();
            _removeJobError = true;
            // Let the other jobs finish
        }
    }

    // If no more Delete jobs are running, we can continue
    bool runningDeleteJobs = false;
    for (auto *otherJob : childJobs()) {
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
    OC_ASSERT(propagator()->_activeJobList.count(this) == 1);
    _transferId = QRandomGenerator::global()->generate();
    _sent = 0;

    propagator()->reportProgress(*_item, 0);

    auto pi = _item->toUploadInfo();
    pi._transferid = _transferId;
    propagator()->_journal->setUploadInfo(_item->_file, pi);
    propagator()->_journal->commit(QStringLiteral("Upload info"));
    QMap<QByteArray, QByteArray> headers;
    headers["OC-Total-Length"] = QByteArray::number(_item->_size);
    auto job = new MkColJob(propagator()->account(), propagator()->account()->url(), chunkPath(), headers, this);
    connect(job, &MkColJob::finishedWithError,
        this, &PropagateUploadFileNG::slotMkColFinished);
    connect(job, &MkColJob::finishedWithoutError,
        this, &PropagateUploadFileNG::slotMkColFinished);
    job->start();
}

void PropagateUploadFileNG::slotMkColFinished()
{
    propagator()->_activeJobList.removeOne(this);
    auto job = qobject_cast<MkColJob *>(sender());
    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = job->responseTimestamp();
    _item->_requestId = job->requestId();

    QNetworkReply::NetworkError err = job->reply()->error();
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
    // Still not finished all ranges.
    if (!_rangesToUpload.isEmpty())
        return;
    Q_ASSERT_X(childJobs().empty(), Q_FUNC_INFO, "MOVE for upload even though jobs are still running");

    _finished = true;

    // Finish with a MOVE
    QString destination = QDir::cleanPath(propagator()->webDavUrl().path()
        + propagator()->fullRemotePath(_item->_file));
    auto headers = PropagateUploadFileCommon::headers();

    // "If-Match applies to the source, but we are interested in comparing the etag of the destination
    auto ifMatch = headers.take(QByteArrayLiteral("If-Match"));
    if (!ifMatch.isEmpty()) {
        headers[QByteArrayLiteral("If")] = "<" + QUrl::toPercentEncoding(destination, "/") + "> ([" + ifMatch + "])";
    }
    if (!_transmissionChecksumHeader.isEmpty()) {
        headers[checkSumHeaderC] = _transmissionChecksumHeader;
    }
    headers[QByteArrayLiteral("OC-Total-Length")] = QByteArray::number(_bytesToUpload);
    headers[QByteArrayLiteral("OC-Total-File-Length")] = QByteArray::number(_item->_size);

    const QString source = chunkPath() + QStringLiteral("/.file");

    auto job = new MoveJob(propagator()->account(), propagator()->account()->url(), source, destination, headers, this);
    addChildJob(job);
    connect(job, &MoveJob::finishedSignal, this, &PropagateUploadFileNG::slotMoveJobFinished);
    propagator()->_activeJobList.append(this);
    adjustLastJobTimeout(job, _item->_size);
    job->start();
    return;
}

void PropagateUploadFileNG::startNextChunk()
{
    if (propagator()->_abortRequested)
        return;

    OC_ENFORCE_X(_bytesToUpload >= _sent, "Sent data exceeds file size");

    // All ranges complete!
    if (_rangesToUpload.isEmpty()) {
        doFinalMove();
        return;
    }

    _currentChunkOffset = _rangesToUpload.first().start;
    _currentChunkSize = qMin(propagator()->_chunkSize, _rangesToUpload.first().size);

    const QString fileName = propagator()->fullLocalPath(_item->_file);
    auto device = std::make_unique<UploadDevice>(fileName, _currentChunkOffset, _currentChunkSize,
        propagator()->_bandwidthManager);
    if (!device->open(QIODevice::ReadOnly)) {
        qCWarning(lcPropagateUploadNG) << "Could not prepare upload device: " << device->errorString();
        // Soft error because this is likely caused by the user modifying his files while syncing
        abortWithError(SyncFileItem::SoftError, device->errorString());
        return;
    }

    // job takes ownership of device via a QScopedPointer. Job deletes itself when finishing
    auto devicePtr = device.get(); // for connections later
    PUTFileJob *job = new PUTFileJob(propagator()->account(), propagator()->account()->url(), chunkPath(_currentChunkOffset), std::move(device), {}, 0, this);
    addChildJob(job);
    connect(job, &PUTFileJob::finishedSignal, this, &PropagateUploadFileNG::slotPutFinished);
    connect(job, &PUTFileJob::uploadProgress,
        this, &PropagateUploadFileNG::slotUploadProgress);
    connect(job, &PUTFileJob::uploadProgress,
        devicePtr, &UploadDevice::slotJobUploadProgress);
    job->start();
    propagator()->_activeJobList.append(this);
}

void PropagateUploadFileNG::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    OC_ASSERT(job);

    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = job->responseTimestamp();
    _item->_requestId = job->requestId();

    propagator()->_activeJobList.removeOne(this);

    if (_finished) {
        // We have sent the finished signal already. We don't need to handle any remaining jobs
        return;
    }

    QNetworkReply::NetworkError err = job->reply()->error();

    if (err != QNetworkReply::NoError) {
        commonErrorHandling(job);
        return;
    }

    // Mark the range as uploaded
    markRangeAsDone(_currentChunkOffset, _currentChunkSize);
    _sent += _currentChunkSize;

    OC_ENFORCE_X(_sent <= _bytesToUpload, "can't send more than size");

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

        qCInfo(lcPropagateUploadNG) << "Chunked upload of" << _currentChunkSize << "bytes took" << uploadTime.count()
                                  << "ms, desired is" << targetDuration.count() << "ms, expected good chunk size is"
                                  << predictedGoodSize << "bytes and nudged next chunk size to "
                                  << propagator()->_chunkSize << "bytes";
    }

    _finished = _sent == _bytesToUpload;

    // Check if the file still exists
    const QString fullFilePath(propagator()->fullLocalPath(_item->_file));
    if (!FileSystem::fileExists(fullFilePath)) {
        if (!_finished) {
            abortWithError(SyncFileItem::SoftError, tr("The local file was removed during sync."));
            return;
        } else {
            propagator()->_anotherSyncNeeded = true;
        }
    }

    // Check whether the file changed since discovery.
    if (FileSystem::fileChanged(QFileInfo{fullFilePath}, _item->_size, _item->_modtime)) {
        propagator()->_anotherSyncNeeded = true;
        if (!_finished) {
            abortWithError(SyncFileItem::Message, fileChangedMessage());
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
        propagator()->_journal->commit(QStringLiteral("Upload info"));
    }
    startNextChunk();
}

void PropagateUploadFileNG::slotMoveJobFinished()
{
    propagator()->_activeJobList.removeOne(this);
    auto job = qobject_cast<MoveJob *>(sender());
    QNetworkReply::NetworkError err = job->reply()->error();
    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = job->responseTimestamp();
    _item->_requestId = job->requestId();

    if (err != QNetworkReply::NoError) {
        commonErrorHandling(job);
        return;
    }

    if (_item->_httpErrorCode == 202) {
        done(SyncFileItem::NormalError, tr("The server did ask for a removed legacy feature(polling)"));
        return;
    }

    if (_item->_httpErrorCode != 201 && _item->_httpErrorCode != 204) {
        abortWithError(SyncFileItem::NormalError, tr("Unexpected return code from server (%1)").arg(_item->_httpErrorCode));
        return;
    }

    QByteArray fid = job->reply()->rawHeader("OC-FileID");
    if (fid.isEmpty()) {
        qCWarning(lcPropagateUploadNG) << "Server did not return a OC-FileID" << _item->_file;
        abortWithError(SyncFileItem::NormalError, tr("Missing File ID from server"));
        return;
    } else {
        // the old file id should only be empty for new files uploaded
        if (!_item->_fileId.isEmpty() && _item->_fileId != fid) {
            qCWarning(lcPropagateUploadNG) << "File ID changed!" << _item->_fileId << fid;
        }
        _item->_fileId = fid;
    }

    _item->_etag = getEtagFromReply(job->reply());
    if (_item->_etag.isEmpty()) {
        qCWarning(lcPropagateUploadNG) << "Server did not return an ETAG" << _item->_file;
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
