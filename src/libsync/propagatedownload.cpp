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
#include "owncloudpropagator_p.h"
#include "propagatedownload.h"
#include "networkjobs.h"
#include "account.h"
#include "syncjournaldb.h"
#include "syncjournalfilerecord.h"
#include "utility.h"
#include "filesystem.h"
#include "propagatorjobs.h"
#include "transmissionchecksumvalidator.h"

#include <json.h>
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <cmath>

namespace OCC {

// Always coming in with forward slashes.
// In csync_excluded_no_ctx we ignore all files with longer than 254 chars
// This function also adds a dot at the begining of the filename to hide the file on OS X and Linux
QString OWNCLOUDSYNC_EXPORT createDownloadTmpFileName(const QString &previous) {
    QString tmpFileName;
    QString tmpPath;
    int slashPos = previous.lastIndexOf('/');
    // work with both pathed filenames and only filenames
    if (slashPos == -1) {
        tmpFileName = previous;
        tmpPath = QString();
    } else {
        tmpFileName = previous.mid(slashPos+1);
        tmpPath = previous.left(slashPos);
    }
    int overhead =  1 + 1 + 2 + 8; // slash dot dot-tilde ffffffff"
    int spaceForFileName = qMin(254, tmpFileName.length() + overhead) - overhead;
    if (tmpPath.length() > 0) {
        return tmpPath + '/' + '.' + tmpFileName.left(spaceForFileName) + ".~" + (QString::number(uint(qrand() % 0xFFFFFFFF), 16));
    } else {
        return '.' + tmpFileName.left(spaceForFileName) + ".~" + (QString::number(uint(qrand() % 0xFFFFFFFF), 16));
    }
}

// DOES NOT take owncership of the device.
GETFileJob::GETFileJob(AccountPtr account, const QString& path, QFile *device,
                    const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
                    quint64 resumeStart,  QObject* parent)
: AbstractNetworkJob(account, path, parent),
  _device(device), _headers(headers), _expectedEtagForResume(expectedEtagForResume)
, _resumeStart(resumeStart) , _errorStatus(SyncFileItem::NoStatus)
, _bandwidthLimited(false), _bandwidthChoked(false), _bandwidthQuota(0), _bandwidthManager(0)
, _hasEmittedFinishedSignal(false), _lastModified()
{
}

GETFileJob::GETFileJob(AccountPtr account, const QUrl& url, QFile *device,
                    const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
                       quint64 resumeStart, QObject* parent)

: AbstractNetworkJob(account, url.toEncoded(), parent),
  _device(device), _headers(headers), _expectedEtagForResume(expectedEtagForResume)
, _resumeStart(resumeStart), _errorStatus(SyncFileItem::NoStatus), _directDownloadUrl(url)
, _bandwidthLimited(false), _bandwidthChoked(false), _bandwidthQuota(0), _bandwidthManager(0)
, _hasEmittedFinishedSignal(false), _lastModified()
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
        _followRedirects = true;  // (follow redirections for the direct download)
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
    connect(this, SIGNAL(networkActivity()), account().data(), SIGNAL(propagatorNetworkActivity()));

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
    _etag = getEtagFromReply(reply());

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
        if (ranges.isEmpty()) {
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

    auto lastModified = reply()->header(QNetworkRequest::LastModifiedHeader);
    if (!lastModified.isNull()) {
        _lastModified = Utility::qDateTimeToTime_t(lastModified.toDateTime());
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
    if (_device && _device->pos() > 0 && _device->pos() > qint64(_resumeStart)) {
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
                //qDebug() << Q_FUNC_INFO << "Out of quota";
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

QString GETFileJob::errorString() const
{
    if (!_errorString.isEmpty()) {
        return _errorString;
    } else if (reply()->hasRawHeader("OC-ErrorString")) {
        return reply()->rawHeader("OC-ErrorString");
    } else {
        return reply()->errorString();
    }
}

void PropagateDownloadFileQNAM::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qDebug() << Q_FUNC_INFO << _item->_file << _propagator->_activeJobs;

    // do a klaas' case clash check.
    if( _propagator->localFileNameClash(_item->_file) ) {
        done( SyncFileItem::NormalError, tr("File %1 can not be downloaded because of a local file name clash!")
              .arg(QDir::toNativeSeparators(_item->_file)) );
        return;
    }

    emit progress(*_item, 0);

    QString tmpFileName;
    QByteArray expectedEtagForResume;
    const SyncJournalDb::DownloadInfo progressInfo = _propagator->_journal->getDownloadInfo(_item->_file);
    if (progressInfo._valid) {
        // if the etag has changed meanwhile, remove the already downloaded part.
        if (progressInfo._etag != _item->_etag) {
            QFile::remove(_propagator->getFilePath(progressInfo._tmpfile));
            _propagator->_journal->setDownloadInfo(_item->_file, SyncJournalDb::DownloadInfo());
        } else {
            tmpFileName = progressInfo._tmpfile;
            expectedEtagForResume = progressInfo._etag;
        }

    }

    if (tmpFileName.isEmpty()) {
        tmpFileName = createDownloadTmpFileName(_item->_file);
    }

    _tmpFile.setFileName(_propagator->getFilePath(tmpFileName));
    if (!_tmpFile.open(QIODevice::Append | QIODevice::Unbuffered)) {
        done(SyncFileItem::NormalError, _tmpFile.errorString());
        return;
    }

    FileSystem::setFileHidden(_tmpFile.fileName(), true);

    {
        SyncJournalDb::DownloadInfo pi;
        pi._etag = _item->_etag;
        pi._tmpfile = tmpFileName;
        pi._valid = true;
        _propagator->_journal->setDownloadInfo(_item->_file, pi);
        _propagator->_journal->commit("download file start");
    }


    QMap<QByteArray, QByteArray> headers;

    quint64 startSize = _tmpFile.size();
    if (startSize > 0) {
        if (startSize == _item->_size) {
            qDebug() << "File is already complete, no need to download";
            _tmpFile.close();
            downloadFinished();
            return;
        }
    }

    if (_item->_directDownloadUrl.isEmpty()) {
        // Normal job, download from oC instance
        _job = new GETFileJob(_propagator->account(),
                            _propagator->_remoteFolder + _item->_file,
                            &_tmpFile, headers, expectedEtagForResume, startSize);
    } else {
        // We were provided a direct URL, use that one
        qDebug() << Q_FUNC_INFO << "directDownloadUrl given for " << _item->_file << _item->_directDownloadUrl;

        if (!_item->_directDownloadCookies.isEmpty()) {
            headers["Cookie"] = _item->_directDownloadCookies.toUtf8();
        }

        QUrl url = QUrl::fromUserInput(_item->_directDownloadUrl);
        _job = new GETFileJob(_propagator->account(),
                              url,
                              &_tmpFile, headers, expectedEtagForResume, startSize);
    }
    _job->setBandwidthManager(&_propagator->_bandwidthManager);
    connect(_job, SIGNAL(finishedSignal()), this, SLOT(slotGetFinished()));
    connect(_job, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(slotDownloadProgress(qint64,qint64)));
    _propagator->_activeJobs ++;
    _job->start();
}

const char owncloudCustomSoftErrorStringC[] = "owncloud-custom-soft-error-string";
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
        _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // If we sent a 'Range' header and get 416 back, we want to retry
        // without the header.
        const bool badRangeHeader = job->resumeStart() > 0 && _item->_httpErrorCode == 416;
        if (badRangeHeader) {
            qDebug() << Q_FUNC_INFO << "server replied 416 to our range request, trying again without";
            _propagator->_anotherSyncNeeded = true;
        }

        // Getting a 404 probably means that the file was deleted on the server.
        const bool fileNotFound = _item->_httpErrorCode == 404;
        if (fileNotFound) {
            qDebug() << Q_FUNC_INFO << "server replied 404, assuming file was deleted";
        }

        // Don't keep the temporary file if it is empty or we
        // used a bad range header or the file's not on the server anymore.
        if (_tmpFile.size() == 0 || badRangeHeader || fileNotFound) {
            _tmpFile.close();
            _tmpFile.remove();
            _propagator->_journal->setDownloadInfo(_item->_file, SyncJournalDb::DownloadInfo());
        }

        if(!_item->_directDownloadUrl.isEmpty() && err != QNetworkReply::OperationCanceledError) {
            // If this was with a direct download, retry without direct download
            qWarning() << "Direct download of" << _item->_directDownloadUrl << "failed. Retrying through owncloud.";
            _item->_directDownloadUrl.clear();
            start();
            return;
        }

        // This gives a custom QNAM (by the user of libowncloudsync) to abort() a QNetworkReply in its metaDataChanged() slot and
        // set a custom error string to make this a soft error. In contrast to the default hard error this won't bring down
        // the whole sync and allows for a custom error message.
        QNetworkReply *reply = job->reply();
        if (err == QNetworkReply::OperationCanceledError && reply->property(owncloudCustomSoftErrorStringC).isValid()) {
            job->setErrorString(reply->property(owncloudCustomSoftErrorStringC).toString());
            job->setErrorStatus(SyncFileItem::SoftError);
        } else if (badRangeHeader) {
            // Can't do this in classifyError() because 416 without a
            // Range header should result in NormalError.
            job->setErrorStatus(SyncFileItem::SoftError);
        } else if (fileNotFound) {
            job->setErrorString(tr("File was deleted from server"));
            job->setErrorStatus(SyncFileItem::SoftError);
        }

        SyncFileItem::Status status = job->errorStatus();
        if (status == SyncFileItem::NoStatus) {
            status = classifyError(err, _item->_httpErrorCode);
        }

        done(status, job->errorString());
        return;
    }

    if (!job->etag().isEmpty()) {
        // The etag will be empty if we used a direct download URL.
        // (If it was really empty by the server, the GETFileJob will have errored
        _item->_etag = parseEtag(job->etag());
    }
    if (job->lastModified()) {
        // It is possible that the file was modified on the server since we did the discovery phase
        // so make sure we have the up-to-date time
        _item->_modtime = job->lastModified();
    }
    _item->_requestDuration = job->duration();
    _item->_responseTimeStamp = job->responseTimestamp();

    _tmpFile.close();
    _tmpFile.flush();

    /* Check that the size of the GET reply matches the file size. There have been cases
     * reported that if a server breaks behind a proxy, the GET is still a 200 but is
     * truncated, as described here: https://github.com/owncloud/mirall/issues/2528
     */
    const QByteArray sizeHeader("Content-Length");
    quint64 bodySize = job->reply()->rawHeader(sizeHeader).toULongLong();

    if(bodySize > 0 && bodySize != _tmpFile.size() - job->resumeStart() ) {
        qDebug() << bodySize << _tmpFile.size() << job->resumeStart();
        _propagator->_anotherSyncNeeded = true;
        done(SyncFileItem::SoftError, tr("The file could not be downloaded completely."));
        return;
    }

    // Do checksum validation for the download. If there is no checksum header, the validator
    // will also emit the validated() signal to continue the flow in slot downloadFinished()
    // as this is (still) also correct.
    TransmissionChecksumValidator *validator = new TransmissionChecksumValidator(_tmpFile.fileName(), this);
    connect(validator, SIGNAL(validated(QByteArray)), this, SLOT(downloadFinished()));
    connect(validator, SIGNAL(validationFailed(QString)), this, SLOT(slotChecksumFail(QString)));
    validator->downloadValidation(job->reply()->rawHeader(checkSumHeaderC));

}

void PropagateDownloadFileQNAM::slotChecksumFail( const QString& errMsg )
{
    _tmpFile.remove();
    _propagator->_anotherSyncNeeded = true;
    done(SyncFileItem::SoftError, errMsg ); // tr("The file downloaded with a broken checksum, will be redownloaded."));
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


namespace { // Anonymous namespace for the recall feature
static QString makeRecallFileName(const QString &fn)
{
    QString recallFileName(fn);
    // Add _recall-XXXX  before the extention.
    int dotLocation = recallFileName.lastIndexOf('.');
    // If no extention, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
    if (dotLocation <= recallFileName.lastIndexOf('/') + 1) {
        dotLocation = recallFileName.size();
    }

    QString timeString = QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss");
    recallFileName.insert(dotLocation, "_.sys.admin#recall#-" + timeString);

    return recallFileName;
}

static void handleRecallFile(const QString &fn)
{
    qDebug() << "handleRecallFile: " << fn;

    FileSystem::setFileHidden(fn, true);

    QFile file(fn);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open recall file" << file.errorString();
        return;
    }
    QFileInfo existingFile(fn);
    QDir thisDir = existingFile.dir();

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        line.chop(1); // remove trailing \n
        QString fpath = thisDir.filePath(line);
        QString rpath = makeRecallFileName(fpath);

        // if previously recalled file exists then remove it (copy will not overwrite it)
        QFile(rpath).remove();
        qDebug() << "Copy recall file: " << fpath << " -> " << rpath;
        QFile::copy(fpath,rpath);
    }
}
} // end namespace

void PropagateDownloadFileQNAM::downloadFinished()
{
    QString fn = _propagator->getFilePath(_item->_file);

    // In case of file name clash, report an error
    // This can happen if another parallel download saved a clashing file.
    if (_propagator->localFileNameClash(_item->_file)) {
        done( SyncFileItem::NormalError, tr("File %1 cannot be saved because of a local file name clash!")
              .arg(QDir::toNativeSeparators(_item->_file)) );
        return;
    }

    // In case of conflict, make a backup of the old file
    // Ignore conflicts where both files are binary equal
    bool isConflict = _item->_instruction == CSYNC_INSTRUCTION_CONFLICT
            && !FileSystem::fileEquals(fn, _tmpFile.fileName());
    if (isConflict) {
        QString renameError;
        QString conflictFileName = makeConflictFileName(fn, Utility::qDateTimeFromTime_t(_item->_modtime));
        if (!FileSystem::rename(fn, conflictFileName, &renameError)) {
            //If the rename fails, don't replace it.
            done(SyncFileItem::SoftError, renameError);
            return;
        }
        qDebug() << "Created conflict file" << fn << "->" << conflictFileName;
    }

    FileSystem::setModTime(_tmpFile.fileName(), _item->_modtime);
    // We need to fetch the time again because some file system such as FAT have a less than a second
    // Accuracy, and we really need the time from the file system. (#3103)
    _item->_modtime = FileSystem::getModTime(_tmpFile.fileName());

    if (FileSystem::fileExists(fn)) {
        // Preserve the existing file permissions.
        QFileInfo existingFile(fn);
        if (existingFile.permissions() != _tmpFile.permissions()) {
            _tmpFile.setPermissions(existingFile.permissions());
        }

        // Check whether the existing file has changed since the discovery
        // phase by comparing size and mtime to the previous values. This
        // is necessary to avoid overwriting user changes that happened between
        // the discovery phase and now.
        const qint64 expectedSize = _item->log._other_size;
        const time_t expectedMtime = _item->log._other_modtime;
        if (! FileSystem::verifyFileUnchanged(fn, expectedSize, expectedMtime)) {
            _propagator->_anotherSyncNeeded = true;
            done(SyncFileItem::SoftError, tr("File has changed since discovery"));
            return;
        }
    }

    QString error;
    _propagator->addTouchedFile(fn);
    // The fileChanged() check is done above to generate better error messages.
    if (!FileSystem::uncheckedRenameReplace(_tmpFile.fileName(), fn, &error)) {
        qDebug() << Q_FUNC_INFO << QString("Rename failed: %1 => %2").arg(_tmpFile.fileName()).arg(fn);

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
        }

        _propagator->_anotherSyncNeeded = true;
        done(SyncFileItem::SoftError, error);
        return;
    }
    FileSystem::setFileHidden(fn, false);

    // Maybe we downloaded a newer version of the file than we thought we would...
    // Get up to date information for the journal.
    _item->_size = FileSystem::getSize(fn);

    _propagator->_journal->setFileRecord(SyncJournalFileRecord(*_item, fn));
    _propagator->_journal->setDownloadInfo(_item->_file, SyncJournalDb::DownloadInfo());
    _propagator->_journal->commit("download file start2");
    done(isConflict ? SyncFileItem::Conflict : SyncFileItem::Success);

    // handle the special recall file
    if(_item->_file == QLatin1String(".sys.admin#recall#") || _item->_file.endsWith("/.sys.admin#recall#")) {
        handleRecallFile(fn);
    }
}

void PropagateDownloadFileQNAM::slotDownloadProgress(qint64 received, qint64)
{
    if (!_job) return;
    emit progress(*_item, received + _job->resumeStart());
}


void PropagateDownloadFileQNAM::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
}


}
