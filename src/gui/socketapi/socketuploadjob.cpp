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
#include <QTemporaryFile>

using namespace OCC;

SocketUploadJob::SocketUploadJob(const QSharedPointer<SocketApiJobV2> &job)
    : _apiJob(job)
{
    connect(job.data(), &SocketApiJobV2::finished, this, &SocketUploadJob::deleteLater);
}

void SocketUploadJob::start()
{
    const auto localPath = _apiJob->arguments()[QLatin1String("localPath")].toString();
    auto remotePath = _apiJob->arguments()[QLatin1String("remotePath")].toString();
    if (!remotePath.startsWith(QLatin1Char('/'))) {
        remotePath = QLatin1Char('/') + remotePath;
    }

    const auto pattern = _apiJob->arguments()[QLatin1String("pattern")].toString();
    const auto accname = _apiJob->arguments()[QLatin1String("account")][QLatin1String("name")].toString();
    const auto accUUID = QUuid::fromString(_apiJob->arguments()[QLatin1String("account")][QLatin1String("uuid")].toString());
    AccountStatePtr account;
    if (accUUID.isNull()) {
        _apiJob->setWarning("Using the name as identifier is deprecated, please use the uuid");
        account = AccountManager::instance()->account(accname);
    } else {
        account = AccountManager::instance()->account(accUUID);
    }

    if (!account) {
        _apiJob->failure(QStringLiteral("Failed to find %1").arg(QString::fromUtf8(QJsonDocument(_apiJob->arguments()[QLatin1String("account")].toObject()).toJson())));
        return;
    }

    if (!QFileInfo(localPath).isAbsolute()) {
        _apiJob->failure(QStringLiteral("Local path must be a an absolute path"));
        return;
    }
    auto tmp = new QTemporaryFile(this);
    if (!tmp->open()) {
        _apiJob->failure(QStringLiteral("Failed to create temporary database"));
        return;
    }

    auto db = new SyncJournalDb(tmp->fileName(), this);
    auto engine = new SyncEngine(account->account(), localPath.endsWith(QLatin1Char('/')) ? localPath : localPath + QLatin1Char('/'), remotePath, db);
    engine->setParent(db);

    connect(engine, &OCC::SyncEngine::itemCompleted, this, [this](const OCC::SyncFileItemPtr item) {
        _syncedFiles.append(item->_file);
    });

    connect(engine, &OCC::SyncEngine::finished, this, [this, localPath](bool ok) {
        if (ok) {
            _apiJob->success({ { "localPath", localPath }, { "syncedFiles", QJsonArray::fromStringList(_syncedFiles) } });
        }
    });
    connect(engine, &OCC::SyncEngine::syncError, this, [this](const QString &error, ErrorCategory) {
        _apiJob->failure(error);
    });

    auto opt = engine->syncOptions();
    opt.setFilePattern(pattern);
    if (!opt.fileRegex().isValid()) {
        _apiJob->failure(opt.fileRegex().errorString());
        return;
    }
    engine->setSyncOptions(opt);

    // create the dir, fail if it already exists
    auto mkdir = new OCC::MkColJob(engine->account(), remotePath);
    connect(mkdir, &OCC::MkColJob::finishedWithoutError, engine, &OCC::SyncEngine::startSync);
    connect(mkdir, &OCC::MkColJob::finishedWithError, this, [this, remotePath](QNetworkReply *reply) {
        if (reply->error() == 202) {
            _apiJob->failure(QStringLiteral("Destination %1 already exists").arg(remotePath));
        } else {
            _apiJob->failure(reply->errorString());
        }
    });
    mkdir->start();
}
