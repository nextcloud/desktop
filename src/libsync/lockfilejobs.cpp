/*
 * Copyright (C) by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#include "lockfilejobs.h"

#include "account.h"
#include "common/syncjournaldb.h"
#include "filesystem.h"

#include <QLoggingCategory>
#include <QXmlStreamReader>

namespace OCC {

Q_LOGGING_CATEGORY(lcLockFileJob, "nextcloud.sync.networkjob.lockfile", QtInfoMsg)

LockFileJob::LockFileJob(const AccountPtr account,
                         SyncJournalDb* const journal,
                         const QString &path,
                         const QString &remoteSyncPathWithTrailingSlash,
                         const QString &localSyncPath,
                         const SyncFileItem::LockStatus requestedLockState,
                         const SyncFileItem::LockOwnerType lockOwnerType,
                         QObject *parent)
    : AbstractNetworkJob(account, path, parent)
    , _journal(journal)
    , _requestedLockState(requestedLockState)
    , _requestedLockOwnerType(lockOwnerType)
    , _remoteSyncPathWithTrailingSlash(remoteSyncPathWithTrailingSlash)
    , _localSyncPath(localSyncPath)
{
    if (!_localSyncPath.endsWith(QLatin1Char('/'))) {
        _localSyncPath.append(QLatin1Char('/'));
    }
}

void LockFileJob::start()
{
    qCInfo(lcLockFileJob()) << "start with path:" << path()
                            << "lock state:" <<  _requestedLockState
                            << "lock owner type:" << _requestedLockOwnerType;

    QNetworkRequest request;
    request.setRawHeader(QByteArrayLiteral("X-User-Lock"), QByteArrayLiteral("1"));
    if (_account->capabilities().filesLockTypeAvailable()) {
        if (_requestedLockOwnerType == SyncFileItem::LockOwnerType::UserLock) {
            request.setRawHeader(QByteArrayLiteral("X-User-Lock-Type"), ("0"));
        } else if (_requestedLockOwnerType == SyncFileItem::LockOwnerType::TokenLock) {
            request.setRawHeader(QByteArrayLiteral("X-User-Lock-Type"), ("2"));
        }
    }

    QByteArray verb;
    switch(_requestedLockState)
    {
    case SyncFileItem::LockStatus::LockedItem:
        verb = "LOCK";
        break;
    case SyncFileItem::LockStatus::UnlockedItem:
        verb = "UNLOCK";
        break;
    }
    sendRequest(verb, makeDavUrl(path()), request);

    AbstractNetworkJob::start();
}

bool LockFileJob::finished()
{
    if (reply()->error() != QNetworkReply::NoError) {
        qCInfo(lcLockFileJob()) << "finished with error" << reply()->error() << reply()->errorString() << _requestedLockState << _requestedLockOwnerType;
        const auto httpErrorCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpErrorCode == LOCKED_HTTP_ERROR_CODE) {
            const auto record = handleReply();
            if (static_cast<SyncFileItem::LockOwnerType>(record._lockstate._lockOwnerType) == SyncFileItem::LockOwnerType::UserLock) {
                Q_EMIT finishedWithError(httpErrorCode, {}, record._lockstate._lockOwnerDisplayName);
            } else {
                Q_EMIT finishedWithError(httpErrorCode, {}, record._lockstate._lockEditorApp);
            }
        } else if (httpErrorCode == PRECONDITION_FAILED_ERROR_CODE) {
            const auto record = handleReply();
            if (_requestedLockState == SyncFileItem::LockStatus::UnlockedItem && !record._lockstate._locked) {
                Q_EMIT finishedWithoutError();
            } else {
                Q_EMIT finishedWithError(httpErrorCode, reply()->errorString(), {});
            }
        } else {
            Q_EMIT finishedWithError(httpErrorCode, reply()->errorString(), {});
        }
    } else {
        qCInfo(lcLockFileJob()) << "success" << path() << _requestedLockState << _requestedLockOwnerType;
        handleReply();
        Q_EMIT finishedWithoutError();
    }
    return true;
}

void LockFileJob::setFileRecordLocked(SyncJournalFileRecord &record) const
{
    record._lockstate._locked = (_lockStatus == SyncFileItem::LockStatus::LockedItem);
    record._lockstate._lockOwnerType = static_cast<int>(_lockOwnerType);
    record._lockstate._lockOwnerDisplayName = _userDisplayName;
    record._lockstate._lockOwnerId = _userId;
    record._lockstate._lockEditorApp = _editorName;
    record._lockstate._lockTime = _lockTime;
    record._lockstate._lockTimeout = _lockTimeout;
    if (!_etag.isEmpty()) {
        record._etag = _etag;
    }
}

void LockFileJob::resetState()
{
    _lockStatus = SyncFileItem::LockStatus::UnlockedItem;
    _lockOwnerType = SyncFileItem::LockOwnerType::UserLock;
    _userDisplayName.clear();
    _editorName.clear();
    _userId.clear();
    _lockTime = 0;
    _lockTimeout = 0;
}

SyncJournalFileRecord LockFileJob::handleReply()
{
    const auto xml = reply()->readAll();

    QXmlStreamReader reader(xml);

    resetState();

    while (!reader.atEnd()) {
        const auto type = reader.readNext();
        const auto name = reader.name().toString();

        switch (type) {
        case QXmlStreamReader::TokenType::NoToken:
        case QXmlStreamReader::TokenType::Invalid:
        case QXmlStreamReader::TokenType::DTD:
        case QXmlStreamReader::TokenType::EntityReference:
        case QXmlStreamReader::TokenType::ProcessingInstruction:
        case QXmlStreamReader::TokenType::Comment:
        case QXmlStreamReader::TokenType::StartDocument:
        case QXmlStreamReader::TokenType::Characters:
        case QXmlStreamReader::TokenType::EndDocument:
        case QXmlStreamReader::TokenType::EndElement:
            break;
        case QXmlStreamReader::TokenType::StartElement:
            decodeStartElement(name, reader);
            break;
        }
    }

    SyncJournalFileRecord record;

    if (_lockStatus == SyncFileItem::LockStatus::LockedItem) {
        if (_lockOwnerType == SyncFileItem::LockOwnerType::UserLock && _userDisplayName.isEmpty()) {
            return record;
        }

        if (_lockOwnerType == SyncFileItem::LockOwnerType::AppLock && _editorName.isEmpty()) {
            return record;
        }

        if (_userId.isEmpty()) {
            return record;
        }

        if (_lockTime <= 0) {
            return record;
        }
    }

    const auto relativePathInDb = path().mid(_remoteSyncPathWithTrailingSlash.size());
    if (_journal->getFileRecord(relativePathInDb, &record) && record.isValid()) {
        setFileRecordLocked(record);
        if ((_lockStatus == SyncFileItem::LockStatus::LockedItem)
            && (_lockOwnerType == SyncFileItem::LockOwnerType::AppLock || _userId != account()->davUser())) {
            FileSystem::setFileReadOnly(_localSyncPath + relativePathInDb, true);
        }
        const auto result = _journal->setFileRecord(record);
        if (!result) {
            qCWarning(lcLockFileJob) << "Error when setting the file record to the database" << record._path << result.error();
        }
        _journal->commit("lock file job");
    }

    return record;
}

void LockFileJob::decodeStartElement(const QString &name,
                                     QXmlStreamReader &reader)
{
    if (name == QStringLiteral("lock")) {
        const auto valueText = reader.readElementText();
        if (!valueText.isEmpty()) {
            bool isValid = false;
            const auto convertedValue = valueText.toInt(&isValid);
            if (isValid) {
                _lockStatus = static_cast<SyncFileItem::LockStatus>(convertedValue);
            }
        }
    } else if (name == QStringLiteral("lock-owner-type")) {
        const auto valueText = reader.readElementText();
        bool isValid = false;
        const auto convertedValue = valueText.toInt(&isValid);
        if (isValid) {
            _lockOwnerType = static_cast<SyncFileItem::LockOwnerType>(convertedValue);
        } else {
            _lockOwnerType = SyncFileItem::LockOwnerType::UserLock;
        }
    } else if (name == QStringLiteral("lock-owner-displayname")) {
        _userDisplayName = reader.readElementText();
    } else if (name == QStringLiteral("lock-owner")) {
        _userId = reader.readElementText();
    } else if (name == QStringLiteral("lock-time")) {
        const auto valueText = reader.readElementText();
        bool isValid = false;
        const auto convertedValue = valueText.toLongLong(&isValid);
        if (isValid) {
            _lockTime = convertedValue;
        }
    } else if (name == QStringLiteral("lock-timeout")) {
        const auto valueText = reader.readElementText();
        bool isValid = false;
        const auto convertedValue = valueText.toLongLong(&isValid);
        if (isValid) {
            _lockTimeout = convertedValue;
        }
    } else if (name == QStringLiteral("lock-owner-editor")) {
        _editorName = reader.readElementText();
    } else if (name == QStringLiteral("getetag")) {
        _etag = reader.readElementText().toUtf8();
    }
}

}
