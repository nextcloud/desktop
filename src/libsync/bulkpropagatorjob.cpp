/*
 * Copyright 2021 (c) Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#include "bulkpropagatorjob.h"

#include "putmultifilejob.h"
#include "owncloudpropagator_p.h"
#include "syncfileitem.h"
#include "syncengine.h"
#include "propagateupload.h"
#include "propagatorjobs.h"
#include "filesystem.h"
#include "account.h"
#include "common/utility.h"
#include "common/checksums.h"
#include "networkjobs.h"

#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace {

QByteArray getEtagFromJsonReply(const QJsonObject &reply)
{
    const auto ocEtag = OCC::parseEtag(reply.value("OC-ETag").toString().toLatin1());
    const auto ETag = OCC::parseEtag(reply.value("ETag").toString().toLatin1());
    const auto  etag = OCC::parseEtag(reply.value("etag").toString().toLatin1());
    QByteArray ret = ocEtag;
    if (ret.isEmpty()) {
        ret = ETag;
    }
    if (ret.isEmpty()) {
        ret = etag;
    }
    if (ocEtag.length() > 0 && ocEtag != etag && ocEtag != ETag) {
        qCDebug(OCC::lcBulkPropagatorJob) << "Quite peculiar, we have an etag != OC-Etag [no problem!]" << etag << ETag << ocEtag;
    }
    return ret;
}

QByteArray getHeaderFromJsonReply(const QJsonObject &reply, const QByteArray &headerName)
{
    return reply.value(headerName).toString().toLatin1();
}

constexpr auto batchSize = 100;
constexpr auto parallelJobsMaximumCount = 1;

}

namespace OCC {

Q_LOGGING_CATEGORY(lcBulkPropagatorJob, "nextcloud.sync.propagator.bulkupload", QtInfoMsg)

BulkPropagatorJob::BulkPropagatorJob(OwncloudPropagator *propagator, const std::deque<SyncFileItemPtr> &items)
    : PropagatorJob(propagator)
    , _items(items)
{
    _filesToUpload.reserve(batchSize);
    _pendingChecksumFiles.reserve(batchSize);
}

bool BulkPropagatorJob::scheduleSelfOrChild()
{
    if (_items.empty() || !_pendingChecksumFiles.empty()) {
        return false;
    }

    _state = Running;

    for(auto i = 0; i < batchSize && !_items.empty(); ++i) {
        const auto currentItem = _items.front();
        _items.pop_front();
        _pendingChecksumFiles.insert(currentItem->_file);

        QMetaObject::invokeMethod(this, [this, currentItem] {
            UploadFileInfo fileToUpload;
            fileToUpload._file = currentItem->_file;
            fileToUpload._size = currentItem->_size;
            fileToUpload._path = propagator()->fullLocalPath(fileToUpload._file);

            qCDebug(lcBulkPropagatorJob) << "Scheduling bulk propagator job:" << this
                                         << "and starting upload of item"
                                         << "with file:" << fileToUpload._file
                                         << "with size:" << fileToUpload._size
                                         << "with path:" << fileToUpload._path;

            startUploadFile(currentItem, fileToUpload);
        }); // We could be in a different thread (neon jobs)
    }

    return _items.empty() && _filesToUpload.empty();
}

PropagatorJob::JobParallelism BulkPropagatorJob::parallelism() const
{
    return PropagatorJob::JobParallelism::FullParallelism;
}

void BulkPropagatorJob::startUploadFile(SyncFileItemPtr item, UploadFileInfo fileToUpload)
{
    if (propagator()->_abortRequested) {
        return;
    }

    // Check if the specific file can be accessed
    if (propagator()->hasCaseClashAccessibilityProblem(fileToUpload._file)) {
        done(item, SyncFileItem::NormalError, tr("File %1 cannot be uploaded because another file with the same name, differing only in case, exists").arg(QDir::toNativeSeparators(item->_file)), ErrorCategory::GenericError);
        return;
    }

    return slotComputeTransmissionChecksum(item, fileToUpload);
}

void BulkPropagatorJob::doStartUpload(SyncFileItemPtr item,
                                      UploadFileInfo fileToUpload,
                                      QByteArray transmissionChecksumHeader)
{
    if (propagator()->_abortRequested) {
        return;
    }

    // write the checksum in the database, so if the POST is sent
    // to the server, but the connection drops before we get the etag, we can check the checksum
    // in reconcile (issue #5106)
    SyncJournalDb::UploadInfo pi;
    pi._valid = true;
    pi._chunkUploadV1 = 0;
    pi._transferid = 0; // We set a null transfer id because it is not chunked.
    pi._modtime = item->_modtime;
    pi._errorCount = 0;
    pi._contentChecksum = item->_checksumHeader;
    pi._size = item->_size;

    propagator()->_journal->setUploadInfo(item->_file, pi);
    propagator()->_journal->commit("Upload info");

    auto currentHeaders = headers(item);
    currentHeaders[QByteArrayLiteral("Content-Length")] = QByteArray::number(fileToUpload._size);

    if (!item->_renameTarget.isEmpty() && item->_file != item->_renameTarget) {
        // Try to rename the file
        const auto originalFilePathAbsolute = propagator()->fullLocalPath(item->_file);
        const auto newFilePathAbsolute = propagator()->fullLocalPath(item->_renameTarget);
        const auto renameSuccess = QFile::rename(originalFilePathAbsolute, newFilePathAbsolute);

        if (!renameSuccess) {
            done(item, SyncFileItem::NormalError, "File contains trailing spaces and couldn't be renamed", ErrorCategory::GenericError);
            return;
        }

        qCWarning(lcBulkPropagatorJob()) << item->_file << item->_renameTarget;

        fileToUpload._file = item->_file = item->_renameTarget;
        fileToUpload._path = propagator()->fullLocalPath(fileToUpload._file);

        item->_modtime = FileSystem::getModTime(newFilePathAbsolute);
        if (item->_modtime <= 0) {
            _pendingChecksumFiles.remove(item->_file);
            slotOnErrorStartFolderUnlock(item, SyncFileItem::NormalError, tr("File %1 has invalid modified time. Do not upload to the server.").arg(QDir::toNativeSeparators(item->_file)), ErrorCategory::GenericError);
            checkPropagationIsDone();
            return;
        }
    }

    const auto remotePath = propagator()->fullRemotePath(fileToUpload._file);

    currentHeaders["X-File-MD5"] = transmissionChecksumHeader;

    BulkUploadItem newUploadFile{propagator()->account(), item, fileToUpload,
                remotePath, fileToUpload._path,
                fileToUpload._size, currentHeaders};

    qCInfo(lcBulkPropagatorJob) << remotePath << "transmission checksum" << transmissionChecksumHeader << fileToUpload._path;
    _filesToUpload.push_back(std::move(newUploadFile));
    _pendingChecksumFiles.remove(item->_file);

    if (_pendingChecksumFiles.empty()) {
        triggerUpload();
    }
}

void BulkPropagatorJob::triggerUpload()
{
    auto uploadParametersData = std::vector<SingleUploadFileData>{};
    uploadParametersData.reserve(_filesToUpload.size());

    int timeout = 0;
    for(auto &singleFile : _filesToUpload) {
        // job takes ownership of device via a QScopedPointer. Job deletes itself when finishing
        auto device = std::make_unique<UploadDevice>(singleFile._localPath,
                                                     0,
                                                     singleFile._fileSize,
                                                     &propagator()->_bandwidthManager);

        if (!device->open(QIODevice::ReadOnly)) {
            qCWarning(lcBulkPropagatorJob) << "Could not prepare upload device: " << device->errorString();

            // If the file is currently locked, we want to retry the sync
            // when it becomes available again.
            if (FileSystem::isFileLocked(singleFile._localPath)) {
                emit propagator()->seenLockedFile(singleFile._localPath);
            }

            abortWithError(singleFile._item, SyncFileItem::NormalError, device->errorString());
            emit finished(SyncFileItem::NormalError);

            return;
        }

        singleFile._headers["X-File-Path"] = singleFile._remotePath.toUtf8();
        uploadParametersData.push_back({std::move(device), singleFile._headers});
        timeout += singleFile._fileSize;
    }

    const auto bulkUploadUrl = Utility::concatUrlPath(propagator()->account()->url(), QStringLiteral("/remote.php/dav/bulk"));
    auto job = new PutMultiFileJob(propagator()->account(), bulkUploadUrl, std::move(uploadParametersData), this);
    connect(job, &PutMultiFileJob::finishedSignal, this, &BulkPropagatorJob::slotPutFinished);

    for(auto &singleFile : _filesToUpload) {
        connect(job, &PutMultiFileJob::uploadProgress, this, [this, singleFile] (const qint64 sent, const qint64 total) {
            slotUploadProgress(singleFile._item, sent, total);
        });
    }

    adjustLastJobTimeout(job, timeout);
    _jobs.append(job);
    job->start();

    if (parallelism() == PropagatorJob::JobParallelism::FullParallelism && _jobs.size() < parallelJobsMaximumCount) {
        scheduleSelfOrChild();
    }
}

void BulkPropagatorJob::checkPropagationIsDone()
{
    if (_items.empty()) {
        if (!_jobs.empty() || !_pendingChecksumFiles.empty()) {
            // just wait for the other job to finish.
            return;
        }

        qCInfo(lcBulkPropagatorJob) << "final status" << _finalStatus;
        emit finished(_finalStatus);
        propagator()->scheduleNextJob();
    } else {
        scheduleSelfOrChild();
    }
}

void BulkPropagatorJob::slotComputeTransmissionChecksum(SyncFileItemPtr item,
                                                        UploadFileInfo fileToUpload)
{
    // Compute the transmission checksum.
    const auto computeChecksum = new ComputeChecksum(this);
    const auto checksumType = uploadChecksumEnabled() ? "MD5" : "";
    computeChecksum->setChecksumType(checksumType);

    connect(computeChecksum, &ComputeChecksum::done, this, [this, item, fileToUpload] (const QByteArray &contentChecksumType, const QByteArray &contentChecksum) {
        slotStartUpload(item, fileToUpload, contentChecksumType, contentChecksum);
    });
    connect(computeChecksum, &ComputeChecksum::done, computeChecksum, &QObject::deleteLater);

    computeChecksum->start(fileToUpload._path);
}

void BulkPropagatorJob::slotStartUpload(SyncFileItemPtr item,
                                        UploadFileInfo fileToUpload,
                                        const QByteArray &transmissionChecksumType,
                                        const QByteArray &transmissionChecksum)
{
    const auto transmissionChecksumHeader = makeChecksumHeader(transmissionChecksumType, transmissionChecksum);

    item->_checksumHeader = transmissionChecksumHeader;

    const auto fullFilePath = fileToUpload._path;
    const auto originalFilePath = propagator()->fullLocalPath(item->_file);

    if (!FileSystem::fileExists(fullFilePath)) {
        _pendingChecksumFiles.remove(item->_file);
        slotOnErrorStartFolderUnlock(item, SyncFileItem::SoftError, tr("File Removed (start upload) %1").arg(fullFilePath), ErrorCategory::GenericError);
        checkPropagationIsDone();
        return;
    }

    const auto prevModtime = item->_modtime; // the _item value was set in PropagateUploadFile::start()
    // but a potential checksum calculation could have taken some time during which the file could
    // have been changed again, so better check again here.

    item->_modtime = FileSystem::getModTime(originalFilePath);
    if (item->_modtime <= 0) {
        _pendingChecksumFiles.remove(item->_file);
        slotOnErrorStartFolderUnlock(item, SyncFileItem::NormalError, tr("File %1 has invalid modification time. Do not upload to the server.").arg(QDir::toNativeSeparators(item->_file)), ErrorCategory::GenericError);
        checkPropagationIsDone();
        return;
    }
    if (prevModtime != item->_modtime) {
        propagator()->_anotherSyncNeeded = true;
        _pendingChecksumFiles.remove(item->_file);

        qCDebug(lcBulkPropagatorJob) << "trigger another sync after checking modified time of item" << item->_file
                                     << "prevModtime" << prevModtime
                                     << "Curr" << item->_modtime;

        slotOnErrorStartFolderUnlock(item, SyncFileItem::SoftError, tr("Local file changed during syncing. It will be resumed."), ErrorCategory::GenericError);
        checkPropagationIsDone();
        return;
    }

    fileToUpload._size = FileSystem::getSize(fullFilePath);
    item->_size = FileSystem::getSize(originalFilePath);

    // But skip the file if the mtime is too close to 'now'!
    // That usually indicates a file that is still being changed
    // or not yet fully copied to the destination.
    if (fileIsStillChanging(*item)) {
        propagator()->_anotherSyncNeeded = true;
        _pendingChecksumFiles.remove(item->_file);
        slotOnErrorStartFolderUnlock(item, SyncFileItem::SoftError, tr("Local file changed during sync."), ErrorCategory::GenericError);
        checkPropagationIsDone();
        return;
    }

    doStartUpload(item, fileToUpload, transmissionChecksum);
}

void BulkPropagatorJob::slotOnErrorStartFolderUnlock(SyncFileItemPtr item,
                                                     const SyncFileItem::Status status,
                                                     const QString &errorString,
                                                     const ErrorCategory errorCategory)
{
    qCInfo(lcBulkPropagatorJob()) << status << errorString << errorCategory;
    done(item, status, errorString, errorCategory);
}

void BulkPropagatorJob::slotPutFinishedOneFile(const BulkUploadItem &singleFile,
                                               PutMultiFileJob *job,
                                               const QJsonObject &fileReply)
{
    auto finished = false;

    qCInfo(lcBulkPropagatorJob()) << singleFile._item->_file << "file headers" << fileReply;

    if (fileReply.contains("error") && !fileReply[QStringLiteral("error")].toBool()) {
        singleFile._item->_httpErrorCode = static_cast<quint16>(200);
    } else {
        singleFile._item->_httpErrorCode = static_cast<quint16>(412);
    }

    singleFile._item->_responseTimeStamp = job->responseTimestamp();
    singleFile._item->_requestId = job->requestId();
    if (singleFile._item->_httpErrorCode != 200) {
        commonErrorHandling(singleFile._item, fileReply[QStringLiteral("message")].toString());
        const auto exceptionParsed = getExceptionFromReply(job->reply());
        singleFile._item->_errorExceptionName = exceptionParsed.first;
        singleFile._item->_errorExceptionMessage = exceptionParsed.second;
        return;
    }

    singleFile._item->_status = SyncFileItem::Success;

    // Check the file again post upload.
    // Two cases must be considered separately: If the upload is finished,
    // the file is on the server and has a changed ETag. In that case,
    // the etag has to be properly updated in the client journal, and because
    // of that we can bail out here with an error. But we can reschedule a
    // sync ASAP.
    // But if the upload is ongoing, because not all chunks were uploaded
    // yet, the upload can be stopped and an error can be displayed, because
    // the server hasn't registered the new file yet.
    const auto etag = getEtagFromJsonReply(fileReply);
    finished = etag.length() > 0;

    const auto fullFilePath(propagator()->fullLocalPath(singleFile._item->_file));

    // Check if the file still exists
    if (!checkFileStillExists(singleFile._item, finished, fullFilePath)) {
        return;
    }

    // Check whether the file changed since discovery. the file check here is the original and not the temporary.
    if (!checkFileChanged(singleFile._item, finished, fullFilePath)) {
        return;
    }

    // the file id should only be empty for new files up- or downloaded
    computeFileId(singleFile._item, fileReply);

    if (SyncJournalFileRecord oldRecord; propagator()->_journal->getFileRecord(singleFile._item->destination(), &oldRecord) && oldRecord.isValid()) {
        if (oldRecord._etag != singleFile._item->_etag) {
            singleFile._item->updateLockStateFromDbRecord(oldRecord);
        }
    }

    singleFile._item->_etag = etag;
    singleFile._item->_fileId = getHeaderFromJsonReply(fileReply, "fileid");
    singleFile._item->_remotePerm = RemotePermissions::fromServerString(getHeaderFromJsonReply(fileReply, "permissions"));
    singleFile._item->_isShared = singleFile._item->_remotePerm.hasPermission(RemotePermissions::IsShared) || singleFile._item->_sharedByMe;
    singleFile._item->_lastShareStateFetchedTimestamp = QDateTime::currentMSecsSinceEpoch();

    if (getHeaderFromJsonReply(fileReply, "X-OC-MTime") != "accepted") {
        // X-OC-MTime is supported since owncloud 5.0.   But not when chunking.
        // Normally Owncloud 6 always puts X-OC-MTime
        qCWarning(lcBulkPropagatorJob) << "Server does not support X-OC-MTime" << getHeaderFromJsonReply(fileReply, "X-OC-MTime");
        // Well, the mtime was not set
    }
}

void BulkPropagatorJob::slotPutFinished()
{
    const auto job = qobject_cast<PutMultiFileJob *>(sender());
    Q_ASSERT(job);

    slotJobDestroyed(job); // remove it from the _jobs list

    const auto jobError = job->reply()->error();

    const auto replyData = job->reply()->readAll();
    const auto replyJson = QJsonDocument::fromJson(replyData);
    const auto fullReplyObject = replyJson.object();

    for (const auto &singleFile : _filesToUpload) {
        if (!fullReplyObject.contains(singleFile._remotePath)) {
            if (jobError != QNetworkReply::NoError) {
                singleFile._item->_status = SyncFileItem::NormalError;
                abortWithError(singleFile._item, SyncFileItem::NormalError, tr("Network error: %1").arg(jobError));
            }
            continue;
        }
        const auto singleReplyObject = fullReplyObject[singleFile._remotePath].toObject();
        slotPutFinishedOneFile(singleFile, job, singleReplyObject);
    }

    finalize(fullReplyObject);
}

void BulkPropagatorJob::slotUploadProgress(SyncFileItemPtr item, qint64 sent, qint64 total)
{
    // Completion is signaled with sent=0, total=0; avoid accidentally
    // resetting progress due to the sent being zero by ignoring it.
    // finishedSignal() is bound to be emitted soon anyway.
    // See https://bugreports.qt.io/browse/QTBUG-44782.
    _sentTotal += sent;
    if (sent == 0 && total == 0) {
        return;
    }
    propagator()->reportProgress(*item, _sentTotal);
}

void BulkPropagatorJob::slotJobDestroyed(QObject *job)
{
    _jobs.erase(std::remove(_jobs.begin(), _jobs.end(), job), _jobs.end());
}

void BulkPropagatorJob::adjustLastJobTimeout(AbstractNetworkJob *job, qint64 fileSize) const
{
    constexpr double threeMinutes = 3.0 * 60 * 1000;
    const auto timeBound = qBound(job->timeoutMsec(),
                                  // Calculate 3 minutes for each gigabyte of data
                                  qRound64(threeMinutes * static_cast<double>(fileSize) / 1e9),
                                  // Maximum of 30 minutes
                                  static_cast<qint64>(30 * 60 * 1000));

    job->setTimeout(timeBound);
}

void BulkPropagatorJob::finalizeOneFile(const BulkUploadItem &oneFile)
{
    // Update the database entry
    const auto result = propagator()->updateMetadata(*oneFile._item, Vfs::UpdateMetadataType::DatabaseMetadata);
    if (!result) {
        done(oneFile._item, SyncFileItem::FatalError, tr("Error updating metadata: %1").arg(result.error()), ErrorCategory::GenericError);
        return;
    } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
        done(oneFile._item, SyncFileItem::SoftError, tr("The file %1 is currently in use").arg(oneFile._item->_file), ErrorCategory::GenericError);
        return;
    }

    // Files that were new on the remote shouldn't have online-only pin state
    // even if their parent folder is online-only.
    if (oneFile._item->_instruction == CSYNC_INSTRUCTION_NEW
        || oneFile._item->_instruction == CSYNC_INSTRUCTION_TYPE_CHANGE) {
        auto &vfs = propagator()->syncOptions()._vfs;
        const auto pin = vfs->pinState(oneFile._item->_file);
        if (pin && *pin == PinState::OnlineOnly && !vfs->setPinState(oneFile._item->_file, PinState::Unspecified)) {
            qCWarning(lcBulkPropagatorJob) << "Could not set pin state of" << oneFile._item->_file << "to unspecified";
        }
    }

    // Remove from the progress database:
    propagator()->_journal->setUploadInfo(oneFile._item->_file, SyncJournalDb::UploadInfo());
    propagator()->_journal->commit("upload file start");
}

void BulkPropagatorJob::finalize(const QJsonObject &fullReply)
{
    qCDebug(lcBulkPropagatorJob) << "Received a full reply" << fullReply;

    for(auto singleFileIt = std::begin(_filesToUpload); singleFileIt != std::end(_filesToUpload); ) {
        const auto &singleFile = *singleFileIt;

        if (!fullReply.contains(singleFile._remotePath)) {
            ++singleFileIt;
            continue;
        }
        if (!singleFile._item->hasErrorStatus()) {
            finalizeOneFile(singleFile);
            singleFile._item->_status = OCC::SyncFileItem::Success;
        }

        done(singleFile._item, singleFile._item->_status, {}, ErrorCategory::GenericError);

        singleFileIt = _filesToUpload.erase(singleFileIt);
    }

    checkPropagationIsDone();
}

void BulkPropagatorJob::done(SyncFileItemPtr item,
                             const SyncFileItem::Status status,
                             const QString &errorString,
                             const ErrorCategory category)
{
    item->_status = status;
    item->_errorString = errorString;

    qCInfo(lcBulkPropagatorJob) << "Item completed"
                                << item->destination()
                                << item->_status
                                << item->_instruction
                                << item->_errorString;

    handleFileRestoration(item, errorString);

    if (propagator()->_abortRequested && (item->_status == SyncFileItem::NormalError
                                          || item->_status == SyncFileItem::FatalError)) {
        // an abort request is ongoing. Change the status to Soft-Error
        item->_status = SyncFileItem::SoftError;
    }

    if (item->_status != SyncFileItem::Success) {
        // Blacklist handling
        handleBulkUploadBlackList(item);
        propagator()->_anotherSyncNeeded = true;
    }

    handleJobDoneErrors(item, status);

    emit propagator()->itemCompleted(item, category);
}

QMap<QByteArray, QByteArray> BulkPropagatorJob::headers(SyncFileItemPtr item) const
{
    QMap<QByteArray, QByteArray> headers;
    headers[QByteArrayLiteral("Content-Type")] = QByteArrayLiteral("application/octet-stream");
    headers[QByteArrayLiteral("X-File-Mtime")] = QByteArray::number(qint64(item->_modtime));

    if (qEnvironmentVariableIntValue("OWNCLOUD_LAZYOPS")) {
        headers[QByteArrayLiteral("OC-LazyOps")] = QByteArrayLiteral("true");
    }

    if (item->_file.contains(QLatin1String(".sys.admin#recall#"))) {
        // This is a file recall triggered by the admin.  Note: the
        // recall list file created by the admin and downloaded by the
        // client (.sys.admin#recall#) also falls into this category
        // (albeit users are not supposed to mess up with it)

        // We use a special tag header so that the server may decide to store this file away in some admin stage area
        // And not directly in the user's area (which would trigger redownloads etc).
        headers["OC-Tag"] = ".sys.admin#recall#";
    }

    if (!item->_etag.isEmpty() && item->_etag != "empty_etag"
        && item->_instruction != CSYNC_INSTRUCTION_NEW // On new files never send a If-Match
        && item->_instruction != CSYNC_INSTRUCTION_TYPE_CHANGE) {
        // We add quotes because the owncloud server always adds quotes around the etag, and
        //  csync_owncloud.c's owncloud_file_id always strips the quotes.
        headers[QByteArrayLiteral("If-Match")] = '"' + item->_etag + '"';
    }

    // Set up a conflict file header pointing to the original file
    auto conflictRecord = propagator()->_journal->conflictRecord(item->_file.toUtf8());
    if (conflictRecord.isValid()) {
        headers[QByteArrayLiteral("OC-Conflict")] = "1";
        if (!conflictRecord.initialBasePath.isEmpty()) {
            headers[QByteArrayLiteral("OC-ConflictInitialBasePath")] = conflictRecord.initialBasePath;
        }
        if (!conflictRecord.baseFileId.isEmpty()) {
            headers[QByteArrayLiteral("OC-ConflictBaseFileId")] = conflictRecord.baseFileId;
        }
        if (conflictRecord.baseModtime != -1) {
            headers[QByteArrayLiteral("OC-ConflictBaseMtime")] = QByteArray::number(conflictRecord.baseModtime);
        }
        if (!conflictRecord.baseEtag.isEmpty()) {
            headers[QByteArrayLiteral("OC-ConflictBaseEtag")] = conflictRecord.baseEtag;
        }
    }

    return headers;
}

void BulkPropagatorJob::abortWithError(SyncFileItemPtr item,
                                       SyncFileItem::Status status,
                                       const QString &error)
{
    abort(AbortType::Synchronous);
    done(item, status, error, ErrorCategory::GenericError);
}

void BulkPropagatorJob::checkResettingErrors(SyncFileItemPtr item) const
{
    if (item->_httpErrorCode == 412
        || propagator()->account()->capabilities().httpErrorCodesThatResetFailingChunkedUploads().contains(item->_httpErrorCode)) {

        auto uploadInfo = propagator()->_journal->getUploadInfo(item->_file);
        uploadInfo._errorCount += 1;
        if (uploadInfo._errorCount > 3) {
            qCInfo(lcBulkPropagatorJob) << "Reset transfer of" << item->_file
                                        << "due to repeated error" << item->_httpErrorCode;
            uploadInfo = SyncJournalDb::UploadInfo();
        } else {
            qCInfo(lcBulkPropagatorJob) << "Error count for maybe-reset error" << item->_httpErrorCode
                                        << "on file" << item->_file
                                        << "is" << uploadInfo._errorCount;
        }
        propagator()->_journal->setUploadInfo(item->_file, uploadInfo);
        propagator()->_journal->commit("Upload info");
    }
}

void BulkPropagatorJob::commonErrorHandling(SyncFileItemPtr item,
                                            const QString &errorMessage)
{
    // Ensure errors that should eventually reset the chunked upload are tracked.
    checkResettingErrors(item);
    abortWithError(item, SyncFileItem::NormalError, errorMessage);
}

bool BulkPropagatorJob::checkFileStillExists(SyncFileItemPtr item,
                                             const bool finished,
                                             const QString &fullFilePath)
{
    if (!FileSystem::fileExists(fullFilePath)) {
        if (!finished) {
            abortWithError(item, SyncFileItem::SoftError, tr("The local file was removed during sync."));
            return false;
        } else {
            propagator()->_anotherSyncNeeded = true;
        }
    }

    return true;
}

bool BulkPropagatorJob::checkFileChanged(SyncFileItemPtr item,
                                         const bool finished,
                                         const QString &fullFilePath)
{
    if (!FileSystem::verifyFileUnchanged(fullFilePath, item->_size, item->_modtime)) {
        propagator()->_anotherSyncNeeded = true;

        if (!finished) {
            abortWithError(item, SyncFileItem::SoftError, tr("Local file changed during sync."));
            // FIXME:  the legacy code was retrying for a few seconds.
            //         and also checking that after the last chunk, and removed the file in case of INSTRUCTION_NEW
            return false;
        }
    }

    return true;
}

void BulkPropagatorJob::computeFileId(SyncFileItemPtr item,
                                      const QJsonObject &fileReply) const
{
    const auto fid = getHeaderFromJsonReply(fileReply, "OC-FileID");
    if (!fid.isEmpty()) {
        if (!item->_fileId.isEmpty() && item->_fileId != fid) {
            qCWarning(lcBulkPropagatorJob) << "File ID changed!" << item->_fileId << fid;
        }
        item->_fileId = fid;
    }
}

void BulkPropagatorJob::handleFileRestoration(SyncFileItemPtr item,
                                              const QString &errorString) const
{
    if (item->_isRestoration) {
        if (item->_status == SyncFileItem::Success
            || item->_status == SyncFileItem::Conflict) {
            item->_status = SyncFileItem::Restoration;
        } else {
            item->_errorString += tr("Restoration failed: %1").arg(errorString);
        }
    } else if (item->_errorString.isEmpty()) {
        item->_errorString = errorString;
    }
}

void BulkPropagatorJob::handleBulkUploadBlackList(SyncFileItemPtr item) const
{
    propagator()->addToBulkUploadBlackList(item->_file);
}

void BulkPropagatorJob::handleJobDoneErrors(SyncFileItemPtr item,
                                            SyncFileItem::Status status)
{
    if (item->hasErrorStatus()) {
        qCWarning(lcPropagator) << "Could not complete propagation of" << item->destination()
                                << "by" << this
                                << "with status" << item->_status
                                << "and error:" << item->_errorString;
    } else {
        qCInfo(lcPropagator) << "Completed propagation of" << item->destination()
                             << "by" << this
                             << "with status" << item->_status;
    }

    if (item->_status == SyncFileItem::FatalError) {
        // Abort all remaining jobs.
        propagator()->abort();
    }

    switch (item->_status)
    {
    case SyncFileItem::BlacklistedError:
    case SyncFileItem::Conflict:
    case SyncFileItem::FatalError:
    case SyncFileItem::FileIgnored:
    case SyncFileItem::FileLocked:
    case SyncFileItem::FileNameInvalid:
    case SyncFileItem::FileNameInvalidOnServer:
    case SyncFileItem::FileNameClash:
    case SyncFileItem::NoStatus:
    case SyncFileItem::NormalError:
    case SyncFileItem::Restoration:
    case SyncFileItem::SoftError:
        _finalStatus = SyncFileItem::NormalError;
        qCInfo(lcBulkPropagatorJob) << "modify final status NormalError" << _finalStatus << status;
        break;
    case SyncFileItem::DetailError:
        _finalStatus = SyncFileItem::DetailError;
        qCInfo(lcBulkPropagatorJob) << "modify final status DetailError" << _finalStatus << status;
        break;
    case SyncFileItem::Success:
        break;
    }
}

}
