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
#include "progressdispatcher.h"
#include "syncengine.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTemporaryFile>

using namespace OCC;

namespace {

// create a fake SyncFileItemPtr to display a message in the protocol
void logMessage(const QString &localPath, const QString &message)
{
    auto item = SyncFileItemPtr::create();
    item->_status = SyncFileItem::Success;
    item->_messageString = message;
    item->_responseTimeStamp = QDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8();
    Q_EMIT ProgressDispatcher::instance()->itemCompleted(QDir::toNativeSeparators(localPath), item);
}
}

SocketUploadJob::SocketUploadJob(const QSharedPointer<SocketApiJobV2> &job)
    : _apiJob(job)
{
    connect(job.data(), &SocketApiJobV2::finished, this, &SocketUploadJob::deleteLater);
}


void SocketUploadJob::start()
{
    _localPath = _apiJob->arguments()[QLatin1String("localPath")].toString();
    auto remotePath = _apiJob->arguments()[QLatin1String("remotePath")].toString();
    if (!remotePath.startsWith(QLatin1Char('/'))) {
        remotePath = QLatin1Char('/') + remotePath;
    }

    const auto pattern = _apiJob->arguments()[QLatin1String("pattern")].toString();
    const auto accname = _apiJob->arguments()[QLatin1String("account")][QLatin1String("name")].toString();
    AccountStatePtr account;
    account = AccountManager::instance()->account(accname);

    logMessage(_localPath, tr("Backup of %1 started").arg(QDir::toNativeSeparators(_localPath)));

    if (!account) {
        fail(tr("Failed to find %1").arg(QString::fromUtf8(QJsonDocument(_apiJob->arguments()[QLatin1String("account")].toObject()).toJson())));
        return;
    }

    if (!QFileInfo(_localPath).isAbsolute()) {
        fail(tr("Local path must be a an absolute path"));
        return;
    }
    auto tmp = new QTemporaryFile(this);
    if (!tmp->open()) {
        fail(tr("Failed to create temporary database"));
        return;
    }

    auto db = new SyncJournalDb(tmp->fileName(), this);
    auto engine = new SyncEngine(account->account(), _localPath.endsWith(QLatin1Char('/')) ? _localPath : _localPath + QLatin1Char('/'), remotePath, db);
    engine->setParent(db);

    connect(engine, &OCC::SyncEngine::transmissionProgress, this, [this](const ProgressInfo &info) {
        Q_EMIT ProgressDispatcher::instance()->progressInfo(_localPath, info);
    });
    connect(engine, &OCC::SyncEngine::itemCompleted, this, [this](const OCC::SyncFileItemPtr item) {
        _syncedFiles.append(item->_file);
    });

    connect(engine, &OCC::SyncEngine::finished, this, [this](bool ok) {
        if (ok) {
            logMessage(_localPath, tr("Backup of %1 succeeded").arg(QDir::toNativeSeparators(_localPath)));
            _apiJob->success({ { QStringLiteral("localPath"), _localPath }, { QStringLiteral("syncedFiles"), QJsonArray::fromStringList(_syncedFiles) } });
        }
    });
    connect(engine, &OCC::SyncEngine::syncError, this, [this](const QString &error, ErrorCategory) {
        fail(error);
    });

    auto opt = engine->syncOptions();
    opt.setFilePattern(pattern);
    if (!opt.fileRegex().isValid()) {
        fail(opt.fileRegex().errorString());
        return;
    }
    engine->setSyncOptions(opt);

    // create the dir, fail if it already exists
    auto mkdir = new OCC::MkColJob(engine->account(), remotePath);
    connect(mkdir, qOverload<QNetworkReply::NetworkError>(&OCC::MkColJob::finished), this, [remotePath, engine, mkdir, this]() {
        auto reply = mkdir->reply();
        if (reply->error() == QNetworkReply::NoError) {
            engine->startSync();
        } else if (reply->error() == 202) {
            fail(QStringLiteral("Destination %1 already exists").arg(remotePath));
        } else {
            fail(reply->errorString());
        }
    });
    mkdir->start();
}

void SocketUploadJob::fail(const QString &error)
{
    logMessage(_localPath, tr("Backup of %1 failed with: %2").arg(QDir::toNativeSeparators(_localPath), error));
    _apiJob->failure(error);
}
