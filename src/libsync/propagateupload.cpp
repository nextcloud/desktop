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
#include "checksums.h"
#include "syncengine.h"
#include "propagateremotedelete.h"

#include <json.h>
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>
#include <cmath>
#include <cstring>

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 2)
namespace {
const char owncloudShouldSoftCancelPropertyName[] = "owncloud-should-soft-cancel";
}
#endif

namespace OCC {

/**
 * We do not want to upload files that are currently being modified.
 * To avoid that, we don't upload files that have a modification time
 * that is too close to the current time.
 *
 * This interacts with the msBetweenRequestAndSync delay in the folder
 * manager. If that delay between file-change notification and sync
 * has passed, we should accept the file for upload here.
 */
static bool fileIsStillChanging(const SyncFileItem & item)
{
    const QDateTime modtime = Utility::qDateTimeFromTime_t(item._modtime);
    const qint64 msSinceMod = modtime.msecsTo(QDateTime::currentDateTime());

    return msSinceMod < SyncEngine::minimumFileAgeForUpload
            // if the mtime is too much in the future we *do* upload the file
            && msSinceMod > -10000;
}

PUTFileJob::~PUTFileJob()
{
    // Make sure that we destroy the QNetworkReply before our _device of which it keeps an internal pointer.
    setReply(0);
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
    connect(this, SIGNAL(networkActivity()), account().data(), SIGNAL(propagatorNetworkActivity()));

    // For Qt versions not including https://codereview.qt-project.org/110150
    // Also do the runtime check if compiled with an old Qt but running with fixed one.
    // (workaround disabled on windows and mac because the binaries we ship have patched qt)
#if QT_VERSION < QT_VERSION_CHECK(4, 8, 7)
    if (QLatin1String(qVersion()) < QLatin1String("4.8.7"))
        connect(_device.data(), SIGNAL(wasReset()), this, SLOT(slotSoftAbort()));
#elif QT_VERSION > QT_VERSION_CHECK(5, 0, 0) && QT_VERSION < QT_VERSION_CHECK(5, 4, 2) && !defined Q_OS_WIN && !defined Q_OS_MAC
    if (QLatin1String(qVersion()) < QLatin1String("5.4.2"))
        connect(_device.data(), SIGNAL(wasReset()), this, SLOT(slotSoftAbort()));
#endif

    AbstractNetworkJob::start();
}

void PUTFileJob::slotTimeout() {
    qDebug() << "Timeout" << (reply() ? reply()->request().url() : path());
    if (!reply())
        return;
    _errorString =  tr("Connection Timeout");
    reply()->abort();
}

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 2)
void PUTFileJob::slotSoftAbort() {
    reply()->setProperty(owncloudShouldSoftCancelPropertyName, true);
    reply()->abort();
}
#endif

void PollJob::start()
{
    setTimeout(120 * 1000);
    QUrl accountUrl = account()->url();
    QUrl finalUrl = QUrl::fromUserInput(accountUrl.scheme() + QLatin1String("://") +  accountUrl.authority()
        + (path().startsWith('/') ? QLatin1String("") : QLatin1String("/")) + path());
    setReply(getRequest(finalUrl));
    setupConnections(reply());
    connect(reply(), SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(resetTimeout()));
    AbstractNetworkJob::start();
}

bool PollJob::finished()
{
    QNetworkReply::NetworkError err = reply()->error();
    if (err != QNetworkReply::NoError) {
        _item->_httpErrorCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        _item->_status = classifyError(err, _item->_httpErrorCode);
        _item->_errorString = reply()->errorString();

        if (reply()->hasRawHeader("OC-ErrorString")) {
            _item->_errorString = reply()->rawHeader("OC-ErrorString");
        }

        if (_item->_status == SyncFileItem::FatalError || _item->_httpErrorCode >= 400) {
            if (_item->_status != SyncFileItem::FatalError
                    && _item->_httpErrorCode != 503) {
                SyncJournalDb::PollInfo info;
                info._file = _item->_file;
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
        _item->_errorString = tr("Invalid JSON reply from the poll URL");
        _item->_status = SyncFileItem::NormalError;
        emit finishedSignal();
        return true;
    }

    if (status["unfinished"].isValid()) {
        start();
        return false;
    }

    _item->_errorString = status["error"].toString();
    _item->_status = _item->_errorString.isEmpty() ? SyncFileItem::Success : SyncFileItem::NormalError;
    _item->_fileId = status["fileid"].toByteArray();
    _item->_etag = status["etag"].toByteArray();
    _item->_responseTimeStamp = responseTimestamp();

    SyncJournalDb::PollInfo info;
    info._file = _item->_file;
    // no info._url removes it from the database
    _journal->setPollInfo(info);
    _journal->commit("remove poll info");

    emit finishedSignal();
    return true;
}

void PropagateUploadFile::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0)) {
        return;
    }

    _propagator->_activeJobList.append(this);

    if (!_deleteExisting) {
        return slotComputeContentChecksum();
    }

    auto job = new DeleteJob(_propagator->account(),
                             _propagator->_remoteFolder + _item->_file,
                             this);
    _jobs.append(job);
    connect(job, SIGNAL(finishedSignal()), SLOT(slotComputeContentChecksum()));
    connect(job, SIGNAL(destroyed(QObject*)), SLOT(slotJobDestroyed(QObject*)));
    job->start();
}

void PropagateUploadFile::slotComputeContentChecksum()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0)) {
        return;
    }

    const QString filePath = _propagator->getFilePath(_item->_file);

    // remember the modtime before checksumming to be able to detect a file
    // change during the checksum calculation
    _item->_modtime = FileSystem::getModTime(filePath);

    _stopWatch.start();

    QByteArray checksumType = contentChecksumType();

    // Maybe the discovery already computed the checksum?
    if (_item->_contentChecksumType == checksumType
            && !_item->_contentChecksum.isEmpty()) {
        slotComputeTransmissionChecksum(checksumType, _item->_contentChecksum);
        return;
    }

    // Compute the content checksum.
    auto computeChecksum = new ComputeChecksum(this);
    computeChecksum->setChecksumType(checksumType);

    connect(computeChecksum, SIGNAL(done(QByteArray,QByteArray)),
            SLOT(slotComputeTransmissionChecksum(QByteArray,QByteArray)));
    connect(computeChecksum, SIGNAL(done(QByteArray,QByteArray)),
            computeChecksum, SLOT(deleteLater()));
    computeChecksum->start(filePath);
}

void PropagateUploadFile::setDeleteExisting(bool enabled)
{
    _deleteExisting = enabled;
}

void PropagateUploadFile::slotComputeTransmissionChecksum(const QByteArray& contentChecksumType, const QByteArray& contentChecksum)
{
    _item->_contentChecksum = contentChecksum;
    _item->_contentChecksumType = contentChecksumType;

    _stopWatch.addLapTime(QLatin1String("ContentChecksum"));
    _stopWatch.start();

    // Reuse the content checksum as the transmission checksum if possible
    const auto supportedTransmissionChecksums =
            _propagator->account()->capabilities().supportedChecksumTypes();
    if (supportedTransmissionChecksums.contains(contentChecksumType)) {
        slotStartUpload(contentChecksumType, contentChecksum);
        return;
    }

    // Compute the transmission checksum.
    auto computeChecksum = new ComputeChecksum(this);
    if (uploadChecksumEnabled()) {
        computeChecksum->setChecksumType(_propagator->account()->capabilities().uploadChecksumType());
    } else {
        computeChecksum->setChecksumType(QByteArray());
    }

    connect(computeChecksum, SIGNAL(done(QByteArray,QByteArray)),
            SLOT(slotStartUpload(QByteArray,QByteArray)));
    connect(computeChecksum, SIGNAL(done(QByteArray,QByteArray)),
            computeChecksum, SLOT(deleteLater()));
    const QString filePath = _propagator->getFilePath(_item->_file);
    computeChecksum->start(filePath);
}

void PropagateUploadFile::slotStartUpload(const QByteArray& transmissionChecksumType, const QByteArray& transmissionChecksum)
{
    // Remove ourselfs from the list of active job, before any posible call to done()
    // When we start chunks, we will add it again, once for every chunks.
    _propagator->_activeJobList.removeOne(this);

    _transmissionChecksum = transmissionChecksum;
    _transmissionChecksumType = transmissionChecksumType;

    if (_item->_contentChecksum.isEmpty() && _item->_contentChecksumType.isEmpty())  {
        // If the _contentChecksum was not set, reuse the transmission checksum as the content checksum.
        _item->_contentChecksum = transmissionChecksum;
        _item->_contentChecksumType = transmissionChecksumType;
    }

    const QString fullFilePath = _propagator->getFilePath(_item->_file);

    if (!FileSystem::fileExists(fullFilePath)) {
        done(SyncFileItem::SoftError, tr("File Removed"));
        return;
    }
    _stopWatch.addLapTime(QLatin1String("TransmissionChecksum"));

    time_t prevModtime = _item->_modtime; // the _item value was set in PropagateUploadFile::start()
    // but a potential checksum calculation could have taken some time during which the file could
    // have been changed again, so better check again here.

    _item->_modtime = FileSystem::getModTime(fullFilePath);
    if( prevModtime != _item->_modtime ) {
        _propagator->_anotherSyncNeeded = true;
        done(SyncFileItem::SoftError, tr("Local file changed during syncing. It will be resumed."));
        return;
    }

    quint64 fileSize = FileSystem::getSize(fullFilePath);
    _item->_size = fileSize;

    // But skip the file if the mtime is too close to 'now'!
    // That usually indicates a file that is still being changed
    // or not yet fully copied to the destination.
    if (fileIsStillChanging(*_item)) {
        _propagator->_anotherSyncNeeded = true;
        done(SyncFileItem::SoftError, tr("Local file changed during sync."));
        return;
    }

    _chunkCount = std::ceil(fileSize/double(chunkSize()));
    _startChunk = 0;
    _transferId = qrand() ^ _item->_modtime ^ (_item->_size << 16);

    const SyncJournalDb::UploadInfo progressInfo = _propagator->_journal->getUploadInfo(_item->_file);

    if (progressInfo._valid && Utility::qDateTimeToTime_t(progressInfo._modtime) == _item->_modtime ) {
        _startChunk = progressInfo._chunk;
        _transferId = progressInfo._transferid;
        qDebug() << Q_FUNC_INFO << _item->_file << ": Resuming from chunk " << _startChunk;
    }

    _currentChunk = 0;
    _duration.start();

    emit progress(*_item, 0);
    this->startNextChunk();
}

UploadDevice::UploadDevice(BandwidthManager *bwm)
    : _read(0),
      _bandwidthManager(bwm),
      _bandwidthQuota(0),
      _readWithProgress(0),
      _bandwidthLimited(false), _choked(false)
{
    _bandwidthManager->registerUploadDevice(this);
}


UploadDevice::~UploadDevice() {
    if (_bandwidthManager) {
        _bandwidthManager->unregisterUploadDevice(this);
    }
}

bool UploadDevice::prepareAndOpen(const QString& fileName, qint64 start, qint64 size)
{
    _data.clear();
    _read = 0;

    QFile file(fileName);
    QString openError;
    if (!FileSystem::openAndSeekFileSharedRead(&file, &openError, start)) {
        setErrorString(openError);
        return false;
    }

    size = qBound(0ll, size, FileSystem::getSize(fileName) - start);
    _data.resize(size);
    auto read = file.read(_data.data(), size);
    if (read != size) {
        setErrorString(file.errorString());
        return false;
    }

    return QIODevice::open(QIODevice::ReadOnly);
}


qint64 UploadDevice::writeData(const char* , qint64 ) {
    Q_ASSERT(!"write to read only device");
    return 0;
}

qint64 UploadDevice::readData(char* data, qint64 maxlen) {
    //qDebug() << Q_FUNC_INFO << maxlen << _read << _size << _bandwidthQuota;
    if (_data.size() - _read <= 0) {
        // at end
        if (_bandwidthManager) {
            _bandwidthManager->unregisterUploadDevice(this);
        }
        return -1;
    }
    maxlen = qMin(maxlen, _data.size() - _read);
    if (maxlen == 0) {
        return 0;
    }
    if (isChoked()) {
        return 0;
    }
    if (isBandwidthLimited()) {
        maxlen = qMin(maxlen, _bandwidthQuota);
        if (maxlen <= 0) {  // no quota
            //qDebug() << "no quota";
            return 0;
        }
        _bandwidthQuota -= maxlen;
    }
    std::memcpy(data, _data.data()+_read, maxlen);
    _read += maxlen;
    return maxlen;
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
    return _read >= _data.size();
}

qint64 UploadDevice::size() const{
//    qDebug() << this << Q_FUNC_INFO << _size;
    return _data.size();
}

qint64 UploadDevice::bytesAvailable() const
{
//    qDebug() << this << Q_FUNC_INFO << _size << _read << QIODevice::bytesAvailable()
//             <<   _size - _read + QIODevice::bytesAvailable();
    return _data.size() - _read + QIODevice::bytesAvailable();
}

// random access, we can seek
bool UploadDevice::isSequential() const{
    return false;
}

bool UploadDevice::seek ( qint64 pos ) {
    if (! QIODevice::seek(pos)) {
        return false;
    }
    if (pos < 0 || pos > _data.size()) {
        return false;
    }
    _read = pos;
    return true;
}

void UploadDevice::giveBandwidthQuota(qint64 bwq) {
    if (!atEnd()) {
        _bandwidthQuota = bwq;
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

void PropagateUploadFile::startNextChunk()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    if (! _jobs.isEmpty() &&  _currentChunk + _startChunk >= _chunkCount - 1) {
        // Don't do parallel upload of chunk if this might be the last chunk because the server cannot handle that
        // https://github.com/owncloud/core/issues/11106
        // We return now and when the _jobs are finished we will proceed with the last chunk
        // NOTE: Some other parts of the code such as slotUploadProgress also assume that the last chunk
        // is sent last.
        return;
    }
    quint64 fileSize = _item->_size;
    QMap<QByteArray, QByteArray> headers;
    headers["OC-Total-Length"] = QByteArray::number(fileSize);
    headers["OC-Async"] = "1";
    headers["OC-Chunk-Size"]= QByteArray::number(quint64(chunkSize()));
    headers["Content-Type"] = "application/octet-stream";
    headers["X-OC-Mtime"] = QByteArray::number(qint64(_item->_modtime));

    if(_item->_file.contains(".sys.admin#recall#")) {
        // This is a file recall triggered by the admin.  Note: the
        // recall list file created by the admin and downloaded by the
        // client (.sys.admin#recall#) also falls into this category
        // (albeit users are not supposed to mess up with it)

        // We use a special tag header so that the server may decide to store this file away in some admin stage area
        // And not directly in the user's area (which would trigger redownloads etc).
        headers["OC-Tag"] = ".sys.admin#recall#";
    }

    if (!_item->_etag.isEmpty() && _item->_etag != "empty_etag"
            && _item->_instruction != CSYNC_INSTRUCTION_NEW  // On new files never send a If-Match
            && _item->_instruction != CSYNC_INSTRUCTION_TYPE_CHANGE
            && !_deleteExisting
            ) {
        // We add quotes because the owncloud server always adds quotes around the etag, and
        //  csync_owncloud.c's owncloud_file_id always strips the quotes.
        headers["If-Match"] = '"' + _item->_etag + '"';
    }

    QString path = _item->_file;

    UploadDevice *device = new UploadDevice(&_propagator->_bandwidthManager);
    qint64 chunkStart = 0;
    qint64 currentChunkSize = fileSize;
    bool isFinalChunk = false;
    if (_chunkCount > 1) {
        int sendingChunk = (_currentChunk + _startChunk) % _chunkCount;
        // XOR with chunk size to make sure everything goes well if chunk size changes between runs
        uint transid = _transferId ^ chunkSize();
        qDebug() << "Upload chunk" << sendingChunk << "of" << _chunkCount << "transferid(remote)=" << transid;
        path +=  QString("-chunking-%1-%2-%3").arg(transid).arg(_chunkCount).arg(sendingChunk);

        headers["OC-Chunked"] = "1";

        chunkStart = chunkSize() * quint64(sendingChunk);
        currentChunkSize = chunkSize();
        if (sendingChunk == _chunkCount - 1) { // last chunk
            currentChunkSize = (fileSize % chunkSize());
            if( currentChunkSize == 0 ) { // if the last chunk pretends to be 0, its actually the full chunk size.
                currentChunkSize = chunkSize();
            }
            isFinalChunk = true;
        }
    } else {
        // if there's only one chunk, it's the final one
        isFinalChunk = true;
    }

    if (isFinalChunk && !_transmissionChecksumType.isEmpty()) {
        headers[checkSumHeaderC] = makeChecksumHeader(
                _transmissionChecksumType, _transmissionChecksum);
    }

    const QString fileName = _propagator->getFilePath(_item->_file);
    if (! device->prepareAndOpen(fileName, chunkStart, currentChunkSize)) {
        qDebug() << "ERR: Could not prepare upload device: " << device->errorString();

        // If the file is currently locked, we want to retry the sync
        // when it becomes available again.
        if (FileSystem::isFileLocked(fileName)) {
            emit _propagator->seenLockedFile(fileName);
        }

        // Soft error because this is likely caused by the user modifying his files while syncing
        abortWithError( SyncFileItem::SoftError, device->errorString() );
        delete device;
        return;
    }

    // job takes ownership of device via a QScopedPointer. Job deletes itself when finishing
    PUTFileJob* job = new PUTFileJob(_propagator->account(), _propagator->_remoteFolder + path, device, headers, _currentChunk);
    _jobs.append(job);
    connect(job, SIGNAL(finishedSignal()), this, SLOT(slotPutFinished()));
    connect(job, SIGNAL(uploadProgress(qint64,qint64)), this, SLOT(slotUploadProgress(qint64,qint64)));
    connect(job, SIGNAL(uploadProgress(qint64,qint64)), device, SLOT(slotJobUploadProgress(qint64,qint64)));
    connect(job, SIGNAL(destroyed(QObject*)), this, SLOT(slotJobDestroyed(QObject*)));
    job->start();
    _propagator->_activeJobList.append(this);
    _currentChunk++;

    bool parallelChunkUpload = true;
    QByteArray env = qgetenv("OWNCLOUD_PARALLEL_CHUNK");
    if (!env.isEmpty()) {
        parallelChunkUpload = env != "false" && env != "0";
    } else {
        int versionNum = _propagator->account()->serverVersionInt();
        if (versionNum < 0x080003) {
            // Disable parallel chunk upload severs older than 8.0.3 to avoid too many
            // internal sever errors (#2743, #2938)
            parallelChunkUpload = false;
        }
    }

    if (_currentChunk + _startChunk >= _chunkCount - 1) {
        // Don't do parallel upload of chunk if this might be the last chunk because the server cannot handle that
        // https://github.com/owncloud/core/issues/11106
        parallelChunkUpload = false;
    }

    if (parallelChunkUpload && (_propagator->_activeJobList.count() < _propagator->maximumActiveJob())
            && _currentChunk < _chunkCount ) {
        startNextChunk();
    }
    if (!parallelChunkUpload || _chunkCount - _currentChunk <= 0) {
        emit ready();
    }
}

void PropagateUploadFile::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    Q_ASSERT(job);
    slotJobDestroyed(job); // remove it from the _jobs list

    qDebug() << Q_FUNC_INFO << job->reply()->request().url() << "FINISHED WITH STATUS"
             << job->reply()->error()
             << (job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : job->reply()->errorString())
             << job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute)
             << job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute);

    _propagator->_activeJobList.removeOne(this);

    if (_finished) {
        // We have sent the finished signal already. We don't need to handle any remaining jobs
        return;
    }

    QNetworkReply::NetworkError err = job->reply()->error();

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 2)
    if (err == QNetworkReply::OperationCanceledError && job->reply()->property(owncloudShouldSoftCancelPropertyName).isValid()) {
        // Abort the job and try again later.
        // This works around a bug in QNAM wich might reuse a non-empty buffer for the next request.
        qDebug() << "Forcing job abort on HTTP connection reset with Qt < 5.4.2.";
        _propagator->_anotherSyncNeeded = true;
        abortWithError(SyncFileItem::SoftError, tr("Forcing job abort on HTTP connection reset with Qt < 5.4.2."));
        return;
    }
#endif

    if (err != QNetworkReply::NoError) {
        _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(checkForProblemsWithShared(_item->_httpErrorCode,
            tr("The file was edited locally but is part of a read only share. "
               "It is restored and your edit is in the conflict file."))) {
            return;
        }
        QByteArray replyContent = job->reply()->readAll();
        qDebug() << replyContent; // display the XML error in the debug
        QString errorString = errorMessage(job->errorString(), replyContent);

        if (job->reply()->hasRawHeader("OC-ErrorString")) {
            errorString = job->reply()->rawHeader("OC-ErrorString");
        }

        if (_item->_httpErrorCode == 412) {
            // Precondition Failed:   Maybe the bad etag is in the database, we need to clear the
            // parent folder etag so we won't read from DB next sync.
            _propagator->_journal->avoidReadFromDbOnNextSync(_item->_file);
            _propagator->_anotherSyncNeeded = true;
        }

        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
                                                    &_propagator->_anotherSyncNeeded);
        abortWithError(status, errorString);
        return;
    }

    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // The server needs some time to process the request and provide us with a poll URL
    if (_item->_httpErrorCode == 202) {
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
    QByteArray etag = getEtagFromReply(job->reply());
    bool finished = etag.length() > 0;

    // Check if the file still exists
    const QString fullFilePath(_propagator->getFilePath(_item->_file));
    if( !FileSystem::fileExists(fullFilePath) ) {
        if (!finished) {
            abortWithError(SyncFileItem::SoftError, tr("The local file was removed during sync."));
            return;
        } else {
            _propagator->_anotherSyncNeeded = true;
        }
    }

    // Check whether the file changed since discovery.
    if (! FileSystem::verifyFileUnchanged(fullFilePath, _item->_size, _item->_modtime)) {
        _propagator->_anotherSyncNeeded = true;
        if( !finished ) {
            abortWithError(SyncFileItem::SoftError, tr("Local file changed during sync."));
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
            done(SyncFileItem::NormalError, tr("The server did not acknowledge the last chunk. (No e-tag was present)"));
            return;
        }

        // Deletes an existing blacklist entry on successful chunk upload
        if (_item->_hasBlacklistEntry) {
            _propagator->_journal->wipeErrorBlacklistEntry(_item->_file);
            _item->_hasBlacklistEntry = false;
        }

        SyncJournalDb::UploadInfo pi;
        pi._valid = true;
        auto currentChunk = job->_chunk;
        foreach (auto *job, _jobs) {
            // Take the minimum finished one
            if (auto putJob = qobject_cast<PUTFileJob*>(job)) {
                currentChunk = qMin(currentChunk, putJob->_chunk - 1);
            }
        }
        pi._chunk = (currentChunk + _startChunk + 1) % _chunkCount ; // next chunk to start with
        pi._transferid = _transferId;
        pi._modtime =  Utility::qDateTimeFromTime_t(_item->_modtime);
        _propagator->_journal->setUploadInfo(_item->_file, pi);
        _propagator->_journal->commit("Upload info");
        startNextChunk();
        return;
    }

    // the following code only happens after all chunks were uploaded.
    _finished = true;
    // the file id should only be empty for new files up- or downloaded
    QByteArray fid = job->reply()->rawHeader("OC-FileID");
    if( !fid.isEmpty() ) {
        if( !_item->_fileId.isEmpty() && _item->_fileId != fid ) {
            qDebug() << "WARN: File ID changed!" << _item->_fileId << fid;
        }
        _item->_fileId = fid;
    }

    _item->_etag = etag;

    _item->_responseTimeStamp = job->responseTimestamp();

    if (job->reply()->rawHeader("X-OC-MTime") != "accepted") {
        // X-OC-MTime is supported since owncloud 5.0.   But not when chunking.
        // Normally Owncloud 6 always puts X-OC-MTime
        qWarning() << "Server does not support X-OC-MTime" << job->reply()->rawHeader("X-OC-MTime");
        // Well, the mtime was not set
        done(SyncFileItem::SoftError, "Server does not support X-OC-MTime");
    }

    // performance logging
    _item->_requestDuration = _stopWatch.stop();
    qDebug() << "*==* duration UPLOAD" << _item->_size
             << _stopWatch.durationOfLap(QLatin1String("ContentChecksum"))
             << _stopWatch.durationOfLap(QLatin1String("TransmissionChecksum"))
             << _item->_requestDuration;
    // The job might stay alive for the whole sync, release this tiny bit of memory.
    _stopWatch.reset();

    finalize(*_item);
}

void PropagateUploadFile::finalize(const SyncFileItem &copy)
{
    // Normally, copy == _item,   but when it comes from the UpdateMTimeAndETagJob, we need to do
    // some updates
    _item->_etag = copy._etag;
    _item->_fileId = copy._fileId;

    _item->_requestDuration = _duration.elapsed();

    _finished = true;

    if (!_propagator->_journal->setFileRecord(SyncJournalFileRecord(*_item, _propagator->getFilePath(_item->_file)))) {
        done(SyncFileItem::FatalError, tr("Error writing metadata to the database"));
        return;
    }
    // Remove from the progress database:
    _propagator->_journal->setUploadInfo(_item->_file, SyncJournalDb::UploadInfo());
    _propagator->_journal->commit("upload file start");

    done(SyncFileItem::Success);
}

void PropagateUploadFile::slotUploadProgress(qint64 sent, qint64 total)
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
        amount -= (_jobs.count() -1) * chunkSize();
        foreach (QObject *j, _jobs) {
            amount += j->property("byteWritten").toULongLong();
        }
    } else {
        // sender() is the only current job, no need to look at the byteWritten properties
        amount += sent;
    }
    emit progress(*_item, amount);
}

void PropagateUploadFile::startPollJob(const QString& path)
{
    PollJob* job = new PollJob(_propagator->account(), path, _item,
                               _propagator->_journal, _propagator->_localDir, this);
    connect(job, SIGNAL(finishedSignal()), SLOT(slotPollFinished()));
    SyncJournalDb::PollInfo info;
    info._file = _item->_file;
    info._url = path;
    info._modtime = _item->_modtime;
    _propagator->_journal->setPollInfo(info);
    _propagator->_journal->commit("add poll info");
    _propagator->_activeJobList.append(this);
    job->start();
}

void PropagateUploadFile::slotPollFinished()
{
    PollJob *job = qobject_cast<PollJob *>(sender());
    Q_ASSERT(job);

    _propagator->_activeJobList.removeOne(this);

    if (job->_item->_status != SyncFileItem::Success) {
        _finished = true;
        done(job->_item->_status, job->_item->_errorString);
        return;
    }

    finalize(*job->_item);
}

void PropagateUploadFile::slotJobDestroyed(QObject* job)
{
    _jobs.erase(std::remove(_jobs.begin(), _jobs.end(), job) , _jobs.end());
}

void PropagateUploadFile::abort()
{
    foreach(auto *job, _jobs) {
        if (job->reply()) {
            qDebug() << Q_FUNC_INFO << job << this->_item->_file;
            job->reply()->abort();
        }
    }
}

// This function is used whenever there is an error occuring and jobs might be in progress
void PropagateUploadFile::abortWithError(SyncFileItem::Status status, const QString &error)
{
    _finished = true;
    abort();
    done(status, error);
}


}
