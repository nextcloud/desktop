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
#include "socketapi_p.h"

#include "accountmanager.h"
#include "common/syncjournaldb.h"
#include "syncengine.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>

using namespace OCC;

SocketUploadJob::SocketUploadJob(OCC::SocketListener *listener, const QString &argument, QObject *parent)
    : QObject(parent)
    , _listener(listener)
{
    const auto args = QJsonDocument::fromJson(argument.toUtf8()).object();
    _localPath = args[QLatin1String("localPath")].toString();
    _remotePath = args[QLatin1String("remotePath")].toString();
    if (!_remotePath.startsWith("/")) {
        _remotePath = QLatin1Char('/') + _remotePath;
    }

    _pattern = args[QLatin1String("pattern")].toString();
    // TODO: use uuid
    const auto accname = args[QLatin1String("account")][QLatin1String("name")].toString();
    auto account = AccountManager::instance()->account(accname);

    ENFORCE(QFileInfo(_localPath).isAbsolute())
    ENFORCE(_tmp.open())

    _db = new SyncJournalDb(_tmp.fileName(), this);
    _engine = new SyncEngine(account->account(), _localPath.endsWith(QLatin1Char('/')) ? _localPath : _localPath + QLatin1Char('/'), _remotePath, _db);
    _engine->setParent(_db);

    connect(_engine, &OCC::SyncEngine::itemCompleted, this, [this](const OCC::SyncFileItemPtr item) {
        _syncedFiles.append(item->_file);
    });

    connect(_engine, &OCC::SyncEngine::finished, this, [this](bool ok) {
        if (ok) {
            finish({});
        }
    });
    connect(_engine, &OCC::SyncEngine::syncError, this, &SocketUploadJob::finish);
}

void SocketUploadJob::start()
{
    auto opt = _engine->syncOptions();
    opt.setFilePattern(_pattern);
    if (!opt.fileRegex().isValid()) {
        finish(opt.fileRegex().errorString());
        return;
    }
    _engine->setSyncOptions(opt);

    // create the dir, fail if it already exists
    auto mkdir = new OCC::MkColJob(_engine->account(), _remotePath);
    connect(mkdir, &OCC::MkColJob::finishedWithoutError, _engine, &OCC::SyncEngine::startSync);
    connect(mkdir, &OCC::MkColJob::finishedWithError, this, [this](QNetworkReply *reply) {
        if (reply->error() == 202) {
            finish(QStringLiteral("Destination %1 already exists").arg(_remotePath));
        } else {
            finish(reply->errorString());
        }
    });
    mkdir->start();
}

void SocketUploadJob::finish(const QString &error)
{
    if (!_finished) {
        _finished = true;
        _listener->sendMessage(QStringLiteral("V2/UPLOAD_FILES_RESULT"), { { "localPath", _localPath }, { "error", error }, { "syncedFiles", QJsonArray::fromStringList(_syncedFiles) } });
        deleteLater();
    }
}
