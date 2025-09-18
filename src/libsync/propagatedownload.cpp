/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include <common/asserts.h>
#include <common/constants.h>
#include "clientsideencryptionjobs.h"
#include "propagatedownloadencrypted.h"
#include "common/vfs.h"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QDir>

#include <cmath>

namespace OCC {

static constexpr auto CustomDecompressedSafetyCheckThreshold = 20 * 1024 * 1024;

Q_LOGGING_CATEGORY(lcGetJob, "nextcloud.sync.networkjob.get", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPropagateDownload, "nextcloud.sync.propagator.download", QtInfoMsg)

// Always coming in with forward slashes.
// In csync_excluded_no_ctx we ignore all files with longer than 254 chars
// This function also adds a dot at the beginning of the filename to hide the file on OS X and Linux
QString OWNCLOUDSYNC_EXPORT createDownloadTmpFileName(const QString &previous)
{
    QString tmpFileName;
    QString tmpPath;
    int slashPos = previous.lastIndexOf('/');
    // work with both pathed filenames and only filenames
    if (slashPos == -1) {
        tmpFileName = previous;
        tmpPath = QString();
    } else {
        tmpFileName = previous.mid(slashPos + 1);
        tmpPath = previous.left(slashPos);
    }
    int overhead = 1 + 1 + 2 + 8; // slash dot dot-tilde ffffffff"
    int spaceForFileName = qMin(254, tmpFileName.length() + overhead) - overhead;
    if (tmpPath.length() > 0) {
        return tmpPath + '/' + '.' + tmpFileName.left(spaceForFileName) + ".~" + (QString::number(uint(Utility::rand() % 0xFFFFFFFF), 16));
    } else {
        return '.' + tmpFileName.left(spaceForFileName) + ".~" + (QString::number(uint(Utility::rand() % 0xFFFFFFFF), 16));
    }
}

// DOES NOT take ownership of the device.
GETFileJob::GETFileJob(AccountPtr account, const QString &path, QIODevice *device,
    const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
    qint64 resumeStart, QObject *parent)
    : AbstractNetworkJob(account, path, parent)
    , _device(device)
    , _headers(headers)
    , _expectedEtagForResume(expectedEtagForResume)
    , _expectedContentLength(-1)
    , _resumeStart(resumeStart)
    , _errorStatus(SyncFileItem::NoStatus)
    , _bandwidthLimited(false)
    , _bandwidthChoked(false)
    , _bandwidthQuota(0)
    , _bandwidthManager(nullptr)
    , _hasEmittedFinishedSignal(false)
    , _lastModified()
    , _contentLength(-1)
{
}

GETFileJob::GETFileJob(AccountPtr account, const QUrl &url, QIODevice *device,
    const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
    qint64 resumeStart, QObject *parent)
    : AbstractNetworkJob(account, url.toEncoded(), parent)
    , _device(device)
    , _headers(headers)
    , _expectedEtagForResume(expectedEtagForResume)
    , _expectedContentLength(-1)
    , _resumeStart(resumeStart)
    , _errorStatus(SyncFileItem::NoStatus)
    , _directDownloadUrl(url)
    , _bandwidthLimited(false)
    , _bandwidthChoked(false)
    , _bandwidthQuota(0)
    , _bandwidthManager(nullptr)
    , _hasEmittedFinishedSignal(false)
    , _lastModified()
    , _contentLength(-1)
{
}


void GETFileJob::start()
{
    if (_resumeStart > 0) {
        _headers["Range"] = "bytes=" + QByteArray::number(_resumeStart) + '-';
        _headers["Accept-Ranges"] = "bytes";
        qCDebug(lcGetJob) << "Retry with range " << _headers["Range"];
    }

    QNetworkRequest req;
    for (QMap<QByteArray, QByteArray>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }

    req.setPriority(QNetworkRequest::LowPriority); // Long downloads must not block non-propagation jobs.
    req.setDecompressedSafetyCheckThreshold(CustomDecompressedSafetyCheckThreshold);

    if (_directDownloadUrl.isEmpty()) {
        sendRequest("GET", makeDavUrl(path()), req);
    } else {
        // Use direct URL
        sendRequest("GET", _directDownloadUrl, req);
    }

    qCDebug(lcGetJob) << _bandwidthManager << _bandwidthChoked << _bandwidthLimited;
    if (_bandwidthManager) {
        _bandwidthManager->registerDownloadJob(this);
    }

    connect(this, &AbstractNetworkJob::networkActivity, account().data(), &Account::propagatorNetworkActivity);

    AbstractNetworkJob::start();
}

void GETFileJob::newReplyHook(QNetworkReply *reply)
{
    reply->setReadBufferSize(16 * 1024); // keep low so we can easier limit the bandwidth

    connect(reply, &QNetworkReply::metaDataChanged, this, &GETFileJob::slotMetaDataChanged);
    connect(reply, &QIODevice::readyRead, this, &GETFileJob::slotReadyRead);
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
        // Redirects and auth failures (token renew) are handled by AbstractNetworkJob and
        // will end up restarting the job. We do not want to process further data from the initial
        // request. newReplyHook() will reestablish signal connections for the follow-up request.
        bool ok = disconnect(reply(), &QNetworkReply::finished, this, &GETFileJob::slotReadyRead)
            && disconnect(reply(), &QNetworkReply::readyRead, this, &GETFileJob::slotReadyRead);
        ASSERT(ok);
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

    if (!_directDownloadUrl.isEmpty() && !_etag.isEmpty()) {
        qCInfo(lcGetJob) << "Direct download used, ignoring server ETag" << _etag;
        _etag = QByteArray(); // reset received ETag
    } else if (!_directDownloadUrl.isEmpty()) {
        // All fine, ETag empty and directDownloadUrl used
    } else if (_etag.isEmpty()) {
        qCWarning(lcGetJob) << "No E-Tag reply by server, considering it invalid";
        _errorString = tr("No E-Tag received from server, check Proxy/Gateway");
        _errorStatus = SyncFileItem::NormalError;
        reply()->abort();
        return;
    } else if (!_expectedEtagForResume.isEmpty() && _expectedEtagForResume != _etag) {
        qCWarning(lcGetJob) << "We received a different E-Tag for resuming!"
                            << _expectedEtagForResume << "vs" << _etag;
        _errorString = tr("We received a different E-Tag for resuming. Retrying next time.");
        _errorStatus = SyncFileItem::NormalError;
        reply()->abort();
        return;
    }

    bool ok = false;
    _contentLength = reply()->header(QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
    if (ok && _expectedContentLength != -1 && _contentLength != _expectedContentLength) {
        qCWarning(lcGetJob) << "We received a different content length than expected!"
                            << _expectedContentLength << "vs" << _contentLength;
        _errorString = tr("We received an unexpected download Content-Length.");
        _errorStatus = SyncFileItem::NormalError;
        reply()->abort();
        return;
    }

    qint64 start = 0;
    QByteArray ranges = reply()->rawHeader("Content-Range");
    if (!ranges.isEmpty()) {
        static const QRegularExpression rx("bytes (\\d+)-");
        const auto rxMatch = rx.match(ranges);
        if (rxMatch.hasMatch()) {
            start = rxMatch.captured(1).toLongLong();
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

    _saveBodyToFile = true;
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
    qCDebug(lcGetJob) << "Got" << q << "bytes";
    QMetaObject::invokeMethod(this, "slotReadyRead", Qt::QueuedConnection);
}

qint64 GETFileJob::currentDownloadPosition()
{
    if (_device && _device->pos() > 0 && _device->pos() > qint64(_resumeStart)) {
        return _device->pos();
    }
    return _resumeStart;
}

qint64 GETFileJob::writeToDevice(const QByteArray &data)
{
    return _device->write(data);
}

void GETFileJob::slotReadyRead()
{
    if (!reply())
        return;
    int bufferSize = qMin(1024 * 8ll, reply()->bytesAvailable());
    QByteArray buffer(bufferSize, Qt::Uninitialized);

    while (reply()->bytesAvailable() > 0 && _saveBodyToFile) {
        if (_bandwidthChoked) {
            qCWarning(lcGetJob) << "Download choked";
            break;
        }
        qint64 toRead = bufferSize;
        if (_bandwidthLimited) {
            toRead = qMin(qint64(bufferSize), _bandwidthQuota);
            if (toRead == 0) {
                qCDebug(lcGetJob) << "Out of quota";
                break;
            }
            _bandwidthQuota -= toRead;
        }

        const qint64 readBytes = reply()->read(buffer.data(), toRead);
        if (readBytes < 0) {
            _errorString = networkReplyErrorString(*reply());
            _errorStatus = SyncFileItem::NormalError;
            qCWarning(lcGetJob) << "Error while reading from device: " << _errorString;
            reply()->abort();
            return;
        }

        const qint64 writtenBytes = writeToDevice(buffer.left(readBytes));
        if (writtenBytes != readBytes) {
            _errorString = _device->errorString();
            _errorStatus = SyncFileItem::NormalError;
            qCWarning(lcGetJob) << "Error while writing to file" << writtenBytes << readBytes << _errorString;
            reply()->abort();
            return;
        }
    }

    if (reply()->isFinished() && (reply()->bytesAvailable() == 0 || !_saveBodyToFile)) {
        qCDebug(lcGetJob) << "Get file job finished bytesAvailable/_saveBodyToFile:" << reply()->bytesAvailable() << "/" << _saveBodyToFile ;
        if (_bandwidthManager) {
            _bandwidthManager->unregisterDownloadJob(this);
        }
        if (!_hasEmittedFinishedSignal) {
            qCInfo(lcGetJob) << "GET of" << reply()->request().url().toString() << "FINISHED WITH STATUS"
                             << replyStatusString()
                             << reply()->rawHeader("Content-Range") << reply()->rawHeader("Content-Length");

            emit finishedSignal();
        }
        _hasEmittedFinishedSignal = true;
        deleteLater();
    }
}

void GETFileJob::cancel()
{
    const auto networkReply = reply();
    if (networkReply && networkReply->isRunning()) {
        networkReply->abort();
    }
    if (_device && _device->isOpen()) {
        _device->close();
    }
}

void GETFileJob::onTimedOut()
{
    qCWarning(lcGetJob) << "Timeout" << (reply() ? reply()->request().url() : path());
    if (!reply())
        return;
    _errorString = tr("Connection Timeout");
    _errorStatus = SyncFileItem::FatalError;
    reply()->abort();
}

QString GETFileJob::errorString() const
{
    if (!_errorString.isEmpty()) {
        return _errorString;
    }
    return AbstractNetworkJob::errorString();
}

GETEncryptedFileJob::GETEncryptedFileJob(AccountPtr account, const QString &path, QIODevice *device,
    const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
    qint64 resumeStart, FolderMetadata::EncryptedFile encryptedInfo, QObject *parent)
    : GETFileJob(account, path, device, headers, expectedEtagForResume, resumeStart, parent)
    , _encryptedFileInfo(encryptedInfo)
{
}

GETEncryptedFileJob::GETEncryptedFileJob(AccountPtr account, const QUrl &url, QIODevice *device,
    const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
    qint64 resumeStart, FolderMetadata::EncryptedFile encryptedInfo, QObject *parent)
    : GETFileJob(account, url, device, headers, expectedEtagForResume, resumeStart, parent)
    , _encryptedFileInfo(encryptedInfo)
{
}

qint64 GETEncryptedFileJob::writeToDevice(const QByteArray &data)
{
    if (!_decryptor) {
        // only initialize the decryptor once, because, according to Qt documentation, metadata might get changed during the processing of the data sometimes
        // https://doc.qt.io/qt-5/qnetworkreply.html#metaDataChanged
        _decryptor.reset(new EncryptionHelper::StreamingDecryptor(_encryptedFileInfo.encryptionKey, _encryptedFileInfo.initializationVector, _contentLength));
    }

    if (!_decryptor->isInitialized()) {
        return -1;
    }

    const auto bytesRemaining = _contentLength - _processedSoFar - data.length();

    if (bytesRemaining != 0 && bytesRemaining < OCC::Constants::e2EeTagSize) {
        // decryption is going to fail if last chunk does not include or does not equal to OCC::Constants::e2EeTagSize bytes tag
        // we may end up receiving packets beyond OCC::Constants::e2EeTagSize bytes tag at the end
        // in that case, we don't want to try and decrypt less than OCC::Constants::e2EeTagSize ending bytes of tag, we will accumulate all the incoming data till the end
        // and then, we are going to decrypt the entire chunk containing OCC::Constants::e2EeTagSize bytes at the end
        _pendingBytes += QByteArray(data.constData(), data.length());
        _processedSoFar += data.length();
        if (_processedSoFar != _contentLength) {
            return data.length();
        }
    }

    if (!_pendingBytes.isEmpty()) {
        const auto decryptedChunk = _decryptor->chunkDecryption(_pendingBytes.constData(), _pendingBytes.size());

        if (decryptedChunk.isEmpty()) {
            qCCritical(lcPropagateDownload) << "Decryption failed!";
            return -1;
        }

        GETFileJob::writeToDevice(decryptedChunk);

        return data.length();
    }

    const auto decryptedChunk = _decryptor->chunkDecryption(data.constData(), data.length());

    if (decryptedChunk.isEmpty()) {
        qCCritical(lcPropagateDownload) << "Decryption failed!";
        return -1;
    }

    GETFileJob::writeToDevice(decryptedChunk);

    _processedSoFar += data.length();

    return data.length();
}

void PropagateDownloadFile::start()
{
    if (propagator()->_abortRequested)
        return;
    _isEncrypted = false;

    qCDebug(lcPropagateDownload) << _item->_file << propagator()->_activeJobList.count();

    const auto path = _item->_file;
    const auto slashPosition = path.lastIndexOf('/');
    const auto parentPath = slashPosition >= 0 ? path.left(slashPosition) : QString();

    SyncJournalFileRecord parentRec;
    if (!propagator()->_journal->getFileRecord(parentPath, &parentRec)) {
        qCWarning(lcPropagateDownload) << "Could not get file from local DB" << parentPath;
        done(SyncFileItem::NormalError, tr("Could not get file %1 from local DB").arg(parentPath), ErrorCategory::GenericError);
        return;
    }

    const auto account = propagator()->account();
    if (!account->capabilities().clientSideEncryptionAvailable() ||
        !parentRec.isValid() ||
        !parentRec.isE2eEncrypted()) {
        startAfterIsEncryptedIsChecked();
    } else {
        _downloadEncryptedHelper = new PropagateDownloadEncrypted(propagator(), parentPath, _item, this);
        connect(_downloadEncryptedHelper, &PropagateDownloadEncrypted::fileMetadataFound, [this] {
          _isEncrypted = true;
          startAfterIsEncryptedIsChecked();
        });
        connect(_downloadEncryptedHelper, &PropagateDownloadEncrypted::failed, [this] {
          done(SyncFileItem::NormalError,
               tr("File %1 cannot be downloaded because encryption information is missing.").arg(QDir::toNativeSeparators(_item->_file)), ErrorCategory::GenericError);
        });
        _downloadEncryptedHelper->start();
    }
}

void PropagateDownloadFile::startAfterIsEncryptedIsChecked()
{
    _stopwatch.start();

    auto &syncOptions = propagator()->syncOptions();
    auto &vfs = syncOptions._vfs;

    // For virtual files just dehydrate or create the file and be done
    if (_item->_type == ItemTypeVirtualFileDehydration) {
        const auto fsPath = propagator()->fullLocalPath(_item->_file);
        makeParentFolderModifiable(fsPath);
        if (!FileSystem::verifyFileUnchanged(fsPath, _item->_previousSize, _item->_previousModtime)) {
            propagator()->_anotherSyncNeeded = true;
            done(SyncFileItem::SoftError, tr("File has changed since discovery"), ErrorCategory::GenericError);
            return;
        }

        qCDebug(lcPropagateDownload) << "dehydrating file" << _item->_file;
        if (FileSystem::isLnkFile(fsPath)) {
            makeParentFolderModifiable(_tmpFile.fileName());
            const auto convertResult = vfs->convertToPlaceholder(fsPath, *_item);
            if (!convertResult) {
                qCCritical(lcPropagateDownload()) << "error when converting a shortcut file to placeholder" << convertResult.error();
            }
        }

        auto r = vfs->dehydratePlaceholder(*_item);
        if (!r) {
            done(SyncFileItem::NormalError, r.error(), ErrorCategory::GenericError);
            return;
        }

        if (!propagator()->_journal->deleteFileRecord(_item->_originalFile)) {
            qCWarning(lcPropagateDownload) << "could not delete file from local DB" << _item->_originalFile;
            done(SyncFileItem::NormalError, tr("Could not delete file record %1 from local DB").arg(_item->_originalFile), ErrorCategory::GenericError);
            return;
        }

        updateMetadata(false);

        if (!_item->_remotePerm.isNull() && !_item->_remotePerm.hasPermission(RemotePermissions::CanWrite)) {
            // make sure ReadOnly flag is preserved for placeholder, similarly to regular files
            FileSystem::setFileReadOnly(propagator()->fullLocalPath(_item->_file), true);
        }

        return;
    }
    if (vfs->mode() == Vfs::Off && _item->_type == ItemTypeVirtualFile) {
        qCWarning(lcPropagateDownload) << "ignored virtual file type of" << _item->_file;
        _item->_type = ItemTypeFile;
    }
    if (_item->_type == ItemTypeVirtualFile && !propagator()->localFileNameClash(_item->_file)) {
        qCDebug(lcPropagateDownload) << "creating virtual file" << _item->_file;

        const auto fsPath = propagator()->fullLocalPath(_item->_file);
        makeParentFolderModifiable(fsPath);

        auto r = vfs->createPlaceholder(*_item);
        if (!r) {
            done(SyncFileItem::NormalError, r.error(), ErrorCategory::GenericError);
            return;
        }
        updateMetadata(false);

        if (!_item->_remotePerm.isNull() && !_item->_remotePerm.hasPermission(RemotePermissions::CanWrite)) {
            // make sure ReadOnly flag is preserved for placeholder, similarly to regular files
            FileSystem::setFileReadOnly(propagator()->fullLocalPath(_item->_file), true);
        }

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

    const auto csync_is_collision_safe_hash = [](const QByteArray &checksum_header)
    {
        return checksum_header.startsWith("SHA")
            || checksum_header.startsWith("MD5:");
    };
    if (_item->_modtime <= 0) {
        qCWarning(lcPropagateDownload()) << "invalid modified time" << _item->_file << _item->_modtime;
    }
    if (_item->_instruction == CSYNC_INSTRUCTION_CONFLICT
        && _item->_size == _item->_previousSize
        && !_item->_checksumHeader.isEmpty()
        && (csync_is_collision_safe_hash(_item->_checksumHeader)
            || _item->_modtime == _item->_previousModtime)) {
        qCDebug(lcPropagateDownload) << _item->_file << "may not need download, computing checksum";
        auto computeChecksum = new ComputeChecksum(this);
        computeChecksum->setChecksumType(parseChecksumHeaderType(_item->_checksumHeader));
        connect(computeChecksum, &ComputeChecksum::done,
            this, &PropagateDownloadFile::conflictChecksumComputed);
        propagator()->_activeJobList.append(this);
        computeChecksum->start(propagator()->fullLocalPath(_item->_file));
        return;
    }

    startDownload();
}

void PropagateDownloadFile::conflictChecksumComputed(const QByteArray &checksumType, const QByteArray &checksum)
{
    propagator()->_activeJobList.removeOne(this);
    if (makeChecksumHeader(checksumType, checksum) == _item->_checksumHeader) {
        // No download necessary, just update fs and journal metadata
        qCDebug(lcPropagateDownload) << _item->_file << "remote and local checksum match";

        // Apply the server mtime locally if necessary, ensuring the journal
        // and local mtimes end up identical
        auto fn = propagator()->fullLocalPath(_item->_file);
        Q_ASSERT(_item->_modtime > 0);
        if (_item->_modtime <= 0) {
            qCWarning(lcPropagateDownload()) << "invalid modified time" << _item->_file << _item->_modtime;
            return;
        }
        if (_item->_modtime != _item->_previousModtime) {
            Q_ASSERT(_item->_modtime > 0);
            qCDebug(lcPropagateDownload()) << "setModTime" << fn << _item->_modtime;
            FileSystem::setModTime(fn, _item->_modtime);
            emit propagator()->touchedFile(fn);
        }
        _item->_modtime = FileSystem::getModTime(fn);
        Q_ASSERT(_item->_modtime > 0);
        if (_item->_modtime <= 0) {
            qCWarning(lcPropagateDownload()) << "invalid modified time" << _item->_file << _item->_modtime;
            return;
        }
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
    if (propagator()->localFileNameClash(_item->_file) && _item->_type != ItemTypeVirtualFile) {
        _item->_instruction = CSYNC_INSTRUCTION_CASE_CLASH_CONFLICT;
        qCInfo(lcPropagateDownload) << "setting instruction to" << _item->_instruction << _item->_file;
    } else if (propagator()->localFileNameClash(_item->_file)) {
        _item->_instruction = CSYNC_INSTRUCTION_CASE_CLASH_CONFLICT;
        _item->_type = CSyncEnums::ItemTypeVirtualFileDownload;
        qCInfo(lcPropagateDownload) << "setting instruction to" << _item->_instruction << _item->_file << "setting type to" << _item->_type;
        auto fileName = _item->_file;
        if (propagator()->syncOptions()._vfs->mode() == Vfs::WithSuffix) {
            fileName.chop(propagator()->syncOptions()._vfs->fileSuffix().size());
            _item->_file = fileName;
        }
    }

    propagator()->reportProgress(*_item, 0);

    QString tmpFileName;
    QByteArray expectedEtagForResume;
    const SyncJournalDb::DownloadInfo progressInfo = propagator()->_journal->getDownloadInfo(_item->_file);
    if (progressInfo._valid) {
        // if the etag has changed meanwhile, remove the already downloaded part.
        if (progressInfo._etag != _item->_etag) {
            FileSystem::remove(propagator()->fullLocalPath(progressInfo._tmpfile));
            propagator()->_journal->setDownloadInfo(_item->_file, SyncJournalDb::DownloadInfo());
        } else {
            tmpFileName = progressInfo._tmpfile;
            expectedEtagForResume = progressInfo._etag;
        }
    }

    if (tmpFileName.isEmpty()) {
        tmpFileName = createDownloadTmpFileName(_item->_file);
    }
    _tmpFile.setFileName(propagator()->fullLocalPath(tmpFileName));
    makeParentFolderModifiable(_tmpFile.fileName());

    _resumeStart = _tmpFile.size();
    if (_resumeStart > 0 && _resumeStart == _item->_size) {
        qCInfo(lcPropagateDownload) << "File is already complete, no need to download";
        downloadFinished();
        return;
    }

    // Can't open(Append) read-only files, make sure to make
    // file writable if it exists.
    if (_tmpFile.exists()) {
        FileSystem::setFileReadOnly(_tmpFile.fileName(), false);
    }

    if (!_tmpFile.open(QIODevice::Append | QIODevice::Unbuffered)) {
        qCWarning(lcPropagateDownload) << "could not open temporary file" << _tmpFile.fileName();
        done(SyncFileItem::NormalError, _tmpFile.errorString(), ErrorCategory::GenericError);
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
                tr("The download would reduce free local disk space below the limit"), ErrorCategory::GenericError);
            emit propagator()->insufficientLocalStorage();
        } else if (diskSpaceResult == OwncloudPropagator::DiskSpaceCritical) {
            done(SyncFileItem::FatalError,
                tr("Free space on disk is less than %1").arg(Utility::octetsToString(criticalFreeSpaceLimit())), ErrorCategory::GenericError);
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
        propagator()->_journal->commit("download file start");
    }

    QMap<QByteArray, QByteArray> headers;

    if (_item->_directDownloadUrl.isEmpty()) {
        // Normal job, download from oC instance
        _job = new GETFileJob(propagator()->account(),
            propagator()->fullRemotePath(isEncrypted() ? _item->_encryptedFileName : _item->_file),
            &_tmpFile, headers, expectedEtagForResume, _resumeStart, this);
    } else {
        // We were provided a direct URL, use that one
        qCInfo(lcPropagateDownload) << "directDownloadUrl given for " << _item->_file << _item->_directDownloadUrl;

        if (!_item->_directDownloadCookies.isEmpty()) {
            headers["Cookie"] = _item->_directDownloadCookies.toUtf8();
        }

        QUrl url = QUrl::fromUserInput(_item->_directDownloadUrl);
        _job = new GETFileJob(propagator()->account(),
            url,
            &_tmpFile, headers, expectedEtagForResume, _resumeStart, this);
    }
    _job->setBandwidthManager(&propagator()->_bandwidthManager);
    connect(_job.data(), &GETFileJob::finishedSignal, this, &PropagateDownloadFile::slotGetFinished);
    connect(_job.data(), &GETFileJob::downloadProgress, this, &PropagateDownloadFile::slotDownloadProgress);
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

void PropagateDownloadFile::done(const SyncFileItem::Status status, const QString &errorString, const ErrorCategory category)
{
    if (_needParentFolderRestorePermissions) {
        FileSystem::setFolderPermissions(QString::fromStdWString(_parentPath.wstring()), FileSystem::FolderPermissions::ReadOnly);
        emit propagator()->touchedFile(QString::fromStdWString(_parentPath.wstring()));
        _needParentFolderRestorePermissions = false;
    }
    PropagateItemJob::done(status, errorString, category);
}

void PropagateDownloadFile::makeParentFolderModifiable(const QString &fileName)
{
    try {
        const auto newDirPath = std::filesystem::path{fileName.toStdWString()};
        Q_ASSERT(newDirPath.has_parent_path());
        _parentPath = newDirPath.parent_path();
    }
    catch (const std::filesystem::filesystem_error &e)
    {
        qCWarning(lcPropagateDownload) << "exception when checking parent folder access rights" << e.what() << e.path1().c_str() << e.path2().c_str();
    }
    catch (const std::system_error &e)
    {
        qCWarning(lcPropagateDownload) << "exception when checking parent folder access rights" << e.what();
    }
    catch (...)
    {
        qCWarning(lcPropagateDownload) << "exception when checking parent folder access rights";
    }

    if (FileSystem::isFolderReadOnly(_parentPath)) {
        FileSystem::setFolderPermissions(QString::fromStdWString(_parentPath.wstring()), FileSystem::FolderPermissions::ReadWrite);
        emit propagator()->touchedFile(QString::fromStdWString(_parentPath.wstring()));
        _needParentFolderRestorePermissions = true;
    }
}

const char owncloudCustomSoftErrorStringC[] = "owncloud-custom-soft-error-string";
void PropagateDownloadFile::slotGetFinished()
{
    propagator()->_activeJobList.removeOne(this);

    GETFileJob *job = _job;
    ASSERT(job);

    _item->_httpErrorCode = job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
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

        // Getting a 423 means that the file is locked
        const bool fileLocked = _item->_httpErrorCode == 423;
        if (fileLocked) {
            qCWarning(lcPropagateDownload) << "server replied 423, file is Locked";
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

        done(status, errorString, errorCategoryFromNetworkError(err));
        return;
    }

    _item->_responseTimeStamp = job->responseTimestamp();

    if (!job->etag().isEmpty()) {
        // The etag will be empty if we used a direct download URL.
        // (If it was really empty by the server, the GETFileJob will have errored
        _item->_etag = parseEtag(job->etag());
    }
    if (job->lastModified()) {
        // It is possible that the file was modified on the server since we did the discovery phase
        // so make sure we have the up-to-date time
        _item->_modtime = job->lastModified();
        Q_ASSERT(_item->_modtime > 0);
        if (_item->_modtime <= 0) {
            qCWarning(lcPropagateDownload()) << "invalid modified time" << _item->_file << _item->_modtime;
        }
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
        && job->reply()->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool()) {
        bodySize = 0;
        hasSizeHeader = false;
    }

    if (hasSizeHeader && _tmpFile.size() > 0 && bodySize == 0) {
        // Strange bug with broken webserver or webfirewall https://github.com/owncloud/client/issues/3373#issuecomment-122672322
        // This happened when trying to resume a file. The Content-Range header was files, Content-Length was == 0
        qCDebug(lcPropagateDownload) << bodySize << _item->_size << _tmpFile.size() << job->resumeStart();
        FileSystem::remove(_tmpFile.fileName());
        done(SyncFileItem::SoftError, QLatin1String("Broken webserver returning empty content length for non-empty file on resume"), ErrorCategory::GenericError);
        return;
    }

    if (bodySize > 0 && bodySize != _tmpFile.size() - job->resumeStart()) {
        qCDebug(lcPropagateDownload) << bodySize << _tmpFile.size() << job->resumeStart();
        propagator()->_anotherSyncNeeded = true;
        done(SyncFileItem::SoftError, tr("The file could not be downloaded completely."), ErrorCategory::GenericError);
        return;
    }

    if (_tmpFile.size() == 0 && _item->_size > 0) {
        FileSystem::remove(_tmpFile.fileName());
        done(SyncFileItem::NormalError,
            tr("The downloaded file is empty, but the server said it should have been %1.")
                .arg(Utility::octetsToString(_item->_size)), ErrorCategory::GenericError);
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
    auto *validator = new ValidateChecksumHeader(this);
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

void PropagateDownloadFile::slotChecksumFail(const QString &errMsg,
    const QByteArray &calculatedChecksumType, const QByteArray &calculatedChecksum, const ValidateChecksumHeader::FailureReason reason)
{
    if (reason == ValidateChecksumHeader::FailureReason::ChecksumMismatch && propagator()->account()->isChecksumRecalculateRequestSupported()) {
            const QByteArray calculatedChecksumHeader(calculatedChecksumType + ':' + calculatedChecksum);
            const QString fullRemotePathForFile(propagator()->fullRemotePath(isEncrypted() ? _item->_encryptedFileName : _item->_file));
            auto *job = new SimpleFileJob(propagator()->account(), fullRemotePathForFile);
            QObject::connect(job, &SimpleFileJob::finishedSignal, this,
                [this, calculatedChecksumHeader, errMsg](const QNetworkReply *reply) { processChecksumRecalculate(reply, calculatedChecksumHeader, errMsg);
            });

            qCWarning(lcPropagateDownload) << "Checksum validation has failed for file:" << fullRemotePathForFile
                                           << " Requesting checksum recalculation on the server...";
            QNetworkRequest req;
            req.setRawHeader(checksumRecalculateOnServerHeaderC, calculatedChecksumType);
            job->startRequest(QByteArrayLiteral("PATCH"), req);
            return;
    }

    checksumValidateFailedAbortDownload(errMsg);
}

void PropagateDownloadFile::processChecksumRecalculate(const QNetworkReply *reply, const QByteArray &originalChecksumHeader, const QString &errorMessage)
{
    if (reply->error() != QNetworkReply::NoError) {
        qCCritical(lcPropagateDownload) << "Checksum recalculation has failed for file:" << reply->url()
                                        << " Recalculation request has finished with error:" << reply->errorString();
        checksumValidateFailedAbortDownload(errorMessage);
        return;
    }

    const auto newChecksumHeaderFromServer = reply->rawHeader(checkSumHeaderC);
    if (newChecksumHeaderFromServer == originalChecksumHeader) {
        const auto newChecksumHeaderFromServerSplit = newChecksumHeaderFromServer.split(':');
        if (newChecksumHeaderFromServerSplit.size() > 1) {
            transmissionChecksumValidated(newChecksumHeaderFromServerSplit.first(), newChecksumHeaderFromServerSplit.last());
            return;
        }
    }

    qCCritical(lcPropagateDownload) << "Checksum recalculation has failed for file:" << reply->url() << " "
                                    << checkSumHeaderC << " received is:" << newChecksumHeaderFromServer;
    checksumValidateFailedAbortDownload(errorMessage);
}

void PropagateDownloadFile::checksumValidateFailedAbortDownload(const QString &errMsg)
{
    FileSystem::remove(_tmpFile.fileName());
    propagator()->_anotherSyncNeeded = true;
    done(SyncFileItem::SoftError, errMsg, ErrorCategory::GenericError); // tr("The file downloaded with a broken checksum, will be redownloaded."));
}

void PropagateDownloadFile::deleteExistingFolder()
{
    QString existingDir = propagator()->fullLocalPath(_item->_file);
    if (!FileSystem::isDir(existingDir)) {
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
        done(SyncFileItem::NormalError, error, ErrorCategory::GenericError);
    }
}

namespace { // Anonymous namespace for the recall feature
    static QString makeRecallFileName(const QString &fn)
    {
        QString recallFileName(fn);
        // Add _recall-XXXX  before the extension.
        int dotLocation = recallFileName.lastIndexOf('.');
        // If no extension, add it at the end  (take care of cases like foo/.hidden or foo.bar/file)
        if (dotLocation <= recallFileName.lastIndexOf('/') + 1) {
            dotLocation = recallFileName.size();
        }

        QString timeString = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss");
        recallFileName.insert(dotLocation, "_.sys.admin#recall#-" + timeString);

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

            QString recalledFile = QDir::cleanPath(baseDir.filePath(line));
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
        int chownErr = chown(fileName.toLocal8Bit().constData(), -1, fi.groupId());
        if (chownErr) {
            // TODO: Consider further error handling!
            qCWarning(lcPropagateDownload) << QStringLiteral("preserveGroupOwnership: chown error %1: setting group %2 failed on file %3").arg(chownErr).arg(fi.groupId()).arg(fileName);
        }
#else
        Q_UNUSED(fileName);
        Q_UNUSED(fi);
#endif
    }
} // end namespace

void PropagateDownloadFile::transmissionChecksumValidated(const QByteArray &checksumType, const QByteArray &checksum)
{
    const QByteArray theContentChecksumType = propagator()->account()->capabilities().preferredUploadChecksumType();

    // Reuse transmission checksum as content checksum.
    //
    // We could do this more aggressively and accept both MD5 and SHA1
    // instead of insisting on the exactly correct checksum type.
    if (theContentChecksumType == checksumType || theContentChecksumType.isEmpty()) {
        return contentChecksumComputed(checksumType, checksum);
    }

    // Compute the content checksum.
    auto computeChecksum = new ComputeChecksum(this);
    computeChecksum->setChecksumType(theContentChecksumType);

    connect(computeChecksum, &ComputeChecksum::done,
        this, &PropagateDownloadFile::contentChecksumComputed);
    computeChecksum->start(_tmpFile.fileName());
}

void PropagateDownloadFile::contentChecksumComputed(const QByteArray &checksumType, const QByteArray &checksum)
{
    _item->_checksumHeader = makeChecksumHeader(checksumType, checksum);

    const auto localFilePath = propagator()->fullLocalPath(_item->_file);
    SyncJournalFileRecord record;
    if (_item->_instruction != CSYNC_INSTRUCTION_CONFLICT && FileSystem::fileExists(localFilePath)
        && (propagator()->_journal->getFileRecord(_item->_file, &record) && record.isValid())
        && (record._modtime == _item->_modtime && record._etag != _item->_etag)
        && _item->_type == ItemTypeFile) {
        const auto computeChecksum = new ComputeChecksum(this);
        computeChecksum->setChecksumType(checksumType);
        connect(computeChecksum, &ComputeChecksum::done, this, &PropagateDownloadFile::localFileContentChecksumComputed);
        computeChecksum->start(localFilePath);
        return;
    }

    finalizeDownload();
}

void PropagateDownloadFile::localFileContentChecksumComputed(const QByteArray &checksumType, const QByteArray &checksum)
{
    if (_item->_checksumHeader == makeChecksumHeader(checksumType, checksum)) {
        FileSystem::remove(_tmpFile.fileName());
        updateMetadata(false);
        return;
    }
    finalizeDownload();
}

void PropagateDownloadFile::finalizeDownload()
{
    if (isEncrypted()) {
        if (_downloadEncryptedHelper->decryptFile(_tmpFile)) {
            downloadFinished();
        } else {
            done(SyncFileItem::NormalError, _downloadEncryptedHelper->errorString(), ErrorCategory::GenericError);
        }
    } else {
        downloadFinished();
    }
}

void PropagateDownloadFile::downloadFinished()
{
    ASSERT(!_tmpFile.isOpen());
    const auto filename = propagator()->fullLocalPath(_item->_file);

    if (_item->_modtime <= 0) {
        FileSystem::remove(_tmpFile.fileName());
        done(SyncFileItem::NormalError, tr("File %1 has invalid modified time reported by server. Do not save it.").arg(QDir::toNativeSeparators(_item->_file)), ErrorCategory::GenericError);
        return;
    }
    Q_ASSERT(_item->_modtime > 0);
    if (_item->_modtime <= 0) {
        qCWarning(lcPropagateDownload()) << "invalid modified time" << _item->_file << _item->_modtime;
    }
    qCDebug(lcPropagateDownload()) << "setModTime" << _tmpFile.fileName() << _item->_modtime;
    FileSystem::setModTime(_tmpFile.fileName(), _item->_modtime);
    // We need to fetch the time again because some file systems such as FAT have worse than a second
    // Accuracy, and we really need the time from the file system. (#3103)
    _item->_modtime = FileSystem::getModTime(_tmpFile.fileName());
    if (_item->_modtime <= 0) {
        FileSystem::remove(_tmpFile.fileName());
        done(SyncFileItem::NormalError, tr("File %1 has invalid modified time reported by server. Do not save it.").arg(QDir::toNativeSeparators(_item->_file)), ErrorCategory::GenericError);
        return;
    }
    Q_ASSERT(_item->_modtime > 0);
    if (_item->_modtime <= 0) {
        qCWarning(lcPropagateDownload()) << "invalid modified time" << _item->_file << _item->_modtime;
    }

    if (propagator()->localFileNameClash(_item->_file)) {
        _item->_instruction = CSYNC_INSTRUCTION_CASE_CLASH_CONFLICT;
        qCInfo(lcPropagateDownload) << "setting instruction to" << _item->_instruction << _item->_file;
    }

    auto previousFileExists = FileSystem::fileExists(filename) && _item->_instruction != CSYNC_INSTRUCTION_CASE_CLASH_CONFLICT;
    if (previousFileExists) {
        // Preserve the existing file permissions.
        const auto existingFile = QFileInfo{filename};
#ifdef Q_OS_WIN
        try {
            const auto existingPermissions = FileSystem::filePermissionsWin(filename);
            const auto tmpFilePermissions = FileSystem::filePermissionsWin(_tmpFile.fileName());
            if (existingPermissions != tmpFilePermissions) {
                FileSystem::setFilePermissionsWin(_tmpFile.fileName(), existingPermissions);
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            qCWarning(lcPropagateDownload()) << _item->_instruction << _item->_file << e.what();
        }
        catch (const std::system_error &e)
        {
            qCWarning(lcPropagateDownload()) << _item->_instruction << _item->_file << e.what();
        }
        catch (...)
        {
            qCWarning(lcPropagateDownload()) << _item->_instruction << _item->_file;
        }
#else
        if (existingFile.permissions() != _tmpFile.permissions()) {
            _tmpFile.setPermissions(existingFile.permissions());
        }
#endif
        preserveGroupOwnership(_tmpFile.fileName(), existingFile);

        // Make the file a hydrated placeholder if possible
        const auto result = propagator()->syncOptions()._vfs->convertToPlaceholder(_tmpFile.fileName(), *_item, filename);
        if (!result) {
            done(SyncFileItem::NormalError, result.error(), ErrorCategory::GenericError);
            return;
        }
    }

    if (_item->_locked == SyncFileItem::LockStatus::LockedItem && (_item->_lockOwnerType != SyncFileItem::LockOwnerType::UserLock || _item->_lockOwnerId != propagator()->account()->davUser())) {
        qCDebug(lcPropagateDownload()) << _tmpFile.fileName() << "file is locked: making it read only";
        FileSystem::setFileReadOnly(_tmpFile.fileName(), true);
    } else {
        qCDebug(lcPropagateDownload()) << _tmpFile.fileName() << "file is not locked: making it"
                                       << ((!_item->_remotePerm.isNull() && !_item->_remotePerm.hasPermission(RemotePermissions::CanWrite)) ? "read only"
                                                                                                                                            : "read write");
        FileSystem::setFileReadOnlyWeak(_tmpFile.fileName(), (!_item->_remotePerm.isNull() && !_item->_remotePerm.hasPermission(RemotePermissions::CanWrite)));
    }

    const auto isConflict = (_item->_instruction == CSYNC_INSTRUCTION_CONFLICT
                             && (FileSystem::isDir(filename) || !FileSystem::fileEquals(filename, _tmpFile.fileName()))) ||
        _item->_instruction == CSYNC_INSTRUCTION_CASE_CLASH_CONFLICT;

    if (isConflict) {
        if (_item->_instruction == CSYNC_INSTRUCTION_CASE_CLASH_CONFLICT) {
            qCInfo(lcPropagateDownload) << "downloading case clashed file" << _item->_file;
            const auto caseClashConflictResult = propagator()->createCaseClashConflict(_item, _tmpFile.fileName());
            if (caseClashConflictResult) {
                done(SyncFileItem::SoftError, *caseClashConflictResult, ErrorCategory::GenericError);
            } else {
                done(SyncFileItem::FileNameClash, tr("File %1 downloaded but it resulted in a local file name clash!").arg(QDir::toNativeSeparators(_item->_file)), ErrorCategory::GenericError);
            }
            return;
        } else {
            QString error;
            if (!propagator()->createConflict(_item, _associatedComposite, &error)) {
                done(SyncFileItem::SoftError, error, ErrorCategory::GenericError);
                return;
            } else {
                previousFileExists = false;
            }
        }
    }

    const auto vfs = propagator()->syncOptions()._vfs;

    // In the case of an hydration, this size is likely to change for placeholders
    // (except with the cfapi backend)
    const auto isVirtualDownload = _item->_type == ItemTypeVirtualFileDownload;
    const auto isCfApiVfs = vfs && vfs->mode() == Vfs::WindowsCfApi;
    if (previousFileExists && (isCfApiVfs || !isVirtualDownload)) {
        // Check whether the existing file has changed since the discovery
        // phase by comparing size and mtime to the previous values. This
        // is necessary to avoid overwriting user changes that happened between
        // the discovery phase and now.
        const qint64 expectedSize = _item->_previousSize;
        const time_t expectedMtime = _item->_previousModtime;
        if (!FileSystem::verifyFileUnchanged(filename, expectedSize, expectedMtime)) {
            propagator()->_anotherSyncNeeded = true;
            done(SyncFileItem::SoftError, tr("File has changed since discovery"), ErrorCategory::GenericError);
            return;
        }
    }

    QString error;
    emit propagator()->touchedFile(filename);
    // The fileChanged() check is done above to generate better error messages.
    if (!FileSystem::uncheckedRenameReplace(_tmpFile.fileName(), filename, &error)) {
        qCWarning(lcPropagateDownload) << QStringLiteral("Rename failed: %1 => %2").arg(_tmpFile.fileName()).arg(filename);
        // If the file is locked, we want to retry this sync when it
        // becomes available again, otherwise try again directly
        if (FileSystem::isFileLocked(filename)) {
            emit propagator()->seenLockedFile(filename);
        } else {
            propagator()->_anotherSyncNeeded = true;
        }

        done(SyncFileItem::SoftError, error, ErrorCategory::GenericError);
        return;
    }

    if (_needParentFolderRestorePermissions) {
        FileSystem::setFolderPermissions(QString::fromStdWString(_parentPath.wstring()), FileSystem::FolderPermissions::ReadOnly);
        emit propagator()->touchedFile(QString::fromStdWString(_parentPath.wstring()));
        _needParentFolderRestorePermissions = false;
    }

    FileSystem::setFileHidden(filename, false);

    // Maybe we downloaded a newer version of the file than we thought we would...
    // Get up to date information for the journal.
    _item->_size = FileSystem::getSize(filename);

    // Maybe what we downloaded was a conflict file? If so, set a conflict record.
    // (the data was prepared in slotGetFinished above)
    if (_conflictRecord.isValid())
        propagator()->_journal->setConflictRecord(_conflictRecord);

    if (vfs && vfs->mode() == Vfs::WithSuffix) {
        // If the virtual file used to have a different name and db
        // entry, remove it transfer its old pin state.
        if (_item->_type == ItemTypeVirtualFileDownload) {
            QString virtualFile = _item->_file + vfs->fileSuffix();
            auto fn = propagator()->fullLocalPath(virtualFile);
            qCDebug(lcPropagateDownload) << "Download of previous virtual file finished" << fn;
            QFile::remove(fn);

            if (!propagator()->_journal->deleteFileRecord(virtualFile)) {
                qCWarning(lcPropagateDownload) << "could not delete file from local DB" << virtualFile;
                done(SyncFileItem::NormalError, tr("Could not delete file record %1 from local DB").arg(virtualFile), ErrorCategory::GenericError);
                return;
            }

            // Move the pin state to the new location
            auto pin = propagator()->_journal->internalPinStates().rawForPath(virtualFile.toUtf8());
            if (pin && *pin != PinState::Inherited) {
                if (!vfs->setPinState(_item->_file, *pin)) {
                    qCWarning(lcPropagateDownload) << "Could not set pin state of" << _item->_file;
                }
                if (!vfs->setPinState(virtualFile, PinState::Inherited)) {
                    qCWarning(lcPropagateDownload) << "Could not set pin state of" << virtualFile << " to inherited";
                }
            }
        }

        // Ensure the pin state isn't contradictory
        auto pin = vfs->pinState(_item->_file);
        if (pin && *pin == PinState::OnlineOnly)
            if (!vfs->setPinState(_item->_file, PinState::Unspecified)) {
                qCWarning(lcPropagateDownload) << "Could not set pin state of" << _item->_file << "to unspecified";
            }
    }

    updateMetadata(isConflict);
}

void PropagateDownloadFile::updateMetadata(bool isConflict)
{
    const QString fn = propagator()->fullLocalPath(_item->_file);
    const auto result = propagator()->updateMetadata(*_item);
    if (!result) {
        done(SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()), ErrorCategory::GenericError);
        return;
    } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
        done(SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(_item->_file), ErrorCategory::GenericError);
        return;
    }

    if (isEncrypted()) {
        propagator()->_journal->setDownloadInfo(_item->_file, SyncJournalDb::DownloadInfo());
    } else {
        propagator()->_journal->setDownloadInfo(_item->_encryptedFileName, SyncJournalDb::DownloadInfo());
    }

    propagator()->_journal->commit("download file start2");

    done(isConflict ? SyncFileItem::Conflict : SyncFileItem::Success, {}, ErrorCategory::NoError);

    // handle the special recall file
    if (!_item->_remotePerm.hasPermission(RemotePermissions::IsShared)
        && (_item->_file == QLatin1String(".sys.admin#recall#")
               || _item->_file.endsWith(QLatin1String("/.sys.admin#recall#")))) {
        handleRecallFile(fn, propagator()->localPath(), *propagator()->_journal);
    }

    const auto isLockOwnedByCurrentUser = _item->_lockOwnerId == propagator()->account()->davUser();

    const auto isUserLockOwnedByCurrentUser = (_item->_lockOwnerType == SyncFileItem::LockOwnerType::UserLock && isLockOwnedByCurrentUser);
    const auto isTokenLockOwnedByCurrentUser = (_item->_lockOwnerType == SyncFileItem::LockOwnerType::TokenLock && isLockOwnedByCurrentUser);

    if (_item->_locked == SyncFileItem::LockStatus::LockedItem && !isUserLockOwnedByCurrentUser && !isTokenLockOwnedByCurrentUser) {
        qCDebug(lcPropagateDownload()) << fn << "file is locked: making it read only";
        FileSystem::setFileReadOnly(fn, true);
    } else {
        qCDebug(lcPropagateDownload()) << fn << "file is not locked: making it" << ((!_item->_remotePerm.isNull() && !_item->_remotePerm.hasPermission(RemotePermissions::CanWrite)) ? "read only" : "read write");
        FileSystem::setFileReadOnlyWeak(fn, (!_item->_remotePerm.isNull() && !_item->_remotePerm.hasPermission(RemotePermissions::CanWrite)));
    }

    qint64 duration = _stopwatch.elapsed();
    if (isLikelyFinishedQuickly() && duration > 5 * 1000) {
        qCWarning(lcPropagateDownload) << "WARNING: Unexpectedly slow connection, took" << duration << "msec for" << _item->_size - _resumeStart << "bytes for" << _item->_file;
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
    if (_job && _job->reply())
        _job->reply()->abort();

    if (abortType == AbortType::Asynchronous) {
        emit abortFinished();
    }
}
}
