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
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include "utility.h"
#include "filesystem.h"
#include "propagatorjobs.h"
#include "syncengine.h"
#include "propagateremotemove.h"
#include "propagateremotedelete.h"


#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>
#include <cmath>
#include <cstring>

namespace OCC {

QUrl PropagateUploadFileNG::chunkUrl(int chunk)
{
    QString path = QLatin1String("remote.php/dav/uploads/")
        + propagator()->account()->davUser()
        + QLatin1Char('/') + QString::number(_transferId);
    if (chunk >= 0) {
        // We need to do add leading 0 because the server orders the chunk alphabetically
        path += QLatin1Char('/') + QString::number(chunk).rightJustified(8, '0');
    }
    return Utility::concatUrlPath(propagator()->account()->url(), path);
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

void PropagateUploadFileNG::doStartUpload()
{
    propagator()->_activeJobList.append(this);

    const SyncJournalDb::UploadInfo progressInfo = propagator()->_journal->getUploadInfo(_item->_file);
    if (progressInfo._valid && Utility::qDateTimeToTime_t(progressInfo._modtime) == _item->_modtime ) {
        _transferId = progressInfo._transferid;
        auto url = chunkUrl();
        auto job = new LsColJob(propagator()->account(), url, this);
        _jobs.append(job);
        job->setProperties(QList<QByteArray>() << "resourcetype" << "getcontentlength");
        connect(job, SIGNAL(finishedWithoutError()), this, SLOT(slotPropfindFinished()));
        connect(job, SIGNAL(finishedWithError(QNetworkReply*)),
                this, SLOT(slotPropfindFinishedWithError()));
        connect(job, SIGNAL(destroyed(QObject*)), this, SLOT(slotJobDestroyed(QObject*)));
        connect(job, SIGNAL(directoryListingIterated(QString,QMap<QString,QString>)),
                this, SLOT(slotPropfindIterate(QString,QMap<QString,QString>)));
        job->start();
        return;
    } else if (progressInfo._valid) {
        // The upload info is stale. remove the stale chunks on the server
        _transferId = progressInfo._transferid;
        // Fire and forget. Any error will be ignored.
        (new DeleteJob(propagator()->account(), chunkUrl(), this))->start();
        // startNewUpload will reset the _transferId and the UploadInfo in the db.
    }

    startNewUpload();
}

void PropagateUploadFileNG::slotPropfindIterate(const QString &name, const QMap<QString,QString> &properties)
{
    if (name == chunkUrl().path()) {
        return; // skip the info about the path itself
    }
    bool ok = false;
    QString chunkName = name.mid(name.lastIndexOf('/')+1);
    auto chunkId = chunkName.toUInt(&ok);
    if (ok) {
        ServerChunkInfo chunkinfo = { properties["getcontentlength"].toULongLong(), chunkName };
        _serverChunks[chunkId] = chunkinfo;
    }
}

void PropagateUploadFileNG::slotPropfindFinished()
{
    auto job = qobject_cast<LsColJob *>(sender());
    slotJobDestroyed(job); // remove it from the _jobs list
    propagator()->_activeJobList.removeOne(this);

    _currentChunk = 0;
    _sent = 0;
    while (_serverChunks.contains(_currentChunk)) {
        _sent += _serverChunks[_currentChunk].size;
        _serverChunks.remove(_currentChunk);
        ++_currentChunk;
    }

    if (_sent > _item->_size) {
        // Normally this can't happen because the size is xor'ed with the transfer id, and it is
        // therefore impossible that there is more data on the server than on the file.
        qWarning() << "Inconsistency while resuming " << _item->_file
            << ": the size on the server (" << _sent << ") is bigger than the size of the file ("
            << _item->_size << ")";
        startNewUpload();
        return;
    }

    qDebug() << "Resuming "<< _item->_file << " from chunk " << _currentChunk << "; sent ="<< _sent;

    if (!_serverChunks.isEmpty()) {
        qDebug() << "To Delete" << _serverChunks.keys();
        propagator()->_activeJobList.append(this);
        _removeJobError = false;

        // Make sure that if there is a "hole" and then a few more chunks, on the server
        // we should remove the later chunks. Otherwise when we do dynamic chunk sizing, we may end up
        // with corruptions if there are too many chunks, or if we abort and there are still stale chunks.
        for (auto it = _serverChunks.begin(); it != _serverChunks.end(); ++it) {
            auto job = new DeleteJob(propagator()->account(), Utility::concatUrlPath(chunkUrl(), it->originalName), this);
            QObject::connect(job, SIGNAL(finishedSignal()), this, SLOT(slotDeleteJobFinished()));
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
        propagator()->_activeJobList.removeOne(this);
        QString errorString = errorMessage(job->reply()->errorString(), job->reply()->readAll());
        abortWithError(status, errorString);
        return;
    }
    startNewUpload();
}

void PropagateUploadFileNG::slotDeleteJobFinished()
{
    auto job = qobject_cast<DeleteJob *>(sender());
    Q_ASSERT(job);
    _jobs.remove(_jobs.indexOf(job));

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        const int httpStatus = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        SyncFileItem::Status status = classifyError(err, httpStatus);
        if (status == SyncFileItem::FatalError) {
            abortWithError(status, job->errorString());
            return;
        } else {
            qWarning() << "DeleteJob errored out" << job->errorString() << job->reply()->url();
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
    Q_ASSERT(propagator()->_activeJobList.count(this) == 1);
    _transferId = qrand() ^ _item->_modtime ^ (_item->_size << 16) ^ qHash(_item->_file);
    _sent = 0;
    _currentChunk = 0;

    emit progress(*_item, 0);

    SyncJournalDb::UploadInfo pi;
    pi._valid = true;
    pi._transferid = _transferId;
    pi._modtime =  Utility::qDateTimeFromTime_t(_item->_modtime);
    propagator()->_journal->setUploadInfo(_item->_file, pi);
    propagator()->_journal->commit("Upload info");
    QMap<QByteArray, QByteArray> headers;
    headers["OC-Total-Length"] = QByteArray::number(_item->_size);
    auto job = new MkColJob(propagator()->account(), chunkUrl(), headers, this);

    connect(job, SIGNAL(finished(QNetworkReply::NetworkError)),
            this, SLOT(slotMkColFinished(QNetworkReply::NetworkError)));
    connect(job, SIGNAL(destroyed(QObject*)), this, SLOT(slotJobDestroyed(QObject*)));
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
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
                                                    &propagator()->_anotherSyncNeeded);
        QString errorString = errorMessage(job->reply()->errorString(), job->reply()->readAll());
        if (job->reply()->hasRawHeader("OC-ErrorString")) {
            errorString = job->reply()->rawHeader("OC-ErrorString");
        }
        abortWithError(status, errorString);
        return;
    }
    startNextChunk();
}

void PropagateUploadFileNG::startNextChunk()
{
    if (propagator()->_abortRequested.fetchAndAddRelaxed(0))
        return;

    quint64 fileSize = _item->_size;
    Q_ASSERT(fileSize >= _sent);
    quint64 currentChunkSize = qMin(chunkSize(), fileSize - _sent);

    if (currentChunkSize == 0) {
        Q_ASSERT(_jobs.isEmpty()); // There should be no running job anymore
        _finished = true;
        // Finish with a MOVE
        QString destination = QDir::cleanPath(propagator()->account()->url().path() + QLatin1Char('/')
            + propagator()->account()->davPath() + propagator()->_remoteFolder + _item->_file);
        auto headers = PropagateUploadFileCommon::headers();

        // "If-Match applies to the source, but we are interested in comparing the etag of the destination
        auto ifMatch = headers.take("If-Match");
        if (!ifMatch.isEmpty()) {
            headers["If"] =  "<" + destination.toUtf8() + "> ([" + ifMatch + "])";
        }
        if (!_transmissionChecksumType.isEmpty()) {
            headers[checkSumHeaderC] = makeChecksumHeader(
                _transmissionChecksumType, _transmissionChecksum);
        }

        auto job = new MoveJob(propagator()->account(), Utility::concatUrlPath(chunkUrl(), "/.file"),
                               destination, headers, this);
        _jobs.append(job);
        connect(job, SIGNAL(finishedSignal()), this, SLOT(slotMoveJobFinished()));
        connect(job, SIGNAL(destroyed(QObject*)), this, SLOT(slotJobDestroyed(QObject*)));
        propagator()->_activeJobList.append(this);
        job->start();
        return;
    }

    auto device = new UploadDevice(&propagator()->_bandwidthManager);
    const QString fileName = propagator()->getFilePath(_item->_file);

    if (! device->prepareAndOpen(fileName, _sent, currentChunkSize)) {
        qDebug() << "ERR: Could not prepare upload device: " << device->errorString();

        // If the file is currently locked, we want to retry the sync
        // when it becomes available again.
        if (FileSystem::isFileLocked(fileName)) {
            emit propagator()->seenLockedFile(fileName);
        }
        // Soft error because this is likely caused by the user modifying his files while syncing
        abortWithError( SyncFileItem::SoftError, device->errorString() );
        return;
    }

    QMap<QByteArray, QByteArray> headers;
    headers["OC-Chunk-Offset"] = QByteArray::number(_sent);

    _sent += currentChunkSize;
    QUrl url = chunkUrl(_currentChunk);

    // job takes ownership of device via a QScopedPointer. Job deletes itself when finishing
    PUTFileJob* job = new PUTFileJob(propagator()->account(), url, device, headers, _currentChunk, this);
    _jobs.append(job);
    connect(job, SIGNAL(finishedSignal()), this, SLOT(slotPutFinished()));
    connect(job, SIGNAL(uploadProgress(qint64,qint64)),
            this, SLOT(slotUploadProgress(qint64,qint64)));
    connect(job, SIGNAL(uploadProgress(qint64,qint64)),
            device, SLOT(slotJobUploadProgress(qint64,qint64)));
    connect(job, SIGNAL(destroyed(QObject*)), this, SLOT(slotJobDestroyed(QObject*)));
    job->start();
    propagator()->_activeJobList.append(this);
    _currentChunk++;

    // FIXME! parallel chunk?

}

void PropagateUploadFileNG::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    Q_ASSERT(job);
    slotJobDestroyed(job); // remove it from the _jobs list

    qDebug() << job->reply()->request().url() << "FINISHED WITH STATUS"
             << job->reply()->error()
             << (job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : job->reply()->errorString())
             << job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute)
             << job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute);

    propagator()->_activeJobList.removeOne(this);

    if (_finished) {
        // We have sent the finished signal already. We don't need to handle any remaining jobs
        return;
    }

    QNetworkReply::NetworkError err = job->reply()->error();

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 2)
    if (err == QNetworkReply::OperationCanceledError && job->reply()->property("owncloud-should-soft-cancel").isValid()) {
        // Abort the job and try again later.
        // This works around a bug in QNAM wich might reuse a non-empty buffer for the next request.
        qDebug() << "Forcing job abort on HTTP connection reset with Qt < 5.4.2.";
        propagator()->_anotherSyncNeeded = true;
        abortWithError(SyncFileItem::SoftError, tr("Forcing job abort on HTTP connection reset with Qt < 5.4.2."));
        return;
    }
#endif

    if (err != QNetworkReply::NoError) {
        _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray replyContent = job->reply()->readAll();
        qDebug() << replyContent; // display the XML error in the debug
        QString errorString = errorMessage(job->errorString(), replyContent);

        if (job->reply()->hasRawHeader("OC-ErrorString")) {
            errorString = job->reply()->rawHeader("OC-ErrorString");
        }

        // Ensure errors that should eventually reset the chunked upload are tracked.
        checkResettingErrors();

        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
                                                    &propagator()->_anotherSyncNeeded);
        abortWithError(status, errorString);
        return;
    }

    Q_ASSERT(_sent <= _item->_size);
    bool finished = _sent == _item->_size;

    // Check if the file still exists
    const QString fullFilePath(propagator()->getFilePath(_item->_file));
    if( !FileSystem::fileExists(fullFilePath) ) {
        if (!finished) {
            abortWithError(SyncFileItem::SoftError, tr("The local file was removed during sync."));
            return;
        } else {
            propagator()->_anotherSyncNeeded = true;
        }
    }

    // Check whether the file changed since discovery.
    if (! FileSystem::verifyFileUnchanged(fullFilePath, _item->_size, _item->_modtime)) {
        propagator()->_anotherSyncNeeded = true;
        if( !finished ) {
            abortWithError(SyncFileItem::SoftError, tr("Local file changed during sync."));
            return;
        }
    }

    if (!finished) {
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

    if (err != QNetworkReply::NoError) {
        if (_item->_httpErrorCode == 412) {
            // Precondition Failed: Either an etag or a checksum mismatch.

            // Maybe the bad etag is in the database, we need to clear the
            // parent folder etag so we won't read from DB next sync.
            propagator()->_journal->avoidReadFromDbOnNextSync(_item->_file);
            propagator()->_anotherSyncNeeded = true;
        }

        // Ensure errors that should eventually reset the chunked upload are tracked.
        checkResettingErrors();

        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
                                                    &propagator()->_anotherSyncNeeded);
        QString errorString = errorMessage(job->errorString(), job->reply()->readAll());
        abortWithError(status, errorString);
        return;
    }
    if (_item->_httpErrorCode != 201 && _item->_httpErrorCode != 204) {
        abortWithError(SyncFileItem::NormalError, tr("Unexpected return code from server (%1)").arg(_item->_httpErrorCode));
        return;
    }

    QByteArray fid = job->reply()->rawHeader("OC-FileID");
    if(fid.isEmpty()) {
        qWarning() << "Server did not return a OC-FileID" << _item->_file;
        abortWithError(SyncFileItem::NormalError, tr("Missing File ID from server"));
        return;
    } else {
        // the old file id should only be empty for new files uploaded
        if( !_item->_fileId.isEmpty() && _item->_fileId != fid ) {
            qDebug() << "WARN: File ID changed!" << _item->_fileId << fid;
        }
        _item->_fileId = fid;
    }

    _item->_etag = getEtagFromReply(job->reply());;
    if (_item->_etag.isEmpty()) {
        qWarning() << "Server did not return an ETAG" << _item->_file;
        abortWithError(SyncFileItem::NormalError, tr("Missing ETag from server"));
        return;
    }
    _item->_responseTimeStamp = job->responseTimestamp();

    // performance logging
    quint64 duration = _stopWatch.stop();
    qDebug() << "*==* duration UPLOAD" << _item->_size
             << _stopWatch.durationOfLap(QLatin1String("ContentChecksum"))
             << _stopWatch.durationOfLap(QLatin1String("TransmissionChecksum"))
             << duration;
    // The job might stay alive for the whole sync, release this tiny bit of memory.
    _stopWatch.reset();
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
    emit progress(*_item, _sent + sent - total);
}

}
