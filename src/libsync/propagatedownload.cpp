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
#include "common/syncjournaldb.h"
#include "common/syncjournalfilerecord.h"
#include "common/utility.h"
#include "filesystem.h"
#include "propagatorjobs.h"
#include "common/checksums.h"
#include "common/asserts.h"
#include "common/vfs.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QRandomGenerator>

#include <cmath>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace std::chrono_literals;

namespace OCC {

Q_LOGGING_CATEGORY(lcGetJob, "sync.networkjob.get", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropagateDownload, "sync.propagator.download", QtInfoMsg)

// Always coming in with forward slashes.
// In csync_excluded_no_ctx we ignore all files with longer than 254 chars
// This function also adds a dot at the beginning of the filename to hide the file on OS X and Linux
QString OWNCLOUDSYNC_EXPORT createDownloadTmpFileName(const QString &previous)
{
    QString tmpFileName;
    QString tmpPath;
    const int slashPos = previous.lastIndexOf(QLatin1Char('/'));
    // work with both pathed filenames and only filenames
    if (slashPos == -1) {
        tmpFileName = previous;
    } else {
        tmpFileName = previous.mid(slashPos + 1);
        tmpPath = previous.left(slashPos);
    }

    auto rg = QRandomGenerator::global();
    const int overhead = 1 + 1 + 2 + 8; // slash dot dot-tilde ffffffff"
    const int spaceForFileName = qMin(254, tmpFileName.length() + overhead) - overhead;
    if (tmpPath.length() > 0) {
        return QStringLiteral("%1/.%2.~%3").arg(tmpPath, tmpFileName.left(spaceForFileName), QString::number(uint(rg->generate() % 0xFFFFFFFF), 16));
    } else {
        return QStringLiteral(".%1.~%2").arg(tmpFileName.left(spaceForFileName), QString::number(uint(rg->generate() % 0xFFFFFFFF), 16));
    }
}

// DOES NOT take ownership of the device.
GETFileJob::GETFileJob(AccountPtr account, const QUrl &url, const QString &path, QIODevice *device,
    const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
    qint64 resumeStart, QObject *parent)
    : AbstractNetworkJob(account, url, path, parent)
    , _device(device)
    , _headers(headers)
    , _expectedEtagForResume(expectedEtagForResume)
    , _expectedContentLength(-1)
    , _contentLength(-1)
    , _resumeStart(resumeStart)
{
    connect(this, &GETFileJob::networkError, this, [this] {
        if (timedOut()) {
            qCWarning(lcGetJob) << this << "timeout";
            _errorString = tr("Connection Timeout");
            _errorStatus = SyncFileItem::FatalError;
        }
    });

    // Long downloads must not block non-propagation jobs.
    setPriority(QNetworkRequest::LowPriority);
}

void GETFileJob::start()
{
    if (_resumeStart > 0) {
        _headers["Range"] = "bytes=" + QByteArray::number(_resumeStart) + '-';
        _headers["Accept-Ranges"] = "bytes";
        qCDebug(lcGetJob) << "Retry with range " << _headers["Range"];
    }

    QNetworkRequest req;
    for (auto it = _headers.cbegin(); it != _headers.cend(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }

    sendRequest("GET", req);

    qCDebug(lcGetJob) << _bandwidthManager << _bandwidthChoked << _bandwidthLimited;
    if (_bandwidthManager) {
        _bandwidthManager->registerDownloadJob(this);
    }
    AbstractNetworkJob::start();
}

void GETFileJob::finished()
{
    if (_bandwidthManager) {
        _bandwidthManager->unregisterDownloadJob(this);
    }
    if (reply()->bytesAvailable() && _httpOk) {
        // we were throttled, write out the remaining data
        slotReadyRead();
        Q_ASSERT(!reply()->bytesAvailable());
    }
}

void GETFileJob::newReplyHook(QNetworkReply *reply)
{
    reply->setReadBufferSize(16 * 1024); // keep low so we can easier limit the bandwidth

    connect(reply, &QNetworkReply::metaDataChanged, this, &GETFileJob::slotMetaDataChanged);
    connect(reply, &QNetworkReply::finished, this, &GETFileJob::slotReadyRead);
    connect(reply, &QNetworkReply::downloadProgress, this, &GETFileJob::downloadProgress);
}

void GETFileJob::slotMetaDataChanged()
{
    // For some reason setting the read buffer in GETFileJob::start doesn't seem to go
    // through the HTTP layer thread(?)
    reply()->setReadBufferSize(16 * 1024);

    int httpStatus = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (httpStatus == 301 || httpStatus == 302 || httpStatus == 303 || httpStatus == 307
        || httpStatus == 308 || httpStatus == 401) {
        // Redirects and auth failures (oauth token renew) are handled by AbstractNetworkJob and
        // will end up restarting the job. We do not want to process further data from the initial
        // request. newReplyHook() will reestablish signal connections for the follow-up request.
        bool ok = disconnect(reply(), &QNetworkReply::finished, this, &GETFileJob::slotReadyRead)
            && disconnect(reply(), &QNetworkReply::readyRead, this, &GETFileJob::slotReadyRead);
        OC_ASSERT(ok);
        return;
    }

    // If the status code isn't 2xx, don't write the reply body to the file.
    // For any error: handle it when the job is finished, not here.
    if (httpStatus / 100 != 2) {
        // Disable the buffer limit, as we don't limit the bandwidth for error messages.
        // (We are only going to do a readAll() at the end.)
        reply()->setReadBufferSize(0);
        return;
    }
    if (reply()->error() != QNetworkReply::NoError) {
        return;
    }
    _etag = getEtagFromReply(reply());

    if (_etag.isEmpty()) {
        qCWarning(lcGetJob) << "No E-Tag reply by server, considering it invalid";
        _errorString = tr("No E-Tag received from server, check Proxy/Gateway");
        _errorStatus = SyncFileItem::NormalError;
        abort();
        return;
    } else if (!_expectedEtagForResume.isEmpty() && _expectedEtagForResume != _etag) {
        qCWarning(lcGetJob) << "We received a different E-Tag for resuming!"
                            << _expectedEtagForResume << "vs" << _etag;
        _errorString = tr("We received a different E-Tag for resuming. Retrying next time.");
        _errorStatus = SyncFileItem::NormalError;
        abort();
        return;
    }

    bool ok;
    _contentLength = reply()->header(QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
    if (ok && _expectedContentLength != -1 && _contentLength != _expectedContentLength) {
        qCWarning(lcGetJob) << "We received a different content length than expected!"
                            << _expectedContentLength << "vs" << _contentLength;
        _errorString = tr("We received an unexpected download Content-Length.");
        _errorStatus = SyncFileItem::NormalError;
        abort();
        return;
    }

    qint64 start = 0;
    const QString ranges = QString::fromUtf8(reply()->rawHeader("Content-Range"));
    if (!ranges.isEmpty()) {
        static QRegularExpression rx(QStringLiteral("bytes (\\d+)-"));
        const auto match = rx.match(ranges);
        if (match.hasMatch()) {
            start = match.captured(1).toLongLong();
        }
    }
    if (start != _resumeStart) {
        qCWarning(lcGetJob) << "Wrong content-range: " << ranges << " while expecting start was" << _resumeStart;
        if (ranges.isEmpty()) {
            // device doesn't support range, just try again from scratch
            _device->close();
            if (!_device->open(QIODevice::WriteOnly)) {
                _errorString = _device->errorString();
                _errorStatus = SyncFileItem::NormalError;
                abort();
                return;
            }
            _resumeStart = 0;
        } else {
            _errorString = tr("Server returned wrong content-range");
            _errorStatus = SyncFileItem::NormalError;
            abort();
            return;
        }
    }

    auto lastModified = reply()->header(QNetworkRequest::LastModifiedHeader);
    if (!lastModified.isNull()) {
        _lastModified = Utility::qDateTimeToTime_t(lastModified.toDateTime());
    }
    _httpOk = true;
    connect(reply(), &QIODevice::readyRead, this, &GETFileJob::slotReadyRead);
}

void GETFileJob::setBandwidthManager(BandwidthManager *bwm)
{
    _bandwidthManager = bwm;
}

void GETFileJob::setChoked(bool c)
{
    if (c != _bandwidthChoked) {
        _bandwidthChoked = c;
        QMetaObject::invokeMethod(this, &GETFileJob::slotReadyRead, Qt::QueuedConnection);
    }
}

void GETFileJob::setBandwidthLimited(bool b)
{
    if (_bandwidthLimited != b) {
        _bandwidthLimited = b;
        QMetaObject::invokeMethod(this, &GETFileJob::slotReadyRead, Qt::QueuedConnection);
    }
}

void GETFileJob::giveBandwidthQuota(qint64 q)
{
    _bandwidthQuota = q;
    qCDebug(lcGetJob) << "Got" << q << "bytes";
    QMetaObject::invokeMethod(this, &GETFileJob::slotReadyRead, Qt::QueuedConnection);
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
    Q_ASSERT(reply());
    if (!_httpOk) {
        return;
    }

    const qint64 bufferSize = std::min<qint64>(1024 * 8, reply()->bytesAvailable());
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    while (reply()->bytesAvailable() > 0) {
        if (_bandwidthChoked) {
            qCWarning(lcGetJob) << "Download choked";
            break;
        }
        qint64 toRead = bufferSize;
        if (_bandwidthLimited) {
            toRead = std::min<qint64>(bufferSize, _bandwidthQuota);
            if (toRead == 0) {
                qCWarning(lcGetJob) << "Out of badnwidth quota";
                break;
            }
            _bandwidthQuota -= toRead;
        }

        const qint64 read = reply()->read(buffer.data(), toRead);
        if (read < 0) {
            _errorString = networkReplyErrorString(*reply());
            _errorStatus = SyncFileItem::NormalError;
            qCWarning(lcGetJob) << "Error while reading from device: " << _errorString;
            abort();
            return;
        }

        const qint64 written = _device->write(buffer.constData(), read);
        if (written != read) {
            _errorString = _device->errorString();
            _errorStatus = SyncFileItem::NormalError;
            qCWarning(lcGetJob) << "Error while writing to file" << written << read << _errorString;
            abort();
            return;
        }
    }
}


GETFileJob::~GETFileJob()
{
    if (_bandwidthManager) {
        _bandwidthManager->unregisterDownloadJob(this);
    }
}

QString GETFileJob::errorString() const
{
    if (!_errorString.isEmpty()) {
        return _errorString;
    }
    return AbstractNetworkJob::errorString();
}

void PropagateDownloadFile::start()
{
    if (propagator()->_abortRequested)
        return;

    qCDebug(lcPropagateDownload) << _item->_file << propagator()->_activeJobList.count();
    _stopwatch.start();

    auto &syncOptions = propagator()->syncOptions();
    auto &vfs = syncOptions._vfs;

    // For virtual files just dehydrate or create the file and be done
    if (_item->_type == ItemTypeVirtualFileDehydration) {
        const QString fsPath = propagator()->fullLocalPath(_item->_file);
        if (FileSystem::fileChanged(fsPath, _item->_previousSize, _item->_previousModtime)) {
            propagator()->_anotherSyncNeeded = true;
            done(SyncFileItem::SoftError, tr("File has changed since discovery"));
            return;
        }
        if (FileSystem::isFileLocked(fsPath, FileSystem::LockMode::Exclusive)) {
            Q_EMIT propagator()->seenLockedFile(fsPath, FileSystem::LockMode::Exclusive);
            done(SyncFileItem::SoftError, tr("Failed to free up space, the file %1 is currently in use").arg(fsPath));
            return;
        }
        qCDebug(lcPropagateDownload) << "dehydrating file" << _item->_file;
        updateMetadata(false);
        return;
    }
    if (vfs->mode() == Vfs::Off && _item->_type == ItemTypeVirtualFile) {
        qCWarning(lcPropagateDownload) << "ignored virtual file type of" << _item->_file;
        _item->_type = ItemTypeFile;
    }
    if (_item->_type == ItemTypeVirtualFile) {
        qCDebug(lcPropagateDownload) << "creating virtual file" << _item->_file;
        // do a klaas' case clash check.
        if (auto clash = propagator()->localFileNameClash(_item->_file)) {
            done(SyncFileItem::NormalError, tr("File %1 can not be downloaded because of a local file name clash with %2!").arg(QDir::toNativeSeparators(_item->_file), QDir::toNativeSeparators(clash.get())));
            return;
        }
        auto r = vfs->createPlaceholder(*_item);
        if (!r) {
            done(SyncFileItem::NormalError, r.error());
            return;
        }
        updateMetadata(false);
        return;
    }

    if (_deleteExisting) {
        deleteExistingFolder();

        // check for error with deletion
        if (_state == Finished) {
            return;
        }
    }

    // If we have a conflict where size of the file is unchanged,
    // compare the remote checksum to the local one.
    // Maybe it's not a real conflict and no download is necessary!
    // If the hashes are collision safe and identical, we assume the content is too.
    // For weak checksums, we only do that if the mtimes are also identical.

    const auto csync_is_collision_safe_hash = [](const QByteArray &checksum_header) {
        const bool safe = std::any_of(CheckSums::SafeAlgorithms.begin(), CheckSums::SafeAlgorithms.end(), [checksum_header = checksum_header.toUpper()](auto &it) {
            return checksum_header.startsWith(it.second.data());
        });
        if (!safe) {
            qWarning(lcPropagateDownload) << checksum_header << "is considered unsave";
            return false;
        }
        return true;
    };

    if (_item->_instruction == CSYNC_INSTRUCTION_CONFLICT
        && _item->_size == _item->_previousSize
        && !_item->_checksumHeader.isEmpty()
        && (csync_is_collision_safe_hash(_item->_checksumHeader)
            || _item->_modtime == _item->_previousModtime)) {
        qCDebug(lcPropagateDownload) << _item->_file << "may not need download, computing checksum";
        auto computeChecksum = new ComputeChecksum(this);
        const auto checksumHeader = ChecksumHeader::parseChecksumHeader(_item->_checksumHeader);
        computeChecksum->setChecksumType(checksumHeader.type());
        connect(computeChecksum, &ComputeChecksum::done,
            this, &PropagateDownloadFile::conflictChecksumComputed);
        propagator()->_activeJobList.append(this);
        computeChecksum->start(propagator()->fullLocalPath(_item->_file));
        return;
    }

    startDownload();
}

void PropagateDownloadFile::conflictChecksumComputed(CheckSums::Algorithm checksumType, const QByteArray &checksum)
{
    propagator()->_activeJobList.removeOne(this);
    const auto checksumHeader = ChecksumHeader::parseChecksumHeader(_item->_checksumHeader);
    if (checksumHeader == ChecksumHeader(checksumType, checksum)) {
        // No download necessary, just update fs and journal metadata
        qCDebug(lcPropagateDownload) << _item->_file << "remote and local checksum match";

        // Apply the server mtime locally if necessary, ensuring the journal
        // and local mtimes end up identical
        auto fn = propagator()->fullLocalPath(_item->_file);
        if (_item->_modtime != _item->_previousModtime) {
            FileSystem::setModTime(fn, _item->_modtime);
            emit propagator()->touchedFile(fn);
        }
        _item->_modtime = FileSystem::getModTime(fn);
        updateMetadata(/*isConflict=*/false);
        return;
    }
    startDownload();
}

void PropagateDownloadFile::startDownload()
{
    if (propagator()->_abortRequested)
        return;

    // do a klaas' case clash check.
    if (auto clash = propagator()->localFileNameClash(_item->_file)) {
        done(SyncFileItem::NormalError, tr("File %1 can not be downloaded because of a local file name clash with %2!").arg(QDir::toNativeSeparators(_item->_file), QDir::toNativeSeparators(clash.get())));
        return;
    }
    // If the file is locked, we want to retry this sync when it
    // becomes available again
    const auto targetFile = propagator()->fullLocalPath(_item->_file);
    if (FileSystem::isFileLocked(targetFile, FileSystem::LockMode::Exclusive)) {
        emit propagator()->seenLockedFile(targetFile, FileSystem::LockMode::Exclusive);
        done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(QDir::toNativeSeparators(_item->_file)));
        return;
    }
    propagator()->reportProgress(*_item, 0);

    QString tmpFileName;
    const SyncJournalDb::DownloadInfo progressInfo = propagator()->_journal->getDownloadInfo(_item->_file);
    if (progressInfo._valid) {
        // if the etag has changed meanwhile, remove the already downloaded part.
        if (progressInfo._etag != _item->_etag) {
            FileSystem::remove(propagator()->fullLocalPath(progressInfo._tmpfile));
            propagator()->_journal->setDownloadInfo(_item->_file, SyncJournalDb::DownloadInfo());
        } else {
            tmpFileName = progressInfo._tmpfile;
            _expectedEtagForResume = progressInfo._etag;
        }
    }

    if (tmpFileName.isEmpty()) {
        tmpFileName = createDownloadTmpFileName(_item->_file);
    }
    _tmpFile.setFileName(propagator()->fullLocalPath(tmpFileName));

    _resumeStart = _tmpFile.size();
    if (_resumeStart > 0 && _resumeStart == _item->_size) {
        qCInfo(lcPropagateDownload) << "File is already complete, no need to download";
        downloadFinished();
        return;
    }

    // Can't open(Append) read-only files, make sure to make
    // file writable if it exists.
    if (_tmpFile.exists())
        FileSystem::setFileReadOnly(_tmpFile.fileName(), false);
    if (!_tmpFile.open(QIODevice::Append | QIODevice::Unbuffered)) {
        qCWarning(lcPropagateDownload) << "could not open temporary file" << _tmpFile.fileName();
        done(SyncFileItem::NormalError, _tmpFile.errorString());
        return;
    }
    // Hide temporary after creation
    FileSystem::setFileHidden(_tmpFile.fileName(), true);

    // If there's not enough space to fully download this file, stop.
    const auto diskSpaceResult = propagator()->diskSpaceCheck();
    if (diskSpaceResult != OwncloudPropagator::DiskSpaceOk) {
        if (diskSpaceResult == OwncloudPropagator::DiskSpaceFailure) {
            // Using DetailError here will make the error not pop up in the account
            // tab: instead we'll generate a general "disk space low" message and show
            // these detail errors only in the error view.
            done(SyncFileItem::DetailError,
                tr("The download would reduce free local disk space below the limit"));
            emit propagator()->insufficientLocalStorage();
        } else if (diskSpaceResult == OwncloudPropagator::DiskSpaceCritical) {
            done(SyncFileItem::FatalError,
                tr("Free space on disk is less than %1").arg(Utility::octetsToString(criticalFreeSpaceLimit())));
        }

        // Remove the temporary, if empty.
        if (_resumeStart == 0) {
            _tmpFile.remove();
        }

        return;
    }

    {
        SyncJournalDb::DownloadInfo pi;
        pi._etag = _item->_etag;
        pi._tmpfile = tmpFileName;
        pi._valid = true;
        propagator()->_journal->setDownloadInfo(_item->_file, pi);
        propagator()->_journal->commit(QStringLiteral("download file start"));
    }

    startFullDownload();
}

void PropagateDownloadFile::startFullDownload()
{
    QMap<QByteArray, QByteArray> headers;

    if (_item->_directDownloadUrl.isEmpty()) {
        // Normal job, download from oC instance
        _job = new GETFileJob(propagator()->account(), propagator()->webDavUrl(),
            propagator()->fullRemotePath(_item->_file),
            &_tmpFile, headers, _expectedEtagForResume, _resumeStart, this);
    } else {
        // We were provided a direct URL, use that one
        qCInfo(lcPropagateDownload) << "directDownloadUrl given for " << _item->_file << _item->_directDownloadUrl;

        if (!_item->_directDownloadCookies.isEmpty()) {
            headers["Cookie"] = _item->_directDownloadCookies.toUtf8();
        }

        QUrl url = QUrl::fromUserInput(_item->_directDownloadUrl);
        _job = new GETFileJob(propagator()->account(),
            url,
            {},
            &_tmpFile, headers, _expectedEtagForResume, _resumeStart, this);
    }
    _job->setBandwidthManager(propagator()->_bandwidthManager);
    connect(_job.data(), &GETFileJob::finishedSignal, this, &PropagateDownloadFile::slotGetFinished);
    connect(qobject_cast<GETFileJob *>(_job.data()), &GETFileJob::downloadProgress,
        this, &PropagateDownloadFile::slotDownloadProgress);
    propagator()->_activeJobList.append(this);
    _job->start();
}

qint64 PropagateDownloadFile::committedDiskSpace() const
{
    if (_state == Running) {
        return qBound(0LL, _item->_size - _resumeStart - _downloadProgress, _item->_size);
    }
    return 0;
}

void PropagateDownloadFile::setDeleteExistingFolder(bool enabled)
{
    _deleteExisting = enabled;
}

const char owncloudCustomSoftErrorStringC[] = "owncloud-custom-soft-error-string";
void PropagateDownloadFile::slotGetFinished()
{
    propagator()->_activeJobList.removeOne(this);

    GETFileJob *job = _job;
    OC_ASSERT(job);

    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = job->responseTimestamp();
    _item->_requestId = job->requestId();

    QNetworkReply::NetworkError err = job->reply()->error();
    if (err != QNetworkReply::NoError) {

        // If we sent a 'Range' header and get 416 back, we want to retry
        // without the header.
        const bool badRangeHeader = job->resumeStart() > 0 && _item->_httpErrorCode == 416;
        if (badRangeHeader) {
            qCWarning(lcPropagateDownload) << "server replied 416 to our range request, trying again without";
            propagator()->_anotherSyncNeeded = true;
        }

        // Getting a 404 probably means that the file was deleted on the server.
        const bool fileNotFound = _item->_httpErrorCode == 404;
        if (fileNotFound) {
            qCWarning(lcPropagateDownload) << "server replied 404, assuming file was deleted";
        }

        // Don't keep the temporary file if it is empty or we
        // used a bad range header or the file's not on the server anymore.
        if (_tmpFile.exists() && (_tmpFile.size() == 0 || badRangeHeader || fileNotFound)) {
            _tmpFile.close();
            FileSystem::remove(_tmpFile.fileName());
            propagator()->_journal->setDownloadInfo(_item->_file, SyncJournalDb::DownloadInfo());
        }

        if (!_item->_directDownloadUrl.isEmpty() && err != QNetworkReply::OperationCanceledError) {
            // If this was with a direct download, retry without direct download
            qCWarning(lcPropagateDownload) << "Direct download of" << _item->_directDownloadUrl << "failed. Retrying through owncloud.";
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

            // As a precaution against bugs that cause our database and the
            // reality on the server to diverge, rediscover this folder on the
            // next sync run.
            propagator()->_journal->schedulePathForRemoteDiscovery(_item->_file);
        }

        QByteArray errorBody;
        QString errorString = _item->_httpErrorCode >= 400 ? job->errorStringParsingBody(&errorBody)
                                                           : job->errorString();
        SyncFileItem::Status status = job->errorStatus();
        if (status == SyncFileItem::NoStatus) {
            status = classifyError(err, _item->_httpErrorCode,
                &propagator()->_anotherSyncNeeded, errorBody);
        }

        done(status, errorString);
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

    _tmpFile.close();
    _tmpFile.flush();

    /* Check that the size of the GET reply matches the file size. There have been cases
     * reported that if a server breaks behind a proxy, the GET is still a 200 but is
     * truncated, as described here: https://github.com/owncloud/mirall/issues/2528
     */
    const QByteArray sizeHeader("Content-Length");
    qint64 bodySize = job->reply()->rawHeader(sizeHeader).toLongLong();
    bool hasSizeHeader = !job->reply()->rawHeader(sizeHeader).isEmpty();

    // Qt removes the content-length header for transparently decompressed HTTP1 replies
    // but not for HTTP2 or SPDY replies. For these it remains and contains the size
    // of the compressed data. See QTBUG-73364.
    const auto contentEncoding = job->reply()->rawHeader("content-encoding").toLower();
    if ((contentEncoding == "gzip" || contentEncoding == "deflate")
        && (job->reply()->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool()
            || job->reply()->attribute(QNetworkRequest::SpdyWasUsedAttribute).toBool())) {
        bodySize = 0;
        hasSizeHeader = false;
    }

    if (hasSizeHeader && _tmpFile.size() > 0 && bodySize == 0) {
        // Strange bug with broken webserver or webfirewall https://github.com/owncloud/client/issues/3373#issuecomment-122672322
        // This happened when trying to resume a file. The Content-Range header was files, Content-Length was == 0
        qCDebug(lcPropagateDownload) << bodySize << _item->_size << _tmpFile.size() << job->resumeStart();
        FileSystem::remove(_tmpFile.fileName());
        done(SyncFileItem::SoftError, tr("Broken webserver returned empty content length for non-empty file on resume"));
        return;
    }

    if (bodySize > 0 && (bodySize != (_tmpFile.size() - job->resumeStart()))) {
        qCDebug(lcPropagateDownload) << bodySize << "!=" << (_tmpFile.size() - job->resumeStart()) << _tmpFile.size() << job->resumeStart();
        propagator()->_anotherSyncNeeded = true;
        done(SyncFileItem::SoftError, tr("The file could not be downloaded completely."));
        return;
    }

    if (_tmpFile.size() == 0 && _item->_size > 0) {
        FileSystem::remove(_tmpFile.fileName());
        done(SyncFileItem::NormalError,
            tr("The downloaded file is empty despite the server announced it should have been %1.")
                .arg(Utility::octetsToString(_item->_size)));
        return;
    }

    // Did the file come with conflict headers? If so, store them now!
    // If we download conflict files but the server doesn't send conflict
    // headers, the record will be established by SyncEngine::conflictRecordMaintenance.
    // (we can't reliably determine the file id of the base file here,
    // it might still be downloaded in a parallel job and not exist in
    // the database yet!)
    if (job->reply()->rawHeader("OC-Conflict") == "1") {
        _conflictRecord.path = _item->_file.toUtf8();
        _conflictRecord.initialBasePath = job->reply()->rawHeader("OC-ConflictInitialBasePath");
        _conflictRecord.baseFileId = job->reply()->rawHeader("OC-ConflictBaseFileId");
        _conflictRecord.baseEtag = job->reply()->rawHeader("OC-ConflictBaseEtag");

        auto mtimeHeader = job->reply()->rawHeader("OC-ConflictBaseMtime");
        if (!mtimeHeader.isEmpty())
            _conflictRecord.baseModtime = mtimeHeader.toLongLong();

        // We don't set it yet. That will only be done when the download finished
        // successfully, much further down. Here we just grab the headers because the
        // job will be deleted later.
    }

    // Do checksum validation for the download. If there is no checksum header, the validator
    // will also emit the validated() signal to continue the flow in slot transmissionChecksumValidated()
    // as this is (still) also correct.
    ValidateChecksumHeader *validator = new ValidateChecksumHeader(this);
    connect(validator, &ValidateChecksumHeader::validated,
        this, &PropagateDownloadFile::transmissionChecksumValidated);
    connect(validator, &ValidateChecksumHeader::validationFailed,
        this, &PropagateDownloadFile::slotChecksumFail);
    auto checksumHeader = findBestChecksum(job->reply()->rawHeader(checkSumHeaderC));
    auto contentMd5Header = job->reply()->rawHeader(contentMd5HeaderC);
    if (checksumHeader.isEmpty() && !contentMd5Header.isEmpty())
        checksumHeader = "MD5:" + contentMd5Header;
    validator->start(_tmpFile.fileName(), checksumHeader);
}

void PropagateDownloadFile::slotChecksumFail(const QString &errMsg)
{
    FileSystem::remove(_tmpFile.fileName());
    propagator()->_anotherSyncNeeded = true;
    done(SyncFileItem::SoftError, errMsg); // tr("The file downloaded with a broken checksum, will be redownloaded."));
}

void PropagateDownloadFile::deleteExistingFolder()
{
    QString existingDir = propagator()->fullLocalPath(_item->_file);
    if (!QFileInfo(existingDir).isDir()) {
        return;
    }

    // Delete the directory if it is empty!
    QDir dir(existingDir);
    if (dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).count() == 0) {
        if (dir.rmdir(existingDir)) {
            return;
        }
        // on error, just try to move it away...
    }

    QString error;
    if (!propagator()->createConflict(_item, _associatedComposite, &error)) {
        done(SyncFileItem::NormalError, error);
    }
}

namespace { // Anonymous namespace for the recall feature
    static QString makeRecallFileName(const QString &fn)
    {
        QString recallFileName(fn);
        // Add _recall-XXXX  before the extension.
        int dotLocation = recallFileName.lastIndexOf(QLatin1Char('.'));
        // If no extension, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
        if (dotLocation <= recallFileName.lastIndexOf(QLatin1Char('/')) + 1) {
            dotLocation = recallFileName.size();
        }

        QString timeString = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-hhmmss"));
        recallFileName.insert(dotLocation, QStringLiteral("_.sys.admin#recall#-") + timeString);

        return recallFileName;
    }

    void handleRecallFile(const QString &filePath, const QString &folderPath, SyncJournalDb &journal)
    {
        qCDebug(lcPropagateDownload) << "handleRecallFile: " << filePath;

        FileSystem::setFileHidden(filePath, true);

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qCWarning(lcPropagateDownload) << "Could not open recall file" << file.errorString();
            return;
        }
        QFileInfo existingFile(filePath);
        QDir baseDir = existingFile.dir();

        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            line.chop(1); // remove trailing \n

            QString recalledFile = QDir::cleanPath(baseDir.filePath(QString::fromUtf8(line)));
            if (!recalledFile.startsWith(folderPath) || !recalledFile.startsWith(baseDir.path())) {
                qCWarning(lcPropagateDownload) << "Ignoring recall of " << recalledFile;
                continue;
            }

            // Path of the recalled file in the local folder
            QString localRecalledFile = recalledFile.mid(folderPath.size());

            SyncJournalFileRecord record;
            if (!journal.getFileRecord(localRecalledFile, &record) || !record.isValid()) {
                qCWarning(lcPropagateDownload) << "No db entry for recall of" << localRecalledFile;
                continue;
            }

            qCInfo(lcPropagateDownload) << "Recalling" << localRecalledFile << "Checksum:" << record._checksumHeader;

            QString targetPath = makeRecallFileName(recalledFile);

            qCDebug(lcPropagateDownload) << "Copy recall file: " << recalledFile << " -> " << targetPath;
            // Remove the target first, QFile::copy will not overwrite it.
            FileSystem::remove(targetPath);
            QFile::copy(recalledFile, targetPath);
        }
    }

    static void preserveGroupOwnership(const QString &fileName, const QFileInfo &fi)
    {
#ifdef Q_OS_UNIX
        if (chown(fileName.toLocal8Bit().constData(), -1, fi.groupId()) != 0) {
            qCWarning(lcPropagateDownload) << "Unable to chown" << fileName << "to previous group owner" << strerror(errno);
        }
#else
        Q_UNUSED(fileName);
        Q_UNUSED(fi);
#endif
    }
} // end namespace

void PropagateDownloadFile::transmissionChecksumValidated(CheckSums::Algorithm checksumType, const QByteArray &checksum)
{
    const CheckSums::Algorithm theContentChecksumType = propagator()->account()->capabilities().preferredUploadChecksumType();

    // Reuse transmission checksum as content checksum.
    //
    // We could do this more aggressively and accept both MD5 and SHA1
    // instead of insisting on the exactly correct checksum type.
    if (theContentChecksumType == checksumType || theContentChecksumType != CheckSums::Algorithm::PARSE_ERROR) {
        return contentChecksumComputed(checksumType, checksum);
    }

    // Compute the content checksum.
    auto computeChecksum = new ComputeChecksum(this);
    computeChecksum->setChecksumType(theContentChecksumType);

    connect(computeChecksum, &ComputeChecksum::done,
        this, &PropagateDownloadFile::contentChecksumComputed);
    computeChecksum->start(_tmpFile.fileName());
}

void PropagateDownloadFile::contentChecksumComputed(CheckSums::Algorithm checksumType, const QByteArray &checksum)
{
    _item->_checksumHeader = ChecksumHeader(checksumType, checksum).makeChecksumHeader();

    downloadFinished();
}

void PropagateDownloadFile::downloadFinished()
{
    OC_ASSERT(!_tmpFile.isOpen());
    const QString fn = propagator()->fullLocalPath(_item->destination());

    // In case of file name clash, report an error
    // This can happen if another parallel download saved a clashing file.
    if (auto clash = propagator()->localFileNameClash(_item->_file)) {
        done(SyncFileItem::NormalError, tr("File %1 cannot be saved because of a local file name clash with %2!").arg(QDir::toNativeSeparators(_item->_file), QDir::toNativeSeparators(clash.get())));
        return;
    }

    FileSystem::setModTime(_tmpFile.fileName(), _item->_modtime);
    // We need to fetch the time again because some file systems such as FAT have worse than a second
    // Accuracy, and we really need the time from the file system. (#3103)
    _item->_modtime = FileSystem::getModTime(_tmpFile.fileName());

    bool previousFileExists = FileSystem::fileExists(fn);
    if (previousFileExists) {
        // Preserve the existing file permissions.
        QFileInfo existingFile(fn);
        if (existingFile.permissions() != _tmpFile.permissions()) {
            _tmpFile.setPermissions(existingFile.permissions());
        }
        preserveGroupOwnership(_tmpFile.fileName(), existingFile);

        // Make the file a hydrated placeholder if possible
        const auto result = propagator()->updatePlaceholder(*_item, _tmpFile.fileName(), fn);
        if (!result) {
            done(SyncFileItem::NormalError, result.error());
            return;
        } else if (result.get() == Vfs::ConvertToPlaceholderResult::Locked) {
            done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(_item->_file));
            return;
        }
    }

    bool isConflict = _item->_instruction == CSYNC_INSTRUCTION_CONFLICT
        && (QFileInfo(fn).isDir() || !FileSystem::fileEquals(fn, _tmpFile.fileName()));
    if (isConflict) {
        QString error;
        if (!propagator()->createConflict(_item, _associatedComposite, &error)) {
            done(SyncFileItem::SoftError, error);
            return;
        }
        previousFileExists = false;
    }

    if (previousFileExists) {
        // Check whether the existing file has changed since the discovery
        // phase by comparing size and mtime to the previous values. This
        // is necessary to avoid overwriting user changes that happened between
        // the discovery phase and now.
        const qint64 expectedSize = _item->_previousSize;
        const time_t expectedMtime = _item->_previousModtime;
        if (FileSystem::fileChanged(fn, expectedSize, expectedMtime)) {
            propagator()->_anotherSyncNeeded = true;
            done(SyncFileItem::SoftError, tr("File has changed since discovery"));
            return;
        }
    }
    // If the file is locked, we want to retry this sync when it
    // becomes available again
    if (FileSystem::isFileLocked(fn, FileSystem::LockMode::Exclusive)) {
        emit propagator()->seenLockedFile(fn, FileSystem::LockMode::Exclusive);
        done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(fn));
        return;
    }

    QString error;
    emit propagator()->touchedFile(fn);
    // The fileChanged() check is done above to generate better error messages.
    if (!FileSystem::uncheckedRenameReplace(_tmpFile.fileName(), fn, &error)) {
        qCWarning(lcPropagateDownload) << "Rename failed:" << _tmpFile.fileName() << "=>" << fn << "with error:" << error;
        propagator()->_anotherSyncNeeded = true;
        done(SyncFileItem::SoftError, error);
        return;
    }

    FileSystem::setFileHidden(fn, false);

    // Maybe we downloaded a newer version of the file than we thought we would...
    // Get up to date information for the journal.
    _item->_size = FileSystem::getSize(fn);

    // Maybe what we downloaded was a conflict file? If so, set a conflict record.
    // (the data was prepared in slotGetFinished above)
    if (_conflictRecord.isValid())
        propagator()->_journal->setConflictRecord(_conflictRecord);

    auto vfs = propagator()->syncOptions()._vfs;
    if (vfs && vfs->mode() == Vfs::WithSuffix) {
        // TODO: move to the suffix plugin
        // If the virtual file used to have a different name and db
        // entry, remove it transfer its old pin state.
        if (_item->_type == ItemTypeVirtualFileDownload) {
            const QString virtualFile = _item->_file + vfs->fileSuffix();
            const QString virtualFileAbsPath = propagator()->fullLocalPath(virtualFile);
            qCDebug(lcPropagateDownload) << "Download of previous virtual file finished" << virtualFileAbsPath;
            if (QFileInfo::exists(virtualFileAbsPath)) {
                if (!FileSystem::remove(virtualFileAbsPath, &error)) {
                    done(SyncFileItem::FatalError, error);
                    return;
                }
            }
            OC_ASSERT(propagator()->_journal->deleteFileRecord(virtualFile));
            // Move the pin state to the new location
            auto pin = propagator()->_journal->internalPinStates().rawForPath(virtualFile.toUtf8());
            if (pin && *pin != PinState::Inherited) {
                vfs->setPinState(_item->_file, *pin);
                vfs->setPinState(virtualFile, PinState::Inherited);
            }
        }

        // Ensure the pin state isn't contradictory
        auto pin = vfs->pinState(_item->_file);
        if (pin && *pin == PinState::OnlineOnly)
            vfs->setPinState(_item->_file, PinState::Unspecified);
    }

    updateMetadata(isConflict);
}

void PropagateDownloadFile::updateMetadata(bool isConflict)
{
    const auto result = propagator()->updateMetadata(*_item);
    if (!result) {
        done(SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()));
        return;
    } else if (result.get() == Vfs::ConvertToPlaceholderResult::Locked) {
        done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(_item->_file));
        return;
    }
    propagator()->_journal->setDownloadInfo(_item->_file, SyncJournalDb::DownloadInfo());
    propagator()->_journal->commit(QStringLiteral("download file start2"));

    done(isConflict ? SyncFileItem::Conflict : SyncFileItem::Success);

    // handle the special recall file
    if (!_item->_remotePerm.hasPermission(RemotePermissions::IsShared)
        && (_item->_file == QLatin1String(".sys.admin#recall#")
               || _item->_file.endsWith(QLatin1String("/.sys.admin#recall#")))) {
        const QString fn = propagator()->fullLocalPath(_item->destination());
        handleRecallFile(fn, propagator()->localPath(), *propagator()->_journal);
    }

    const auto duration = std::chrono::milliseconds(_stopwatch.elapsed());
    if (isLikelyFinishedQuickly() && duration > 5s) {
        qCWarning(lcPropagateDownload) << "WARNING: Unexpectedly slow connection, took" << duration.count() << "ms for" << _item->_size - _resumeStart << "bytes for" << _item->_file;
    }
}

void PropagateDownloadFile::slotDownloadProgress(qint64 received, qint64)
{
    if (!_job)
        return;
    _downloadProgress = received;

    propagator()->reportProgress(*_item, _resumeStart + received);
}


void PropagateDownloadFile::abort(PropagatorJob::AbortType abortType)
{
    if (_job) {
        _job->abort();
    }
    if (abortType == AbortType::Asynchronous) {
        emit abortFinished();
    }
}
}
