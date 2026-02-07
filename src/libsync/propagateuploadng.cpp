/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "syncengine.h"
#include "propagateremotemove.h"
#include "deletejob.h"
#include "common/asserts.h"

#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>
#include <cmath>
#include <cstring>

namespace OCC {

constexpr auto relativeUploadsPath = "remote.php/dav/uploads/";

QUrl PropagateUploadFileNG::chunkUploadFolderUrl() const
{
    const QString userUploadPath = relativeUploadsPath + propagator()->account()->davUser();
    const QString chunksUploadPath = userUploadPath + QLatin1Char('/') + QString::number(_transferId);
    return Utility::concatUrlPath(propagator()->account()->url(), chunksUploadPath);
}

QUrl PropagateUploadFileNG::chunkUrl(const int chunk) const
{
    Q_ASSERT(chunk >= 1);
    constexpr auto maxChunkDigits = 5; // Chunk V2: max num of chunks is 10000

    // We need to do add leading 0 because the server orders the chunk alphabetically
    const auto chunkNumString = QStringLiteral("%1").arg(chunk, maxChunkDigits, 10, QChar('0'));
    return Utility::concatUrlPath(chunkUploadFolderUrl(), chunkNumString);
}

/*
  State machine:

     *----> doStartUpload()
            Check the db: is there an entry?
              /               \
             no                yes
            /                   \
           /                  PROPFIND
       startNewUpload() <-+        +----------------------------\
          |               |        |                             \
         MKCOL            + slotPropfindFinishedWithError()     slotPropfindFinished()
          |                                                       Is there stale files to remove?
      slotMkColFinished()                                         |                      |
          |                                                       no                    yes
          |                                                       |                      |
          |                                                       |                  DeleteJob
          |                                                       |                      |
    +-----+<------------------------------------------------------+<---  slotDeleteJobFinished()
    |
    +---->  startNextChunk()  ---finished?  --+
                  ^               |          |
                  +---------------+          |
                                             |
    +----------------------------------------+
    |
    +-> MOVE ------> moveJobFinished() ---> finalize()


 */

QByteArray PropagateUploadFileNG::destinationHeader() const
{
    const auto davUrl = Utility::trailingSlashPath(propagator()->account()->davUrl().toString());
    const auto remotePath = Utility::noLeadingSlashPath(propagator()->fullRemotePath(_fileToUpload._file));
    const auto destination = QString(davUrl + remotePath);
    return QUrl::toPercentEncoding(destination, "/");
}

void PropagateUploadFileNG::doStartUpload()
{
    propagator()->_activeJobList.append(this);

    const SyncJournalDb::UploadInfo progressInfo = propagator()->_journal->getUploadInfo(_item->_file);
    Q_ASSERT(_item->_modtime > 0);
    if (_item->_modtime <= 0) {
        qCWarning(lcPropagateUpload()) << "invalid modified time" << _item->_file << _item->_modtime;
    }
    if (progressInfo._valid && progressInfo.isChunked() && progressInfo._modtime == _item->_modtime && progressInfo._size == _item->_size) {
        _transferId = progressInfo._transferid;

        const auto job = new LsColJob(propagator()->account(), chunkUploadFolderUrl());
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
        (new DeleteJob(propagator()->account(), chunkUploadFolderUrl(), {}, this))->start();
        // startNewUpload will reset the _transferId and the UploadInfo in the db.
    }

    startNewUpload();
}

void PropagateUploadFileNG::slotPropfindIterate(const QString &name, const QMap<QString, QString> &properties)
{
    if (name == chunkUploadFolderUrl().path()) {
        return; // skip the info about the path itself
    }

    bool ok = false;
    QString chunkName = name.mid(name.lastIndexOf('/') + 1);
    auto chunkId = chunkName.toLongLong(&ok);

    if (ok) {
        ServerChunkInfo chunkinfo = { properties["getcontentlength"].toLongLong(), chunkName };
        _serverChunks[chunkId] = chunkinfo;
    }
}

void PropagateUploadFileNG::slotPropfindFinished()
{
    auto job = qobject_cast<LsColJob *>(sender());
    slotJobDestroyed(job); // remove it from the _jobs list
    propagator()->_activeJobList.removeOne(this);

    // Chunked upload v2: numbers range from 1 to 10000
    _currentChunk = 1;
    _sent = 0;
    while (_serverChunks.contains(_currentChunk)) {
        _sent += _serverChunks[_currentChunk].size;
        _serverChunks.remove(_currentChunk);
        ++_currentChunk;
    }

    if (_sent > _fileToUpload._size) {
        // Normally this can't happen because the size is xor'ed with the transfer id, and it is
        // therefore impossible that there is more data on the server than on the file.
        qCCritical(lcPropagateUploadNG) << "Inconsistency while resuming " << _item->_file
                                      << ": the size on the server (" << _sent << ") is bigger than the size of the file ("
                                      << _fileToUpload._size << ")";

        // Wipe the old chunking data.
        // Fire and forget. Any error will be ignored.
        (new DeleteJob(propagator()->account(), chunkUploadFolderUrl(), {}, this))->start();

        propagator()->_activeJobList.append(this);
        startNewUpload();
        return;
    }

    qCInfo(lcPropagateUploadNG) << "Resuming " << _item->_file << " from chunk " << _currentChunk << "; sent =" << _sent;

    if (!_serverChunks.isEmpty()) {
        qCInfo(lcPropagateUploadNG) << "To Delete" << _serverChunks.keys();
        propagator()->_activeJobList.append(this);
        _removeJobError = false;

        // Make sure that if there is a "hole" and then a few more chunks, on the server
        // we should remove the later chunks. Otherwise when we do dynamic chunk sizing, we may end up
        // with corruptions if there are too many chunks, or if we abort and there are still stale chunks.
        for (const auto &serverChunk : std::as_const(_serverChunks)) {
            auto job = new DeleteJob(propagator()->account(), Utility::concatUrlPath(chunkUploadFolderUrl(), serverChunk.originalName), {}, this);
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
            qCWarning(lcPropagateUploadNG) << "DeleteJob errored out" << job->errorString() << job->reply()->url();
            _removeJobError = true;
            // Let the other jobs finish
        }
    }

    if (_jobs.isEmpty()) {
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
    Q_ASSERT(_item->_modtime > 0);
    if (_item->_modtime <= 0) {
        qCWarning(lcPropagateUpload()) << "invalid modified time" << _item->_file << _item->_modtime;
    }
    _transferId = uint(Utility::rand() ^ uint(_item->_modtime) ^ (uint(_fileToUpload._size) << 16) ^ qHash(_fileToUpload._file));
    _sent = 0;
    _currentChunk = 1; // Chunked upload v2: numbers range from 1 to 10000

    propagator()->reportProgress(*_item, 0);

    SyncJournalDb::UploadInfo pi;
    pi._valid = true;
    pi._transferid = _transferId;
    Q_ASSERT(_item->_modtime > 0);
    if (_item->_modtime <= 0) {
        qCWarning(lcPropagateUpload()) << "invalid modified time" << _item->_file << _item->_modtime;
    }
    pi._modtime = _item->_modtime;
    pi._contentChecksum = _item->_checksumHeader;
    pi._size = _item->_size;
    propagator()->_journal->setUploadInfo(_item->_file, pi);
    propagator()->_journal->commit("Upload info");
    QMap<QByteArray, QByteArray> headers;

    // But we should send the temporary (or something) one.
    headers["OC-Total-Length"] = QByteArray::number(_fileToUpload._size);
    headers["Destination"] = destinationHeader();
    const auto job = new MkColJob(propagator()->account(), chunkUploadFolderUrl(), headers, this);

    connect(job, &MkColJob::finishedWithError,
        this, &PropagateUploadFileNG::slotMkColFinished);
    connect(job, &MkColJob::finishedWithoutError,
        this, &PropagateUploadFileNG::slotMkColFinished);
    connect(job, &QObject::destroyed, this, &PropagateUploadFileCommon::slotJobDestroyed);
    job->start();
}

void PropagateUploadFileNG::slotMkColFinished()
{
    propagator()->_activeJobList.removeOne(this);
    auto job = qobject_cast<MkColJob *>(sender());
    slotJobDestroyed(job); // remove it from the _jobs list
    QNetworkReply::NetworkError err = job->reply()->error();
    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toUInt();

    if (err != QNetworkReply::NoError || _item->_httpErrorCode != 201) {
        _item->_requestId = job->requestId();
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
            &propagator()->_anotherSyncNeeded);
        abortWithError(status, job->errorStringParsingBody());
        return;
    }
    startNextChunk();
}

void PropagateUploadFileNG::finishUpload()
{
    Q_ASSERT(_jobs.isEmpty()); // There should be no running job anymore
    _finished = true;

    // Finish with a MOVE
    // If we changed the file name, we must store the changed filename in the remote folder, not the original one.
    const auto destination = QDir::cleanPath(propagator()->account()->davUrl().path() + propagator()->fullRemotePath(_fileToUpload._file));
    auto headers = PropagateUploadFileCommon::headers();

    // "If-Match applies to the source, but we are interested in comparing the etag of the destination
    const auto ifMatch = headers.take(QByteArrayLiteral("If-Match"));
    if (!ifMatch.isEmpty()) {
        headers[QByteArrayLiteral("If")] = "<" + QUrl::toPercentEncoding(destination, "/") + "> ([" + ifMatch + "])";
    }

    if (!_transmissionChecksumHeader.isEmpty()) {
        qCInfo(lcPropagateUpload) << destination << _transmissionChecksumHeader;
        headers[checkSumHeaderC] = _transmissionChecksumHeader;
    }

    const auto fileSize = _fileToUpload._size;
    headers[QByteArrayLiteral("OC-Total-Length")] = QByteArray::number(fileSize);
    if (_item->_lockOwnerType == SyncFileItem::LockOwnerType::TokenLock &&
        _item->_locked == SyncFileItem::LockStatus::LockedItem &&
        _item->_instruction != CSYNC_INSTRUCTION_NEW &&
        _item->_instruction != CSYNC_INSTRUCTION_TYPE_CHANGE) {
        headers[QByteArrayLiteral("If")] = (QLatin1String("<") + propagator()->account()->davUrl().toString() + _fileToUpload._file + "> (<opaquelocktoken:" + _item->_lockToken.toUtf8() + ">)").toUtf8();
    }

    const auto job = new MoveJob(propagator()->account(), Utility::concatUrlPath(chunkUploadFolderUrl(), "/.file"), destination, headers, this);
    _jobs.append(job);
    connect(job, &MoveJob::finishedSignal, this, &PropagateUploadFileNG::slotMoveJobFinished);
    connect(job, &QObject::destroyed, this, &PropagateUploadFileCommon::slotJobDestroyed);
    propagator()->_activeJobList.append(this);
    adjustLastJobTimeout(job, fileSize);
    job->start();
    return;
}

void PropagateUploadFileNG::startNextChunk()
{
    if (propagator()->_abortRequested)
        return;

    const auto fileSize = _fileToUpload._size;
    ENFORCE(fileSize >= _sent, "Sent data exceeds file size")
    // prevent situation that chunk size is bigger then required one to send
    _currentChunkSize = qMin(propagator()->_chunkSize, fileSize - _sent);

    if (_currentChunkSize == 0) {
        finishUpload();
        return;
    }

    const auto fileName = _fileToUpload._path;
    auto device = std::make_unique<UploadDevice>(fileName, _sent, _currentChunkSize, &propagator()->_bandwidthManager);
    if (!device->open(QIODevice::ReadOnly)) {
        qCWarning(lcPropagateUploadNG) << "Could not prepare upload device: " << device->errorString();

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
    headers["OC-Chunk-Offset"] = QByteArray::number(_sent);
    headers["Destination"] = destinationHeader();
    headers[QByteArrayLiteral("OC-Total-Length")] = QByteArray::number(fileSize);

    _sent += _currentChunkSize;
    const auto url = chunkUrl(_currentChunk);

    // job takes ownership of device via a QScopedPointer. Job deletes itself when finishing
    const auto devicePtr = device.get(); // for connections later
    const auto job = new PUTFileJob(propagator()->account(), url, std::move(device), headers, _currentChunk, this);
    _jobs.append(job);
    connect(job, &PUTFileJob::finishedSignal, this, &PropagateUploadFileNG::slotPutFinished);
    connect(job, &PUTFileJob::uploadProgress,
        this, &PropagateUploadFileNG::slotUploadProgress);
    connect(job, &PUTFileJob::uploadProgress,
        devicePtr, &UploadDevice::slotJobUploadProgress);
    connect(job, &QObject::destroyed, this, &PropagateUploadFileCommon::slotJobDestroyed);
    job->start();
    propagator()->_activeJobList.append(this);
    _currentChunk++;
}

void PropagateUploadFileNG::slotPutFinished()
{
    auto *job = qobject_cast<PUTFileJob *>(sender());
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
        const auto exceptionParsed = getExceptionFromReply(job->reply());
        _item->_errorExceptionName = exceptionParsed.first;
        _item->_errorExceptionMessage = exceptionParsed.second;
        return;
    }

    ENFORCE(_sent <= _fileToUpload._size, "can't send more than size");

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
        propagator()->_chunkSize = ::qBound(propagator()->syncOptions().minChunkSize(), targetSize, propagator()->syncOptions().maxChunkSize());

        qCInfo(lcPropagateUploadNG) << "Chunked upload of" << _currentChunkSize << "bytes took" << uploadTime.count()
                                  << "ms, desired is" << targetDuration.count() << "ms, expected good chunk size is"
                                  << predictedGoodSize << "bytes and nudged next chunk size to "
                                  << propagator()->_chunkSize << "bytes";
    }

    _finished = _sent == _item->_size;

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

    // Check whether the file changed since discovery - this acts on the original file.
    Q_ASSERT(_item->_modtime > 0);
    if (_item->_modtime <= 0) {
        qCWarning(lcPropagateUpload()) << "invalid modified time" << _item->_file << _item->_modtime;
    }
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
        const auto exceptionParsed = getExceptionFromReply(job->reply());
        _item->_errorExceptionName = exceptionParsed.first;
        _item->_errorExceptionMessage = exceptionParsed.second;
        return;
    }

    if (_item->_httpErrorCode == 202) {
        QString path = QString::fromUtf8(job->reply()->rawHeader("OC-JobStatus-Location"));
        if (path.isEmpty()) {
            done(SyncFileItem::NormalError, tr("Poll URL missing"));
            return;
        }
        _finished = true;
        startPollJob(path);
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

    if (SyncJournalFileRecord oldRecord; propagator()->_journal->getFileRecord(_item->destination(), &oldRecord) && oldRecord.isValid()) {
        if (oldRecord._etag != _item->_etag) {
            _item->updateLockStateFromDbRecord(oldRecord);
        }
    }

    _item->_etag = getEtagFromReply(job->reply());

    if (_item->_etag.isEmpty()) {
        qCWarning(lcPropagateUploadNG) << "Server did not return an ETAG" << _item->_file;
        const auto errorMessage = _item->isDirectory() ? tr("Folder is not accessible on the server.", "server error")
                                                       : tr("File is not accessible on the server.", "server error");
        abortWithError(SyncFileItem::NormalError, errorMessage);
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
    propagator()->reportProgress(*_item, _sent + sent - total);
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
