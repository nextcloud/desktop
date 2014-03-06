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
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <cmath>

namespace Mirall {

void PUTFileJob::start() {
    QNetworkRequest req;
    for(QMap<QByteArray, QByteArray>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }

    setReply(davRequest("PUT", path(), req, _device));
    _device->setParent(reply());
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qDebug() << "getting etag: request network error: " << reply()->errorString();
    }
    AbstractNetworkJob::start();
}

// FIXME:  increase and make configurable
static const int CHUNKING_SIZE = (100*1024);

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
    _chunkCount = std::ceil(fileSize/double(CHUNKING_SIZE));
    _startChunk = 0;
    _transferId = qrand() ^ _item._modtime ^ (_item._size << 16);

    const SyncJournalDb::UploadInfo progressInfo = _propagator->_journal->getUploadInfo(_item._file);

    if (progressInfo._valid && Utility::qDateTimeToTime_t(progressInfo._modtime) == _item._modtime ) {
        _startChunk = progressInfo._chunk;
        _transferId = progressInfo._transferid;
        qDebug() << Q_FUNC_INFO << _item._file << ": Resuming from chunk " << _startChunk;
    }

    _currentChunk = 0;

    _propagator->_activeJobs++;
    emit progress(Progress::StartUpload, _item, 0, fileSize);
    emitReady();
    this->startNextChunk();
}

struct ChunkDevice : QIODevice {
    QIODevice *_file;
    qint64 _read;

    ChunkDevice(QIODevice *file,  qint64 start)
            : QIODevice(file), _file(file), _read(0) {
        _file->seek(start);
    }

    virtual qint64 writeData(const char* , qint64 ) {
        Q_ASSERT(!"write to read only device");
        return 0;
    }

    virtual qint64 readData(char* data, qint64 maxlen) {
        maxlen = qMin(maxlen, CHUNKING_SIZE - _read);
        if (maxlen == 0)
            return 0;
        qint64 ret = _file->read(data, maxlen);
        _read += ret;
        return ret;
    }

    virtual bool atEnd() const {
        return  _read >= CHUNKING_SIZE || _file->atEnd();
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
        path +=  QString("-chunking-%1-%2-%3").arg(uint(_transferId)).arg(_chunkCount).arg(sendingChunk);
        headers["OC-Chunked"] = "1";
        device = new ChunkDevice(_file, CHUNKING_SIZE * sendingChunk);
    } else {
        device = _file;
    }

    _job = new PUTFileJob(AccountManager::instance()->account(), _propagator->_remoteFolder + path, device, headers);
    connect(_job, SIGNAL(finishedSignal()), this, SLOT(slotPutFinished()));
    _job->start();
}

void PropagateUploadFileQNAM::slotPutFinished()
{
    PUTFileJob *job = qobject_cast<PUTFileJob *>(sender());
    Q_ASSERT(job);

    qDebug() << Q_FUNC_INFO << job->reply()->request().url() << "FINISHED WITH STATUS" << job->reply()->error() << job->reply()->errorString();

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError) {
        _item._httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        //FIXME: classify error
        _propagator->_activeJobs--;
        if(checkForProblemsWithShared(_item._httpErrorCode,
            tr("The file was edited locally but is part of a read only share. "
               "It is restored and your edit is in the conflict file."))) {
            return;
        }

        done(SyncFileItem::NormalError, job->reply()->errorString());
        return;
    }

    bool finished = job->reply()->hasRawHeader("ETag");

    if (!finished) {

        if (Utility::qDateTimeToTime_t(QFileInfo(_propagator->_localDir + _item._file).lastModified())
                != _item._modtime) {
            /* Uh oh:  The local file has changed during upload */
            _propagator->_activeJobs--;
            done(SyncFileItem::SoftError, tr("Local file changed during sync."));
            // FIXME:  the previous code was retrying for a few seconds.
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
        pi._chunk = _currentChunk; // next chunk to start with
        pi._transferid = _transferId;
        pi._modtime =  Utility::qDateTimeFromTime_t(_item._modtime);
        _propagator->_journal->setUploadInfo(_item._file, pi);
        _propagator->_journal->commit("Upload info");
        startNextChunk();
        return;
    }

    _propagator->_activeJobs--;

    // FIXME:  hack to check that the server did not accept the first chunk as a file
//     if (transfer->block_cnt > 1 && state == HBF_SUCCESS && cnt == 0) {
//         /* Success on the first chunk is suspicious.
//          *      It could happen that the server did not support chunking */
//         int rc = ne_delete(session, transfer_url);
//         if (rc == NE_OK && _hbf_http_error_code(session) == 204) {
//             /* If delete suceeded, it means some proxy strips the OC_CHUNKING header
//              *          start again without chunking: */
//             free( transfer_url );
//             return _hbf_transfer_no_chunk(session, transfer, verb);
//         }
//     }

    // the file id should only be empty for new files up- or downloaded
    QString fid = QString::fromUtf8(job->reply()->rawHeader("OC-FileID"));
    if( !fid.isEmpty() ) {
        if( !_item._fileId.isEmpty() && _item._fileId != fid ) {
            qDebug() << "WARN: File ID changed!" << _item._fileId << fid;
        }
        _item._fileId = fid;
    }

    _item._etag = parseEtag(job->reply()->rawHeader("ETag"));


    if (job->reply()->rawHeader("X-OC-MTime") != "accepted") {
        //FIXME
//             updateMTimeAndETag(uri.data(), _item._modtime);
        done(SyncFileItem::NormalError, tr("No X-OC-MTime extension,  ownCloud 5 is required"));
        return;
    }

    _propagator->_journal->setFileRecord(SyncJournalFileRecord(_item, _propagator->_localDir + _item._file));
    // Remove from the progress database:
    _propagator->_journal->setUploadInfo(_item._file, SyncJournalDb::UploadInfo());
    _propagator->_journal->commit("upload file start");

//     if (hbf_validate_source_file(trans.data()) == HBF_SOURCE_FILE_CHANGE) {
//             /* Did the source file changed since the upload ?
//              *               This is different from the previous check because the previous check happens between
//              *               chunks while this one happens when the whole file has been uploaded.
//              *
//              *               The new etag is already stored in the database in the previous lines so in case of
//              *               crash, we won't have a conflict but we will properly do a new upload
//              */
//
//             if( attempts++ < 5 ) { /* FIXME: How often do we want to try? */
//                 qDebug("SOURCE file has changed after upload, retry #%d in %d seconds!", attempts, 2*attempts);
//                 sleep(2*attempts);
//                 continue;
//             }
//
//             // Still the file change error, but we tried a couple of times.
//             // Ignore this file for now.
//             // Lets remove the file from the server (at least if it is new) as it is different
//             // from our file here.
//             if( _item._instruction == CSYNC_INSTRUCTION_NEW ) {
//                 QScopedPointer<char, QScopedPointerPodDeleter> uri(
//                     ne_path_escape((_propagator->_remoteDir + _item._file).toUtf8()));
//
//                 int rc = ne_delete(_propagator->_session, uri.data());
//                 qDebug() << "Remove the invalid file from server:" << rc;
//             }
//
//             const QString errMsg = tr("Local file changed during sync, syncing once it arrived completely");
//             done( SyncFileItem::SoftError, errMsg );
//             return;
//         }
//
    emit progress(Progress::EndUpload, _item, _item._size, _item._size);
    done(SyncFileItem::Success);
}

void PropagateUploadFileQNAM::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
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
        qDebug() << "getting etag: request network error: " << reply()->errorString();
    }

    connect(reply(), SIGNAL(readyRead()), this, SLOT(slotReadyRead()));

    AbstractNetworkJob::start();
}

void GETFileJob::slotReadyRead()
{
    // FIXME: error handling (hard drive full, ....)
    _device->write(reply()->readAll());
}


void PropagateDownloadFileQNAM::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qDebug() << Q_FUNC_INFO << _item._file << _propagator->_activeJobs;

    emit progress(Progress::StartDownload, _item, 0, _item._size);

    QString tmpFileName;
    const SyncJournalDb::DownloadInfo progressInfo = _propagator->_journal->getDownloadInfo(_item._file);
    if (progressInfo._valid) {
        // if the etag has changed meanwhile, remove the already downloaded part.
        if (progressInfo._etag != _item._etag) {
            QFile::remove(_propagator->_localDir + progressInfo._tmpfile);
            _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
        } else {
            tmpFileName = progressInfo._tmpfile;
            _expectedEtagForResume = progressInfo._etag;
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
    }

    _job = new GETFileJob(AccountManager::instance()->account(), _propagator->_remoteFolder + _item._file, &_tmpFile, headers);
    connect(_job, SIGNAL(finishedSignal()), this, SLOT(slotGetFinished()));
    _propagator->_activeJobs ++;
    _job->start();
    emitReady();
}

void PropagateDownloadFileQNAM::slotGetFinished()
{
    _propagator->_activeJobs--;

    GETFileJob *job = qobject_cast<GETFileJob *>(sender());
    Q_ASSERT(job);

    qDebug() << Q_FUNC_INFO << job->reply()->request().url() << "FINISHED WITH STATUS" << job->reply()->error() << job->reply()->errorString();

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError) {
        if (_tmpFile.size() == 0) {
            // don't keep the temporary file if it is empty.
            _tmpFile.close();
            _tmpFile.remove();
            _propagator->_journal->setDownloadInfo(_item._file, SyncJournalDb::DownloadInfo());
        }
        // FIXME!
        _item._httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        _propagator->_activeJobs--;
        done(SyncFileItem::NormalError, job->reply()->errorString());
        return;
    }

    _item._etag = parseEtag(job->reply()->rawHeader("Etag"));
    _tmpFile.close();
    _tmpFile.flush();
    downloadFinished();
}

void PropagateDownloadFileQNAM::downloadFinished()
{

    QString fn = _propagator->_localDir + _item._file;


    bool isConflict = _item._instruction == CSYNC_INSTRUCTION_CONFLICT
            && !FileSystem::fileEquals(fn, _tmpFile.fileName()); // compare the files to see if there was an actual conflict.
    //In case of conflict, make a backup of the old file
    if (isConflict) {
        QFile f(fn);
        QString conflictFileName(fn);
        // Add _conflict-XXXX  before the extention.
        int dotLocation = conflictFileName.lastIndexOf('.');
        // If no extention, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
        if (dotLocation <= conflictFileName.lastIndexOf('/') + 1) {
            dotLocation = conflictFileName.size();
        }
        QString timeString = Utility::qDateTimeFromTime_t(_item._modtime).toString("yyyyMMdd-hhmmss");
        conflictFileName.insert(dotLocation, "_conflict-" + timeString);
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
    emit progress(Progress::EndDownload, _item, _item._size, _item._size);
    done(isConflict ? SyncFileItem::Conflict : SyncFileItem::Success);
}

void PropagateDownloadFileQNAM::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
}


}
