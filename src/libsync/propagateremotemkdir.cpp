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
#include "propagateuploadencrypted.h"
#include "deletejob.h"
#include "common/asserts.h"
#include "encryptfolderjob.h"
#include "filesystem.h"
#include "csync/csync.h"

#include <QFile>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateRemoteMkdir, "nextcloud.sync.propagator.remotemkdir", QtInfoMsg)

PropagateRemoteMkdir::PropagateRemoteMkdir(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
    : PropagateItemJob(propagator, item)
{
    const auto path = _item->_file;
    const auto slashPosition = path.lastIndexOf('/');
    const auto parentPath = slashPosition >= 0 ? path.left(slashPosition) : QString();

    SyncJournalFileRecord parentRec;
    bool ok = propagator->_journal->getFileRecord(parentPath, &parentRec);
    if (!ok) {
        return;
    }
}

void PropagateRemoteMkdir::start()
{
    if (propagator()->_abortRequested)
        return;

    qCDebug(lcPropagateRemoteMkdir) << _item->_file;

    propagator()->_activeJobList.append(this);

    if (!_deleteExisting) {
        slotMkdir();
        return;
    }

    _job = new DeleteJob(propagator()->account(),
        propagator()->fullRemotePath(_item->_file),
        this);
    connect(qobject_cast<DeleteJob *>(_job), &DeleteJob::finishedSignal, this, &PropagateRemoteMkdir::slotMkdir);
    _job->start();
}

void PropagateRemoteMkdir::slotStartMkcolJob()
{
    if (propagator()->_abortRequested)
        return;

    qCDebug(lcPropagateRemoteMkdir) << _item->_file;

    _job = new MkColJob(propagator()->account(),
        propagator()->fullRemotePath(_item->_file),
        this);
    connect(qobject_cast<MkColJob *>(_job), &MkColJob::finishedWithError, this, &PropagateRemoteMkdir::slotMkcolJobFinished);
    connect(qobject_cast<MkColJob *>(_job), &MkColJob::finishedWithoutError, this, &PropagateRemoteMkdir::slotMkcolJobFinished);
    _job->start();
}

void PropagateRemoteMkdir::slotStartEncryptedMkcolJob(const QString &path, const QString &filename, quint64 size)
{
    Q_UNUSED(path)
    Q_UNUSED(size)

    if (propagator()->_abortRequested)
        return;

    qDebug() << filename;
    qCDebug(lcPropagateRemoteMkdir) << filename;

    auto job = new MkColJob(propagator()->account(),
                            propagator()->fullRemotePath(filename),
                            {{"e2e-token", _uploadEncryptedHelper->folderToken() }},
                            this);
    connect(job, &MkColJob::finishedWithError, this, &PropagateRemoteMkdir::slotMkcolJobFinished);
    connect(job, &MkColJob::finishedWithoutError, this, &PropagateRemoteMkdir::slotMkcolJobFinished);
    _job = job;
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

void PropagateRemoteMkdir::finalizeMkColJob(QNetworkReply::NetworkError err, const QString &jobHttpReasonPhraseString, const QString &jobPath)
{
    if (_item->_httpErrorCode == 405) {
        // This happens when the directory already exists. Nothing to do.
        qDebug(lcPropagateRemoteMkdir) << "Folder" << jobPath << "already exists.";
    } else if (err != QNetworkReply::NoError) {
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
            &propagator()->_anotherSyncNeeded);
        done(status, _item->_errorString, errorCategoryFromNetworkError(err));
        return;
    } else if (_item->_httpErrorCode != 201) {
        // Normally we expect "201 Created"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError,
            tr("Wrong HTTP code returned by server. Expected 201, but received \"%1 %2\".")
                .arg(_item->_httpErrorCode)
                .arg(jobHttpReasonPhraseString), ErrorCategory::GenericError);
        return;
    }

    propagator()->_activeJobList.append(this);
    auto propfindJob = new PropfindJob(propagator()->account(), jobPath, this);
    propfindJob->setProperties({QByteArrayLiteral("http://owncloud.org/ns:share-types"), QByteArrayLiteral("http://owncloud.org/ns:permissions"), QByteArrayLiteral("http://nextcloud.org/ns:is-mount-root")});
    connect(propfindJob, &PropfindJob::result, this, [this, jobPath](const QVariantMap &result){
        propagator()->_activeJobList.removeOne(this);
        _item->_remotePerm = RemotePermissions::fromServerString(result.value(QStringLiteral("permissions")).toString(),
                                                                 propagator()->account()->serverHasMountRootProperty() ? RemotePermissions::MountedPermissionAlgorithm::UseMountRootProperty : RemotePermissions::MountedPermissionAlgorithm::WildGuessMountedSubProperty,
                                                                 result);

        _item->_sharedByMe = !result.value(QStringLiteral("share-types")).toString().isEmpty();
        _item->_isShared = _item->_remotePerm.hasPermission(RemotePermissions::IsShared) || _item->_sharedByMe;
        _item->_lastShareStateFetchedTimestamp = QDateTime::currentMSecsSinceEpoch();

        if (!_uploadEncryptedHelper && !_item->isEncrypted()) {
            success();
        } else {
            // We still need to mark that folder encrypted in case we were uploading it as encrypted one
            // Another scenario, is we are creating a new folder because of move operation on an encrypted folder that works via remove + re-upload
            propagator()->_activeJobList.append(this);

            // We're expecting directory path in /Foo/Bar convention...
            Q_ASSERT(jobPath.startsWith('/') && !jobPath.endsWith('/'));
            // But encryption job expect it in Foo/Bar/ convention
            auto job = new OCC::EncryptFolderJob(propagator()->account(),
                                                 propagator()->_journal,
                                                 jobPath.mid(1),
                                                 _item->_file,
                                                 propagator()->remotePath(),
                                                 _item->_fileId,
                                                 propagator(),
                                                 _item);
            job->setParent(this);
            connect(job, &OCC::EncryptFolderJob::finished, this, &PropagateRemoteMkdir::slotEncryptFolderFinished);
            job->start();
        }
    });
    connect(propfindJob, &PropfindJob::finishedWithError, this, [this] (QNetworkReply *reply) {
        const auto err = reply ? reply->error() : QNetworkReply::NetworkError::UnknownNetworkError;
        propagator()->_activeJobList.removeOne(this);
        done(SyncFileItem::NormalError, {}, errorCategoryFromNetworkError(err));
    });
    propfindJob->start();
}

void PropagateRemoteMkdir::slotMkdir()
{
    if (!_item->_originalFile.isEmpty() && !_item->_renameTarget.isEmpty() && _item->_renameTarget != _item->_originalFile) {
        const auto existingFile = propagator()->fullLocalPath(propagator()->adjustRenamedPath(_item->_originalFile));
        const auto targetFile = propagator()->fullLocalPath(_item->_renameTarget);
        QString renameError;
        if (!FileSystem::rename(existingFile, targetFile, &renameError)) {
            done(SyncFileItem::NormalError, renameError, ErrorCategory::GenericError);
            return;
        }
        emit propagator()->touchedFile(existingFile);
        emit propagator()->touchedFile(targetFile);
    }

    const auto path = _item->_file;
    const auto slashPosition = path.lastIndexOf('/');
    const auto parentPath = slashPosition >= 0 ? path.left(slashPosition) : QString();

    SyncJournalFileRecord parentRec;
    bool ok = propagator()->_journal->getFileRecord(parentPath, &parentRec);
    if (!ok) {
        done(SyncFileItem::NormalError, {}, ErrorCategory::GenericError);
        return;
    }

    if (!hasEncryptedAncestor()) {
        slotStartMkcolJob();
        return;
    }

    // We should be encrypted as well since our parent is
    const auto remoteParentPath = parentRec._e2eMangledName.isEmpty() ? parentPath : parentRec._e2eMangledName;
    _uploadEncryptedHelper = new PropagateUploadEncrypted(propagator(), remoteParentPath, _item, this);
    connect(_uploadEncryptedHelper, &PropagateUploadEncrypted::finalized,
      this, &PropagateRemoteMkdir::slotStartEncryptedMkcolJob);
    connect(_uploadEncryptedHelper, &PropagateUploadEncrypted::error,
      []{ qCDebug(lcPropagateRemoteMkdir) << "Error setting up encryption."; });
    _uploadEncryptedHelper->start();
}

void PropagateRemoteMkdir::slotMkcolJobFinished()
{
    propagator()->_activeJobList.removeOne(this);

    ASSERT(_job);

    QNetworkReply::NetworkError err = _job->reply()->error();
    _item->_httpErrorCode = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = _job->responseTimestamp();
    _item->_requestId = _job->requestId();

    _item->_fileId = _job->reply()->rawHeader("OC-FileId");

    qCInfo(lcPropagateRemoteMkdir()) << "mkcol job error string:" << _item->_errorString << _job->errorString();
    _item->_errorString = _job->errorString();

    const auto jobHttpReasonPhraseString = _job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

    const auto jobPath = _job->path();

    if (_uploadEncryptedHelper && _uploadEncryptedHelper->isFolderLocked() && !_uploadEncryptedHelper->isUnlockRunning()) {
        // since we are done, we need to unlock a folder in case it was locked
        connect(_uploadEncryptedHelper, &PropagateUploadEncrypted::folderUnlocked, this, [this, err, jobHttpReasonPhraseString, jobPath]() {
            finalizeMkColJob(err, jobHttpReasonPhraseString, jobPath);
        });
        _uploadEncryptedHelper->unlockFolder();
    } else {
        finalizeMkColJob(err, jobHttpReasonPhraseString, jobPath);
    }
}

void PropagateRemoteMkdir::slotEncryptFolderFinished(int status, EncryptionStatusEnums::ItemEncryptionStatus encryptionStatus)
{
    if (status != EncryptFolderJob::Success) {
        done(SyncFileItem::FatalError, tr("Failed to encrypt a folder %1").arg(_item->_file), ErrorCategory::GenericError);
        return;
    }
    qCDebug(lcPropagateRemoteMkdir) << "Success making the new folder encrypted";
    propagator()->_activeJobList.removeOne(this);
    _item->_e2eEncryptionStatus = encryptionStatus;
    _item->_e2eCertificateFingerprint = propagator()->account()->encryptionCertificateFingerprint();
    _item->_e2eEncryptionStatusRemote = encryptionStatus;
    if (_item->isEncrypted()) {
        _item->_e2eEncryptionServerCapability = EncryptionStatusEnums::fromEndToEndEncryptionApiVersion(propagator()->account()->capabilities().clientSideEncryptionVersion());
    }
    success();
}

void PropagateRemoteMkdir::success()
{
    // Never save the etag on first mkdir.
    // Only fully propagated directories should have the etag set.
    auto itemCopy = *_item;
    itemCopy._etag.clear();

    // save the file id already so we can detect rename or remove
    const auto result = propagator()->updateMetadata(itemCopy);
    if (!result) {
        done(SyncFileItem::FatalError, tr("Error writing metadata to the database: %1").arg(result.error()), ErrorCategory::GenericError);
        return;
    } else if (*result == Vfs::ConvertToPlaceholderResult::Locked) {
        done(SyncFileItem::FatalError, tr("The file %1 is currently in use").arg(_item->_file), ErrorCategory::GenericError);
        return;
    }

    done(SyncFileItem::Success, {}, ErrorCategory::NoError);
}
}
