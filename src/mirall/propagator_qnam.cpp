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

#include "propagator_qnam.h"
#include "networkjobs.h"
#include "account.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include "utility.h"
#include "filesystem.h"
#include "propagatorjobs.h"
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <cmath>

namespace Mirall {

static uint chunkSize() {
    static uint chunkSize;
    if (!chunkSize) {
        chunkSize = qgetenv("OWNCLOUD_CHUNK_SIZE").toUInt();
        if (chunkSize == 0) {
            chunkSize = 10*1024*1024; // default to 10 MiB
        }
    }
    return chunkSize;
}

/**
 * Fiven an error from the network, map to a SyncFileItem::Status error
 */
static SyncFileItem::Status classifyError(QNetworkReply::NetworkError nerror, int httpCode) {
    Q_ASSERT (nerror != QNetworkReply::NoError); // we should only be called when there is an error

    if (nerror > QNetworkReply::NoError &&  nerror <= QNetworkReply::UnknownProxyError) {
        // network error or proxy error -> fatal
        return SyncFileItem::FatalError;
    }

    if (httpCode == 412) {
        // "Precondition Failed"
        // Happens when the e-tag has changed
        return SyncFileItem::SoftError;
    }

    return SyncFileItem::NormalError;
}

void PUTFileJob::start() {
    QNetworkRequest req;
    for(QMap<QByteArray, QByteArray>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }

    setReply(davRequest("PUT", path(), req, _device));
    _device->setParent(reply());
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qWarning() << Q_FUNC_INFO << " Network error: " << reply()->errorString();
    }

    connect(reply(), SIGNAL(uploadProgress(qint64,qint64)), this, SIGNAL(uploadProgress(qint64,qint64)));
    connect(reply(), SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(resetTimeout()));

    AbstractNetworkJob::start();
}

void PUTFileJob::slotTimeout() {
    _errorString =  tr("Connection Timeout");
    reply()->abort();
}

void PropagateUploadFileQNAM::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    _file = new QFile(_propagator->_localDir + _item._file, this);
    if (!_file->open(QIODevice::ReadOnly)) {
        done(SyncFileItem::NormalError, _file->errorString());
        delete _file;
        return;
    }

    quint64 fileSize = _file->size();
    _chunkCount = std::ceil(fileSize/double(chunkSize()));
    _startChunk = 0;
    _transferId = qrand() ^ _item._modtime ^ (_item._size << 16);

    const SyncJournalDb::UploadInfo progressInfo = _propagator->_journal->getUploadInfo(_item._file);

    if (progressInfo._valid && Utility::qDateTimeToTime_t(progressInfo._modtime) == _item._modtime ) {
        _startChunk = progressInfo._chunk;
        _transferId = progressInfo._transferid;
        qDebug() << Q_FUNC_INFO << _item._file << ": Resuming from chunk " << _startChunk;
    }

    _currentChunk = 0;
    _duration.start();

    _propagator->_activeJobs++;
    emit progress(_item, 0);
    emitReady();
    this->startNextChunk();
}

struct ChunkDevice : QIODevice {
public:
    QIODevice *_file;
    qint64 _read;
    qint64 _size;
    qint64 _start;

    ChunkDevice(QIODevice *file,  qint64 start, qint64 size)
            : QIODevice(file), _file(file), _read(0), _size(size), _start(start) {
        _file->seek(start);
    }

    virtual qint64 writeData(const char* , qint64 ) {
        Q_ASSERT(!"write to read only device");
        return 0;
    }

    virtual qint64 readData(char* data, qint64 maxlen) {
        maxlen = qMin(maxlen, chunkSize() - _read);
        if (maxlen == 0)
            return 0;
        qint64 ret = _file->read(data, maxlen);
        if (ret < 0)
            return -1;
        _read += ret;
        return ret;
    }

    virtual bool atEnd() const {
        return  _read >= chunkSize() || _file->atEnd();
    }

    virtual qint64 size() const{
        return _size;
    }

    qint64 bytesAvailable() const
    {
        return _size - _read + QIODevice::bytesAvailable();
    }

    // random access, we can seek
    virtual bool isSequential() const{
        return false;
    }

    virtual bool seek ( qint64 pos ) {
        _read = pos;
        return _file->seek(pos + _start);
    }
};

void PropagateUploadFileQNAM::startNextChunk()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;


    /*
     *        // If the source file has changed during upload, it is detected and the
     *        // variable _previousFileSize is set accordingly. The propagator waits a
     *        // couple of seconds and retries.
     *        if(_previousFileSize > 0) {
     *            qDebug() << "File size changed underway: " << trans->stat_size - _previousFileSize;
     *            // Report the change of the overall transmission size to the propagator
     *            _propagator->overallTransmissionSizeChanged(qint64(trans->stat_size - _previousFileSize));
     *            // update the item's values to the current from trans. hbf_splitlist does a stat
     *            _item._size = trans->stat_size;
     *            _item._modtime = trans->modtime;
     *
     */
    quint64 fileSize = _item._size;
    QMap<QByteArray, QByteArray> headers;
    headers["OC-Total-Length"] = QByteArray::number(fileSize);
    headers["Content-Type"] = "application/octet-stream";
    headers["X-OC-Mtime"] = QByteArray::number(qint64(_item._modtime));
    if (!_item._etag.isEmpty() && _item._etag != "empty_etag") {
        // We add quotes because the owncloud server always add quotes around the etag, and
        //  csync_owncloud.c's owncloud_file_id always strip the quotes.
        headers["If-Match"] = '"' + _item._etag + '"';
    }

    QString path = _item._file;
    QIODevice *device;
    if (_chunkCount > 1) {
        int sendingChunk = (_currentChunk + _startChunk) % _chunkCount;
        // XOR with chunk size to make sure everything goes well if chunk size change between runs
        uint transid = _transferId ^ chunkSize();
        path +=  QString("-chunking-%1-%2-%3").arg(transid).arg(_chunkCount).arg(sendingChunk);
        headers["OC-Chunked"] = "1";
        int currentChunkSize = chunkSize();
        if (sendingChunk == _chunkCount - 1) { // last chunk
            currentChunkSize = (fileSize % chunkSize());
            if( currentChunkSize == 0 ) { // if the last chunk pretents to be 0, its actually the full chunk size.
                currentChunkSize = chunkSize();
            }
        }
        device = new ChunkDevice(_file, chunkSize() * sendingChunk, currentChunkSize);
    } else {
        device = _file;
    }
    if (!device->isOpen())
        device->open(QIODevice::ReadOnly);

    _job = new PUTFileJob(AccountManager::instance()->account(), _propagator->_remoteFolder + path, device, headers);
    _job->setTimeout(_propagator->httpTimeout() * 1000);
    connect(_job, SIGNAL(finishedSignal()), this, SLOT(slotPutFinished()));
    connect(_job, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(slotUploadProgress(qint64,qint64)));
    _job->start();
}

void PropagateUploadFileQNAM::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    Q_ASSERT(job);

    qDebug() << Q_FUNC_INFO << job->reply()->request().url() << "FINISHED WITH STATUS"
             << job->reply()->error()
             << (job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : job->reply()->errorString())
             << job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute)
             << job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute);

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError) {
        _item._httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        _propagator->_activeJobs--;
        if(checkForProblemsWithShared(_item._httpErrorCode,
            tr("The file was edited locally but is part of a read only share. "
               "It is restored and your edit is in the conflict file."))) {
            return;
        }
        QString errorString = job->errorString();

        QByteArray replyContent = job->reply()->readAll();
        qDebug() << replyContent; // display the XML error in the debug
        QRegExp rx("<s:message>(.*)</s:message>"); // Issue #1366: display server exception
        if (rx.indexIn(QString::fromUtf8(replyContent)) != -1) {
            errorString += QLatin1String(" (") + rx.cap(1) + QLatin1Char(')');
        }

        done(classifyError(err, _item._httpErrorCode), errorString);
        return;
    }

    bool finished = job->reply()->hasRawHeader("ETag");

    if (!finished) {

        if (Utility::qDateTimeToTime_t(QFileInfo(_propagator->_localDir + _item._file).lastModified())
                != _item._modtime) {
            /* Uh oh:  The local file has changed during upload */
            _propagator->_activeJobs--;
            done(SyncFileItem::SoftError, tr("Local file changed during sync."));
            // FIXME:  the legacy code was retrying for a few seconds.
            //         and also checking that after the last chunk, and removed the file in case of INSTRUCTION_NEW
            return;
        }

        // Proceed to next chunk.
        _currentChunk++;
        if (_currentChunk >= _chunkCount) {
            _propagator->_activeJobs--;
            done(SyncFileItem::NormalError, tr("The server did not aknoledge the last chunk. (No e-tag were present)"));
            return;
        }

        SyncJournalDb::UploadInfo pi;
        pi._valid = true;
        pi._chunk = (_currentChunk + _startChunk) % _chunkCount; // next chunk to start with
        pi._transferid = _transferId;
        pi._modtime =  Utility::qDateTimeFromTime_t(_item._modtime);
        _propagator->_journal->setUploadInfo(_item._file, pi);
        _propagator->_journal->commit("Upload info");
        startNextChunk();
        return;
    }

    // the following code only happens after all chunks were uploaded.
    //
    // the file id should only be empty for new files up- or downloaded
    QByteArray fid = job->reply()->rawHeader("OC-FileID");
    if( !fid.isEmpty() ) {
        if( !_item._fileId.isEmpty() && _item._fileId != fid ) {
            qDebug() << "WARN: File ID changed!" << _item._fileId << fid;
        }
        _item._fileId = fid;
    }

    _item._etag = parseEtag(job->reply()->rawHeader("ETag"));
    _item._responseTimeStamp = job->responseTimestamp();

    if (job->reply()->rawHeader("X-OC-MTime") != "accepted") {
        // X-OC-MTime is supported since owncloud 5.0.   But not when chunking.
        // Normaly Owncloud 6 always put X-OC-MTime
        qDebug() << "Server do not support X-OC-MTime";
        PropagatorJob *newJob = new UpdateMTimeAndETagJob(_propagator, _item);
        QObject::connect(newJob, SIGNAL(completed(SyncFileItem)), this, SLOT(finalize(SyncFileItem)));
        QMetaObject::invokeMethod(newJob, "start");
        return;
    }
    finalize(_item);
}

void PropagateUploadFileQNAM::finalize(const SyncFileItem &copy)
{
    // Normally, copy == _item,   but when it comes from the UpdateMTimeAndETagJob, we need to do
    // some updates
    _item._etag = copy._etag;
    _item._fileId = copy._fileId;

    _propagator->_activeJobs--;

    _item._requestDuration = _duration.elapsed();

    _propagator->_journal->setFileRecord(SyncJournalFileRecord(_item, _propagator->_localDir + _item._file));
    // Remove from the progress database:
    _propagator->_journal->setUploadInfo(_item._file, SyncJournalDb::UploadInfo());
    _propagator->_journal->commit("upload file start");

    done(SyncFileItem::Success);
}

void PropagateUploadFileQNAM::slotUploadProgress(qint64 sent, qint64)
{
    int progressChunk = _currentChunk + _startChunk;
    if (progressChunk >= _chunkCount)
        progressChunk = _currentChunk;
    emit progress(_item, sent + _currentChunk * chunkSize());
}


void PropagateUploadFileQNAM::abort()
{
    if (_job &&  _job->reply()) {
        qDebug() << Q_FUNC_INFO << this->_item._file;
        _job->reply()->abort();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void GETFileJob::start() {
    QNetworkRequest req;
    for(QMap<QByteArray, QByteArray>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }

    setReply(davRequest("GET", path(), req));
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qWarning() << Q_FUNC_INFO << " Network error: " << reply()->errorString();
    }

    connect(reply(), SIGNAL(metaDataChanged()), this, SLOT(slotMetaDataChanged()));
    connect(reply(), SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
    connect(reply(), SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(downloadProgress(qint64,qint64)));

    AbstractNetworkJob::start();
}

void GETFileJob::slotMetaDataChanged()
{
    if (reply()->error() != QNetworkReply::NoError
            || reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() / 100 != 2) {
        // We will handle the error when the job is finished.
        return;
    }

    QByteArray etag = parseEtag(reply()->rawHeader("Etag"));

    if (etag.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "No E-Tag reply by server, considering it invalid";
        _errorString = tr("No E-Tag received from server, check Proxy/Gateway");
        _errorStatus = SyncFileItem::NormalError;
        reply()->abort();
        return;
    } else if (!_expectedEtagForResume.isEmpty() && _expectedEtagForResume != etag) {
        qDebug() << Q_FUNC_INFO <<  "We received a different E-Tag for resuming!"
                << _expectedEtagForResume << "vs" << etag;
        _errorString = tr("We received a different E-Tag for resuming. Retrying next time.");
        _errorStatus = SyncFileItem::NormalError;
        reply()->abort();
        return;
    }
}

void GETFileJob::slotReadyRead()
{
    int bufferSize = qMax(1024*8ll , reply()->bytesAvailable());
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    while(reply()->bytesAvailable() > 0) {
        qint64 r = reply()->read(buffer.data(), bufferSize);
        if (r < 0) {
            _errorString = reply()->errorString();
            _errorStatus = SyncFileItem::NormalError;
            qDebug() << "Error while reading from device: " << _errorString;
            reply()->abort();
            return;
        }

        qint64 w = _device->write(buffer.constData(), r);
        if (w != r) {
            _errorString = _device->errorString();
            _errorStatus = SyncFileItem::NormalError;
            qDebug() << "Error while writing to file" << w << r <<  _errorString;
            reply()->abort();
            return;
        }
    }
    resetTimeout();
}



void PropagateDownloadFileQNAM::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qDebug() << Q_FUNC_INFO << _item._file << _propagator->_activeJobs;

    emit progress(_item, 0);

    QString tmpFileName;
    QByteArray expectedEtagForResume;
    const SyncJournalDb::DownloadInfo progressInfo = _propagator->_journal->getDownloadInfo(_item._file);
    if (progressInfo._valid) {
        // if the etag has changed meanwhile, remove the already downloaded part.
        if (progressInfo._etag != _item._etag) {
            QFile::remove(_propagator->_localDir + progressInfo._tmpfile);
            _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
        } else {
            tmpFileName = progressInfo._tmpfile;
            expectedEtagForResume = progressInfo._etag;
        }

    }

    if (tmpFileName.isEmpty()) {
        tmpFileName = _item._file;
        //add a dot at the begining of the filename to hide the file.
        int slashPos = tmpFileName.lastIndexOf('/');
        tmpFileName.insert(slashPos+1, '.');
        //add the suffix
        tmpFileName += ".~" + QString::number(uint(qrand()), 16);
    }

    _tmpFile.setFileName(_propagator->_localDir + tmpFileName);
    if (!_tmpFile.open(QIODevice::Append | QIODevice::Unbuffered)) {
        done(SyncFileItem::NormalError, _tmpFile.errorString());
        return;
    }

    FileSystem::setFileHidden(_tmpFile.fileName(), true);

    {
        SyncJournalDb::DownloadInfo pi;
        pi._etag = _item._etag;
        pi._tmpfile = tmpFileName;
        pi._valid = true;
        _propagator->_journal->setDownloadInfo(_item._file, pi);
        _propagator->_journal->commit("download file start");
    }


    QMap<QByteArray, QByteArray> headers;
    /* Allow compressed content by setting the header */
    //headers["Accept-Encoding"] = "gzip";

    if (_tmpFile.size() > 0) {
        quint64 done = _tmpFile.size();
        if (done == _item._size) {
            qDebug() << "File is already complete, no need to download";
            downloadFinished();
            return;
        }
        headers["Range"] = "bytes=" + QByteArray::number(done) +'-';
        headers["Accept-Ranges"] = "bytes";
        qDebug() << "Retry with range " << headers["Range"];
        _startSize = done;
    }

    _job = new GETFileJob(AccountManager::instance()->account(),
                          _propagator->_remoteFolder + _item._file,
                          &_tmpFile, headers, expectedEtagForResume);
    _job->setTimeout(_propagator->httpTimeout() * 1000);
    connect(_job, SIGNAL(finishedSignal()), this, SLOT(slotGetFinished()));
    connect(_job, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(slotDownloadProgress(qint64,qint64)));
    _propagator->_activeJobs ++;
    _job->start();
    emitReady();
}

void PropagateDownloadFileQNAM::slotGetFinished()
{
    _propagator->_activeJobs--;

    GETFileJob *job = qobject_cast<GETFileJob *>(sender());
    Q_ASSERT(job);

    qDebug() << Q_FUNC_INFO << job->reply()->request().url() << "FINISHED WITH STATUS"
             << job->reply()->error()
             << (job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : job->reply()->errorString());

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError) {
        if (_tmpFile.size() == 0) {
            // don't keep the temporary file if it is empty.
            _tmpFile.close();
            _tmpFile.remove();
            _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
        }
        _item._httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        _propagator->_activeJobs--;
        SyncFileItem::Status status = job->errorStatus();
        if (status == SyncFileItem::NoStatus) {
            status = classifyError(err, _item._httpErrorCode);
        }
        done(status, job->errorString());
        return;
    }

    _item._etag = parseEtag(job->reply()->rawHeader("Etag"));
    _item._requestDuration = job->duration();
    _item._responseTimeStamp = job->responseTimestamp();

    _tmpFile.close();
    _tmpFile.flush();
    downloadFinished();
}

QString makeConflictFileName(const QString &fn, const QDateTime &dt)
{
    QString conflictFileName(fn);
    // Add _conflict-XXXX  before the extention.
    int dotLocation = conflictFileName.lastIndexOf('.');
    // If no extention, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
    if (dotLocation <= conflictFileName.lastIndexOf('/') + 1) {
        dotLocation = conflictFileName.size();
    }
    QString timeString = dt.toString("yyyyMMdd-hhmmss");

    // Additional marker
    QByteArray conflictFileUserName = qgetenv("CSYNC_CONFLICT_FILE_USERNAME");
    if (conflictFileUserName.isEmpty())
        conflictFileName.insert(dotLocation, "_conflict-" + timeString);
    else
        conflictFileName.insert(dotLocation, "_conflict_" + QString::fromUtf8(conflictFileUserName)  + "-" + timeString);

    return conflictFileName;
}

void PropagateDownloadFileQNAM::downloadFinished()
{

    QString fn = _propagator->_localDir + _item._file;


    bool isConflict = _item._instruction == CSYNC_INSTRUCTION_CONFLICT
            && !FileSystem::fileEquals(fn, _tmpFile.fileName()); // compare the files to see if there was an actual conflict.
    //In case of conflict, make a backup of the old file
    if (isConflict) {
        QFile f(fn);
        QString conflictFileName = makeConflictFileName(fn, Utility::qDateTimeFromTime_t(_item._modtime));
        if (!f.rename(conflictFileName)) {
            //If the rename fails, don't replace it.
            done(SyncFileItem::NormalError, f.errorString());
            return;
        }
    }

    QFileInfo existingFile(fn);
    if(existingFile.exists() && existingFile.permissions() != _tmpFile.permissions()) {
        _tmpFile.setPermissions(existingFile.permissions());
    }

    FileSystem::setFileHidden(_tmpFile.fileName(), false);

    QString error;
    if (!FileSystem::renameReplace(_tmpFile.fileName(), fn, &error)) {
        done(SyncFileItem::NormalError, error);
        return;
    }

    FileSystem::setModTime(fn, _item._modtime);

    _propagator->_journal->setFileRecord(SyncJournalFileRecord(_item, fn));
    _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
    _propagator->_journal->commit("download file start2");
    done(isConflict ? SyncFileItem::Conflict : SyncFileItem::Success);
}

void PropagateDownloadFileQNAM::slotDownloadProgress(qint64 received, qint64)
{
    emit progress(_item, received + _startSize);
}


void PropagateDownloadFileQNAM::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
}

void GETFileJob::slotTimeout()
{
    _errorString =  tr("Connection Timeout");
    _errorStatus = SyncFileItem::FatalError;
    reply()->abort();
}



}
