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
#include "filesystem.h"
#include <QFile>
#include <QStringList>

namespace OCC {

MoveJob::MoveJob(AccountPtr account, const QString& path,
                 const QString &destination, QObject* parent)
    : AbstractNetworkJob(account, path, parent), _destination(destination)
{ }

MoveJob::MoveJob(AccountPtr account, const QUrl& url, const QString &destination,
                 QMap<QByteArray, QByteArray> extraHeaders, QObject* parent)
    : AbstractNetworkJob(account, QString(), parent), _destination(destination), _url(url)
    , _extraHeaders(extraHeaders)
{ }

void MoveJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("Destination", QUrl::toPercentEncoding(_destination, "/"));
    for(auto it = _extraHeaders.constBegin(); it != _extraHeaders.constEnd(); ++it) {
        req.setRawHeader(it.key(), it.value());
    }
    setReply(_url.isValid() ? davRequest("MOVE", _url, req) : davRequest("MOVE", path(), req));
    setupConnections(reply());

    if( reply()->error() != QNetworkReply::NoError ) {
        qWarning() << Q_FUNC_INFO << " Network error: " << reply()->errorString();
    }
    AbstractNetworkJob::start();
}


QString MoveJob::errorString()
{
    if (_timedout) {
        return tr("Connection timed out");
    } else if (reply()->hasRawHeader("OC-ErrorString")) {
        return reply()->rawHeader("OC-ErrorString");
    } else {
        return reply()->errorString();
    }
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

    qDebug() << Q_FUNC_INFO << _item->_file << _item->_renameTarget;

    QString targetFile(_propagator->getFilePath(_item->_renameTarget));

    if (_item->_file == _item->_renameTarget) {
        // The parent has been renamed already so there is nothing more to do.
        finalize();
        return;
    }
    if (_item->_file == QLatin1String("Shared") ) {
        // Before owncloud 7, there was no permissions system. At the time all the shared files were
        // in a directory called "Shared" and were not supposed to be moved, otherwise bad things happened

        QString versionString = _propagator->account()->serverVersion();
        if (versionString.contains('.') && versionString.split('.')[0].toInt() < 7) {
            QString originalFile(_propagator->getFilePath(QLatin1String("Shared")));
            emit _propagator->touchedFile(originalFile);
            emit _propagator->touchedFile(targetFile);
            QString renameError;
            if( FileSystem::rename(targetFile, originalFile, &renameError) ) {
                done(SyncFileItem::NormalError, tr("This folder must not be renamed. It is renamed back to its original name."));
            } else {
                done(SyncFileItem::NormalError, tr("This folder must not be renamed. Please name it back to Shared."));
            }
            return;
        }
    }

    _job = new MoveJob(_propagator->account(),
                        _propagator->_remoteFolder + _item->_file,
                        _propagator->_remoteDir + _item->_renameTarget,
                        this);
    connect(_job, SIGNAL(finishedSignal()), this, SLOT(slotMoveJobFinished()));
    _propagator->_activeJobList.append(this);
    _job->start();

}

void PropagateRemoteMove::abort()
{
    if (_job &&  _job->reply())
        _job->reply()->abort();
}

void PropagateRemoteMove::slotMoveJobFinished()
{
    _propagator->_activeJobList.removeOne(this);

    Q_ASSERT(_job);

    qDebug() << Q_FUNC_INFO << _job->reply()->request().url() << "FINISHED WITH STATUS"
        << _job->reply()->error()
        << (_job->reply()->error() == QNetworkReply::NoError ? QLatin1String("") : _job->reply()->errorString());

    QNetworkReply::NetworkError err = _job->reply()->error();
    _item->_httpErrorCode = _job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (err != QNetworkReply::NoError) {

        if( checkForProblemsWithShared(_item->_httpErrorCode,
                tr("The file was renamed but is part of a read only share. The original file was restored."))) {
            return;
        }

        SyncFileItem::Status status = classifyError(err, _item->_httpErrorCode,
                                                    &_propagator->_anotherSyncNeeded);
        done(status, _job->errorString());
        return;
    }

    _item->_requestDuration = _job->duration();
    _item->_responseTimeStamp = _job->responseTimestamp();

    if (_item->_httpErrorCode != 201 ) {
        // Normally we expect "201 Created"
        // If it is not the case, it might be because of a proxy or gateway intercepting the request, so we must
        // throw an error.
        done(SyncFileItem::NormalError, tr("Wrong HTTP code returned by server. Expected 201, but received \"%1 %2\".")
            .arg(_item->_httpErrorCode).arg(_job->reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
        return;
    }

    finalize();

}

void PropagateRemoteMove::finalize()
{
    SyncJournalFileRecord oldRecord =
            _propagator->_journal->getFileRecord(_item->_originalFile);
    // if reading from db failed still continue hoping that deleteFileRecord
    // reopens the db successfully.
    // The db is only queried to transfer the content checksum from the old
    // to the new record. It is not a problem to skip it here.
    _propagator->_journal->deleteFileRecord(_item->_originalFile);

    SyncJournalFileRecord record(*_item, _propagator->getFilePath(_item->_renameTarget));
    record._path = _item->_renameTarget;
    if (oldRecord.isValid()) {
        record._contentChecksum = oldRecord._contentChecksum;
        record._contentChecksumType = oldRecord._contentChecksumType;
        if (record._fileSize != oldRecord._fileSize) {
            qDebug() << "Warning: file sizes differ on server vs csync_journal: " << record._fileSize << oldRecord._fileSize;
            record._fileSize = oldRecord._fileSize; // server might have claimed different size, we take the old one from the DB
        }
    }

    if (!_propagator->_journal->setFileRecord(record)) {
        done(SyncFileItem::FatalError, tr("Error writing metadata to the database"));
        return;
    }
    _propagator->_journal->commit("Remote Rename");
    done(SyncFileItem::Success);
}


}

