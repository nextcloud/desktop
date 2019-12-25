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

#include "propagateremotemkdir.h"
#include "owncloudpropagator_p.h"
#include "account.h"
#include "common/syncjournalfilerecord.h"
#include "propagateremotedelete.h"
#include "clientsideencryptionjobs.h"
#include "clientsideencryption.h"
#include "common/asserts.h"

#include <QFile>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateRemoteMkdir, "nextcloud.sync.propagator.remotemkdir", QtInfoMsg)


void PropagateRemoteMkdir::start()
{
    // TODO: add code to check if the server supports e2e at all, if not, go to normal path directly

    // JORE: adding code here
    int slashPos = _item->_file.lastIndexOf('/');
    QString parentDir = slashPos <= 0 ? "" : _item->_file.mid(0, slashPos);
    qCInfo(lcPropagateRemoteMkdir) << "JORE parentDir=" << parentDir;

    // code to check if parentDir is encrypted on the server 
    auto getEncryptedStatus = new GetFolderEncryptStatusJob(_propagator->account(), parentDir);

    connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusFolderReceived,
          this, &PropagateRemoteMkdir::slotFolderEncryptedStatusFetched);
    connect(getEncryptedStatus, &GetFolderEncryptStatusJob::encryptStatusError,
         this, &PropagateRemoteMkdir::slotFolderEncryptedStatusError);
    getEncryptedStatus->start();
    _job = getEncryptedStatus;
    // JORE: end code
}

void PropagateRemoteMkdir::slotFolderEncryptedStatusFetched(const QString &folder, bool isEncrypted)
{
    qCDebug(lcPropagateRemoteMkdir) << "Encrypted Status Fetched" << folder << isEncrypted;

    /* We are inside an encrypted folder, we need to mark the current (new) directory as encrypted */
    if (isEncrypted) {
        qCInfo(lcPropagateRemoteMkdir) << "JORE: Parent Folder is encrypted, let's encrypt this new folder";
        // TODO : add code to mark this folder as encrypted as well, check if this needs to be done 
        //        before or after creating it. probably after... how?
        _needsEncryption = true;
        slotStartMkdir(); // for now following the normal path
    } else {
        qCInfo(lcPropagateRemoteMkdir) << "JORE: Parent folder is not encrypted, getting back to default.";
        slotStartMkdir();
    }
}


void PropagateRemoteMkdir::slotFolderEncryptedStatusError(int error)
{
    qCInfo(lcPropagateRemoteMkdir) << "Failed to retrieve the encryption status of the parent folder." << error;
}



void PropagateRemoteMkdir::slotStartMkdir()
{
    if (propagator()->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qCDebug(lcPropagateRemoteMkdir) << _item->_file;

    propagator()->_activeJobList.append(this);

    if (!_deleteExisting) {
        return slotStartMkcolJob();
    }

    _job = new DeleteJob(propagator()->account(),
        propagator()->_remoteFolder + _item->_file,
        this);
    connect(_job, SIGNAL(finishedSignal()), SLOT(slotStartMkcolJob()));
    _job->start();
}

void PropagateRemoteMkdir::slotStartMkcolJob()
{
    if (propagator()->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qCDebug(lcPropagateRemoteMkdir) << _item->_file;

    _job = new MkColJob(propagator()->account(),
        propagator()->_remoteFolder + _item->_file,
        this);
    connect(_job, SIGNAL(finished(QNetworkReply::NetworkError)), this, SLOT(slotMkcolJobFinished()));
    _job->start();
}

void PropagateRemoteMkdir::abort(PropagatorJob::AbortType abortType)
{
    if (_job && _job->reply())
        _job->reply()->abort();

    if (abortType == AbortType::Asynchronous) {
        emit abortFinished();
    }
}

void PropagateRemoteMkdir::setDeleteExisting(bool enabled)
{
    _deleteExisting = enabled;
}

void PropagateRemoteMkdir::slotMkcolJobFinished()
{
    propagator()->_activeJobList.removeOne(this);

    ASSERT(_job);

    QNetworkReply::NetworkError err = _job->reply()->error();
    _item->_httpErrorCode = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (_item->_httpErrorCode == 405) {
        // This happens when the directory already exists. Nothing to do.
    } else if (err != QNetworkReply::NoError) {
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
            &propagator()->_anotherSyncNeeded);
        done(status, _job->errorString());
        return;
    } else if (_item->_httpErrorCode != 201) {
        // Normally we expect "201 Created"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError,
            tr("Wrong HTTP code returned by server. Expected 201, but received \"%1 %2\".")
                .arg(_item->_httpErrorCode)
                .arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
        return;
    }

    _item->_responseTimeStamp = _job->responseTimestamp();
    _item->_fileId = _job->reply()->rawHeader("OC-FileId");

    if (_item->_fileId.isEmpty()) {
        // Owncloud 7.0.0 and before did not have a header with the file id.
        // (https://github.com/owncloud/core/issues/9000)
        // So we must get the file id using a PROPFIND
        // This is required so that we can detect moves even if the folder is renamed on the server
        // while files are still uploading
        propagator()->_activeJobList.append(this);
        auto propfindJob = new PropfindJob(_job->account(), _job->path(), this);
        propfindJob->setProperties(QList<QByteArray>() << "getetag"
                                                       << "http://owncloud.org/ns:id");
        QObject::connect(propfindJob, &PropfindJob::result, this, &PropagateRemoteMkdir::propfindResult);
        QObject::connect(propfindJob, &PropfindJob::finishedWithError, this, &PropagateRemoteMkdir::propfindError);
        propfindJob->start();
        _job = propfindJob;
        return;
    }

    // mark folder encrypted on the server after creating it but before adding any more files to it
    if (_needsEncryption) {
        slotStartMarkEncryptedJob();
    }
}

void PropagateRemoteMkdir::propfindResult(const QVariantMap &result)
{
    propagator()->_activeJobList.removeOne(this);
    if (result.contains("getetag")) {
        _item->_etag = result["getetag"].toByteArray();
    }
    if (result.contains("id")) {
        _item->_fileId = result["id"].toByteArray();
    }

    // mark folder encrypted on the server after creating it but before adding any more files to it
    if (_needsEncryption) {
        slotStartMarkEncryptedJob();
    }
}

void PropagateRemoteMkdir::propfindError()
{
    // ignore the PROPFIND error
    propagator()->_activeJobList.removeOne(this);
    done(SyncFileItem::Success);
}


/////////////////////////////////////////
// Encryption functions 
//

void PropagateRemoteMkdir::slotStartMarkEncryptedJob()
{
    qCInfo(lcPropagateRemoteMkdir) << "Encrypting the new folder " << _item->_file;
    propagator()->_activeJobList.append(this);
    auto encryptJob = new OCC::SetEncryptionFlagApiJob(_job->account(), _item->_fileId);
    connect(encryptJob, &OCC::SetEncryptionFlagApiJob::success, this, &PropagateRemoteMkdir::slotEncryptionFlagSuccess);
    connect(encryptJob, &OCC::SetEncryptionFlagApiJob::error, this, &PropagateRemoteMkdir::slotEncryptionFlagError);
    encryptJob->start();
    _job = encryptJob;
}

void PropagateRemoteMkdir::slotEncryptionFlagSuccess(const QByteArray& fileId)
{
//    if (auto info = _model->infoForFileId(fileId)) {
      _job->account()->e2e()->setFolderEncryptedStatus(_item->_file, true);
//    } else {
//      qCInfo(lcPropagateRemoteMkdir) << "Could not get information from the current folder.";
//    }
    auto lockJob = new LockEncryptFolderApiJob(_job->account(), fileId);
    connect(lockJob, &LockEncryptFolderApiJob::success,
            this, &PropagateRemoteMkdir::slotLockForEncryptionSuccess);
    connect(lockJob, &LockEncryptFolderApiJob::error,
            this, &PropagateRemoteMkdir::slotLockForEncryptionError);
    lockJob->start();
    _job = lockJob;
}

void PropagateRemoteMkdir::slotEncryptionFlagError(const QByteArray& fileId, int httpErrorCode)
{
    propagator()->_activeJobList.removeOne(this);

    Q_UNUSED(fileId);
    Q_UNUSED(httpErrorCode);
    qDebug() << "Error on the encryption flag";
}

void PropagateRemoteMkdir::slotLockForEncryptionSuccess(const QByteArray& fileId, const QByteArray &token)
{
    _job->account()->e2e()->setTokenForFolder(fileId, token);

    FolderMetadata emptyMetadata(_job->account());
    auto encryptedMetadata = emptyMetadata.encryptedMetadata();
    if (encryptedMetadata.isEmpty()) {
        //TODO: Mark the folder as unencrypted as the metadata generation failed.
        // TODO: delete the folder and fail the operation !!
        qCWarning(lcPropagateRemoteMkdir) << "Error marking the folder as encrypted. This is a problem.";
/*
      QMessageBox::warning(nullptr, "Warning",
          "Could not generate the metadata for encryption, Unlocking the folder. \n"
          "This can be an issue with your OpenSSL libraries, please note that OpenSSL 1.1 is \n"
          "not compatible with Nextcloud yet."
      );
*/
      return;
    }
    auto storeMetadataJob = new StoreMetaDataApiJob(_job->account(), fileId, emptyMetadata.encryptedMetadata());
    connect(storeMetadataJob, &StoreMetaDataApiJob::success,
                    this, &PropagateRemoteMkdir::slotUploadMetadataSuccess);
    connect(storeMetadataJob, &StoreMetaDataApiJob::error,
                    this, &PropagateRemoteMkdir::slotUpdateMetadataError);

    storeMetadataJob->start();
    _job = storeMetadataJob;
}

void PropagateRemoteMkdir::slotUploadMetadataSuccess(const QByteArray& folderId)
{
    const auto token = _job->account()->e2e()->tokenForFolder(folderId);
    auto unlockJob = new UnlockEncryptFolderApiJob(_job->account(), folderId, token);
    connect(unlockJob, &UnlockEncryptFolderApiJob::success,
                    this, &PropagateRemoteMkdir::slotUnlockFolderSuccess);
    connect(unlockJob, &UnlockEncryptFolderApiJob::error,
                    this, &PropagateRemoteMkdir::slotUnlockFolderError);
    unlockJob->start();
    _job = unlockJob;
}

void PropagateRemoteMkdir::slotUpdateMetadataError(const QByteArray& folderId, int httpReturnCode)
{
    Q_UNUSED(httpReturnCode);

    const auto token = _job->account()->e2e()->tokenForFolder(folderId);
    auto unlockJob = new UnlockEncryptFolderApiJob(_job->account(), folderId, token);
    connect(unlockJob, &UnlockEncryptFolderApiJob::success,
                    this, &PropagateRemoteMkdir::slotUnlockFolderSuccess);
    connect(unlockJob, &UnlockEncryptFolderApiJob::error,
                    this, &PropagateRemoteMkdir::slotUnlockFolderError);
    unlockJob->start();
    _job = unlockJob;
}

void PropagateRemoteMkdir::slotLockForEncryptionError(const QByteArray& fileId, int httpErrorCode)
{
    propagator()->_activeJobList.removeOne(this);

    Q_UNUSED(fileId);
    Q_UNUSED(httpErrorCode);

    qCInfo(lcPropagateRemoteMkdir) << "Locking error" << httpErrorCode;

    done(SyncFileItem::NormalError, "Error locking directory");
}

void PropagateRemoteMkdir::slotUnlockFolderError(const QByteArray& fileId, int httpErrorCode)
{
    propagator()->_activeJobList.removeOne(this);

    Q_UNUSED(fileId);
    Q_UNUSED(httpErrorCode);

    qCInfo(lcPropagateRemoteMkdir) << "Unlocking error!" << _item->_file;

    done(SyncFileItem::NormalError, "Error unlocking directory");
}

void PropagateRemoteMkdir::slotUnlockFolderSuccess(const QByteArray& fileId)
{
    propagator()->_activeJobList.removeOne(this);

    Q_UNUSED(fileId);

    qCInfo(lcPropagateRemoteMkdir) << "Unlocking success!" << _item->_file;

    success();
}


// call when all is done (including encryption, locking, etc)
void PropagateRemoteMkdir::success()
{
    // save the file id already so we can detect rename or remove
    SyncJournalFileRecord record = _item->toSyncJournalFileRecordWithInode(propagator()->_localDir + _item->destination());
    if (!propagator()->_journal->setFileRecord(record)) {
        done(SyncFileItem::FatalError, tr("Error writing metadata to the database"));
        return;
    }

    qCInfo(lcPropagateRemoteMkdir) << "Marking job as a success" << _item->_file;
    done(SyncFileItem::Success);
}
}
