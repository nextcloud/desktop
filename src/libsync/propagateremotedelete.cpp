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

    if (propagator()->_abortRequested)
        return;

    if (!_item->_encryptedFileName.isEmpty() || _item->_isEncrypted) {
        if (!_item->_encryptedFileName.isEmpty()) {
            _deleteEncryptedHelper = new PropagateRemoteDeleteEncrypted(propagator(), _item, this);
        } else {
            _deleteEncryptedHelper = new PropagateRemoteDeleteEncryptedRootFolder(propagator(), _item, this);
        }
        connect(_deleteEncryptedHelper, &AbstractPropagateRemoteDeleteEncrypted::finished, this, [this] (bool success) {
            if (!success) {
                SyncFileItem::Status status = SyncFileItem::NormalError;
                if (_deleteEncryptedHelper->networkError() != QNetworkReply::NoError && _deleteEncryptedHelper->networkError() != QNetworkReply::ContentNotFoundError) {
                    status = classifyError(_deleteEncryptedHelper->networkError(), _item->_httpErrorCode, &propagator()->_anotherSyncNeeded);
                }
                done(status, _deleteEncryptedHelper->errorString());
            } else {
                done(SyncFileItem::Success);
            }
        });
        _deleteEncryptedHelper->start();
    } else {
        createDeleteJob(_item->_file);
    }
}

void PropagateRemoteDelete::createDeleteJob(const QString &filename)
{
    qCInfo(lcPropagateRemoteDelete) << "Deleting file, local" << _item->_file << "remote" << filename;

    _job = new DeleteJob(propagator()->account(),
        propagator()->fullRemotePath(filename),
        this);

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
        done(status, _job->errorString());
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
                .arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
        return;
    }

    propagator()->_journal->deleteFileRecord(_item->_originalFile, _item->isDirectory());
    propagator()->_journal->commit("Remote Remove");

    done(SyncFileItem::Success);
}
}
