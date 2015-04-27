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
#include "owncloudpropagator_p.h"
#include "account.h"

namespace OCC {

DeleteJob::DeleteJob(AccountPtr account, const QString& path, QObject* parent)
    : AbstractNetworkJob(account, path, parent)
{ }


void DeleteJob::start()
{
    QNetworkRequest req;
    setReply(davRequest("DELETE", path(), req));
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qWarning() << Q_FUNC_INFO << " Network error: " << reply()->errorString();
    }
    AbstractNetworkJob::start();
}


QString DeleteJob::errorString()
{
    if (_timedout) {
        return tr("Connection timed out");
    } else if (reply()->hasRawHeader("OC-ErrorString")) {
        return reply()->rawHeader("OC-ErrorString");
    } else {
        return reply()->errorString();
    }
}

bool DeleteJob::finished()
{
    emit finishedSignal();
    return true;
}

void PropagateRemoteDelete::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qDebug() << Q_FUNC_INFO << _item->_file;

    _job = new DeleteJob(_propagator->account(),
                         _propagator->_remoteFolder + _item->_file,
                         this);
    connect(_job, SIGNAL(finishedSignal()), this, SLOT(slotDeleteJobFinished()));
    _propagator->_activeJobs ++;
    _job->start();
}

void PropagateRemoteDelete::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
}

void PropagateRemoteDelete::slotDeleteJobFinished()
{
    _propagator->_activeJobs--;

    Q_ASSERT(_job);

    qDebug() << Q_FUNC_INFO << _job->reply()->request().url() << "FINISHED WITH STATUS"
        << _job->reply()->error()
        << (_job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : _job->reply()->errorString());

    QNetworkReply::NetworkError err = _job->reply()->error();
    const int httpStatus = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    _item->_httpErrorCode = httpStatus;

    if (err != QNetworkReply::NoError && err != QNetworkReply::ContentNotFoundError) {

        if( checkForProblemsWithShared(_item->_httpErrorCode,
            tr("The file has been removed from a read only share. It was restored.")) ) {
            return;
        }

        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode);
        done(status, _job->errorString());
        return;
    }

    _item->_requestDuration = _job->duration();
    _item->_responseTimeStamp = _job->responseTimestamp();

    // A 404 reply is also considered a success here: We want to make sure
    // a file is gone from the server. It not being there in the first place
    // is ok. This will happen for files that are in the DB but not on
    // the server or the local file system.
    if (httpStatus != 204 && httpStatus != 404) {
        // Normaly we expect "204 No Content"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError, tr("Wrong HTTP code returned by server. Expected 204, but received \"%1 %2\".")
            .arg(_item->_httpErrorCode).arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
        return;
    }

    _propagator->_journal->deleteFileRecord(_item->_originalFile, _item->_isDirectory);
    _propagator->_journal->commit("Remote Remove");
    done(SyncFileItem::Success);
}


}

