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
#include "common/asserts.h"

#include <QFile>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcPropagateRemoteMkdir, "sync.propagator.remotemkdir", QtInfoMsg)

void PropagateRemoteMkdir::start()
{
    if (propagator()->_abortRequested)
        return;

    qCDebug(lcPropagateRemoteMkdir) << _item->_file;

    propagator()->_activeJobList.append(this);

    if (!_deleteExisting) {
        return slotStartMkcolJob();
    }

    _job = new DeleteJob(propagator()->account(),
        propagator()->fullRemotePath(_item->_file),
        this);
    connect(qobject_cast<DeleteJob *>(_job), &DeleteJob::finishedSignal, this, &PropagateRemoteMkdir::slotStartMkcolJob);
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

    OC_ASSERT(_job);

    QNetworkReply::NetworkError err = _job->reply()->error();
    _item->_httpErrorCode = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_responseTimeStamp = _job->responseTimestamp();
    _item->_requestId = _job->requestId();

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

    _item->_fileId = _job->reply()->rawHeader("OC-FileId");

    propagator()->_activeJobList.append(this);
    auto propfindJob = new PropfindJob(_job->account(), _job->path(), this);
    propfindJob->setProperties({"http://owncloud.org/ns:permissions"});
    connect(propfindJob, &PropfindJob::result, this, [this](const QMap<QString, QString> &result) {
        propagator()->_activeJobList.removeOne(this);
        _item->_remotePerm = RemotePermissions::fromServerString(result.value(QStringLiteral("permissions")));
        success();
    });
    connect(propfindJob, &PropfindJob::finishedWithError, this, [this]{
        // ignore the PROPFIND error
        propagator()->_activeJobList.removeOne(this);
        done(SyncFileItem::NormalError);
    });
    propfindJob->start();
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
        done(SyncFileItem::FatalError, tr("Error writing metadata to the database: %1").arg(result.error()));
        return;
    } else if (result.get() == Vfs::ConvertToPlaceholderResult::Locked) {
        done(SyncFileItem::FatalError, tr("The file %1 is currently in use").arg(_item->_file));
        return;
    }

    done(SyncFileItem::Success);
}
}
