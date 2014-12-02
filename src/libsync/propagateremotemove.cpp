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

#include "propagateremotemove.h"
#include "owncloudpropagator_p.h"
#include "account.h"
#include "syncjournalfilerecord.h"
#include <QFile>

namespace OCC {

MoveJob::MoveJob(Account* account, const QString& path,
                 const QString &destination, QObject* parent)
    : AbstractNetworkJob(account, path, parent), _destination(destination)
{ }


void MoveJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("Destination", QUrl::toPercentEncoding(_destination, "/"));
    setReply(davRequest("MOVE", path(), req));
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qWarning() << Q_FUNC_INFO << " Network error: " << reply()->errorString();
    }
    AbstractNetworkJob::start();
}


QString MoveJob::errorString()
{
    return _timedout ? tr("Connection timed out") :  reply()->errorString();
}

bool MoveJob::finished()
{
    emit finishedSignal();
    return true;
}

void PropagateRemoteMove::start()
{
    if (_propagator->_abortRequested.fetchAndAddRelaxed(0))
        return;

    qDebug() << Q_FUNC_INFO << _item._file << _item._renameTarget;

    QString targetFile(_propagator->getFilePath(_item._renameTarget));

    if (_item._file == _item._renameTarget) {
        // The parents has been renamed already so there is nothing more to do.
        finalize();
        return;
    } else if (AbstractNetworkJob::preOc7WasDetected && _item._file == QLatin1String("Shared") ) {
        // Check if it is the toplevel Shared folder and do not propagate it.
        QString originalFile(_propagator->getFilePath(QLatin1String("Shared")));
        _propagator->addTouchedFile(originalFile);
        _propagator->addTouchedFile(targetFile);
        if( QFile::rename( targetFile, originalFile) ) {
            done(SyncFileItem::NormalError, tr("This folder must not be renamed. It is renamed back to its original name."));
        } else {
            done(SyncFileItem::NormalError, tr("This folder must not be renamed. Please name it back to Shared."));
        }
        return;
    } else {
        _job = new MoveJob(AccountManager::instance()->account(),
                           _propagator->_remoteFolder + _item._file,
                           _propagator->_remoteDir + _item._renameTarget,
                           this);
        connect(_job, SIGNAL(finishedSignal()), this, SLOT(slotMoveJobFinished()));
        _propagator->_activeJobs++;
        _job->start();
    }
}

void PropagateRemoteMove::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
}

void PropagateRemoteMove::slotMoveJobFinished()
{
    _propagator->_activeJobs--;

    Q_ASSERT(_job);

    qDebug() << Q_FUNC_INFO << _job->reply()->request().url() << "FINISHED WITH STATUS"
        << _job->reply()->error()
        << (_job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : _job->reply()->errorString());

    QNetworkReply::NetworkError err = _job->reply()->error();
    _item._httpErrorCode = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (err != QNetworkReply::NoError) {

        if( checkForProblemsWithShared(_item._httpErrorCode,
                tr("The file was renamed but is part of a read only share. The original file was restored."))) {
            return;
        }

        SyncFileItem::Status status = classifyError(err, _item._httpErrorCode);
        done(status, _job->errorString());
        return;
    }

    _item._requestDuration = _job->duration();
    _item._responseTimeStamp = _job->responseTimestamp();

    if (_item._httpErrorCode != 201 ) {
        // Normaly we expect "201 Created"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError, tr("Wrong HTTP code returned by server. Expected 201, but recieved \"%1 %2\".")
            .arg(_item._httpErrorCode).arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
        return;
    }

    finalize();

}

void PropagateRemoteMove::finalize()
{
    _propagator->_journal->deleteFileRecord(_item._originalFile);
    SyncJournalFileRecord record(_item, _propagator->getFilePath(_item._renameTarget));
    record._path = _item._renameTarget;

    _propagator->_journal->setFileRecord(record);
    _propagator->_journal->commit("Remote Rename");
    done(SyncFileItem::Success);
}


}

