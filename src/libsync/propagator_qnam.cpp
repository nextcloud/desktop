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
#include <json.h>
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>
#include <cmath>

namespace Mirall {

/**
 * The mtime of a file must be at least this many milliseconds in
 * the past for an upload to be started. Otherwise the propagator will
 * assume it's still being changed and skip it.
 *
 * This value must be smaller than the msBetweenRequestAndSync in
 * the folder manager.
 *
 * Two seconds has shown to be a good value in tests.
 */
static int minFileAgeForUpload = 2000;

static qint64 chunkSize() {
    static uint chunkSize;
    if (!chunkSize) {
        chunkSize = qgetenv("OWNCLOUD_CHUNK_SIZE").toUInt();
        if (chunkSize == 0) {
            chunkSize = 20*1024*1024; // default to 20 MiB
        }
    }
    return chunkSize;
}

static QByteArray get_etag_from_reply(QNetworkReply *reply)
{
    QByteArray ret = parseEtag(reply->rawHeader("OC-ETag"));
    if (ret.isEmpty()) {
        ret = parseEtag(reply->rawHeader("ETag"));
    }
    return ret;
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

    setReply(davRequest("PUT", path(), req, _device.data()));
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qWarning() << Q_FUNC_INFO << " Network error: " << reply()->errorString();
    }

    connect(reply(), SIGNAL(uploadProgress(qint64,qint64)), this, SIGNAL(uploadProgress(qint64,qint64)));
    connect(this, SIGNAL(networkActivity()), account(), SIGNAL(propagatorNetworkActivity()));

    AbstractNetworkJob::start();
}

void PUTFileJob::slotTimeout() {
    _errorString =  tr("Connection Timeout");
    reply()->abort();
}

void PollJob::start()
{
    setTimeout(120 * 1000);
    QUrl accountUrl = account()->url();
    QUrl finalUrl = QUrl::fromUserInput(accountUrl.scheme() + QLatin1String("://") +  accountUrl.authority()
        + (path().startsWith('/') ? QLatin1String("") : QLatin1Literal("/")) + path());
    setReply(getRequest(finalUrl));
    setupConnections(reply());
    connect(reply(), SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(resetTimeout()));
    AbstractNetworkJob::start();
}

bool PollJob::finished()
{
    QNetworkReply::NetworkError err = reply()->error();
    if (err != QNetworkReply::NoError) {
        _item._httpErrorCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        _item._status = classifyError(err, _item._httpErrorCode);
        _item._errorString = reply()->errorString();
        if (_item._status == SyncFileItem::FatalError || _item._httpErrorCode >= 400) {
            if (_item._status != SyncFileItem::FatalError
                    && _item._httpErrorCode != 503) {
                SyncJournalDb::PollInfo info;
                info._file = _item._file;
                // no info._url removes it from the database
                _journal->setPollInfo(info);
                _journal->commit("remove poll info");

            }
            emit finishedSignal();
            return true;
        }
        start();
        return false;
    }

    bool ok = false;
    QByteArray jsonData = reply()->readAll().trimmed();
    qDebug() << Q_FUNC_INFO << ">" << jsonData << "<" << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QVariantMap status = QtJson::parse(QString::fromUtf8(jsonData), ok).toMap();
    if (!ok || status.isEmpty()) {
        _item._errorString = tr("Invalid json reply from the poll URL");
        _item._status = SyncFileItem::NormalError;
        emit finishedSignal();
        return true;
    }

    if (status["unfinished"].isValid()) {
        start();
        return false;
    }

    _item._errorString = status["error"].toString();
    _item._status = _item._errorString.isEmpty() ? SyncFileItem::Success : SyncFileItem::NormalError;
    _item._fileId = status["fileid"].toByteArray();
    _item._etag = status["etag"].toByteArray();
    _item._responseTimeStamp = responseTimestamp();

    SyncJournalDb::PollInfo info;
    info._file = _item._file;
    // no info._url removes it from the database
    _journal->setPollInfo(info);
    _journal->commit("remove poll info");

    emit finishedSignal();
    return true;
}


void PropagateUploadFileQNAM::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    _file = new QFile(_propagator->getFilePath(_item._file), this);
    if (!_file->open(QIODevice::ReadOnly)) {
        done(SyncFileItem::NormalError, _file->errorString());
        delete _file;
        return;
    }

    // Update the mtime and size, it might have changed since discovery.
    _item._modtime = FileSystem::getModTime(_file->fileName());
    quint64 fileSize = _file->size();
    _item._size = fileSize;

    // But skip the file if the mtime is too close to 'now'!
    // That usually indicates a file that is still being changed
    // or not yet fully copied to the destination.
    QDateTime modtime = Utility::qDateTimeFromTime_t(_item._modtime);
    if (modtime.msecsTo(QDateTime::currentDateTime()) < minFileAgeForUpload) {
        _propagator->_anotherSyncNeeded = true;
        done(SyncFileItem::SoftError, tr("Local file changed during sync."));
        delete _file;
        return;
    }

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

    emit progress(_item, 0);
    this->startNextChunk();
}

UploadDevice::UploadDevice(QIODevice *file,  qint64 start, qint64 size, BandwidthManager *bwm)
    : QIODevice(file), _file(file), _read(0), _size(size), _start(start),
      _bandwidthManager(bwm),
      _bandwidthQuota(0),
      _readWithProgress(0),
      _bandwidthLimited(false), _choked(false)
{
    qDebug() << Q_FUNC_INFO << start << size << chunkSize();
    _bandwidthManager->registerUploadDevice(this);
    _file = QPointer<QIODevice>(file);
}


UploadDevice::~UploadDevice() {
    _bandwidthManager->unregisterUploadDevice(this);
}

qint64 UploadDevice::writeData(const char* , qint64 ) {
    Q_ASSERT(!"write to read only device");
    return 0;
}

qint64 UploadDevice::readData(char* data, qint64 maxlen) {
    if (_file.isNull()) {
        qDebug() << Q_FUNC_INFO << "Upload file object deleted during upload";
        close();
        return -1;
    }
    _file.data()->seek(_start + _read);
    //qDebug() << Q_FUNC_INFO << maxlen << _read << _size << _bandwidthQuota;
    if (_size - _read <= 0) {
        // at end
        qDebug() << Q_FUNC_INFO << _read << _size << _bandwidthQuota << "at end";
        _bandwidthManager->unregisterUploadDevice(this);
        return -1;
    }
    maxlen = qMin(maxlen, _size - _read);
    if (maxlen == 0) {
        return 0;
    }
    if (isChoked()) {
        qDebug() << Q_FUNC_INFO << this << "Upload Choked";
        return 0;
    }
    if (isBandwidthLimited()) {
        qDebug() << Q_FUNC_INFO << "BW LIMITED" << maxlen << _bandwidthQuota
                 << qMin(maxlen, _bandwidthQuota);
        maxlen = qMin(maxlen, _bandwidthQuota);
        if (maxlen <= 0) {  // no quota
            qDebug() << Q_FUNC_INFO << "no quota";
            return 0;
        }
        _bandwidthQuota -= maxlen;
    }
    qDebug() << Q_FUNC_INFO << "reading limited=" << isBandwidthLimited()
             << "maxlen=" << maxlen << "quota=" << _bandwidthQuota;
    qint64 ret = _file.data()->read(data, maxlen);
    //qDebug() << Q_FUNC_INFO << "returning " << ret;

    if (ret < 0)
        return -1;
    _read += ret;
    //qDebug() << Q_FUNC_INFO << "returning2 " << ret << _read;

    return ret;
}

void UploadDevice::slotJobUploadProgress(qint64 sent, qint64 t)
{    
    //qDebug() << Q_FUNC_INFO << sent << _read << t << _size << _bandwidthQuota;
    if (sent == 0 || t == 0) {
        return;
    }
    _readWithProgress = sent;
}

bool UploadDevice::atEnd() const {
    if (_file.isNull()) {
        qDebug() << Q_FUNC_INFO << "Upload file object deleted during upload";
        return true;
    }
//    qDebug() << this << Q_FUNC_INFO << _read << chunkSize()
//             << (_read >= chunkSize() || _file.data()->atEnd())
//             << (_read >= _size);
    return  _file.data()->atEnd() || (_read >= _size);
}

qint64 UploadDevice::size() const{
//    qDebug() << this << Q_FUNC_INFO << _size;
    return _size;
}

qint64 UploadDevice::bytesAvailable() const
{
//    qDebug() << this << Q_FUNC_INFO << _size << _read << QIODevice::bytesAvailable()
//             <<   _size - _read + QIODevice::bytesAvailable();
    return _size - _read + QIODevice::bytesAvailable();
}

// random access, we can seek
bool UploadDevice::isSequential() const{
    return false;
}

bool UploadDevice::seek ( qint64 pos ) {
    if (_file.isNull()) {
        qDebug() << Q_FUNC_INFO << "Upload file object deleted during upload";
        close();
        return false;
    }
    qDebug() << this << Q_FUNC_INFO << pos << _read;
    _read = pos;
    return _file.data()->seek(pos + _start);
}

void UploadDevice::giveBandwidthQuota(qint64 bwq) {
//    qDebug() << Q_FUNC_INFO << bwq;
    if (!atEnd()) {
        _bandwidthQuota = bwq;
//        qDebug() << Q_FUNC_INFO << bwq << "emitting readyRead()" <<  _read << _readWithProgress;
        QMetaObject::invokeMethod(this, "readyRead", Qt::QueuedConnection); // tell QNAM that we have quota
    }
}

void UploadDevice::setBandwidthLimited(bool b) {
    _bandwidthLimited = b;
    QMetaObject::invokeMethod(this, "readyRead", Qt::QueuedConnection);
}

void UploadDevice::setChoked(bool b) {
    _choked = b;
    if (!_choked) {
        QMetaObject::invokeMethod(this, "readyRead", Qt::QueuedConnection);
    }
}

void PropagateUploadFileQNAM::startNextChunk()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    if (! _jobs.isEmpty() &&  _currentChunk + _startChunk >= _chunkCount - 1) {
        // Don't do parallel upload of chunk if this might be the last chunk because the server cannot handle that
        // https://github.com/owncloud/core/issues/11106
        // We return now and when the _jobs will be finished we will proceed the last chunk
        return;
    }
    quint64 fileSize = _item._size;
    QMap<QByteArray, QByteArray> headers;
    headers["OC-Total-Length"] = QByteArray::number(fileSize);
    headers["OC-Async"] = "1";
    headers["Content-Type"] = "application/octet-stream";
    headers["X-OC-Mtime"] = QByteArray::number(qint64(_item._modtime));
    if (!_item._etag.isEmpty() && _item._etag != "empty_etag" &&
            _item._instruction != CSYNC_INSTRUCTION_NEW  // On new files never send a If-Match
            ) {
        // We add quotes because the owncloud server always add quotes around the etag, and
        //  csync_owncloud.c's owncloud_file_id always strip the quotes.
        headers["If-Match"] = '"' + _item._etag + '"';
    }

    QString path = _item._file;
    UploadDevice *device = 0;
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
        device = new UploadDevice(_file, chunkSize() * quint64(sendingChunk), currentChunkSize, &_propagator->_bandwidthManager);
    } else {
        device = new UploadDevice(_file, 0, fileSize, &_propagator->_bandwidthManager);
    }

    bool isOpen = true;
    if (!device->isOpen()) {
        isOpen = device->open(QIODevice::ReadOnly);
    }

    if( isOpen ) {
        PUTFileJob* job = new PUTFileJob(AccountManager::instance()->account(), _propagator->_remoteFolder + path, device, headers, _currentChunk);
        _jobs.append(job);
        job->setTimeout(_propagator->httpTimeout() * 1000);
        connect(job, SIGNAL(finishedSignal()), this, SLOT(slotPutFinished()));
        connect(job, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(slotUploadProgress(qint64,qint64)));
        connect(job, SIGNAL(uploadProgress(qint64,qint64)), device, SLOT(slotJobUploadProgress(qint64,qint64)));
        connect(job, SIGNAL(destroyed(QObject*)), this, SLOT(slotJobDestroyed(QObject*)));
        job->start();
        _propagator->_activeJobs++;
        _currentChunk++;

        QByteArray env = qgetenv("OWNCLOUD_PARALLEL_CHUNK");
        bool parallelChunkUpload = env=="true" || env =="1";;
        if (_currentChunk + _startChunk >= _chunkCount - 1) {
            // Don't do parallel upload of chunk if this might be the last chunk because the server cannot handle that
            // https://github.com/owncloud/core/issues/11106
            parallelChunkUpload = false;
        }

        if (parallelChunkUpload && (_propagator->_activeJobs < _propagator->maximumActiveJob())
                && _currentChunk < _chunkCount ) {
            startNextChunk();
        }
        if (!parallelChunkUpload || _chunkCount - _currentChunk <= 0) {
            emitReady();
        }
    } else {
        qDebug() << "ERR: Could not open upload file: " << device->errorString();
        done( SyncFileItem::NormalError, device->errorString() );
        delete device;
        return;
    }
}

void PropagateUploadFileQNAM::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    Q_ASSERT(job);
    slotJobDestroyed(job); // remove it from the _jobs list

    qDebug() << Q_FUNC_INFO << job->reply()->request().url() << "FINISHED WITH STATUS"
             << job->reply()->error()
             << (job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : job->reply()->errorString())
             << job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute)
             << job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute);

    _propagator->_activeJobs--;

    if (_finished) {
        // We have send the finished signal already. We don't need to handle any remaining jobs
        return;
    }

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError) {
        _item._httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
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

        if (_item._httpErrorCode == 412) {
            // Precondition Failed:   Maybe the bad etag is in the database, we need to clear the
            // parent folder etag so we won't read from DB next sync.
            _propagator->_journal->avoidReadFromDbOnNextSync(_item._file);
            _propagator->_anotherSyncNeeded = true;
        }

        done(classifyError(err, _item._httpErrorCode), errorString);
        return;
    }

    _item._httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // The server needs some time to process the request and provide with a poll URL
    if (_item._httpErrorCode == 202) {
        _finished = true;
        QString path =  QString::fromUtf8(job->reply()->rawHeader("OC-Finish-Poll"));
        if (path.isEmpty()) {
            done(SyncFileItem::NormalError, tr("Poll URL missing"));
            return;
        }
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
    bool finished = job->reply()->hasRawHeader("ETag")
            || job->reply()->hasRawHeader("OC-ETag");


    QFileInfo fi(_propagator->getFilePath(_item._file));

    // Check if the file still exists
    if( !fi.exists() ) {
        if (!finished) {
            _finished = true;
            done(SyncFileItem::SoftError, tr("The local file was removed during sync."));
            return;
        } else {
            _propagator->_anotherSyncNeeded = true;
        }
    }

    // compare expected and real modification time of the file and size
    const time_t new_mtime = FileSystem::getModTime(fi.absoluteFilePath());
    const quint64 new_size = static_cast<quint64>(fi.size());
    if (new_mtime != _item._modtime || new_size != _item._size) {
        qDebug() << "The local file has changed during upload:"
                 << "mtime: " << _item._modtime << "<->" << new_mtime
                 << ", size: " << _item._size << "<->" << new_size
                 << ", QFileInfo: " << Utility::qDateTimeToTime_t(fi.lastModified()) << fi.lastModified();
        _propagator->_anotherSyncNeeded = true;
        if( !finished ) {
            _finished = true;
            done(SyncFileItem::SoftError, tr("Local file changed during sync."));
            // FIXME:  the legacy code was retrying for a few seconds.
            //         and also checking that after the last chunk, and removed the file in case of INSTRUCTION_NEW
            return;
        }
    }

    if (!finished) {
        // Proceed to next chunk.
        if (_currentChunk >= _chunkCount) {
            if (!_jobs.empty()) {
                // just wait for the other job to finish.
                return;
            }
            _finished = true;
            done(SyncFileItem::NormalError, tr("The server did not acknowledge the last chunk. (No e-tag were present)"));
            return;
        }

        SyncJournalDb::UploadInfo pi;
        pi._valid = true;
        auto currentChunk = job->_chunk;
        foreach (auto *job, _jobs) {
            // Take the minimum finished one
            currentChunk = qMin(currentChunk, job->_chunk);
        }
        pi._chunk = (currentChunk + _startChunk + 1) % _chunkCount ; // next chunk to start with
        pi._transferid = _transferId;
        pi._modtime =  Utility::qDateTimeFromTime_t(_item._modtime);
        _propagator->_journal->setUploadInfo(_item._file, pi);
        _propagator->_journal->commit("Upload info");
        startNextChunk();
        return;
    }

    // the following code only happens after all chunks were uploaded.
    _finished = true;
    // the file id should only be empty for new files up- or downloaded
    QByteArray fid = job->reply()->rawHeader("OC-FileID");
    if( !fid.isEmpty() ) {
        if( !_item._fileId.isEmpty() && _item._fileId != fid ) {
            qDebug() << "WARN: File ID changed!" << _item._fileId << fid;
        }
        _item._fileId = fid;
    }

    QByteArray etag = get_etag_from_reply(job->reply());
    _item._etag = etag;

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

    _item._requestDuration = _duration.elapsed();

    _propagator->_journal->setFileRecord(SyncJournalFileRecord(_item, _propagator->getFilePath(_item._file)));
    // Remove from the progress database:
    _propagator->_journal->setUploadInfo(_item._file, SyncJournalDb::UploadInfo());
    _propagator->_journal->commit("upload file start");

    qDebug() << Q_FUNC_INFO << "msec=" <<_duration.elapsed();
    done(SyncFileItem::Success);
}

void PropagateUploadFileQNAM::slotUploadProgress(qint64 sent, qint64 t)
{
    int progressChunk = _currentChunk + _startChunk - 1;
    if (progressChunk >= _chunkCount)
        progressChunk = _currentChunk - 1;
    quint64 amount = progressChunk * chunkSize();
    sender()->setProperty("byteWritten", sent);
    if (_jobs.count() > 1) {
        amount += sent;
    } else {
        amount -= (_jobs.count() -1) * chunkSize();
        foreach (QObject *j, _jobs) {
            amount += j->property("byteWritten").toULongLong();
        }
    }
    emit progress(_item, amount);
}

void PropagateUploadFileQNAM::startPollJob(const QString& path)
{
    PollJob* job = new PollJob(AccountManager::instance()->account(), path, _item,
                               _propagator->_journal, _propagator->_localDir, this);
    connect(job, SIGNAL(finishedSignal()), SLOT(slotPollFinished()));
    SyncJournalDb::PollInfo info;
    info._file = _item._file;
    info._url = path;
    info._modtime = _item._modtime;
    _propagator->_journal->setPollInfo(info);
    _propagator->_journal->commit("add poll info");
    job->start();
}

void PropagateUploadFileQNAM::slotPollFinished()
{
    PollJob *job = qobject_cast<PollJob *>(sender());
    Q_ASSERT(job);

    if (job->_item._status != SyncFileItem::Success) {
        done(job->_item._status, job->_item._errorString);
        return;
    }

    finalize(job->_item);
}

void PropagateUploadFileQNAM::slotJobDestroyed(QObject* job)
{
    _jobs.erase(std::remove(_jobs.begin(), _jobs.end(), job) , _jobs.end());
}

void PropagateUploadFileQNAM::abort()
{
    foreach(auto *job, _jobs) {
        if (job->reply()) {
            qDebug() << Q_FUNC_INFO << job << this->_item._file;
            job->reply()->abort();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// DOES NOT take owncership of the device.
GETFileJob::GETFileJob(Account* account, const QString& path, QFile *device,
                    const QMap<QByteArray, QByteArray> &headers, QByteArray expectedEtagForResume,
                    quint64 _resumeStart,  QObject* parent)
: AbstractNetworkJob(account, path, parent),
  _device(device), _headers(headers), _expectedEtagForResume(expectedEtagForResume),
  _resumeStart(_resumeStart) , _errorStatus(SyncFileItem::NoStatus)
, _bandwidthLimited(false), _bandwidthChoked(false), _bandwidthQuota(0), _bandwidthManager(0)
, _hasEmittedFinishedSignal(false)
{
}

GETFileJob::GETFileJob(Account* account, const QUrl& url, QFile *device,
                    const QMap<QByteArray, QByteArray> &headers,
                    QObject* parent)
: AbstractNetworkJob(account, url.toEncoded(), parent),
  _device(device), _headers(headers), _resumeStart(0),
  _errorStatus(SyncFileItem::NoStatus), _directDownloadUrl(url)
, _bandwidthLimited(false), _bandwidthChoked(false), _bandwidthQuota(0), _bandwidthManager(0)
, _hasEmittedFinishedSignal(false)
{
}


void GETFileJob::start() {
    if (_resumeStart > 0) {
        _headers["Range"] = "bytes=" + QByteArray::number(_resumeStart) +'-';
        _headers["Accept-Ranges"] = "bytes";
        qDebug() << "Retry with range " << _headers["Range"];
    }

    QNetworkRequest req;
    for(QMap<QByteArray, QByteArray>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }

    if (_directDownloadUrl.isEmpty()) {
        setReply(davRequest("GET", path(), req));
    } else {
        // Use direct URL
        setReply(davRequest("GET", _directDownloadUrl, req));
    }
    setupConnections(reply());

    reply()->setReadBufferSize(16 * 1024); // keep low so we can easier limit the bandwidth
    qDebug() << Q_FUNC_INFO << _bandwidthManager << _bandwidthChoked << _bandwidthLimited;
    if (_bandwidthManager) {
        _bandwidthManager->registerDownloadJob(this);
    }

    if( reply()->error() != QNetworkReply::NoError ) {
        qWarning() << Q_FUNC_INFO << " Network error: " << reply()->errorString();
    }

    connect(reply(), SIGNAL(metaDataChanged()), this, SLOT(slotMetaDataChanged()));
    connect(reply(), SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
    connect(reply(), SIGNAL(downloadProgress(qint64,qint64)), this, SIGNAL(downloadProgress(qint64,qint64)));
    connect(this, SIGNAL(networkActivity()), account(), SIGNAL(propagatorNetworkActivity()));

    AbstractNetworkJob::start();
}

void GETFileJob::slotMetaDataChanged()
{
    // For some reason setting the read buffer in GETFileJob::start doesn't seem to go
    // through the HTTP layer thread(?)
    reply()->setReadBufferSize(16 * 1024);

    int httpStatus = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // If the status code isn't 2xx, don't write the reply body to the file.
    // For any error: handle it when the job is finished, not here.
    if (httpStatus / 100 != 2) {
        _device->close();
        return;
    }
    if (reply()->error() != QNetworkReply::NoError) {
        return;
    }
    _etag = get_etag_from_reply(reply());

    if (!_directDownloadUrl.isEmpty() && !_etag.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "Direct download used, ignoring server ETag" << _etag;
        _etag = QByteArray(); // reset received ETag
    } else if (!_directDownloadUrl.isEmpty()) {
        // All fine, ETag empty and directDownloadUrl used
    } else if (_etag.isEmpty()) {
        qDebug() << Q_FUNC_INFO << "No E-Tag reply by server, considering it invalid";
        _errorString = tr("No E-Tag received from server, check Proxy/Gateway");
        _errorStatus = SyncFileItem::NormalError;
        reply()->abort();
        return;
    } else if (!_expectedEtagForResume.isEmpty() && _expectedEtagForResume != _etag) {
        qDebug() << Q_FUNC_INFO <<  "We received a different E-Tag for resuming!"
                << _expectedEtagForResume << "vs" << _etag;
        _errorString = tr("We received a different E-Tag for resuming. Retrying next time.");
        _errorStatus = SyncFileItem::NormalError;
        reply()->abort();
        return;
    }

    quint64 start = 0;
    QByteArray ranges = reply()->rawHeader("Content-Range");
    if (!ranges.isEmpty()) {
        QRegExp rx("bytes (\\d+)-");
        if (rx.indexIn(ranges) >= 0) {
            start = rx.cap(1).toULongLong();
        }
    }
    if (start != _resumeStart) {
        qDebug() << Q_FUNC_INFO <<  "Wrong content-range: "<< ranges << " while expecting start was" << _resumeStart;
        if (start == 0) {
            // device don't support range, just stry again from scratch
            _device->close();
            if (!_device->open(QIODevice::WriteOnly)) {
                _errorString = _device->errorString();
                _errorStatus = SyncFileItem::NormalError;
                reply()->abort();
                return;
            }
            _resumeStart = 0;
        } else {
            _errorString = tr("Server returned wrong content-range");
            _errorStatus = SyncFileItem::NormalError;
            reply()->abort();
            return;
        }
    }

}

void GETFileJob::setBandwidthManager(BandwidthManager *bwm)
{
    _bandwidthManager = bwm;
}

void GETFileJob::setChoked(bool c)
{
    _bandwidthChoked = c;
    QMetaObject::invokeMethod(this, "slotReadyRead", Qt::QueuedConnection);
}

void GETFileJob::setBandwidthLimited(bool b)
{
    _bandwidthLimited = b;
    QMetaObject::invokeMethod(this, "slotReadyRead", Qt::QueuedConnection);
}

void GETFileJob::giveBandwidthQuota(qint64 q)
{
    _bandwidthQuota = q;
    qDebug() << Q_FUNC_INFO << "Got" << q << "bytes";
    QMetaObject::invokeMethod(this, "slotReadyRead", Qt::QueuedConnection);
}

qint64 GETFileJob::currentDownloadPosition()
{
    if (_device && _device->pos() > 0 && _device->pos() > _resumeStart) {
        return _device->pos();
    }
    return _resumeStart;
}

void GETFileJob::slotReadyRead()
{
    int bufferSize = qMin(1024*8ll , reply()->bytesAvailable());
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    //qDebug() << Q_FUNC_INFO << reply()->bytesAvailable() << reply()->isOpen() << reply()->isFinished();

    while(reply()->bytesAvailable() > 0) {
        if (_bandwidthChoked) {
            qDebug() << Q_FUNC_INFO << "Download choked";
            break;
        }
        qint64 toRead = bufferSize;
        if (_bandwidthLimited) {
            toRead = qMin(qint64(bufferSize), _bandwidthQuota);
            if (toRead == 0) {
                qDebug() << Q_FUNC_INFO << "Out of quota";
                break;
            }
            _bandwidthQuota -= toRead;
            //qDebug() << Q_FUNC_INFO << "Reading" << toRead << "remaining" << _bandwidthQuota;
        }

        qint64 r = reply()->read(buffer.data(), toRead);
        if (r < 0) {
            _errorString = reply()->errorString();
            _errorStatus = SyncFileItem::NormalError;
            qDebug() << "Error while reading from device: " << _errorString;
            reply()->abort();
            return;
        }

        if (_device->isOpen()) {
            qint64 w = _device->write(buffer.constData(), r);
            if (w != r) {
                _errorString = _device->errorString();
                _errorStatus = SyncFileItem::NormalError;
                qDebug() << "Error while writing to file" << w << r <<  _errorString;
                reply()->abort();
                return;
            }
        }
    }

    //qDebug() << Q_FUNC_INFO << "END" << reply()->isFinished() << reply()->bytesAvailable() << _hasEmittedFinishedSignal;
    if (reply()->isFinished() && reply()->bytesAvailable() == 0) {
        qDebug() << Q_FUNC_INFO << "Actually finished!";
        if (_bandwidthManager) {
            _bandwidthManager->unregisterDownloadJob(this);
        }
        if (!_hasEmittedFinishedSignal) {
            emit finishedSignal();
        }
        _hasEmittedFinishedSignal = true;
        deleteLater();
    }
}

void GETFileJob::slotTimeout()
{
    _errorString =  tr("Connection Timeout");
    _errorStatus = SyncFileItem::FatalError;
    reply()->abort();
}

void PropagateDownloadFileQNAM::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qDebug() << Q_FUNC_INFO << _item._file << _propagator->_activeJobs;

    // do a klaas' case clash check.
    if( _propagator->localFileNameClash(_item._file) ) {
        done( SyncFileItem::NormalError, tr("File %1 can not be downloaded because of a local file name clash!")
              .arg(QDir::toNativeSeparators(_item._file)) );
        return;
    }

    emit progress(_item, 0);

    QString tmpFileName;
    QByteArray expectedEtagForResume;
    const SyncJournalDb::DownloadInfo progressInfo = _propagator->_journal->getDownloadInfo(_item._file);
    if (progressInfo._valid) {
        // if the etag has changed meanwhile, remove the already downloaded part.
        if (progressInfo._etag != _item._etag) {
            QFile::remove(_propagator->getFilePath(progressInfo._tmpfile));
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

    _tmpFile.setFileName(_propagator->getFilePath(tmpFileName));
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

    quint64 startSize = _tmpFile.size();
    if (startSize > 0) {
        if (startSize == _item._size) {
            qDebug() << "File is already complete, no need to download";
            downloadFinished();
            return;
        }
    }

    if (_item._directDownloadUrl.isEmpty()) {
        // Normal job, download from oC instance
        _job = new GETFileJob(AccountManager::instance()->account(),
                            _propagator->_remoteFolder + _item._file,
                            &_tmpFile, headers, expectedEtagForResume, startSize);
    } else {
        // We were provided a direct URL, use that one
        qDebug() << Q_FUNC_INFO << "directDownloadUrl given for " << _item._file << _item._directDownloadUrl;

        // Direct URLs don't support resuming, so clear an existing tmp file
        if (startSize > 0) {
            qDebug() << Q_FUNC_INFO << "resuming not supported for directDownloadUrl, deleting temporary";
            _tmpFile.close();
            if (!_tmpFile.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
                done(SyncFileItem::NormalError, _tmpFile.errorString());
                return;
            }
            startSize = 0;
        }

        if (!_item._directDownloadCookies.isEmpty()) {
            headers["Cookie"] = _item._directDownloadCookies.toUtf8();
        }

        QUrl url = QUrl::fromUserInput(_item._directDownloadUrl);
        _job = new GETFileJob(AccountManager::instance()->account(),
                              url,
                              &_tmpFile, headers);
    }
    _job->setBandwidthManager(&_propagator->_bandwidthManager);
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
        _item._httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // If we sent a 'Range' header and get 416 back, we want to retry
        // without the header.
        bool badRangeHeader = job->resumeStart() > 0 && _item._httpErrorCode == 416;
        if (badRangeHeader) {
            qDebug() << Q_FUNC_INFO << "server replied 416 to our range request, trying again without";
            _propagator->_anotherSyncNeeded = true;
        }

        // Don't keep the temporary file if it is empty or we
        // used a bad range header.
        if (_tmpFile.size() == 0 || badRangeHeader) {
            _tmpFile.close();
            _tmpFile.remove();
            _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
        }

        _propagator->_activeJobs--;
        SyncFileItem::Status status = job->errorStatus();
        if (status == SyncFileItem::NoStatus) {
            status = classifyError(err, _item._httpErrorCode);
        }
        if (badRangeHeader) {
            // Can't do this in classifyError() because 416 without a
            // Range header should result in NormalError.
            status = SyncFileItem::SoftError;
        }
        done(status, job->errorString());
        return;
    }

    if (!job->etag().isEmpty()) {
        // The etag will be empty if we used a direct download URL.
        // (If it was really empty by the server, the GETFileJob will have errored
        _item._etag = parseEtag(job->etag());
    }
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

    QString fn = _propagator->getFilePath(_item._file);

    // In case of file name clash, report an error
    // This can happen if another parallel download saved a clashing file.
    if (_propagator->localFileNameClash(_item._file)) {
        done( SyncFileItem::NormalError, tr("File %1 cannot be saved because of a local file name clash!")
              .arg(QDir::toNativeSeparators(_item._file)) );
        return;
    }

    // In case of conflict, make a backup of the old file
    // Ignore conflicts where both files are binary equal
    bool isConflict = _item._instruction == CSYNC_INSTRUCTION_CONFLICT
            && !FileSystem::fileEquals(fn, _tmpFile.fileName());
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
        // If we moved away the original file due to a conflict but can't
        // put the downloaded file in its place, we are in a bad spot:
        // If we do nothing the next sync run will assume the user deleted
        // the file!
        // To avoid that, the file is removed from the metadata table entirely
        // which makes it look like we're just about to initially download
        // it.
        if (isConflict) {
            _propagator->_journal->deleteFileRecord(fn);
            _propagator->_journal->commit("download finished");
            _propagator->_anotherSyncNeeded = true;
        }

        done(SyncFileItem::NormalError, error);
        return;
    }

    existingFile.refresh();
    // Maybe we downloaded a newer version of the file than we thought we would...
    // Get up to date information for the journal.
    FileSystem::setModTime(fn, _item._modtime);
    _item._size = existingFile.size();

    _propagator->_journal->setFileRecord(SyncJournalFileRecord(_item, fn));
    _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
    _propagator->_journal->commit("download file start2");
    done(isConflict ? SyncFileItem::Conflict : SyncFileItem::Success);
}

void PropagateDownloadFileQNAM::slotDownloadProgress(qint64 received, qint64)
{
    if (!_job) return;
    emit progress(_item, received + _job->resumeStart());
}


void PropagateDownloadFileQNAM::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
}


}
