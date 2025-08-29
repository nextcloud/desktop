/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "propagateremotedelete.h"
#include "propagateremotedeleteencrypted.h"
#include "propagateremotedeleteencryptedrootfolder.h"
#include "owncloudpropagator_p.h"
#include "account.h"
#include "deletejob.h"
#include "common/asserts.h"

#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateRemoteDelete, "nextcloud.sync.propagator.remotedelete", QtInfoMsg)

void PropagateRemoteDelete::start()
{
    qCInfo(lcPropagateRemoteDelete) << "Start propagate remote delete job for" << _item->_file;
    qCInfo(lcPermanentLog) << "delete" << _item->_file << _item->_discoveryResult;

    if (propagator()->_abortRequested)
        return;

    if (!_item->_encryptedFileName.isEmpty() || _item->isEncrypted()) {
        if (!_item->_encryptedFileName.isEmpty()) {
            _deleteEncryptedHelper = new PropagateRemoteDeleteEncrypted(propagator(), _item, this);
        } else {
            _deleteEncryptedHelper = new PropagateRemoteDeleteEncryptedRootFolder(propagator(), _item, this);
        }
        connect(_deleteEncryptedHelper, &BasePropagateRemoteDeleteEncrypted::finished, this, [this] (bool success) {
            if (!success) {
                SyncFileItem::Status status = SyncFileItem::NormalError;
                if (_deleteEncryptedHelper->networkError() != QNetworkReply::NoError && _deleteEncryptedHelper->networkError() != QNetworkReply::ContentNotFoundError) {
                    status = classifyError(_deleteEncryptedHelper->networkError(), _item->_httpErrorCode, &propagator()->_anotherSyncNeeded);
                }
                done(status, _deleteEncryptedHelper->errorString(), ErrorCategory::GenericError);
            } else {
                done(SyncFileItem::Success, {}, ErrorCategory::NoError);
            }
        });
        _deleteEncryptedHelper->start();
    } else {
        createDeleteJob(_item->_file);
    }
}

void PropagateRemoteDelete::createDeleteJob(const QString &filename)
{
    Q_ASSERT(propagator());
    auto remoteFilename = filename;
    if (_item->_type == ItemType::ItemTypeVirtualFile) {
        if (const auto vfs = propagator()->syncOptions()._vfs; vfs->mode() == Vfs::Mode::WithSuffix) {
            // These are compile-time constants so no need to recreate each time
            static constexpr auto suffixSize = std::string_view(APPLICATION_DOTVIRTUALFILE_SUFFIX).size();
            remoteFilename.chop(suffixSize);
        }
    }

    qCInfo(lcPropagateRemoteDelete) << "Deleting file, local" << _item->_file << "remote" << remoteFilename << "wantsPermanentDeletion" << (_item->_wantsSpecificActions == SyncFileItem::SynchronizationOptions::WantsPermanentDeletion ? "true" : "false");

    auto headers = QMap<QByteArray, QByteArray>{};
    if (_item->_locked == SyncFileItem::LockStatus::LockedItem) {
        headers[QByteArrayLiteral("If")] = (QLatin1String("<") + propagator()->account()->davUrl().toString() + _item->_file + "> (<opaquelocktoken:" + _item->_lockToken.toUtf8() + ">)").toUtf8();
    }
    _job = new DeleteJob(propagator()->account(), propagator()->fullRemotePath(remoteFilename), headers, this);
    _job->setSkipTrashbin(_item->_wantsSpecificActions == SyncFileItem::SynchronizationOptions::WantsPermanentDeletion);
    connect(_job.data(), &DeleteJob::finishedSignal, this, &PropagateRemoteDelete::slotDeleteJobFinished);
    propagator()->_activeJobList.append(this);
    _job->start();
}

void PropagateRemoteDelete::abort(PropagatorJob::AbortType abortType)
{
    if (_job && _job->reply())
        _job->reply()->abort();

    if (abortType == AbortType::Asynchronous) {
        emit abortFinished();
    }
}

void PropagateRemoteDelete::slotDeleteJobFinished()
{
    propagator()->_activeJobList.removeOne(this);

    ASSERT(_job);

    QNetworkReply::NetworkError err = _job->reply()->error();
    const int httpStatus = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_httpErrorCode = httpStatus;
    _item->_responseTimeStamp = _job->responseTimestamp();
    _item->_requestId = _job->requestId();

    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
            &propagator()->_anotherSyncNeeded);

        done(status, _job->errorString(), errorCategoryFromNetworkError(err));
        return;
    }

    // A 404 reply is also considered a success here: We want to make sure
    // a file is gone from the server. It not being there in the first place
    // is ok. This will happen for files that are in the DB but not on
    // the server or the local file system.
    if (httpStatus != 204 && httpStatus != 404) {
        // Normally we expect "204 No Content"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError,
            tr("Wrong HTTP code returned by server. Expected 204, but received \"%1 %2\".")
                .arg(_item->_httpErrorCode)
                .arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()), ErrorCategory::GenericError);
        return;
    }

    if (!propagator()->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory())) {
        qCWarning(lcPropagateRemoteDelete) << "could not delete file from local DB" << _item->_originalFile;
        done(SyncFileItem::NormalError, tr("Could not delete file record %1 from local DB").arg(_item->_originalFile), ErrorCategory::GenericError);
        return;
    }

    propagator()->_journal->commit("Remote Remove");

    done(SyncFileItem::Success, {}, ErrorCategory::NoError);
}
}
