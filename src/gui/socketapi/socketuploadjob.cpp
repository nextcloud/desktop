/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "socketuploadjob.h"
#include "common/utility.h"
#include "socketapi_p.h"

#include "accountmanager.h"
#include "common/syncjournaldb.h"
#include "syncengine.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>

using namespace OCC;

SocketUploadJob::SocketUploadJob(const QSharedPointer<SocketApiJobV2> &job)
    : _apiJob(job)
{
    connect(job.data(), &SocketApiJobV2::finished, this, &SocketUploadJob::deleteLater);

    _localPath = _apiJob->arguments()[QLatin1String("localPath")].toString();
    _remotePath = _apiJob->arguments()[QLatin1String("remotePath")].toString();
    if (!_remotePath.startsWith(QLatin1Char('/'))) {
        _remotePath = QLatin1Char('/') + _remotePath;
    }

    _pattern = job->arguments()[QLatin1String("pattern")].toString();
    // TODO: use uuid
    const auto accname = job->arguments()[QLatin1String("account")][QLatin1String("name")].toString();
    auto account = AccountManager::instance()->account(accname);

    if (!QFileInfo(_localPath).isAbsolute()) {
        job->failure(QStringLiteral("Local path must be a an absolute path"));
        return;
    }
    if (!_tmp.open()) {
        job->failure(QStringLiteral("Failed to create temporary database"));
        return;
    }

    _db = new SyncJournalDb(_tmp.fileName(), this);

    SyncOptions opt;
    opt.fillFromEnvironmentVariables();
    opt.verifyChunkSizes();
    _engine = new SyncEngine(account->account(), Utility::trailingSlashPath(_localPath), opt, _remotePath, _db);
    _engine->setParent(_db);

    connect(_engine, &OCC::SyncEngine::itemCompleted, this, [this](const OCC::SyncFileItemPtr item) {
        _syncedFiles.append(item->_file);
    });

    connect(_engine, &OCC::SyncEngine::finished, this, [this](bool ok) {
        if (ok) {
            _apiJob->success({ { "localPath", _localPath }, { "syncedFiles", QJsonArray::fromStringList(_syncedFiles) } });
        }
    });
    connect(_engine, &OCC::SyncEngine::syncError, this, [this](const QString &error, ErrorCategory) {
        _apiJob->failure(error);
    });
}

void SocketUploadJob::start()
{
    auto opt = _engine->syncOptions();
    opt.setFilePattern(_pattern);
    if (!opt.fileRegex().isValid()) {
        _apiJob->failure(opt.fileRegex().errorString());
        return;
    }
    _engine->setSyncOptions(opt);

    // create the dir, fail if it already exists
    auto mkdir = new OCC::MkColJob(_engine->account(), _remotePath);
    connect(mkdir, &OCC::MkColJob::finishedWithoutError, _engine, &OCC::SyncEngine::startSync);
    connect(mkdir, &OCC::MkColJob::finishedWithError, this, [this](QNetworkReply *reply) {
        if (reply->error() == 202) {
            _apiJob->failure(QStringLiteral("Destination %1 already exists").arg(_remotePath));
        } else {
            _apiJob->failure(reply->errorString());
        }
    });
    mkdir->start();
}
