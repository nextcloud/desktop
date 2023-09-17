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

#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "gui/accountmanager.h"
#include "gui/folderman.h"
#include "gui/sharemanager.h"

#include "syncenginetestutils.h"

using namespace OCC;

struct FakeFileReplyDefinition
{
    QString fileOwnerUid;
    QString fileOwnerDisplayName;
    QString fileTarget;
    bool fileHasPreview = false;
    QString fileFileParent;
    QString fileSource;
    QString fileItemSource;
    QString fileItemType;
    int fileMailSend = 0;
    QString fileMimeType;
    QString fileParent;
    QString filePath;
    int fileStorage = 0;
    QString fileStorageId;
};

struct FakeShareDefinition
{
    FakeShareDefinition() = default;
    FakeShareDefinition(ShareTestHelper *helper,
                        const Share::ShareType type,
                        const QString &shareWith,
                        const QString &displayString,
                        const QString &password = QString(),
                        const QString &note = QString(),
                        const QString &expiration = QString());

    FakeFileReplyDefinition fileDefinition;
    QString shareId;
    bool shareCanDelete = false;
    bool shareCanEdit = false;
    QString shareUidOwner;
    QString shareDisplayNameOwner;
    QString sharePassword;
    int sharePermissions = 0;
    QString shareNote;
    int shareHideDownload = 0;
    QString shareExpiration;
    bool shareSendPasswordByTalk = false;
    int shareType = 0;
    QString shareShareWith;
    QString shareShareWithDisplayName;
    QString shareToken;
    QString linkShareName;
    QString linkShareLabel;
    QString linkShareUrl;

    [[nodiscard]] QJsonObject toShareJsonObject() const;
    [[nodiscard]] QByteArray toRequestReply() const;
};

class ShareTestHelper : public QObject
{
    Q_OBJECT

public:
    ShareTestHelper(QObject *parent = nullptr);
    ~ShareTestHelper() override;

    FolderMan fm;
    FakeFolder fakeFolder{FileInfo{}};
    FakeFileReplyDefinition fakeFileDefinition;

    AccountPtr account;
    AccountStatePtr accountState;

    int latestShareId = 0;

    static constexpr auto testFileName = "file.md";
    static constexpr auto searchResultsReplyDelay = 100;
    static constexpr auto expectedDtFormat = "yyyy-MM-dd 00:00:00";

    const QByteArray createNewShare(const Share::ShareType shareType, const QString &shareWith, const QString &password);
    [[nodiscard]] int shareCount() const;

signals:
    void setupSucceeded();

public slots:
    void setup();
    void appendShareReplyData(const FakeShareDefinition &definition);
    void resetTestShares();
    void resetTestData();

private slots:
    [[nodiscard]] QNetworkReply *qnamOverride(const QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device);
    [[nodiscard]] QNetworkReply *handleSharePostOperation(const QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device);
    [[nodiscard]] QNetworkReply *handleSharePutOperation(const QNetworkAccessManager::Operation op, const QNetworkRequest &req, const QString &reqPath, QIODevice *device);
    [[nodiscard]] QNetworkReply *handleShareDeleteOperation(const QNetworkAccessManager::Operation op, const QNetworkRequest &req, const QString &reqPath);
    [[nodiscard]] QNetworkReply *handleShareGetOperation(const QNetworkAccessManager::Operation op, const QNetworkRequest &req, const QString &reqPath);

private:
    QScopedPointer<FakeQNAM> _fakeQnam;

    QByteArray _fake404Response = R"({"ocs":{"meta":{"status":"failure","statuscode":404,"message":"Invalid query, please check the syntax. API specifications are here: http:\/\/www.freedesktop.org\/wiki\/Specifications\/open-collaboration-services.\n"},"data":[]}})";
    QByteArray _fake403Response = R"({"ocs":{"meta":{"status":"failure","statuscode":403,"message":"Operation not allowed."},"data":[]}})";
    QByteArray _fake400Response = R"({"ocs":{"meta":{"status":"failure","statuscode":400,"message":"Parameter is incorrect.\n"},"data":[]}})";
    QByteArray _fake200JsonResponse = R"({"ocs":{"data":[],"meta":{"message":"OK","status":"ok","statuscode":200}}})";

    QJsonArray _sharesReplyData;
    QVariantMap _fakeCapabilities;
    QSet<int> _liveShareIds;
};
