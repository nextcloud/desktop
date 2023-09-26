/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "sharetestutils.h"

#include "testhelper.h"

using namespace OCC;

FakeShareDefinition::FakeShareDefinition(ShareTestHelper *helper,
                                         const Share::ShareType type,
                                         const QString &shareWith,
                                         const QString &displayString,
                                         const QString &password,
                                         const QString &note,
                                         const QString &expiration)
{
    ++helper->latestShareId;
    const auto idString = QString::number(helper->latestShareId);


    fileDefinition = helper->fakeFileDefinition;
    shareId = idString;
    shareCanDelete = true;
    shareCanEdit = true;
    shareUidOwner = helper->account->davUser();;
    shareDisplayNameOwner = helper->account->davDisplayName();
    sharePassword = password;
    sharePermissions = static_cast<int>(SharePermissions(SharePermissionRead |
                                                         SharePermissionUpdate |
                                                         SharePermissionCreate |
                                                         SharePermissionDelete |
                                                         SharePermissionShare));
    shareNote = note;
    shareHideDownload = 0;
    shareExpiration = expiration;
    shareSendPasswordByTalk = false;
    shareType = type;

    const auto token = QString(QStringLiteral("GQ4aLrZEdJJkopW-") + idString);
    // Weird, but it's what the server does
    const auto finalShareWith = type == Share::TypeLink ? password : shareWith;
    const auto shareWithDisplayName = type == Share::TypeLink ? QStringLiteral("(Shared Link)") : displayString;
    const auto linkLabel = type == Share::TypeLink ? displayString : QString();
    const auto linkName = linkShareLabel;
    const auto linkUrl = type == Share::TypeLink ? QString(helper->account->davUrl().toString() + QStringLiteral("/s/") + token) : QString();

    shareShareWith = finalShareWith;
    shareShareWithDisplayName = shareWithDisplayName;
    shareToken = token;
    linkShareName = linkName;
    linkShareLabel = linkLabel;
    linkShareUrl = linkUrl;
}

QJsonObject FakeShareDefinition::toShareJsonObject() const
{
    QJsonObject newShareJson;
    newShareJson.insert("uid_file_owner", fileDefinition.fileOwnerUid);
    newShareJson.insert("displayname_file_owner", fileDefinition.fileOwnerDisplayName);
    newShareJson.insert("file_target", fileDefinition.fileTarget);
    newShareJson.insert("has_preview", fileDefinition.fileHasPreview);
    newShareJson.insert("file_parent", fileDefinition.fileFileParent);
    newShareJson.insert("file_source", fileDefinition.fileSource);
    newShareJson.insert("item_source", fileDefinition.fileItemSource);
    newShareJson.insert("item_type", fileDefinition.fileItemType);
    newShareJson.insert("mail_send", fileDefinition.fileMailSend);
    newShareJson.insert("mimetype", fileDefinition.fileMimeType);
    newShareJson.insert("parent", fileDefinition.fileParent);
    newShareJson.insert("path", fileDefinition.filePath);
    newShareJson.insert("storage", fileDefinition.fileStorage);
    newShareJson.insert("storage_id", fileDefinition.fileStorageId);
    newShareJson.insert("id", shareId);
    newShareJson.insert("can_delete", shareCanDelete);
    newShareJson.insert("can_edit", shareCanEdit);
    newShareJson.insert("uid_owner", shareUidOwner);
    newShareJson.insert("displayname_owner", shareDisplayNameOwner);
    newShareJson.insert("password", sharePassword);
    newShareJson.insert("permissions", sharePermissions);
    newShareJson.insert("note", shareNote);
    newShareJson.insert("hide_download", shareHideDownload);
    newShareJson.insert("expiration", shareExpiration);
    newShareJson.insert("send_password_by_talk", shareSendPasswordByTalk);
    newShareJson.insert("share_type", shareType);
    newShareJson.insert("share_with", shareShareWith);
    newShareJson.insert("share_with_displayname", shareShareWithDisplayName);
    newShareJson.insert("token", shareToken);
    newShareJson.insert("name", linkShareName);
    newShareJson.insert("label", linkShareLabel);
    newShareJson.insert("url", linkShareUrl);

    return newShareJson;
}

QByteArray FakeShareDefinition::toRequestReply() const
{
    const auto shareJson = toShareJsonObject();
    return jsonValueToOccReply(shareJson);
}

// Below is ShareTestHelper
ShareTestHelper::ShareTestHelper(QObject *parent)
    : QObject(parent)
{
}

ShareTestHelper::~ShareTestHelper()
{
    const auto folder = FolderMan::instance()->folder(fakeFolder.localPath());
    if (folder) {
        FolderMan::instance()->removeFolder(folder);
    }
    AccountManager::instance()->deleteAccount(accountState.data());
}

void ShareTestHelper::setup()
{
    _fakeQnam.reset(new FakeQNAM({}));
    _fakeQnam->setOverride([this](const QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
        return qnamOverride(op, req, device);
    });

    account = Account::create();
    account->setCredentials(new FakeCredentials{_fakeQnam.data()});
    account->setUrl(QUrl(("owncloud://somehost/owncloud")));
    account->setCapabilities(_fakeCapabilities);
    accountState = new AccountState(account);
    AccountManager::instance()->addAccount(account);

    QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    fakeFolder.localModifier().insert(testFileName);

    const auto folderMan = FolderMan::instance();
    QCOMPARE(folderMan, &fm);
    auto folderDef = folderDefinition(fakeFolder.localPath());
    folderDef.targetPath = QString();
    QVERIFY(folderMan->addFolder(accountState.data(), folderDef));
    const auto folder = FolderMan::instance()->folder(fakeFolder.localPath());
    QVERIFY(folder);
    QVERIFY(fakeFolder.syncOnce());
    QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

    const auto fakeFileInfo = fakeFolder.remoteModifier().find(testFileName);
    QVERIFY(fakeFileInfo);
    fakeFileInfo->permissions.setPermission(RemotePermissions::CanReshare);
    QVERIFY(fakeFolder.syncOnce());
    QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    QVERIFY(fakeFileInfo->permissions.CanReshare);

    _fakeCapabilities = QVariantMap {
        {QStringLiteral("files_sharing"), QVariantMap {
            {QStringLiteral("api_enabled"), true},
            {QStringLiteral("default_permissions"), 19},
            {QStringLiteral("public"), QVariantMap {
                {QStringLiteral("enabled"), true},
                {QStringLiteral("expire_date"), QVariantMap {
                    {QStringLiteral("days"), 30},
                    {QStringLiteral("enforced"), false},
                }},
                {QStringLiteral("expire_date_internal"), QVariantMap {
                     {QStringLiteral("days"), 30},
                     {QStringLiteral("enforced"), false},
                }},
                {QStringLiteral("expire_date_remote"), QVariantMap {
                     {QStringLiteral("days"), 30},
                     {QStringLiteral("enforced"), false},
                }},
                {QStringLiteral("password"), QVariantMap {
                    {QStringLiteral("enforced"), false},
                }},
            }},
            {QStringLiteral("sharebymail"), QVariantMap {
                {QStringLiteral("enabled"), true},
                {QStringLiteral("password"), QVariantMap {
                    {QStringLiteral("enforced"), false},
                }},
            }},
        }},
    };

   // Generate test data
   // Properties that apply to the file generally
    const auto fileOwnerUid = account->davUser();
    const auto fileOwnerDisplayName = account->davDisplayName();
    const auto fileTarget = QString(QStringLiteral("/") + fakeFileInfo->name);
    const auto fileHasPreview = true;
    const auto fileFileParent = QString(fakeFolder.remoteModifier().fileId);
    const auto fileSource = QString(fakeFileInfo->fileId);
    const auto fileItemSource = fileSource;
    const auto fileItemType = QStringLiteral("file");
    const auto fileMailSend = 0;
    const auto fileMimeType = QStringLiteral("text/markdown");
    const auto fileParent = QString();
    const auto filePath = fakeFileInfo->path();
    const auto fileStorage = 3;
    const auto fileStorageId = QString(QStringLiteral("home::") + account->davUser());

    fakeFileDefinition = FakeFileReplyDefinition {
        fileOwnerUid,
        fileOwnerDisplayName,
        fileTarget,
        fileHasPreview,
        fileFileParent,
        fileSource,
        fileItemSource,
        fileItemType,
        fileMailSend,
        fileMimeType,
        fileParent,
        filePath,
        fileStorage,
        fileStorageId,
    };

    emit setupSucceeded();
}

QNetworkReply *ShareTestHelper::qnamOverride(QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device)
{
    QNetworkReply *reply = nullptr;

    const auto reqUrl = req.url();
    const auto reqRawPath = reqUrl.path();
    const auto reqPath = reqRawPath.startsWith("/owncloud/") ? reqRawPath.mid(10) : reqRawPath;
    qDebug() << req.url() << reqPath << op;

    // Properly formatted PROPFIND URL goes something like:
    // https://cloud.nextcloud.com/remote.php/dav/files/claudio/Readme.md
    if(reqPath.endsWith(testFileName) && req.attribute(QNetworkRequest::CustomVerbAttribute).toString() == "PROPFIND") {

        reply = new FakePropfindReply(fakeFolder.remoteModifier(), op, req, this);

    } else if (req.url().toString().startsWith(accountState->account()->url().toString()) &&
               reqPath.startsWith(QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares"))) {

        if (op == QNetworkAccessManager::PostOperation) {
            reply = handleSharePostOperation(op, req, device);

        } else if(req.attribute(QNetworkRequest::CustomVerbAttribute).toString() == "DELETE") {
            reply = handleShareDeleteOperation(op, req, reqPath);

        } else if(op == QNetworkAccessManager::PutOperation) {
            reply = handleSharePutOperation(op, req, reqPath, device);

        } else if(req.attribute(QNetworkRequest::CustomVerbAttribute).toString() == "GET") {
            reply = handleShareGetOperation(op, req, reqPath);
        }
    } else {
        reply = new FakeErrorReply(op, req, this, 404, _fake404Response);
    }

    return reply;
}

QNetworkReply *ShareTestHelper::handleSharePostOperation(QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device)
{
    QNetworkReply *reply = nullptr;

    // POST https://somehost/owncloud/ocs/v2.php/apps/files_sharing/api/v1/shares?format=json
    // Header: { Ocs-APIREQUEST: true, Content-Type: application/x-www-form-urlencoded, X-Request-ID: 1527752d-e147-4da7-89b8-fb06315a5fad, }
    // Data: [path=file.md&shareType=3]"
    const QUrlQuery urlQuery(req.url());
    const auto formatParam = urlQuery.queryItemValue(QStringLiteral("format"));

    if (formatParam == QStringLiteral("json")) {
        device->open(QIODevice::ReadOnly);
        const auto requestBody = device->readAll();
        device->close();

        const auto requestData = requestBody.split('&');
        // We don't care about path since we know the file we are testing with
        auto requestShareType = -10; // Just in case
        QString requestShareWith;
        QString requestName;
        QString requestPassword;

        for(const auto &data : requestData) {
            const auto requestDataUrl = QUrl::fromPercentEncoding(data);
            const QString requestDataUrlString(requestDataUrl);

            if (data.contains("shareType=")) {
                const auto shareTypeString = requestDataUrlString.mid(10);
                requestShareType = Share::ShareType(shareTypeString.toInt());
            } else if (data.contains("shareWith=")) {
                requestShareWith = data.mid(10);
            } else if (data.contains("name=")) {
                requestName = data.mid(5);
            } else if (data.contains("password=")) {
                requestPassword = data.mid(9);
            }
        }

        if (requestPassword.isEmpty() &&
            ((requestShareType == Share::TypeEmail && account->capabilities().shareEmailPasswordEnforced()) ||
             (requestShareType == Share::TypeLink && account->capabilities().sharePublicLinkEnforcePassword()))) {

            reply = new FakePayloadReply(op, req, _fake403Response, searchResultsReplyDelay, _fakeQnam.data());

        } else if (requestShareType >= 0) {
            const auto shareType = Share::ShareType(requestShareType);
            reply = new FakePayloadReply(op, req, createNewShare(shareType, requestShareWith, requestPassword), searchResultsReplyDelay, _fakeQnam.data());
        }
    }

    return reply;
}

QNetworkReply *ShareTestHelper::handleSharePutOperation(const QNetworkAccessManager::Operation op, const QNetworkRequest &req, const QString &reqPath, QIODevice *device)
{
    QNetworkReply *reply = nullptr;

    const auto splitUrlPath = reqPath.split('/');
    const auto shareId = splitUrlPath.last();

    const QUrlQuery urlQuery(req.url());
    const auto formatParam = urlQuery.queryItemValue(QStringLiteral("format"));

    if (formatParam == QStringLiteral("json")) {
        device->open(QIODevice::ReadOnly);
        const auto requestBody = device->readAll();
        device->close();

        const auto requestData = requestBody.split('&');

        const auto existingShareIterator = std::find_if(_sharesReplyData.cbegin(), _sharesReplyData.cend(), [&shareId](const QJsonValue &value) {
            return value.toObject().value("id").toString() == shareId;
        });

        if (existingShareIterator == _sharesReplyData.cend()) {
            reply = new FakeErrorReply(op, req, this, 404, _fake404Response);
        } else {
            const auto existingShareValue = *existingShareIterator;
            auto shareObject = existingShareValue.toObject();

            for (const auto &requestDataItem : requestData) {
                const auto requestSplit = requestDataItem.split('=');
                auto requestKey = requestSplit.first();
                auto requestValue = requestSplit.last();

                     // We send expireDate without time but the server returns with time at 00:00:00
                if (requestKey == "expireDate") {
                    requestKey = "expiration";
                    requestValue.append(" 00:00:00");
                }

                shareObject.insert(QString(requestKey), QString(requestValue));
            }

            _sharesReplyData.replace(existingShareIterator - _sharesReplyData.cbegin(), shareObject);
            reply = new FakePayloadReply(op, req, jsonValueToOccReply(shareObject), searchResultsReplyDelay, _fakeQnam.data());
        }
    }

    return reply;
}


QNetworkReply *ShareTestHelper::handleShareDeleteOperation(const QNetworkAccessManager::Operation op, const QNetworkRequest &req, const QString &reqPath)
{
    QNetworkReply *reply = nullptr;

    const auto splitUrlPath = reqPath.split('/');
    const auto shareId = splitUrlPath.last();

    const auto existingShareIterator = std::find_if(_sharesReplyData.cbegin(), _sharesReplyData.cend(), [&shareId](const QJsonValue &value) {
        return value.toObject().value("id").toString() == shareId;
    });

    if (existingShareIterator == _sharesReplyData.cend()) {
        reply = new FakeErrorReply(op, req, this, 404, _fake404Response);
    } else {
        _sharesReplyData.removeAt(existingShareIterator - _sharesReplyData.cbegin());
        reply = new FakePayloadReply(op, req, _fake200JsonResponse, searchResultsReplyDelay, _fakeQnam.data());
    }

    return reply;
}

QNetworkReply *ShareTestHelper::handleShareGetOperation(const QNetworkAccessManager::Operation op, const QNetworkRequest &req, const QString &reqPath)
{
    QNetworkReply *reply = nullptr;

    // Properly formatted request to fetch shares goes something like:
    // GET https://somehost/owncloud/ocs/v2.php/apps/files_sharing/api/v1/shares?path=file.md&reshares=true&format=json
    // Header: { Ocs-APIREQUEST: true, Content-Type: application/x-www-form-urlencoded, X-Request-ID: 8ba8960d-ca0d-45ba-abf4-03ab95ba6064, }
    // Data: []
    const auto urlQuery = QUrlQuery(req.url());
    const auto pathParam = urlQuery.queryItemValue(QStringLiteral("path"));
    const auto resharesParam = urlQuery.queryItemValue(QStringLiteral("reshares"));
    const auto formatParam = urlQuery.queryItemValue(QStringLiteral("format"));

    if (formatParam != QStringLiteral("json") || (!pathParam.isEmpty() && !pathParam.endsWith(QString(testFileName)))) {
        reply = new FakeErrorReply(op, req, this, 400, _fake400Response);
    } else if (reqPath.contains(QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares"))) {
        reply = new FakePayloadReply(op, req, jsonValueToOccReply(_sharesReplyData), searchResultsReplyDelay, _fakeQnam.data());
    }

    return reply;
}

const QByteArray ShareTestHelper::createNewShare(const Share::ShareType shareType, const QString &shareWith, const QString &password)
{
    const auto displayString = shareType == Share::TypeLink ? QString() : shareWith;
    const FakeShareDefinition newShareDefinition(this,
                                                 shareType,
                                                 shareWith,
                                                 displayString,
                                                 password);

    _sharesReplyData.append(newShareDefinition.toShareJsonObject());
    return newShareDefinition.toRequestReply();
}

int ShareTestHelper::shareCount() const
{
    return _sharesReplyData.count();
}

void ShareTestHelper::appendShareReplyData(const FakeShareDefinition &definition)
{
    _sharesReplyData.append(definition.toShareJsonObject());
}

void ShareTestHelper::resetTestShares()
{
    _sharesReplyData = QJsonArray();
}

void ShareTestHelper::resetTestData()
{
    resetTestShares();
    account->setCapabilities(_fakeCapabilities);
}
