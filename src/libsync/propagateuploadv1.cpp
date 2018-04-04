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
#include "common/checksums.h"
#include "syncengine.h"
#include "propagateremotedelete.h"
#include "common/asserts.h"

#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>
#include <cmath>
#include <cstring>

namespace OCC {

void PropagateUploadFileV1::doStartUpload()
{
    _chunkCount = std::ceil(_item->_size / double(chunkSize()));
    _startChunk = 0;
    _transferId = qrand() ^ _item->_modtime ^ (_item->_size << 16);

    const SyncJournalDb::UploadInfo progressInfo = propagator()->_journal->getUploadInfo(_item->_file);

    if (progressInfo._valid && progressInfo.isChunked() && progressInfo._modtime == _item->_modtime
        && (progressInfo._contentChecksum == _item->_checksumHeader || progressInfo._contentChecksum.isEmpty() || _item->_checksumHeader.isEmpty())) {
        _startChunk = progressInfo._chunk;
        _transferId = progressInfo._transferid;
        qCInfo(lcPropagateUpload) << _item->_file << ": Resuming from chunk " << _startChunk;
    } else if (_chunkCount <= 1 && !_item->_checksumHeader.isEmpty()) {
        // If there is only one chunk, write the checksum in the database, so if the PUT is sent
        // to the server, but the connection drops before we get the etag, we can check the checksum
        // in reconcile (issue #5106)
        SyncJournalDb::UploadInfo pi;
        pi._valid = true;
        pi._chunk = 0;
        pi._transferid = 0; // We set a null transfer id because it is not chunked.
        pi._modtime = _item->_modtime;
        pi._errorCount = 0;
        pi._contentChecksum = _item->_checksumHeader;
        propagator()->_journal->setUploadInfo(_item->_file, pi);
        propagator()->_journal->commit("Upload info");
    }

    _currentChunk = 0;

    propagator()->reportProgress(*_item, 0);
    startNextChunk();
}

void PropagateUploadFileV1::startNextChunk()
{
    if (propagator()->_abortRequested.fetchAndAddRelaxed(0))
        return;

    if (!_jobs.isEmpty() && _currentChunk + _startChunk >= _chunkCount - 1) {
        // Don't do parallel upload of chunk if this might be the last chunk because the server cannot handle that
        // https://github.com/owncloud/core/issues/11106
        // We return now and when the _jobs are finished we will proceed with the last chunk
        // NOTE: Some other parts of the code such as slotUploadProgress also assume that the last chunk
        // is sent last.
        return;
    }
    quint64 fileSize = _item->_size;
    auto headers = PropagateUploadFileCommon::headers();
    headers["OC-Total-Length"] = QByteArray::number(fileSize);
    headers["OC-Chunk-Size"] = QByteArray::number(quint64(chunkSize()));

    QString path = _item->_file;

    UploadDevice *device = new UploadDevice(&propagator()->_bandwidthManager);
    qint64 chunkStart = 0;
    qint64 currentChunkSize = fileSize;
    bool isFinalChunk = false;
    if (_chunkCount > 1) {
        int sendingChunk = (_currentChunk + _startChunk) % _chunkCount;
        // XOR with chunk size to make sure everything goes well if chunk size changes between runs
        uint transid = _transferId ^ chunkSize();
        qCInfo(lcPropagateUpload) << "Upload chunk" << sendingChunk << "of" << _chunkCount << "transferid(remote)=" << transid;
        path += QString("-chunking-%1-%2-%3").arg(transid).arg(_chunkCount).arg(sendingChunk);

        headers["OC-Chunked"] = "1";

        chunkStart = chunkSize() * quint64(sendingChunk);
        currentChunkSize = chunkSize();
        if (sendingChunk == _chunkCount - 1) { // last chunk
            currentChunkSize = (fileSize % chunkSize());
            if (currentChunkSize == 0) { // if the last chunk pretends to be 0, its actually the full chunk size.
                currentChunkSize = chunkSize();
            }
            isFinalChunk = true;
        }
    } else {
        // if there's only one chunk, it's the final one
        isFinalChunk = true;
    }
    qCDebug(lcPropagateUpload) << _chunkCount << isFinalChunk << chunkStart << currentChunkSize;

    if (isFinalChunk && !_transmissionChecksumHeader.isEmpty()) {
        qCInfo(lcPropagateUpload) << propagator()->_remoteFolder + path << _transmissionChecksumHeader;
        headers[checkSumHeaderC] = _transmissionChecksumHeader;
    }

    const QString fileName = propagator()->getFilePath(_item->_file);
    if (!device->prepareAndOpen(fileName, chunkStart, currentChunkSize)) {
        qCWarning(lcPropagateUpload) << "Could not prepare upload device: " << device->errorString();

        // If the file is currently locked, we want to retry the sync
        // when it becomes available again.
        if (FileSystem::isFileLocked(fileName)) {
            emit propagator()->seenLockedFile(fileName);
        }
        // Soft error because this is likely caused by the user modifying his files while syncing
        abortWithError(SyncFileItem::SoftError, device->errorString());
        delete device;
        return;
    }

    // job takes ownership of device via a QScopedPointer. Job deletes itself when finishing
    PUTFileJob *job = new PUTFileJob(propagator()->account(), propagator()->_remoteFolder + path, device, headers, _currentChunk, this);
    _jobs.append(job);
    connect(job, &PUTFileJob::finishedSignal, this, &PropagateUploadFileV1::slotPutFinished);
    connect(job, &PUTFileJob::uploadProgress, this, &PropagateUploadFileV1::slotUploadProgress);
    connect(job, &PUTFileJob::uploadProgress, device, &UploadDevice::slotJobUploadProgress);
    connect(job, &QObject::destroyed, this, &PropagateUploadFileCommon::slotJobDestroyed);
    job->start();
    propagator()->_activeJobList.append(this);
    _currentChunk++;

    bool parallelChunkUpload = true;

    if (propagator()->account()->capabilities().chunkingParallelUploadDisabled()) {
        // Server may also disable parallel chunked upload for any higher version
        parallelChunkUpload = false;
    } else {
        QByteArray env = qgetenv("OWNCLOUD_PARALLEL_CHUNK");
        if (!env.isEmpty()) {
            parallelChunkUpload = env != "false" && env != "0";
        } else {
            int versionNum = propagator()->account()->serverVersionInt();
            if (versionNum < Account::makeServerVersion(8, 0, 3)) {
                // Disable parallel chunk upload severs older than 8.0.3 to avoid too many
                // internal sever errors (#2743, #2938)
                parallelChunkUpload = false;
            }
        }
    }


    if (_currentChunk + _startChunk >= _chunkCount - 1) {
        // Don't do parallel upload of chunk if this might be the last chunk because the server cannot handle that
        // https://github.com/owncloud/core/issues/11106
        parallelChunkUpload = false;
    }

    if (parallelChunkUpload && (propagator()->_activeJobList.count() < propagator()->maximumActiveTransferJob())
        && _currentChunk < _chunkCount) {
        startNextChunk();
    }
    if (!parallelChunkUpload || _chunkCount - _currentChunk <= 0) {
        propagator()->scheduleNextJob();
    }
}

void PropagateUploadFileV1::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    ASSERT(job);

    slotJobDestroyed(job); // remove it from the _jobs list

    propagator()->_activeJobList.removeOne(this);

    if (_finished) {
        // We have sent the finished signal already. We don't need to handle any remaining jobs
        return;
    }

    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError) {
        commonErrorHandling(job);
        return;
    }

    // The server needs some time to process the request and provide us with a poll URL
    if (_item->_httpErrorCode == 202) {
        QString path = QString::fromUtf8(job->reply()->rawHeader("OC-Finish-Poll"));
        if (path.isEmpty()) {
            done(SyncFileItem::NormalError, tr("Poll URL missing"));
            return;
        }
        _finished = true;
        startPollJob(path);
        return;
    }

    // Check the file again post upload.
    // Two cases must be considered separately: If the upload is finished,
    // the file is on the server and has a changed ETag. In that case,
    // the etag has to be properly updated in the client journal, and because
    // of that we can bail out here with an error. But we can reschedule a
    // sync ASAP.
    // But if the upload is ongoing, because not all chunks were uploaded
    // yet, the upload can be stopped and an error can be displayed, because
    // the server hasn't registered the new file yet.
    QByteArray etag = getEtagFromReply(job->reply());
    _finished = etag.length() > 0;

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
            // FIXME:  the legacy code was retrying for a few seconds.
            //         and also checking that after the last chunk, and removed the file in case of INSTRUCTION_NEW
            return;
        }
    }

    if (!_finished) {
        // Proceed to next chunk.
        if (_currentChunk >= _chunkCount) {
            if (!_jobs.empty()) {
                // just wait for the other job to finish.
                return;
            }
            done(SyncFileItem::NormalError, tr("The server did not acknowledge the last chunk. (No e-tag was present)"));
            return;
        }

        // Deletes an existing blacklist entry on successful chunk upload
        if (_item->_hasBlacklistEntry) {
            propagator()->_journal->wipeErrorBlacklistEntry(_item->_file);
            _item->_hasBlacklistEntry = false;
        }

        SyncJournalDb::UploadInfo pi;
        pi._valid = true;
        auto currentChunk = job->_chunk;
        foreach (auto *job, _jobs) {
            // Take the minimum finished one
            if (auto putJob = qobject_cast<PUTFileJob *>(job)) {
                currentChunk = qMin(currentChunk, putJob->_chunk - 1);
            }
        }
        pi._chunk = (currentChunk + _startChunk + 1) % _chunkCount; // next chunk to start with
        pi._transferid = _transferId;
        pi._modtime = _item->_modtime;
        pi._errorCount = 0; // successful chunk upload resets
        pi._contentChecksum = _item->_checksumHeader;
        propagator()->_journal->setUploadInfo(_item->_file, pi);
        propagator()->_journal->commit("Upload info");
        startNextChunk();
        return;
    }
    // the following code only happens after all chunks were uploaded.

    // the file id should only be empty for new files up- or downloaded
    QByteArray fid = job->reply()->rawHeader("OC-FileID");
    if (!fid.isEmpty()) {
        if (!_item->_fileId.isEmpty() && _item->_fileId != fid) {
            qCWarning(lcPropagateUpload) << "File ID changed!" << _item->_fileId << fid;
        }
        _item->_fileId = fid;
    }

    _item->_etag = etag;

    _item->_responseTimeStamp = job->responseTimestamp();

    if (job->reply()->rawHeader("X-OC-MTime") != "accepted") {
        // X-OC-MTime is supported since owncloud 5.0.   But not when chunking.
        // Normally Owncloud 6 always puts X-OC-MTime
        qCWarning(lcPropagateUpload) << "Server does not support X-OC-MTime" << job->reply()->rawHeader("X-OC-MTime");
        // Well, the mtime was not set
        done(SyncFileItem::SoftError, "Server does not support X-OC-MTime");
        return;
    }

    finalize();
}


void PropagateUploadFileV1::slotUploadProgress(qint64 sent, qint64 total)
{
    // Completion is signaled with sent=0, total=0; avoid accidentally
    // resetting progress due to the sent being zero by ignoring it.
    // finishedSignal() is bound to be emitted soon anyway.
    // See https://bugreports.qt.io/browse/QTBUG-44782.
    if (sent == 0 && total == 0) {
        return;
    }

    int progressChunk = _currentChunk + _startChunk - 1;
    if (progressChunk >= _chunkCount)
        progressChunk = _currentChunk - 1;

    // amount is the number of bytes already sent by all the other chunks that were sent
    // not including this one.
    // FIXME: this assumes all chunks have the same size, which is true only if the last chunk
    // has not been finished (which should not happen because the last chunk is sent sequentially)
    quint64 amount = progressChunk * chunkSize();

    sender()->setProperty("byteWritten", sent);
    if (_jobs.count() > 1) {
        amount -= (_jobs.count() - 1) * chunkSize();
        foreach (QObject *j, _jobs) {
            amount += j->property("byteWritten").toULongLong();
        }
    } else {
        // sender() is the only current job, no need to look at the byteWritten properties
        amount += sent;
    }
    propagator()->reportProgress(*_item, amount);
}

void PropagateUploadFileV1::abort(PropagatorJob::AbortType abortType)
{
    // Prepare abort
    prepareAbort(abortType);

    // Abort all jobs (if there are any left), except final PUT
    foreach (AbstractNetworkJob *job, _jobs) {
        if (job->reply()) {
            // If asynchronous abort allowed,
            // dont abort final PUT which uploaded its data,
            // since this might result in conflicts
            if (PUTFileJob *putJob = qobject_cast<PUTFileJob *>(job)){
                if (abortType == AbortType::Asynchronous
                    && _chunkCount > 0
                    && (((_currentChunk + _startChunk) % _chunkCount) == 0)
                    && putJob->device()->atEnd()) {
                    continue;
                }
            }

            // Abort the job
            job->reply()->abort();
        }
    }
}

}
