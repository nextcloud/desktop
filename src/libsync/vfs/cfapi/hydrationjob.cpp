/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
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

#include "hydrationjob.h"

#include "common/syncjournaldb.h"
#include "propagatedownload.h"

#include <QLocalServer>
#include <QLocalSocket>

Q_LOGGING_CATEGORY(lcHydration, "nextcloud.sync.vfs.hydrationjob", QtInfoMsg)

OCC::HydrationJob::HydrationJob(QObject *parent)
    : QObject(parent)
{
    connect(this, &HydrationJob::finished, this, &HydrationJob::deleteLater);
}

OCC::AccountPtr OCC::HydrationJob::account() const
{
    return _account;
}

void OCC::HydrationJob::setAccount(const AccountPtr &account)
{
    _account = account;
}

QString OCC::HydrationJob::remotePath() const
{
    return _remotePath;
}

void OCC::HydrationJob::setRemotePath(const QString &remotePath)
{
    _remotePath = remotePath;
}

QString OCC::HydrationJob::localPath() const
{
    return _localPath;
}

void OCC::HydrationJob::setLocalPath(const QString &localPath)
{
    _localPath = localPath;
}

OCC::SyncJournalDb *OCC::HydrationJob::journal() const
{
    return _journal;
}

void OCC::HydrationJob::setJournal(SyncJournalDb *journal)
{
    _journal = journal;
}

QString OCC::HydrationJob::requestId() const
{
    return _requestId;
}

void OCC::HydrationJob::setRequestId(const QString &requestId)
{
    _requestId = requestId;
}

QString OCC::HydrationJob::folderPath() const
{
    return _folderPath;
}

void OCC::HydrationJob::setFolderPath(const QString &folderPath)
{
    _folderPath = folderPath;
}

OCC::HydrationJob::Status OCC::HydrationJob::status() const
{
    return _status;
}

void OCC::HydrationJob::start()
{
    Q_ASSERT(_account);
    Q_ASSERT(_journal);
    Q_ASSERT(!_remotePath.isEmpty() && !_localPath.isEmpty());
    Q_ASSERT(!_requestId.isEmpty() && !_folderPath.isEmpty());

    Q_ASSERT(_remotePath.endsWith('/'));
    Q_ASSERT(_localPath.endsWith('/'));
    Q_ASSERT(!_folderPath.startsWith('/'));

    _server = new QLocalServer(this);
    const auto listenResult = _server->listen(_requestId);
    if (!listenResult) {
        qCCritical(lcHydration) << "Couldn't get server to listen" << _requestId << _localPath << _folderPath;
        emitFinished(Error);
        return;
    }

    qCInfo(lcHydration) << "Server ready, waiting for connections" << _requestId << _localPath << _folderPath;
    connect(_server, &QLocalServer::newConnection, this, &HydrationJob::onNewConnection);
}

void OCC::HydrationJob::cancel()
{
    if (!_job) {
        return;
    }

    _job->cancel();
}

void OCC::HydrationJob::emitFinished(Status status)
{
    _status = status;
    if (status == Success) {
        connect(_socket, &QLocalSocket::disconnected, this, [=]{
            _socket->close();
            emit finished(this);
        });
        _socket->disconnectFromServer();
    } else {
        _socket->close();
        emit finished(this);
    }
}

void OCC::HydrationJob::emitCanceled()
{
    connect(_socket, &QLocalSocket::disconnected, this, [=] {
        _socket->close();
    });
    _socket->disconnectFromServer();

    emit canceled(this);
}

void OCC::HydrationJob::onNewConnection()
{
    Q_ASSERT(!_socket);
    Q_ASSERT(!_job);

    qCInfo(lcHydration) << "Got new connection starting GETFileJob" << _requestId << _folderPath;
    _socket = _server->nextPendingConnection();
    _job = new GETFileJob(_account, _remotePath + _folderPath, _socket, {}, {}, 0, this);
    connect(_job, &GETFileJob::finishedSignal, this, &HydrationJob::onGetFinished);
    connect(_job, &GETFileJob::canceled, this, &HydrationJob::onGetCanceled);
    _job->start();
}

void OCC::HydrationJob::onGetCanceled()
{
    qCInfo(lcHydration) << "GETFileJob canceled" << _requestId << _folderPath << _job->reply()->error();
    emitCanceled();
}

void OCC::HydrationJob::onGetFinished()
{
    qCInfo(lcHydration) << "GETFileJob finished" << _requestId << _folderPath << _job->reply()->error();

    if (_job->reply()->error()) {
        emitFinished(Error);
        return;
    }

    SyncJournalFileRecord record;
    _journal->getFileRecord(_folderPath, &record);
    Q_ASSERT(record.isValid());
    if (!record.isValid()) {
        qCWarning(lcHydration) << "Couldn't find record to update after hydration" << _requestId << _folderPath;
        emitFinished(Error);
        return;
    }

    record._type = ItemTypeFile;
    _journal->setFileRecord(record);
    emitFinished(Success);
}
