/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "gui/filedetails/sharemodel.h"

#include <QTest>
#include <QSignalSpy>
#include <QFileInfo>
#include <QFlags>

#include "accountmanager.h"
#include "folderman.h"
#include "syncenginetestutils.h"
#include "testhelper.h"
#include "libsync/theme.h"

using namespace OCC;

static QByteArray fake404Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":404,"message":"Invalid query, please check the syntax. API specifications are here: http:\/\/www.freedesktop.org\/wiki\/Specifications\/open-collaboration-services.\n"},"data":[]}}
)";

static QByteArray fake403Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":403,"message":"Operation not allowed."},"data":[]}}
)";

static QByteArray fake400Response = R"(
{"ocs":{"meta":{"status":"failure","statuscode":400,"message":"Parameter is incorrect.\n"},"data":[]}}
)";

static QByteArray fake200JsonResponse = R"(
{"ocs":{"data":[],"meta":{"message":"OK","status":"ok","statuscode":200}}}
)";

constexpr auto testFileName = "file.md";
constexpr auto searchResultsReplyDelay = 100;
constexpr auto expectedDtFormat = "yyyy-MM-dd 00:00:00";

class TestShareModel : public QObject
{
    Q_OBJECT

public:
    TestShareModel() = default;
    ~TestShareModel() override
    {
        const auto folder = FolderMan::instance()->folder(_fakeFolder.localPath());
        if (folder) {
            FolderMan::instance()->removeFolder(folder);
        }
        AccountManager::instance()->deleteAccount(_accountState.data());
    }

    struct FakeFileReplyDefinition
    {
        QString fileOwnerUid;
        QString fileOwnerDisplayName;
        QString fileTarget;
        bool fileHasPreview;
        QString fileFileParent;
        QString fileSource;
        QString fileItemSource;
        QString fileItemType;
        int fileMailSend;
        QString fileMimeType;
        QString fileParent;
        QString filePath;
        int fileStorage;
        QString fileStorageId;
    };

    struct FakeShareDefinition
    {
        FakeFileReplyDefinition fileDefinition;
        QString shareId;
        bool shareCanDelete;
        bool shareCanEdit;
        QString shareUidOwner;
        QString shareDisplayNameOwner;
        QString sharePassword;
        int sharePermissions;
        QString shareNote;
        int shareHideDownload;
        QString shareExpiration;
        bool shareSendPasswordByTalk;
        int shareType;
        QString shareShareWith;
        QString shareShareWithDisplayName;
        QString shareToken;
        QString linkShareName;
        QString linkShareLabel;
        QString linkShareUrl;
    };

    const QByteArray fakeSharesResponse() const
    {
        QJsonObject root;
        QJsonObject ocs;
        QJsonObject meta;

        meta.insert("statuscode", 200);

        ocs.insert(QStringLiteral("data"), _sharesReplyData);
        ocs.insert(QStringLiteral("meta"), meta);
        root.insert(QStringLiteral("ocs"), ocs);

        return QJsonDocument(root).toJson();
    }

    QJsonObject shareDefinitionToJson(const FakeShareDefinition &definition)
    {
        QJsonObject newShareJson;
        newShareJson.insert("uid_file_owner", definition.fileDefinition.fileOwnerUid);
        newShareJson.insert("displayname_file_owner", definition.fileDefinition.fileOwnerDisplayName);
        newShareJson.insert("file_target", definition.fileDefinition.fileTarget);
        newShareJson.insert("has_preview", definition.fileDefinition.fileHasPreview);
        newShareJson.insert("file_parent", definition.fileDefinition.fileFileParent);
        newShareJson.insert("file_source", definition.fileDefinition.fileSource);
        newShareJson.insert("item_source", definition.fileDefinition.fileItemSource);
        newShareJson.insert("item_type", definition.fileDefinition.fileItemType);
        newShareJson.insert("mail_send", definition.fileDefinition.fileMailSend);
        newShareJson.insert("mimetype", definition.fileDefinition.fileMimeType);
        newShareJson.insert("parent", definition.fileDefinition.fileParent);
        newShareJson.insert("path", definition.fileDefinition.filePath);
        newShareJson.insert("storage", definition.fileDefinition.fileStorage);
        newShareJson.insert("storage_id", definition.fileDefinition.fileStorageId);
        newShareJson.insert("id", definition.shareId);
        newShareJson.insert("can_delete", definition.shareCanDelete);
        newShareJson.insert("can_edit", definition.shareCanEdit);
        newShareJson.insert("uid_owner", definition.shareUidOwner);
        newShareJson.insert("displayname_owner", definition.shareDisplayNameOwner);
        newShareJson.insert("password", definition.sharePassword);
        newShareJson.insert("permissions", definition.sharePermissions);
        newShareJson.insert("note", definition.shareNote);
        newShareJson.insert("hide_download", definition.shareHideDownload);
        newShareJson.insert("expiration", definition.shareExpiration);
        newShareJson.insert("send_password_by_talk", definition.shareSendPasswordByTalk);
        newShareJson.insert("share_type", definition.shareType);
        newShareJson.insert("share_with", definition.shareShareWith); // Doesn't seem to make sense but is server behaviour
        newShareJson.insert("share_with_displayname", definition.shareShareWithDisplayName);
        newShareJson.insert("token", definition.shareToken);
        newShareJson.insert("name", definition.linkShareName);
        newShareJson.insert("label", definition.linkShareLabel);
        newShareJson.insert("url", definition.linkShareUrl);

        return newShareJson;
    }

    void appendShareReplyData(const FakeShareDefinition &definition)
    {
        const auto shareJson = shareDefinitionToJson(definition);
        _sharesReplyData.append(shareJson);
    }

    const QByteArray createNewShare(const Share::ShareType shareType, const QString &shareWith)
    {
        ++_latestShareId;
        const auto newShareId = QString::number(_latestShareId);
        const auto newShareCanDelete = true;
        const auto newShareCanEdit = true;
        const auto newShareUidOwner = _account->davUser();
        const auto newShareDisplayNameOwner = _account->davDisplayName();
        const auto newSharePassword = QString();
        const auto newSharePermissions = static_cast<int>(SharePermissions(SharePermissionRead |
                                                                           SharePermissionUpdate |
                                                                           SharePermissionCreate |
                                                                           SharePermissionDelete |
                                                                           SharePermissionShare));
        const auto newShareNote = QString();
        const auto newShareHideDownload = 0;
        const auto newShareExpiration = QString();
        const auto newShareSendPasswordByTalk = false;
        const auto newShareType = shareType;
        const auto newShareShareWith = shareType == Share::TypeLink ? newSharePassword : shareWith;
        const auto newShareShareWithDisplayName = shareType == Share::TypeLink ? QStringLiteral("(Shared Link)") : shareWith;
        const auto newShareToken = QString::number(qHash(newShareId + _fakeFileDefinition.filePath));
        const auto newLinkShareName = QString();
        const auto newLinkShareLabel = QString();
        const auto newLinkShareUrl = shareType == Share::TypeLink ? QString(_account->davUrl().toString() + QStringLiteral("/s/") + newShareToken) : QString();

        const FakeShareDefinition newShareDefinition {
            _fakeFileDefinition,
            newShareId,
            newShareCanDelete,
            newShareCanEdit,
            newShareUidOwner,
            newShareDisplayNameOwner,
            newSharePassword,
            newSharePermissions,
            newShareNote,
            newShareHideDownload,
            newShareExpiration,
            newShareSendPasswordByTalk,
            newShareType,
            newShareShareWith,
            newShareShareWithDisplayName,
            newShareToken,
            newLinkShareName,
            newLinkShareLabel,
            newLinkShareUrl,
        };

        const auto shareJson = shareDefinitionToJson(newShareDefinition);
        _sharesReplyData.append(shareJson);
        return shareWrappedAsReply(shareJson);
    }

    QByteArray shareWrappedAsReply(const QJsonObject &shareObject)
    {
        QJsonObject root;
        QJsonObject ocs;
        QJsonObject meta;

        meta.insert("statuscode", 200);

        ocs.insert(QStringLiteral("data"), shareObject);
        ocs.insert(QStringLiteral("meta"), meta);
        root.insert(QStringLiteral("ocs"), ocs);

        return QJsonDocument(root).toJson();
    }

    void resetTestData()
    {
        _sharesReplyData = QJsonArray();
        _account->setCapabilities(_fakeCapabilities);
    }

private:
    FolderMan _fm;
    FakeFolder _fakeFolder{FileInfo{}};

    AccountPtr _account;
    AccountStatePtr _accountState;
    QScopedPointer<FakeQNAM> _fakeQnam;
    FakeFileReplyDefinition _fakeFileDefinition;
    FakeShareDefinition _testLinkShareDefinition;
    FakeShareDefinition _testEmailShareDefinition;
    FakeShareDefinition _testUserShareDefinition;
    FakeShareDefinition _testRemoteShareDefinition;
    QJsonArray _sharesReplyData;
    QVariantMap _fakeCapabilities;
    QSet<int> _liveShareIds;
    int _latestShareId = 0;

private slots:
    void initTestCase()
    {
        _fakeQnam.reset(new FakeQNAM({}));
        _fakeQnam->setOverride([this](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *device) {
            QNetworkReply *reply = nullptr;

            const auto reqUrl = req.url();
            const auto reqRawPath = reqUrl.path();
            const auto reqPath = reqRawPath.startsWith("/owncloud/") ? reqRawPath.mid(10) : reqRawPath;
            qDebug() << req.url() << reqPath << op;

            // Properly formatted PROPFIND URL goes something like:
            // https://cloud.nextcloud.com/remote.php/dav/files/claudio/Readme.md
            if(reqPath.endsWith(testFileName) && req.attribute(QNetworkRequest::CustomVerbAttribute) == "PROPFIND") {

                reply = new FakePropfindReply(_fakeFolder.remoteModifier(), op, req, this);

            } else if (req.url().toString().startsWith(_accountState->account()->url().toString()) &&
                       reqPath.startsWith(QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares")) &&
                       op == QNetworkAccessManager::PostOperation) {

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
                            ((requestShareType == Share::TypeEmail && _account->capabilities().shareEmailPasswordEnforced()) ||
                            (requestShareType == Share::TypeLink && _account->capabilities().sharePublicLinkEnforcePassword()))) {

                        reply = new FakePayloadReply(op, req, fake403Response, searchResultsReplyDelay, _fakeQnam.data());

                    } else if (requestShareType >= 0) {
                        const auto shareType = Share::ShareType(requestShareType);
                        reply = new FakePayloadReply(op, req, createNewShare(shareType, requestShareWith), searchResultsReplyDelay, _fakeQnam.data());
                    }
                }

            } else if(req.url().toString().startsWith(_accountState->account()->url().toString()) &&
                      reqPath.startsWith(QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares")) &&
                      req.attribute(QNetworkRequest::CustomVerbAttribute) == "DELETE") {

                const auto splitUrlPath = reqPath.split('/');
                const auto shareId = splitUrlPath.last();

                const auto existingShareIterator = std::find_if(_sharesReplyData.cbegin(), _sharesReplyData.cend(), [&shareId](const QJsonValue &value) {
                    return value.toObject().value("id").toString() == shareId;
                });

                if (existingShareIterator == _sharesReplyData.cend()) {
                    reply = new FakeErrorReply(op, req, this, 404, fake404Response);
                } else {
                    _sharesReplyData.removeAt(existingShareIterator - _sharesReplyData.cbegin());
                    reply = new FakePayloadReply(op, req, fake200JsonResponse, searchResultsReplyDelay, _fakeQnam.data());
                }

            } else if(req.url().toString().startsWith(_accountState->account()->url().toString()) &&
                      reqPath.startsWith(QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares")) &&
                      op == QNetworkAccessManager::PutOperation) {

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
                        reply = new FakeErrorReply(op, req, this, 404, fake404Response);
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
                        reply = new FakePayloadReply(op, req, shareWrappedAsReply(shareObject), searchResultsReplyDelay, _fakeQnam.data());
                    }
                }

            } else if(req.url().toString().startsWith(_accountState->account()->url().toString()) &&
                      reqPath.startsWith(QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares")) &&
                      req.attribute(QNetworkRequest::CustomVerbAttribute) == "GET") {

                // Properly formatted request to fetch shares goes something like:
                // GET https://somehost/owncloud/ocs/v2.php/apps/files_sharing/api/v1/shares?path=file.md&reshares=true&format=json
                // Header: { Ocs-APIREQUEST: true, Content-Type: application/x-www-form-urlencoded, X-Request-ID: 8ba8960d-ca0d-45ba-abf4-03ab95ba6064, }
                // Data: []
                const auto urlQuery = QUrlQuery(req.url());
                const auto pathParam = urlQuery.queryItemValue(QStringLiteral("path"));
                const auto resharesParam = urlQuery.queryItemValue(QStringLiteral("reshares"));
                const auto formatParam = urlQuery.queryItemValue(QStringLiteral("format"));

                if (formatParam != QStringLiteral("json") || (!pathParam.isEmpty() && pathParam != QString(testFileName))) {
                    reply = new FakeErrorReply(op, req, this, 400, fake400Response);
                } else if (reqPath.contains(QStringLiteral("ocs/v2.php/apps/files_sharing/api/v1/shares"))) {
                    reply = new FakePayloadReply(op, req, fakeSharesResponse(), searchResultsReplyDelay, _fakeQnam.data());
                }

            } else if (!req.url().toString().startsWith(_accountState->account()->url().toString())) {
                reply = new FakeErrorReply(op, req, this, 404, fake404Response);
            } else if (!reply) {
                return qobject_cast<QNetworkReply*>(new FakeErrorReply(op, req, this, 404, QByteArrayLiteral("{error: \"Not found!\"}")));
            }

            return reply;
        });

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

        _account = Account::create();
        _account->setCredentials(new FakeCredentials{_fakeQnam.data()});
        _account->setUrl(QUrl(("owncloud://somehost/owncloud")));
        _account->setCapabilities(_fakeCapabilities);
        _accountState = new AccountState(_account);
        AccountManager::instance()->addAccount(_account);

        QCOMPARE(_fakeFolder.currentLocalState(), _fakeFolder.currentRemoteState());
        _fakeFolder.localModifier().insert(testFileName);

        const auto folderMan = FolderMan::instance();
        QCOMPARE(folderMan, &_fm);
        QVERIFY(folderMan->addFolder(_accountState.data(), folderDefinition(_fakeFolder.localPath())));
        const auto folder = FolderMan::instance()->folder(_fakeFolder.localPath());
        QVERIFY(folder);
        QVERIFY(_fakeFolder.syncOnce());
        QCOMPARE(_fakeFolder.currentLocalState(), _fakeFolder.currentRemoteState());
        ItemCompletedSpy completeSpy(_fakeFolder);

        const auto fakeFileInfo = _fakeFolder.remoteModifier().find(testFileName);
        QVERIFY(fakeFileInfo);
        fakeFileInfo->permissions.setPermission(RemotePermissions::CanReshare);
        QVERIFY(_fakeFolder.syncOnce());
        QCOMPARE(_fakeFolder.currentLocalState(), _fakeFolder.currentRemoteState());
        QVERIFY(fakeFileInfo->permissions.CanReshare);

        // Generate test data
        // Properties that apply to the file generally
        const auto fileOwnerUid = _account->davUser();
        const auto fileOwnerDisplayName = _account->davDisplayName();
        const auto fileTarget = QString(QStringLiteral("/") + fakeFileInfo->name);
        const auto fileHasPreview = true;
        const auto fileFileParent = QString(_fakeFolder.remoteModifier().fileId);
        const auto fileSource = QString(fakeFileInfo->fileId);
        const auto fileItemSource = fileSource;
        const auto fileItemType = QStringLiteral("file");
        const auto fileMailSend = 0;
        const auto fileMimeType = QStringLiteral("text/markdown");
        const auto fileParent = QString();
        const auto filePath = fakeFileInfo->path();
        const auto fileStorage = 3;
        const auto fileStorageId = QString(QStringLiteral("home::") + _account->davUser());

        _fakeFileDefinition = FakeFileReplyDefinition {
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

        const auto testSharePassword = "3|$argon2id$v=19$m=65536,"
                                       "t=4,"
                                       "p=1$M2FoLnliWkhIZkwzWjFBQg$BPraP+JUqP1sV89rkymXpCGxHBlCct6bZ39xUGaYQ5w";
        const auto testShareToken = "GQ4aLrZEdJJkopW";
        const auto testShareCanDelete = true;
        const auto testShareCanEdit = true;
        const auto testShareUidOwner = _account->davUser();
        const auto testShareDisplayNameOwner = _account->davDisplayName();
        const auto testSharePermissions = static_cast<int>(SharePermissions(SharePermissionRead |
                                                                            SharePermissionUpdate |
                                                                            SharePermissionCreate |
                                                                            SharePermissionDelete |
                                                                            SharePermissionShare));
        const auto testShareNote = QStringLiteral("This is a note!");
        const auto testShareHideDownload = 0;
        const auto testShareExpiration = QDate::currentDate().addDays(1).toString(expectedDtFormat);
        const auto testShareSendPasswordByTalk = false;

        ++_latestShareId;
        const auto linkShareShareWith = testSharePassword; // Weird, but it's what the server does
        const auto linkShareShareWithDisplayName = QStringLiteral("(Shared Link)");
        const auto linkShareUrl = QString(_account->davUrl().toString() + QStringLiteral("/s/") + testShareToken);

        _testLinkShareDefinition = FakeShareDefinition {
            _fakeFileDefinition,
            QString::number(_latestShareId),
            testShareCanDelete,
            testShareCanEdit,
            testShareUidOwner,
            testShareDisplayNameOwner,
            testSharePassword,
            testSharePermissions,
            testShareNote,
            testShareHideDownload,
            testShareExpiration,
            testShareSendPasswordByTalk,
            Share::TypeLink,
            linkShareShareWith,
            linkShareShareWithDisplayName,
            testShareToken,
            QStringLiteral("Link share name"),
            QStringLiteral("Link share label"),
            linkShareUrl,
        };

        ++_latestShareId;
        const auto emailShareShareWith = QStringLiteral("test-email@nextcloud.com");
        const auto emailShareShareWithDisplayName = QStringLiteral("Test email");

        _testEmailShareDefinition = FakeShareDefinition {
            _fakeFileDefinition,
            QString::number(_latestShareId),
            testShareCanDelete,
            testShareCanEdit,
            testShareUidOwner,
            testShareDisplayNameOwner,
            testSharePassword,
            testSharePermissions,
            testShareNote,
            testShareHideDownload,
            testShareExpiration,
            testShareSendPasswordByTalk,
            Share::TypeEmail,
            emailShareShareWith,
            emailShareShareWithDisplayName,
            testShareToken,
            {},
            {},
            {},
        };

        ++_latestShareId;
        const auto userShareShareWith = QStringLiteral("user");
        const auto userShareShareWithDisplayName("A Nextcloud user");

        _testUserShareDefinition = FakeShareDefinition {
            _fakeFileDefinition,
            QString::number(_latestShareId),
            testShareCanDelete,
            testShareCanEdit,
            testShareUidOwner,
            testShareDisplayNameOwner,
            testSharePassword,
            testSharePermissions,
            testShareNote,
            testShareHideDownload,
            testShareExpiration,
            testShareSendPasswordByTalk,
            Share::TypeUser,
            userShareShareWith,
            userShareShareWithDisplayName,
            testShareToken,
            {},
            {},
            {},
        };

        ++_latestShareId;
        const auto remoteShareShareWith = QStringLiteral("remote_share");
        const auto remoteShareShareWithDisplayName("A remote share");

        _testRemoteShareDefinition = FakeShareDefinition {
           _fakeFileDefinition,
           QString::number(_latestShareId),
           testShareCanDelete,
           testShareCanEdit,
           testShareUidOwner,
           testShareDisplayNameOwner,
           testSharePassword,
           testSharePermissions,
           testShareNote,
           testShareHideDownload,
           testShareExpiration,
           testShareSendPasswordByTalk,
           Share::TypeRemote,
           remoteShareShareWith,
           remoteShareShareWithDisplayName,
           testShareToken,
           {},
           {},
           {},
       };

        qRegisterMetaType<ShareePtr>("ShareePtr");
    }

    void testSetAccountAndPath()
    {
        resetTestData();
        // Test with a link share
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy accountStateChanged(&model, &ShareModel::accountStateChanged);
        QSignalSpy localPathChanged(&model, &ShareModel::localPathChanged);

        QSignalSpy accountConnectedChanged(&model, &ShareModel::accountConnectedChanged);
        QSignalSpy sharingEnabledChanged(&model, &ShareModel::sharingEnabledChanged);
        QSignalSpy publicLinkSharesEnabledChanged(&model, &ShareModel::publicLinkSharesEnabledChanged);

        model.setAccountState(_accountState.data());
        QCOMPARE(accountStateChanged.count(), 1);

        // Check all the account-related properties of the model
        QCOMPARE(model.accountConnected(), _accountState->isConnected());
        QCOMPARE(model.sharingEnabled(), _account->capabilities().shareAPI());
        QCOMPARE(model.publicLinkSharesEnabled() && Theme::instance()->linkSharing(), _account->capabilities().sharePublicLink());
        QCOMPARE(Theme::instance()->userGroupSharing(), model.userGroupSharingEnabled());

        const QString localPath(_fakeFolder.localPath() + testFileName);
        model.setLocalPath(localPath);
        QCOMPARE(localPathChanged.count(), 1);
        QCOMPARE(model.localPath(), localPath);
    }

    void testSuccessfulFetchShares()
    {
        resetTestData();
        // Test with a link share and a user/group email share "from the server"
        appendShareReplyData(_testLinkShareDefinition);
        appendShareReplyData(_testEmailShareDefinition);
        appendShareReplyData(_testUserShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), _sharesReplyData.count());
    }

    void testFetchSharesFailedError()
    {
        resetTestData();
        // Test with a link share "from the server"
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy serverError(&model, &ShareModel::serverError);

        // Test fetching the shares of a file that does not exist
        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + "wrong-filename-oops.md");
        QVERIFY(serverError.wait(3000));
        QCOMPARE(model.hasInitialShareFetchCompleted(), true);
        QCOMPARE(model.rowCount(), 0); // Make sure no placeholder
    }

    void testCorrectFetchOngoingSignalling()
    {
        resetTestData();

        // Test with a link share "from the server"
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy fetchOngoingChanged(&model, &ShareModel::fetchOngoingChanged);

        // Make sure we are correctly signalling the loading state of the fetch
        // Model resets twice when we set account and local path, resetting all model state.

        model.setAccountState(_accountState.data());
        QCOMPARE(fetchOngoingChanged.count(), 1);
        QCOMPARE(model.fetchOngoing(), false);

        model.setLocalPath(_fakeFolder.localPath() + testFileName);
        // If we can grab shares it then indicates fetch ongoing...
        QCOMPARE(fetchOngoingChanged.count(), 3);
        QCOMPARE(model.fetchOngoing(), true);

        // Then indicates fetch finished when done.
        QVERIFY(fetchOngoingChanged.wait(3000));
        QCOMPARE(model.fetchOngoing(), false);
    }

    void testCorrectInitialFetchCompleteSignalling()
    {
        resetTestData();

        // Test with a link share "from the server"
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy accountStateChanged(&model, &ShareModel::accountStateChanged);
        QSignalSpy localPathChanged(&model, &ShareModel::localPathChanged);
        QSignalSpy hasInitialShareFetchCompletedChanged(&model, &ShareModel::hasInitialShareFetchCompletedChanged);

        // Make sure we are correctly signalling the loading state of the fetch
        // Model resets twice when we set account and local path, resetting all model state.

        model.setAccountState(_accountState.data());
        QCOMPARE(accountStateChanged.count(), 1);
        QCOMPARE(hasInitialShareFetchCompletedChanged.count(), 1);
        QCOMPARE(model.hasInitialShareFetchCompleted(), false);

        model.setLocalPath(_fakeFolder.localPath() + testFileName);
        QCOMPARE(localPathChanged.count(), 1);
        QCOMPARE(hasInitialShareFetchCompletedChanged.count(), 2);
        QCOMPARE(model.hasInitialShareFetchCompleted(), false);

        // Once we have acquired shares from the server the initial share fetch is completed
        QVERIFY(hasInitialShareFetchCompletedChanged.wait(3000));
        QCOMPARE(hasInitialShareFetchCompletedChanged.count(), 3);
        QCOMPARE(model.hasInitialShareFetchCompleted(), true);
    }

    // Link shares and user group shares have slightly different behaviour in model.data()
    void testModelLinkShareData()
    {
        resetTestData();
        // Test with a link share "from the server"
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QVERIFY(!shareIndex.data(Qt::DisplayRole).toString().isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::ShareTypeRole).toInt(), _testLinkShareDefinition.shareType);
        QCOMPARE(shareIndex.data(ShareModel::ShareIdRole).toString(), _testLinkShareDefinition.shareId);
        QCOMPARE(shareIndex.data(ShareModel::LinkRole).toString(), _testLinkShareDefinition.linkShareUrl);
        QCOMPARE(shareIndex.data(ShareModel::LinkShareNameRole).toString(), _testLinkShareDefinition.linkShareName);
        QCOMPARE(shareIndex.data(ShareModel::LinkShareLabelRole).toString(), _testLinkShareDefinition.linkShareLabel);
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), !_testLinkShareDefinition.shareNote.isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::NoteRole).toString(), _testLinkShareDefinition.shareNote);
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), !_testLinkShareDefinition.sharePassword.isEmpty());
        // We don't expose the fetched password to the user as it's useless to them
        QCOMPARE(shareIndex.data(ShareModel::PasswordRole).toString(), QString());
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), SharePermissions(_testLinkShareDefinition.sharePermissions).testFlag(SharePermissionUpdate));

        const auto expectedLinkShareExpireDate = QDate::fromString(_testLinkShareDefinition.shareExpiration, expectedDtFormat);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), expectedLinkShareExpireDate.isValid());
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), expectedLinkShareExpireDate.startOfDay(Qt::UTC).toMSecsSinceEpoch());

        const auto iconUrl = shareIndex.data(ShareModel::IconUrlRole).toString();
        QVERIFY(iconUrl.contains("public.svg"));
    }

    void testModelEmailShareData()
    {
        resetTestData();
        // Test with a user/group email share "from the server"
        appendShareReplyData(_testEmailShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), 2); // Remember about placeholder link share

        const auto shareIndex = model.index(0, 0, {}); // Placeholder link share gets added after we are done parsing fetched shares
        QVERIFY(!shareIndex.data(Qt::DisplayRole).toString().isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::ShareTypeRole).toInt(), _testEmailShareDefinition.shareType);
        QCOMPARE(shareIndex.data(ShareModel::ShareIdRole).toString(), _testEmailShareDefinition.shareId);
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), !_testEmailShareDefinition.shareNote.isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::NoteRole).toString(), _testEmailShareDefinition.shareNote);
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), !_testEmailShareDefinition.sharePassword.isEmpty());
        // We don't expose the fetched password to the user as it's useless to them
        QCOMPARE(shareIndex.data(ShareModel::PasswordRole).toString(), QString());
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), SharePermissions(_testEmailShareDefinition.sharePermissions).testFlag(SharePermissionUpdate));

        const auto expectedShareExpireDate = QDate::fromString(_testEmailShareDefinition.shareExpiration, expectedDtFormat);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), expectedShareExpireDate.isValid());
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), expectedShareExpireDate.startOfDay(Qt::UTC).toMSecsSinceEpoch());

        const auto iconUrl = shareIndex.data(ShareModel::IconUrlRole).toString();
        QVERIFY(iconUrl.contains("email.svg"));
    }

    void testModelUserShareData()
    {
        resetTestData();
        // Test with a user/group user share "from the server"
        appendShareReplyData(_testUserShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), 2); // Remember about placeholder link share

        const auto shareIndex = model.index(0, 0, {}); // Placeholder link share gets added after we are done parsing fetched shares
        QVERIFY(!shareIndex.data(Qt::DisplayRole).toString().isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::ShareTypeRole).toInt(), _testUserShareDefinition.shareType);
        QCOMPARE(shareIndex.data(ShareModel::ShareIdRole).toString(), _testUserShareDefinition.shareId);
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), !_testUserShareDefinition.shareNote.isEmpty());
        QCOMPARE(shareIndex.data(ShareModel::NoteRole).toString(), _testUserShareDefinition.shareNote);
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), !_testUserShareDefinition.sharePassword.isEmpty());
        // We don't expose the fetched password to the user as it's useless to them
        QCOMPARE(shareIndex.data(ShareModel::PasswordRole).toString(), QString());
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), SharePermissions(_testUserShareDefinition.sharePermissions).testFlag(SharePermissionUpdate));

        const auto expectedShareExpireDate = QDate::fromString(_testUserShareDefinition.shareExpiration, expectedDtFormat);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), expectedShareExpireDate.isValid());
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), expectedShareExpireDate.startOfDay(Qt::UTC).toMSecsSinceEpoch());

        const auto iconUrl = shareIndex.data(ShareModel::IconUrlRole).toString();
        QVERIFY(iconUrl.contains("user.svg"));

        // Check correct user avatar
        const auto avatarUrl = shareIndex.data(ShareModel::AvatarUrlRole).toString();
        const auto relativeAvatarPath = QString("remote.php/dav/avatars/%1/%2.png").arg(_testUserShareDefinition.shareShareWith, QString::number(64));
        const auto expectedAvatarPath = Utility::concatUrlPath(_account->url(), relativeAvatarPath).toString();
        const QString expectedUrl(QStringLiteral("image://tray-image-provider/") + expectedAvatarPath);
        QCOMPARE(avatarUrl, expectedUrl);
    }

    void testSuccessfulCreateShares()
    {
        resetTestData();

        // Test with an existing link share
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Test if it gets added
        model.createNewLinkShare();
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 2); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Test if it's the type we wanted
        const auto newLinkShareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(newLinkShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypeLink);

        // Do it again with a different type
        const ShareePtr sharee(new Sharee("testsharee@nextcloud.com", "Test sharee", Sharee::Type::Email));
        model.createNewUserGroupShare(sharee);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 3); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Test if it's the type we wanted
        const auto newUserGroupShareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(newUserGroupShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypeEmail);

        // Confirm correct addition of share with password
        const auto password = QStringLiteral("a pretty bad password but good thing it doesn't matter!");
        model.createNewLinkShareWithPassword(password);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 4); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        model.createNewUserGroupShareWithPassword(sharee, password);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 5); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        resetTestData();
    }

    void testEnforcePasswordShares()
    {
        resetTestData();

        // Enforce passwords for shares in capabilities
        const QVariantMap enforcePasswordsCapabilities {
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
                        {QStringLiteral("enforced"), true},
                    }},
                }},
                {QStringLiteral("sharebymail"), QVariantMap {
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("password"), QVariantMap {
                        {QStringLiteral("enforced"), true},
                    }},
                }},
            }},
        };

        _account->setCapabilities(enforcePasswordsCapabilities);
        QVERIFY(_account->capabilities().sharePublicLinkEnforcePassword());
        QVERIFY(_account->capabilities().shareEmailPasswordEnforced());

        // Test with a link share "from the server"
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Confirm that the model requests a password
        QSignalSpy requestPasswordForLinkShare(&model, &ShareModel::requestPasswordForLinkShare);
        model.createNewLinkShare();
        QVERIFY(requestPasswordForLinkShare.wait(3000));

        QSignalSpy requestPasswordForEmailShare(&model, &ShareModel::requestPasswordForEmailSharee);
        const ShareePtr sharee(new Sharee("testsharee@nextcloud.com", "Test sharee", Sharee::Type::Email));
        model.createNewUserGroupShare(sharee);
        QCOMPARE(requestPasswordForEmailShare.count(), 1);

        // Test that the model data is correctly reporting that passwords are enforced
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::PasswordEnforcedRole).toBool(), true);
    }

    void testEnforceExpireDate()
    {
        resetTestData();

        const auto internalExpireDays = 45;
        const auto publicExpireDays = 30;
        const auto remoteExpireDays = 25;

        // Enforce expire dates for shares in capabilities
        const QVariantMap enforcePasswordsCapabilities {
            {QStringLiteral("files_sharing"), QVariantMap {
                {QStringLiteral("api_enabled"), true},
                {QStringLiteral("default_permissions"), 19},
                {QStringLiteral("public"), QVariantMap {
                    {QStringLiteral("enabled"), true},
                    {QStringLiteral("expire_date"), QVariantMap {
                         {QStringLiteral("days"), publicExpireDays},
                         {QStringLiteral("enforced"), true},
                    }},
                    {QStringLiteral("expire_date_internal"), QVariantMap {
                         {QStringLiteral("days"), internalExpireDays},
                         {QStringLiteral("enforced"), true},
                    }},
                    {QStringLiteral("expire_date_remote"), QVariantMap {
                         {QStringLiteral("days"), remoteExpireDays},
                         {QStringLiteral("enforced"), true},
                    }},
                    {QStringLiteral("password"), QVariantMap {
                        {QStringLiteral("enforced"), false},
                    }},
                 }},
                 {QStringLiteral("sharebymail"), QVariantMap {
                     {QStringLiteral("enabled"), true},
                     {QStringLiteral("password"), QVariantMap {
                         {QStringLiteral("enforced"), true},
                     }},
                 }},
            }},
        };

        _account->setCapabilities(enforcePasswordsCapabilities);
        QVERIFY(_account->capabilities().sharePublicLinkEnforceExpireDate());
        QVERIFY(_account->capabilities().shareInternalEnforceExpireDate());
        QVERIFY(_account->capabilities().shareRemoteEnforceExpireDate());

        // Test with shares "from the server"
        appendShareReplyData(_testLinkShareDefinition);
        appendShareReplyData(_testEmailShareDefinition);
        appendShareReplyData(_testRemoteShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Test that the model data is correctly reporting that expire dates are enforced for all share types
        for(auto i = 0; i < model.rowCount(); ++i) {
            const auto shareIndex = model.index(i, 0, {});
            QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnforcedRole).toBool(), true);

            QDateTime expectedExpireDateTime;
            switch(shareIndex.data(ShareModel::ShareTypeRole).toInt()) {
            case Share::TypePlaceholderLink:
                break;
            case Share::TypeUser:
            case Share::TypeGroup:
            case Share::TypeCircle:
            case Share::TypeRoom:
                expectedExpireDateTime = QDate::currentDate().addDays(internalExpireDays).startOfDay(QTimeZone::utc());
                break;
            case Share::TypeLink:
            case Share::TypeEmail:
                expectedExpireDateTime = QDate::currentDate().addDays(publicExpireDays).startOfDay(QTimeZone::utc());
                break;
            case Share::TypeRemote:
                expectedExpireDateTime = QDate::currentDate().addDays(remoteExpireDays).startOfDay(QTimeZone::utc());
                break;
            }

            QCOMPARE(shareIndex.data(ShareModel::EnforcedMaximumExpireDateRole).toLongLong(), expectedExpireDateTime.toMSecsSinceEpoch());
        }
    }

    void testSuccessfulDeleteShares()
    {
        resetTestData();

        // Test with an existing link share
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Create share
        model.createNewLinkShare();
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 2); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Test if it gets deleted properly
        const auto latestLinkShare = model.index(model.rowCount() - 1, 0, {}).data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy shareDeleted(latestLinkShare.data(), &LinkShare::shareDeleted);
        model.deleteShare(latestLinkShare);
        QVERIFY(shareDeleted.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        resetTestData();
    }

    void testPlaceholderLinkShare()
    {
        resetTestData();

        // Start with no shares; should show the placeholder link share
        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0); // There should be no placeholder yet

        QSignalSpy hasInitialShareFetchCompletedChanged(&model, &ShareModel::hasInitialShareFetchCompletedChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);
        QVERIFY(hasInitialShareFetchCompletedChanged.wait(5000));
        QVERIFY(model.hasInitialShareFetchCompleted());
        QCOMPARE(model.rowCount(), 1); // There should be a placeholder now

        const QPersistentModelIndex placeholderLinkShareIndex(model.index(model.rowCount() - 1, 0, {}));
        QCOMPARE(placeholderLinkShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypePlaceholderLink);

        // Test adding a user group share -- we should still be showing a placeholder link share
        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);
        const ShareePtr sharee(new Sharee("testsharee@nextcloud.com", "Test sharee", Sharee::Type::Email));
        model.createNewUserGroupShare(sharee);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count() + 1);

        QVERIFY(placeholderLinkShareIndex.isValid());
        QCOMPARE(placeholderLinkShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypePlaceholderLink);

        // Now try adding a link share, which should remove the placeholder
        model.createNewLinkShare();
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 2); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        QVERIFY(!placeholderLinkShareIndex.isValid());

        // Now delete the only link share, which should bring back the placeholder link share
        const auto latestLinkShare = model.index(model.rowCount() - 1, 0, {}).data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy shareDeleted(latestLinkShare.data(), &LinkShare::shareDeleted);
        model.deleteShare(latestLinkShare);
        QVERIFY(shareDeleted.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count() + 1);

        const auto newPlaceholderLinkShareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(newPlaceholderLinkShareIndex.data(ShareModel::ShareTypeRole).toInt(), Share::TypePlaceholderLink);

        resetTestData();
    }

    void testSuccessfulToggleAllowEditing()
    {
        resetTestData();

        // Test with an existing link share
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), SharePermissions(_testLinkShareDefinition.sharePermissions).testFlag(SharePermissionUpdate));

        const auto share = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy permissionsSet(share.data(), &Share::permissionsSet);

        model.toggleShareAllowEditing(share, false);
        QVERIFY(permissionsSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::EditingAllowedRole).toBool(), false);
    }

    void testSuccessfulPasswordSet()
    {
        resetTestData();

        // Test with an existing link share.
        // This one has a pre-existing password
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), true);

        const auto share = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy passwordSet(share.data(), &Share::passwordSet);

        model.toggleSharePasswordProtect(share, false);
        QVERIFY(passwordSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), false);

        const auto password = QStringLiteral("a pretty bad password but good thing it doesn't matter!");
        model.setSharePassword(share, password);
        QVERIFY(passwordSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::PasswordProtectEnabledRole).toBool(), true);
        // The model stores the recently set password.
        // We want to present the user with it in the UI while the model is alive
        QCOMPARE(shareIndex.data(ShareModel::PasswordRole).toString(), password);
    }

    void testSuccessfulExpireDateSet()
    {
        resetTestData();

        // Test with an existing link share.
        // This one has a pre-existing expire date
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Check what we know
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), true);

        // Disable expire date
        const auto sharePtr = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        const auto linkSharePtr = sharePtr.dynamicCast<LinkShare>(); // Need to connect to signal
        QSignalSpy expireDateSet(linkSharePtr.data(), &LinkShare::expireDateSet);
        model.toggleShareExpirationDate(sharePtr, false);

        QVERIFY(expireDateSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), false);

        // Set a new expire date
        const auto expireDateMsecs = QDate::currentDate().addDays(10).startOfDay(Qt::UTC).toMSecsSinceEpoch();
        model.setShareExpireDate(linkSharePtr, expireDateMsecs);
        QVERIFY(expireDateSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), expireDateMsecs);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), true);

        // Test the QML-specific slot
        const QVariant newExpireDateMsecs = QDate::currentDate().addDays(20).startOfDay(Qt::UTC).toMSecsSinceEpoch();
        model.setShareExpireDateFromQml(QVariant::fromValue(sharePtr), newExpireDateMsecs);
        QVERIFY(expireDateSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateRole).toLongLong(), newExpireDateMsecs);
        QCOMPARE(shareIndex.data(ShareModel::ExpireDateEnabledRole).toBool(), true);
    }

    void testSuccessfulNoteSet()
    {
        resetTestData();

        // Test with an existing link share.
        // This one has a pre-existing password
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), true);

        const auto sharePtr = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        const auto linkSharePtr = sharePtr.dynamicCast<LinkShare>(); // Need to connect to signal
        QSignalSpy noteSet(linkSharePtr.data(), &LinkShare::noteSet);

        model.toggleShareNoteToRecipient(sharePtr, false);
        QVERIFY(noteSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), false);

        const auto note = QStringLiteral("Don't forget to test everything!");
        model.setShareNote(sharePtr, note);
        QVERIFY(noteSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::NoteEnabledRole).toBool(), true);
        // The model stores the recently set password.
        // We want to present the user with it in the UI while the model is alive
        QCOMPARE(shareIndex.data(ShareModel::NoteRole).toString(), note);
    }

    void testSuccessfulLinkShareLabelSet()
    {
        resetTestData();

        // Test with an existing link share.
        appendShareReplyData(_testLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        QCOMPARE(shareIndex.data(ShareModel::LinkShareLabelRole).toBool(), true);

        const auto sharePtr = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        const auto linkSharePtr = sharePtr.dynamicCast<LinkShare>(); // Need to connect to signal
        QSignalSpy labelSet(linkSharePtr.data(), &LinkShare::labelSet);
        const auto label = QStringLiteral("New link share label!");
        model.setLinkShareLabel(linkSharePtr, label);
        QVERIFY(labelSet.wait(3000));
        QCOMPARE(shareIndex.data(ShareModel::LinkShareLabelRole).toString(), label);
    }

    void testSharees()
    {
        resetTestData();

        appendShareReplyData(_testLinkShareDefinition);
        appendShareReplyData(_testEmailShareDefinition);
        appendShareReplyData(_testUserShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        QCOMPARE(model.sharees().count(), 2); // Link shares don't have sharees

        // Test adding a user group share -- we should still be showing a placeholder link share
        const ShareePtr sharee(new Sharee("testsharee@nextcloud.com", "Test sharee", Sharee::Type::Email));
        model.createNewUserGroupShare(sharee);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 4); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        const auto sharees = model.sharees();
        QCOMPARE(sharees.count(), 3); // Link shares don't have sharees
        const auto lastSharee = sharees.last().value<ShareePtr>();
        QVERIFY(lastSharee);

        // Remove the user group share we just added
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        const auto sharePtr = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        model.deleteShare(sharePtr);
        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Now check the sharee is gone
        QCOMPARE(model.sharees().count(), 2);
    }

    void testSharePropertySetError()
    {
        resetTestData();

        // Serve a broken share definition from the server to force an error
        auto brokenLinkShareDefinition = _testLinkShareDefinition;
        brokenLinkShareDefinition.shareId = QString();

        appendShareReplyData(brokenLinkShareDefinition);

        ShareModel model;
        QAbstractItemModelTester modelTester(&model);
        QCOMPARE(model.rowCount(), 0);

        QSignalSpy sharesChanged(&model, &ShareModel::sharesChanged);

        model.setAccountState(_accountState.data());
        model.setLocalPath(_fakeFolder.localPath() + testFileName);

        QVERIFY(sharesChanged.wait(5000));
        QCOMPARE(_sharesReplyData.count(), 1); // Check our test is working!
        QCOMPARE(model.rowCount(), _sharesReplyData.count());

        // Reset the fake server to pretend like nothing is wrong there
        _sharesReplyData = QJsonArray();
        appendShareReplyData(_testLinkShareDefinition);

        // Now try changing a property of the share
        const auto shareIndex = model.index(model.rowCount() - 1, 0, {});
        const auto share = shareIndex.data(ShareModel::ShareRole).value<SharePtr>();
        QSignalSpy serverError(&model, &ShareModel::serverError);

        model.toggleShareAllowEditing(share, false);
        QVERIFY(serverError.wait(3000));

        // Specific signal for password set error
        QSignalSpy passwordSetError(&model, &ShareModel::passwordSetError);
        const auto password = QStringLiteral("a pretty bad password but good thing it doesn't matter!");
        model.setSharePassword(share, password);
        QVERIFY(passwordSetError.wait(3000));
    }

};

QTEST_MAIN(TestShareModel)
#include "testsharemodel.moc"
