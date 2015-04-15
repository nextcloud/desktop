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
#include "syncjournalfilerecord.h"
#include <QFile>

namespace OCC {

void PropagateRemoteMkdir::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qDebug() << Q_FUNC_INFO << _item->_file;

    _job = new MkColJob(_propagator->account(),
                        _propagator->_remoteFolder + _item->_file,
                        this);
    connect(_job, SIGNAL(finished(QNetworkReply::NetworkError)), this, SLOT(slotMkcolJobFinished()));
    _propagator->_activeJobs++;
    _job->start();
}

void PropagateRemoteMkdir::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
}

void PropagateRemoteMkdir::slotMkcolJobFinished()
{
    _propagator->_activeJobs--;

    Q_ASSERT(_job);

    qDebug() << Q_FUNC_INFO << _job->reply()->request().url() << "FINISHED WITH STATUS"
        << _job->reply()->error()
        << (_job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : _job->reply()->errorString());

    QNetworkReply::NetworkError err = _job->reply()->error();
    _item->_httpErrorCode = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (_item->_httpErrorCode == 405) {
        // This happens when the directory already exist. Nothing to do.
    } else if (err != QNetworkReply::NoError) {
        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode);
        auto errorString = _job->reply()->errorString();
        if (_job->reply()->hasRawHeader("OC-ErrorString")) {
            errorString = _job->reply()->rawHeader("OC-ErrorString");
        }
        done(status, errorString);
        return;
    } else if (_item->_httpErrorCode != 201) {
        // Normaly we expect "201 Created"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError, tr("Wrong HTTP code returned by server. Expected 201, but received \"%1 %2\".")
            .arg(_item->_httpErrorCode).arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
        return;
    }

    _item->_requestDuration = _job->duration();
    _item->_responseTimeStamp = _job->responseTimestamp();
    _item->_fileId = _job->reply()->rawHeader("OC-FileId");

    if (_item->_fileId.isEmpty()) {
        // Owncloud 7.0.0 and before did not have a header with the file id.
        // (https://github.com/owncloud/core/issues/9000)
        // So we must get the file id using a PROPFIND
        // This is required so that we can detect moves even if the folder is renamed on the server
        // while files are still uploading
        _propagator->_activeJobs++;
        auto propfindJob = new PropfindJob(_job->account(), _job->path(), this);
        propfindJob->setProperties(QList<QByteArray>() << "getetag" << "http://owncloud.org/ns:id");
        QObject::connect(propfindJob, SIGNAL(result(QVariantMap)), this, SLOT(propfindResult(QVariantMap)));
        QObject::connect(propfindJob, SIGNAL(finishedWithError()), this, SLOT(propfindError()));
        propfindJob->start();
        _job = propfindJob;
        return;
    }
    success();
}

void PropagateRemoteMkdir::propfindResult(const QVariantMap &result)
{
    _propagator->_activeJobs--;
    if (result.contains("getetag")) {
        _item->_etag = result["getetag"].toByteArray();
    }
    if (result.contains("id")) {
        _item->_fileId = result["id"].toByteArray();
    }
    success();
}

void PropagateRemoteMkdir::propfindError()
{
    // ignore the PROPFIND error
    _propagator->_activeJobs--;
    done(SyncFileItem::Success);
}

void PropagateRemoteMkdir::success()
{
    // save the file id already so we can detect rename or remove
    SyncJournalFileRecord record(*_item, _propagator->_localDir + _item->destination());
    _propagator->_journal->setFileRecord(record);

    done(SyncFileItem::Success);
}



}

