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

#include "application.h"
#include "accountmanager.h"
#include "common/syncjournaldb.h"
#include "csync_exclude.h"
#include "progressdispatcher.h"
#include "syncengine.h"
#include "theme.h"
#include "version.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTemporaryFile>

using namespace OCC;

namespace {
const QString backupTagNameC()
{
    return QStringLiteral("backup_finished");
}
const QUrl tagUrl(const OCC::AccountPtr &account)
{
    return Utility::concatUrlPath(account->url(), QStringLiteral("remote.php/dav/systemtags"));
}
}

SocketUploadJob::SocketUploadJob(const QSharedPointer<SocketApiJobV2> &job)
    : _apiJob(job)
{
    connect(job.data(), &SocketApiJobV2::finished, this, &SocketUploadJob::deleteLater);
}

void SocketUploadJob::prepareTag(const AccountPtr &account)
{
    auto tagJob = new OCC::SimpleNetworkJob(account, this);
    connect(tagJob, &OCC::SimpleNetworkJob::finishedSignal, this, [account, this] {
        auto propfindJob = new OCC::LsColJob(account, tagUrl(account), this);
        propfindJob->setProperties({ QByteArrayLiteral("http://owncloud.org/ns:display-name"), QByteArrayLiteral("http://owncloud.org/ns:id") });

        connect(propfindJob, &LsColJob::directoryListingIterated, this, [this](const QString &, const QMap<QString, QString> &data) {
            if (data["display-name"] == backupTagNameC()) {
                _finisedTagId = data["id"].toInt();
            }
        });
        connect(propfindJob, &LsColJob::finishedWithError, this, [this] {
            fail(tr("Failed to rerieve tags"));
        });
        propfindJob->start();
    });
    const QJsonObject json({ { QStringLiteral("name"), backupTagNameC() },
        { QStringLiteral("userVisible"), QStringLiteral("true") },
        { QStringLiteral("userAssignable"), QStringLiteral("false") } });
    QNetworkRequest req;
    // TODO: 2.9 prepareRequest
    // remove version.h include
    {
        static_assert(MIRALL_VERSION_MINOR < 9, "Port to new prepareRequest");
        auto requestBody = new QBuffer {};
        requestBody->setData(QJsonDocument(json).toJson());
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        tagJob->startRequest(QByteArrayLiteral("POST"), tagUrl(account), req, requestBody);
    }
}
void SocketUploadJob::start()
{
    _localPath = _apiJob->arguments()[QLatin1String("localPath")].toString();
    auto remotePath = _apiJob->arguments()[QLatin1String("remotePath")].toString();
    if (!remotePath.startsWith(QLatin1Char('/'))) {
        remotePath = QLatin1Char('/') + remotePath;
    }

    const auto pattern = _apiJob->arguments()[QLatin1String("pattern")].toString();
    const auto excludes = _apiJob->arguments()[QLatin1String("excludes")].toArray();
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
    auto tmp = new QTemporaryFile();
    if (!tmp->open()) {
        fail(tr("Failed to create temporary database"));
        return;
    }

    auto db = new SyncJournalDb(tmp->fileName(), this);
    auto engine = new SyncEngine(account->account(), _localPath.endsWith(QLatin1Char('/')) ? _localPath : _localPath + QLatin1Char('/'), remotePath, db);
    engine->setParent(db);
    tmp->setParent(db);

    for (const auto &i : excludes) {
        engine->excludedFiles().addManualExclude(i.toString());
    }

    connect(engine, &OCC::SyncEngine::transmissionProgress, this, [this](const ProgressInfo &info) {
        Q_EMIT ProgressDispatcher::instance()->progressInfo(_localPath, info);
    });
    connect(engine, &OCC::SyncEngine::itemCompleted, this, [this](const OCC::SyncFileItemPtr item) {
        if (item->_errorString.isEmpty()) {
            _syncedFiles.append(item->_file);
        } else {
            _errorFiles.append(QStringLiteral("%1: %2").arg(item->_file, item->_errorString));
        }
    });

    connect(engine, &OCC::SyncEngine::finished, this, [engine, this](bool ok) {
        if (ok) {
            auto tagJob = new OCC::SimpleNetworkJob(engine->account(), this);
            connect(tagJob, &OCC::SimpleNetworkJob::finishedSignal, this, [tagJob, this] {
                if (tagJob->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 201) {
                    logMessage(_localPath, tr("Backup of %1 succeeded").arg(QDir::toNativeSeparators(_localPath)));
                    _apiJob->success({ { QStringLiteral("localPath"), _localPath }, { QStringLiteral("syncedFiles"), QJsonArray::fromStringList(_syncedFiles) } });
                } else {
                    fail(tr("Failed to set success tag"));
                }
            });
            OC_ASSERT(_finisedTagId > 0);
            tagJob->startRequest(QByteArrayLiteral("PUT"), Utility::concatUrlPath(engine->account()->url(), QStringLiteral("remote.php/dav/systemtags-relations/files/%1/%2").arg(_backupFileId, QString::number(_finisedTagId))));
        } else {
            fail(tr("Failed to create backup: %1").arg(_errorFiles.join(", ")));
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

    prepareTag(account->account());

    // create the dir, fail if it already exists
    auto mkdir = new OCC::MkColJob(engine->account(), remotePath);
    connect(mkdir, qOverload<QNetworkReply::NetworkError>(&OCC::MkColJob::finished), this, [remotePath, engine, mkdir, this]() {
        auto reply = mkdir->reply();

        if (reply->error() == QNetworkReply::NoError) {
            _backupFileId = reply->rawHeader(QByteArrayLiteral("OC-FileId"));
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
    logMessage(_localPath, tr("Backup of %1 failed with: %2").arg(QDir::toNativeSeparators(_localPath), error), false);
    _apiJob->failure(error);
}

void SocketUploadJob::logMessage(const QString &localPath, const QString &message, bool ok)
{
    auto item = SyncFileItemPtr::create();
    QIcon icon; // null icon will cause the default icon
    if (ok) {
        item->_status = SyncFileItem::Success;
        item->_messageString = message;
    } else {
        icon = Theme::instance()->syncStateIcon(SyncResult::Error);
        item->_status = SyncFileItem::FatalError;
        item->_errorString = message;
    }
    ocApp()->gui()->slotShowTrayMessage(tr("%1 backup").arg(ocApp()->applicationName()), message, icon);
    item->_responseTimeStamp = QDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8();
    Q_EMIT ProgressDispatcher::instance()->itemCompleted(QDir::toNativeSeparators(localPath), item);
}
