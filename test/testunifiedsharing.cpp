/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/sharing/addrecipientjob.h"
#include "gui/sharing/addsourcejob.h"
#include "gui/sharing/createsharejob.h"
#include "gui/sharing/destroysharejob.h"
#include "gui/sharing/generatesecretjob.h"
#include "gui/sharing/getsharejob.h"
#include "gui/sharing/getsharesjob.h"
#include "gui/sharing/permissionmodel.h"
#include "gui/sharing/property.h"
#include "gui/sharing/propertymodel.h"
#include "gui/sharing/recipientmodel.h"
#include "gui/sharing/recipientsearchmodel.h"
#include "gui/sharing/removerecipientjob.h"
#include "gui/sharing/removesourcejob.h"
#include "gui/sharing/searchrecipientsjob.h"
#include "gui/sharing/setpermissionjob.h"
#include "gui/sharing/setpermissionpresetjob.h"
#include "gui/sharing/setpropertyjob.h"
#include "gui/sharing/setrecipientsecretjob.h"
#include "gui/sharing/setsharestatejob.h"
#include "gui/sharing/share.h"
#include "gui/sharing/sharingconstants.h"
#include "gui/sharing/sharingcontroller.h"
#include "gui/sharing/unifiedsharelistmodel.h"
#include "gui/sharing/unifiedsharingrequest.h"
#include "gui/sharing/updatesharejob.h"
#include "syncenginetestutils.h"

#include <QSignalSpy>
#include <QTest>
#include <QUrlQuery>

#include <algorithm>

using namespace OCC;
using namespace OCC::Gui::Sharing;
using namespace Qt::StringLiterals;

class TestUnifiedSharing : public QObject
{
    Q_OBJECT

private slots:
    void recipientsPreserveServerIdentityAndCapabilities()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto share = Share::fromJson(QJsonDocument{QJsonObject{
                                               {"ocs"_L1,
                                                QJsonObject{
                                                    {"data"_L1,
                                                     QJsonObject{
                                                         {"id"_L1, "share-1"_L1},
                                                         {"recipients"_L1,
                                                          QJsonArray{QJsonObject{
                                                                         {"class"_L1, "federated-user"_L1},
                                                                         {"display_name"_L1, "Alice"_L1},
                                                                         {"value"_L1, "alice"_L1},
                                                                         {"instance"_L1, "cloud.example.com"_L1},
                                                                         {"icon"_L1,
                                                                          QJsonObject{
                                                                              {"svg"_L1, "<svg/>"_L1},
                                                                              {"light"_L1, "https://cloud.example.com/light.svg"_L1},
                                                                              {"dark"_L1, "https://cloud.example.com/dark.svg"_L1},
                                                                          }},
                                                                         {"secret"_L1,
                                                                          QJsonObject{
                                                                              {"updatable"_L1, true},
                                                                              {"value"_L1, "public-secret"_L1},
                                                                              {"url"_L1, "https://cloud.example.com/s/public-secret"_L1},
                                                                          }},
                                                                         {"initiator"_L1, QJsonObject{{"display_name"_L1, "Bob"_L1}}},
                                                                     },
                                                                     QJsonObject{
                                                                         {"class"_L1, "user"_L1},
                                                                         {"display_name"_L1, "Carol"_L1},
                                                                         {"value"_L1, "carol"_L1},
                                                                         {"secret"_L1, QJsonObject{{"updatable"_L1, false}}},
                                                                     }}},
                                                     }},
                                                }},
                                           }},
                                           fakeFolder.account());

        QCOMPARE(share->recipients().size(), 2);
        const auto recipient = share->recipients().constFirst();
        QCOMPARE(recipient->className(), "federated-user"_L1);
        QCOMPARE(recipient->value(), "alice"_L1);
        QCOMPARE(recipient->instance(), std::optional<QString>{"cloud.example.com"_L1});
        QCOMPARE(recipient->instanceString(), "cloud.example.com"_L1);
        QCOMPARE(recipient->iconSvg(), "<svg/>"_L1);
        QCOMPARE(recipient->iconLight(), "https://cloud.example.com/light.svg"_L1);
        QCOMPARE(recipient->iconDark(), "https://cloud.example.com/dark.svg"_L1);
        QVERIFY(recipient->secretUpdatable());
        QCOMPARE(recipient->secretValue(), std::optional<QString>{"public-secret"_L1});
        QCOMPARE(recipient->secretUrl(), std::optional<QString>{"https://cloud.example.com/s/public-secret"_L1});
        QCOMPARE(recipient->secretUrlString(), "https://cloud.example.com/s/public-secret"_L1);
        QCOMPARE(recipient->initiatorDisplayName(), "Bob"_L1);

        RecipientModel model;
        model.setShare(share);
        QCOMPARE(model.rowCount(), 2);
        const auto index = model.index(0);
        QCOMPARE(model.data(index, RecipientModel::InstanceRole).toString(), "cloud.example.com"_L1);
        QCOMPARE(model.data(index, RecipientModel::IconSvgUrlRole).toString(), "data:image/svg+xml;base64,PHN2Zy8+"_L1);
        QVERIFY(model.data(index, RecipientModel::SecretUpdatableRole).toBool());
        QCOMPARE(model.data(index, RecipientModel::SecretUrlRole).toString(), "https://cloud.example.com/s/public-secret"_L1);
        QCOMPARE(model.data(index, RecipientModel::InitiatorDisplayNameRole).toString(), "Bob"_L1);

        const auto recipientWithoutLinkIndex = model.index(1);
        const auto missingSecretUrl = model.data(recipientWithoutLinkIndex, RecipientModel::SecretUrlRole);
        QVERIFY(missingSecretUrl.isValid());
        QCOMPARE(missingSecretUrl.toString(), QString{});

        delete share;
    }

    void sharePropertiesPreserveServerMetadataAndUseTypedFields()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto share = Share::fromJson(QJsonDocument::fromJson(R"json({
                "ocs": {
                    "data": {
                        "id": "share-1",
                        "properties": [{
                            "class": "later-string",
                            "display_name": "Description",
                            "hint": "Add a description",
                            "priority": 20,
                            "required": true,
                            "advanced": false,
                            "type": "string",
                            "min_length": 3,
                            "max_length": 40,
                            "value": "Hello"
                        }, {
                            "class": "first-enum",
                            "display_name": "Visibility",
                            "hint": null,
                            "priority": 10,
                            "required": false,
                            "advanced": true,
                            "type": "enum",
                            "valid_values": ["private", "public"],
                            "value": "private"
                        }, {
                            "class": "expiry",
                            "display_name": "Expiration date",
                            "hint": null,
                            "priority": 30,
                            "required": false,
                            "advanced": false,
                            "type": "date",
                            "min_date": "2026-07-30T00:00:00+00:00",
                            "max_date": "2026-08-30T00:00:00+00:00",
                            "value": null
                        }, {
                            "class": "protected",
                            "display_name": "Password",
                            "hint": null,
                            "priority": 40,
                            "required": false,
                            "advanced": false,
                            "type": "password",
                            "value": null
                        }, {
                            "class": "notify",
                            "display_name": "Notify",
                            "hint": null,
                            "priority": 50,
                            "required": false,
                            "advanced": false,
                            "type": "boolean",
                            "value": "true"
                        }]
                    }
                }
            })json"),
                                           fakeFolder.account());

        PropertyModel model;
        model.setShare(share);

        QCOMPARE(model.rowCount(), 4);
        const auto stringIndex = model.index(3);
        QCOMPARE(model.data(stringIndex, PropertyModel::TypeRole).toInt(), static_cast<int>(PropertyModel::String));
        QCOMPARE(model.data(stringIndex, PropertyModel::RequiredRole).toBool(), true);
        QCOMPARE(model.data(stringIndex, PropertyModel::MinimumRole).toInt(), 3);
        QCOMPARE(model.data(stringIndex, PropertyModel::MaximumRole).toInt(), 40);

        const auto dateIndex = model.index(2);
        QCOMPARE(model.data(dateIndex, PropertyModel::TypeRole).toInt(), static_cast<int>(PropertyModel::Date));
        QCOMPARE(model.data(dateIndex, PropertyModel::MinimumRole).toString(), "2026-07-30T00:00:00+00:00"_L1);
        QCOMPARE(model.data(dateIndex, PropertyModel::MaximumRole).toString(), "2026-08-30T00:00:00+00:00"_L1);
        QCOMPARE(model.data(model.index(1), PropertyModel::TypeRole).toInt(), static_cast<int>(PropertyModel::Password));
        QCOMPARE(model.data(model.index(0), PropertyModel::TypeRole).toInt(), static_cast<int>(PropertyModel::Boolean));

        model.setAdvanced(true);
        QCOMPARE(model.rowCount(), 1);
        const auto enumIndex = model.index(0);
        QCOMPARE(model.data(enumIndex, PropertyModel::PropertyRole).toString(), "first-enum"_L1);
        QCOMPARE(model.data(enumIndex, PropertyModel::TypeRole).toInt(), static_cast<int>(PropertyModel::Enum));
        QCOMPARE(model.data(enumIndex, PropertyModel::AdvancedRole).toBool(), true);
        QCOMPARE(model.data(enumIndex, PropertyModel::ValidValuesRole).toStringList(), QStringList({"private"_L1, "public"_L1}));

        delete share;
    }

    void permissionModelIsReadOnlyAndTracksOnlyItsCurrentShare()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto shareWithOnePermission = Share::fromJson(QJsonDocument::fromJson(R"json({
                "ocs": {"data": {
                    "id": "share-1",
                    "permissions": [{"class": "view", "display_name": "View files", "enabled": true}]
                }}
            })json"),
                                                            fakeFolder.account());
        const auto shareWithTwoPermissions = Share::fromJson(QJsonDocument::fromJson(R"json({
                "ocs": {"data": {
                    "id": "share-2",
                    "permissions": [
                        {"class": "view", "display_name": "View files", "enabled": true},
                        {"class": "download", "display_name": "Download files", "enabled": false}
                    ]
                }}
            })json"),
                                                             fakeFolder.account());

        PermissionModel model;
        model.setShare(shareWithOnePermission);
        QCOMPARE(model.rowCount(), 1);
        const auto index = model.index(0);
        QCOMPARE(model.data(index, PermissionModel::LabelRole).toString(), "View files"_L1);
        QVERIFY(!(model.flags(index) & Qt::ItemIsEditable));
        QVERIFY(!model.data(QModelIndex{}, PermissionModel::LabelRole).isValid());
        QCOMPARE(model.rowCount(model.index(0, 0)), 0);

        model.setShare(shareWithTwoPermissions);
        QCOMPARE(model.rowCount(), 2);
        shareWithOnePermission->updateFromJson(QJsonDocument::fromJson(R"json({
            "ocs": {"data": {"permissions": []}}
        })json"));
        QCOMPARE(model.rowCount(), 2);

        delete shareWithOnePermission;
        delete shareWithTwoPermissions;
    }

    void requestsAreConfiguredBeforeTheyStart()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestCount = 0;
        auto requestPath = QString{};
        auto requestVerb = QByteArray{};
        auto requestQuery = QList<QPair<QString, QString>>{};
        auto requestBody = QJsonObject{};
        auto requestContentType = QByteArray{};

        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *outgoingData) {
            ++requestCount;
            requestPath = request.url().path();
            requestQuery = QUrlQuery{request.url()}.queryItems();
            requestQuery.removeAll({"format"_L1, "json"_L1});
            std::ranges::sort(requestQuery);
            requestContentType = request.rawHeader("Content-Type");
            if (outgoingData) {
                if (!outgoingData->isOpen()) {
                    outgoingData->open(QIODevice::ReadOnly);
                }
                requestBody = QJsonDocument::fromJson(outgoingData->peek(outgoingData->bytesAvailable())).object();
                outgoingData->reset();
            } else {
                requestBody = {};
            }
            requestVerb = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
            if (requestVerb.isEmpty()) {
                switch (operation) {
                case QNetworkAccessManager::GetOperation:
                    requestVerb = "GET";
                    break;
                case QNetworkAccessManager::PostOperation:
                    requestVerb = "POST";
                    break;
                case QNetworkAccessManager::PutOperation:
                    requestVerb = "PUT";
                    break;
                case QNetworkAccessManager::DeleteOperation:
                    requestVerb = "DELETE";
                    break;
                default:
                    break;
                }
            }

            auto statusCode = 200;
            if (requestVerb == "POST" && requestPath.endsWith("/api/v1/share"_L1)) {
                statusCode = 201;
            } else if (requestVerb == "DELETE" && requestPath.endsWith("/api/v1/share/share-1"_L1)) {
                statusCode = 204;
            }
            const auto response = QString{R"json({
                "ocs": {
                    "meta": {
                        "status": "ok",
                        "statuscode": %1,
                        "message": "OK"
                    },
                    "data": {
                        "id": "share-1"
                    }
                }
            })json"}
                                      .arg(statusCode)
                                      .toUtf8();
            return new FakePayloadReply{operation, request, response, this};
        });

        const auto account = fakeFolder.account();
        const auto share = Share::fromJson(QJsonDocument::fromJson(R"json({"ocs":{"data":{"id":"share-1"}}})json"), fakeFolder.account());

        const auto verifyRequest = [&](UnifiedSharingRequest *job,
                                       const QByteArray &expectedVerb,
                                       const QString &expectedPath,
                                       QList<QPair<QString, QString>> expectedQuery = {},
                                       const QJsonObject &expectedBody = {}) {
            QSignalSpy finishedSpy{job, &UnifiedSharingRequest::jobFinished};
            job->start();
            QVERIFY(finishedSpy.wait());
            QCOMPARE(requestVerb, expectedVerb);
            QVERIFY2(requestPath.endsWith(expectedPath), qPrintable(requestPath));
            std::ranges::sort(expectedQuery);
            QCOMPARE(requestQuery, expectedQuery);
            QCOMPARE(requestBody, expectedBody);
            if (!expectedBody.isEmpty()) {
                QCOMPARE(requestContentType, "application/json");
            }
        };

        verifyRequest(new SearchRecipientsJob{account, "ali"_L1, 10, 20, {"user-class"_L1, "group-class"_L1}, "share-1"_L1},
                      "GET",
                      "/ocs/v2.php/apps/sharing/api/v1/recipients",
                      {{"query"_L1, "ali"_L1},
                       {"offset"_L1, "10"_L1},
                       {"limit"_L1, "20"_L1},
                       {"filterRecipientTypeClasses%5B%5D"_L1, "user-class"_L1},
                       {"filterRecipientTypeClasses%5B%5D"_L1, "group-class"_L1},
                       {"id"_L1, "share-1"_L1}});
        verifyRequest(new GenerateSecretJob{account}, "GET", "/ocs/v2.php/apps/sharing/api/v1/secret");
        verifyRequest(new CreateShareJob{account}, "POST", "/ocs/v2.php/apps/sharing/api/v1/share");
        verifyRequest(new SetShareStateJob{account, *share, Share::ShareState::Active},
                      "PUT",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/state",
                      {},
                      {{"state"_L1, "active"_L1}});
        verifyRequest(new AddSourceJob{account, *share, "42"_L1},
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/source",
                      {},
                      {{"class"_L1, SourceTypeClasses::node}, {"value"_L1, "42"_L1}});
        verifyRequest(new RemoveSourceJob{account, *share, "42"_L1},
                      "DELETE",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/source",
                      {{"class"_L1, SourceTypeClasses::node}, {"value"_L1, "42"_L1}});
        verifyRequest(new AddRecipientJob{account, *share, "recipient-class"_L1, "alice"_L1, "https://example.com"_L1},
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/recipient",
                      {},
                      {{"class"_L1, "recipient-class"_L1}, {"value"_L1, "alice"_L1}, {"instance"_L1, "https://example.com"_L1}});
        verifyRequest(new RemoveRecipientJob{account, *share, "recipient-class"_L1, "alice"_L1, "https://example.com"_L1},
                      "DELETE",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/recipient",
                      {{"class"_L1, "recipient-class"_L1}, {"value"_L1, "alice"_L1}, {"instance"_L1, "https%3A%2F%2Fexample.com"_L1}});
        verifyRequest(new SetRecipientSecretJob{account, *share, "recipient-class"_L1, "alice"_L1, "secret"_L1, "https://example.com"_L1},
                      "PUT",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/recipient/secret",
                      {},
                      {{"class"_L1, "recipient-class"_L1}, {"value"_L1, "alice"_L1}, {"secret"_L1, "secret"_L1}, {"instance"_L1, "https://example.com"_L1}});
        verifyRequest(new SetPropertyJob{account, *share, "property-class"_L1, std::nullopt},
                      "PUT",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/property",
                      {},
                      {{"class"_L1, "property-class"_L1}, {"value"_L1, QJsonValue::Null}});
        verifyRequest(new SetPermissionJob{account, *share, "permission-class"_L1, true},
                      "PUT",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/permission",
                      {},
                      {{"class"_L1, "permission-class"_L1}, {"enabled"_L1, true}});
        verifyRequest(new SetPermissionPresetJob{account, *share, "preset-class"_L1},
                      "PUT",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/permission/preset",
                      {},
                      {{"permissionPresetClass"_L1, "preset-class"_L1}});
        verifyRequest(new DestroyShareJob{account, "share-1"_L1}, "DELETE", "/ocs/v2.php/apps/sharing/api/v1/share/share-1");
        verifyRequest(new GetShareJob{account, "share-1"_L1, "secret"_L1, QJsonObject{{"argument-class"_L1, QJsonObject{{"key"_L1, "value"_L1}}}}},
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1",
                      {},
                      {{"secret"_L1, "secret"_L1}, {"arguments"_L1, QJsonObject{{"argument-class"_L1, QJsonObject{{"key"_L1, "value"_L1}}}}}});
        verifyRequest(
            new GetSharesJob{account, "source-class"_L1, "42"_L1, "share-0"_L1, 50},
            "GET",
            "/ocs/v2.php/apps/sharing/api/v1/shares",
            {{"filterSourceTypeClass"_L1, "source-class"_L1}, {"filterSourceTypeValue"_L1, "42"_L1}, {"lastShareID"_L1, "share-0"_L1}, {"limit"_L1, "50"_L1}});
        verifyRequest(new AddRecipientJob{account, *share, "recipient-class"_L1, "alice"_L1},
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/recipient",
                      {},
                      {{"class"_L1, "recipient-class"_L1}, {"value"_L1, "alice"_L1}});
        verifyRequest(new GetShareJob{account, "share-1"_L1}, "POST", "/ocs/v2.php/apps/sharing/api/v1/share/share-1");
        verifyRequest(new UnifiedSharingRequest{account, "/ocs/v2.php/apps/sharing/api/v1/share/share-1"_L1, "POST"_ba, {.body = QJsonObject{}}},
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1");
        QCOMPARE(requestContentType, "application/json");
        verifyRequest(new GetSharesJob{account}, "GET", "/ocs/v2.php/apps/sharing/api/v1/shares", {{"limit"_L1, "100"_L1}});

        QCOMPARE(requestCount, 19);
        delete share;
    }

    void requestStartsOnlyOnce()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestCount = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            ++requestCount;
            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                "ocs": {
                    "meta": {
                        "status": "ok",
                        "statuscode": 200,
                        "message": "OK"
                    },
                    "data": {}
                }
            })json",
                                        this};
        });

        const auto job = new SearchRecipientsJob{fakeFolder.account(), "alice"_L1, 0, 10};
        QSignalSpy finishedSpy{job, &UnifiedSharingRequest::jobFinished};

        job->start();
        job->start();

        QVERIFY(finishedSpy.wait());
        QCOMPARE(requestCount, 1);
    }

    void partialShareUpdatesPreserveOmittedFields()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        const auto share = Share::fromJson(QJsonDocument::fromJson(R"json({
                "ocs": {
                    "data": {
                        "id": "share-1",
                        "state": "draft",
                        "permission_preset": "view-preset",
                        "permissions": [{
                            "class": "view-permission",
                            "display_name": "View files",
                            "enabled": true
                        }],
                        "properties": [{
                            "class": "note-property",
                            "display_name": "Note to recipients",
                            "type": "string",
                            "value": "Hello"
                        }],
                        "recipients": [{
                            "class": "user-class",
                            "display_name": "Alice",
                            "value": "alice"
                        }]
                    }
                }
            })json"),
                                           fakeFolder.account());

        share->updateFromJson(QJsonDocument::fromJson(R"json({
            "ocs": {
                "data": {
                    "state": "active",
                    "recipients": [{
                        "class": "user-class",
                        "display_name": "Bob",
                        "value": "bob"
                    }]
                }
            }
        })json"));

        QCOMPARE(share->id(), "share-1"_L1);
        QCOMPARE(share->state(), Share::ShareState::Active);
        QCOMPARE(share->permissionPreset(), "view-preset"_L1);
        QCOMPARE(share->permissions().size(), 1);
        QCOMPARE(share->properties().size(), 1);
        QCOMPARE(share->properties().constFirst()->displayName(), "Note to recipients"_L1);
        QCOMPARE(share->recipients().size(), 1);
        QCOMPARE(share->recipients().constFirst()->displayName(), "Bob"_L1);

        share->updateFromJson(QJsonDocument::fromJson(R"json({
            "ocs": {
                "data": {
                    "properties": []
                }
            }
        })json"));

        QVERIFY(share->properties().isEmpty());
        QCOMPARE(share->permissions().size(), 1);
        QCOMPARE(share->recipients().size(), 1);

        delete share;
    }

    void sharingControllerLoadsAllSharesWithoutCreatingOne()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestCount = 0;
        auto requestVerb = QByteArray{};
        auto requestQuery = QList<QPair<QString, QString>>{};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            ++requestCount;
            requestVerb = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
            requestQuery = QUrlQuery{request.url()}.queryItems();
            requestQuery.removeAll({"format"_L1, "json"_L1});
            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                "ocs": {
                    "meta": {
                        "status": "ok",
                        "statuscode": 200,
                        "message": "OK"
                    },
                    "data": [
                        {"id": "share-1", "state": "active"},
                        {"id": "share-2", "state": "draft"}
                    ]
                }
            })json",
                                        this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        QSignalSpy sharesChangedSpy{&controller, &SharingController::sharesChanged};

        controller.initialize("42"_L1);

        QTRY_COMPARE(sharesChangedSpy.size(), 1);
        QCOMPARE(requestCount, 1);
        QCOMPARE(requestVerb, "GET");
        QCOMPARE(requestQuery,
                 (QList<QPair<QString, QString>>{
                     {"limit"_L1, "100"_L1},
                     {"filterSourceTypeClass"_L1, SourceTypeClasses::node},
                     {"filterSourceTypeValue"_L1, "42"_L1},
                 }));
        QCOMPARE(controller.shares().size(), 2);
        QCOMPARE(controller.shares().at(0)->id(), "share-1"_L1);
        QCOMPARE(controller.shares().at(1)->id(), "share-2"_L1);
    }

    void unifiedShareListModelGroupsSharesInOneSectionedList()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                "ocs": {
                    "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                    "data": [{
                        "id": "external-share",
                        "state": "active",
                        "recipients": [{
                            "class": "email",
                            "display_name": "alice@example.com",
                            "value": "alice@example.com"
                        }]
                    }, {
                        "id": "internal-share",
                        "state": "active",
                        "recipients": [{
                            "class": "user",
                            "display_name": "Bob",
                            "value": "bob"
                        }]
                    }, {
                        "id": "unfinished-share",
                        "state": "draft",
                        "recipients": [{
                            "class": "user",
                            "display_name": "Carol",
                            "value": "carol"
                        }]
                    }]
                }
            })json",
                                        this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());

        UnifiedShareListModel model;
        model.setSharingController(&controller);
        QSignalSpy modelResetSpy{&model, &QAbstractItemModel::modelReset};

        controller.initialize("42"_L1);

        QTRY_COMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), UnifiedShareListModel::ShareRole).value<Share *>()->id(), "internal-share"_L1);
        QCOMPARE(model.data(model.index(0), UnifiedShareListModel::SectionRole).toString(), "internal"_L1);
        QCOMPARE(model.data(model.index(1), UnifiedShareListModel::ShareRole).value<Share *>()->id(), "external-share"_L1);
        QCOMPARE(model.data(model.index(1), UnifiedShareListModel::SectionRole).toString(), "external"_L1);
        QCOMPARE(model.rowCount(model.index(0)), 0);
        QVERIFY(!model.data({}, UnifiedShareListModel::ShareRole).isValid());

        const auto internalShare = model.data(model.index(0), UnifiedShareListModel::ShareRole).value<Share *>();
        internalShare->updateFromJson(QJsonDocument::fromJson(R"json({
            "ocs": {"data": {"recipients": [{
                "class": "email",
                "display_name": "bob@example.com",
                "value": "bob@example.com"
            }]}}
        })json"));

        QTRY_COMPARE(modelResetSpy.size(), 2);
        QCOMPARE(model.data(model.index(0), UnifiedShareListModel::SectionRole).toString(), "external"_L1);
        QCOMPARE(model.data(model.index(1), UnifiedShareListModel::SectionRole).toString(), "external"_L1);

        model.setSharingController(nullptr);
        QCOMPARE(model.rowCount(), 0);
    }

    void sharingControllerDestroysShareAndRemovesItFromTheList()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto destroyRequests = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto deleting = request.url().path().endsWith("/api/v1/share/share-1"_L1);
            if (deleting) {
                ++destroyRequests;
            }

            const auto response = deleting ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 204, "message": "OK"},
                        "data": {}
                    }
                })json"}
                                           : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{"id": "share-1", "state": "active"}]
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);

        QSignalSpy sharesChangedSpy{&controller, &SharingController::sharesChanged};
        const auto share = controller.shares().constFirst();
        controller.destroyShare(share);
        controller.destroyShare(share);

        QTRY_VERIFY(controller.shares().isEmpty());
        QCOMPARE(destroyRequests, 1);
        QVERIFY(!controller.destroyingShare());
        QVERIFY(controller.shareDestructionError().isEmpty());
        QCOMPARE(sharesChangedSpy.size(), 1);
    }

    void sharingControllerReportsShareDestructionFailure()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto deleting = request.url().path().endsWith("/api/v1/share/share-1"_L1);
            const auto response = deleting ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "failure", "statuscode": 403, "message": "Not allowed"},
                        "data": {}
                    }
                })json"}
                                           : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{"id": "share-1", "state": "active"}]
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);

        QSignalSpy sharesChangedSpy{&controller, &SharingController::sharesChanged};
        const auto share = controller.shares().constFirst();
        controller.destroyShare(share);

        QTRY_COMPARE(controller.shareDestructionError(), "Not allowed"_L1);
        QVERIFY(!controller.destroyingShare());
        QCOMPARE(controller.shares().size(), 1);
        QVERIFY(sharesChangedSpy.isEmpty());
    }

    void sharingControllerCreatesDraftForSelectedRecipient()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestPaths = QStringList{};
        auto requestVerbs = QList<QByteArray>{};
        auto requestBodies = QList<QJsonObject>{};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *outgoingData) {
            const auto path = request.url().path();
            auto verb = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
            if (verb.isEmpty() && operation == QNetworkAccessManager::PostOperation) {
                verb = "POST";
            }
            requestPaths.append(path);
            requestVerbs.append(verb);

            auto body = QJsonObject{};
            if (outgoingData) {
                if (!outgoingData->isOpen()) {
                    outgoingData->open(QIODevice::ReadOnly);
                }
                body = QJsonDocument::fromJson(outgoingData->peek(outgoingData->bytesAvailable())).object();
                outgoingData->reset();
            }
            requestBodies.append(body);

            const auto creating = path.endsWith("/api/v1/share"_L1);
            const auto addingRecipient = path.endsWith("/recipient"_L1);
            const auto response = QString{R"json({
                "ocs": {
                    "meta": {
                        "status": "ok",
                        "statuscode": %1,
                        "message": "OK"
                    },
                    "data": {
                        "id": "share-1",
                        "state": "draft",
                        "recipients": %2
                    }
                }
            })json"}
                                      .arg(creating ? 201 : 200)
                                      .arg(addingRecipient
                                               ? R"json([{"class":"user","display_name":"Alice","value":"alice","instance":"cloud.example.com"}])json"_L1
                                               : "[]"_L1)
                                      .toUtf8();
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        QSignalSpy sharesChangedSpy{&controller, &SharingController::sharesChanged};
        QSignalSpy creatingShareChangedSpy{&controller, &SharingController::creatingShareChanged};
        QSignalSpy shareCreatedSpy{&controller, &SharingController::shareCreated};

        controller.createShareForRecipient("42"_L1, "user"_L1, "alice"_L1, "cloud.example.com"_L1);

        QVERIFY(controller.creatingShare());
        controller.createShareForRecipient("42"_L1, "user"_L1, "alice"_L1, "cloud.example.com"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);
        QCOMPARE(sharesChangedSpy.size(), 1);
        QCOMPARE(shareCreatedSpy.size(), 1);
        QCOMPARE(shareCreatedSpy.constFirst().constFirst().value<Share *>(), controller.shares().constFirst());
        QCOMPARE(creatingShareChangedSpy.size(), 2);
        QVERIFY(!controller.creatingShare());
        QVERIFY(controller.shareCreationError().isEmpty());
        QCOMPARE(controller.shares().constFirst()->id(), "share-1"_L1);
        QCOMPARE(controller.shares().constFirst()->recipients().size(), 1);
        QCOMPARE(controller.shares().constFirst()->recipients().constFirst()->displayName(), "Alice"_L1);
        QCOMPARE(requestPaths.size(), 3);
        QVERIFY(requestPaths.at(0).endsWith("/api/v1/share"_L1));
        QVERIFY(requestPaths.at(1).endsWith("/api/v1/share/share-1/source"_L1));
        QVERIFY(requestPaths.at(2).endsWith("/api/v1/share/share-1/recipient"_L1));
        QCOMPARE(requestVerbs, (QList<QByteArray>{"POST", "POST", "POST"}));
        QCOMPARE(requestBodies.at(0), QJsonObject{});
        QCOMPARE(requestBodies.at(1), (QJsonObject{{"class"_L1, SourceTypeClasses::node}, {"value"_L1, "42"_L1}}));
        QCOMPARE(requestBodies.at(2),
                 (QJsonObject{{"class"_L1, "user"_L1}, {"value"_L1, "alice"_L1}, {"instance"_L1, "cloud.example.com"_L1}}));
    }

    void sharingControllerCleansUpDraftWhenInitialRecipientFails()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto cleanupRequests = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto path = request.url().path();
            if (path.endsWith("/recipient"_L1)) {
                return new FakePayloadReply{operation,
                                            request,
                                            R"json({
                    "ocs": {
                        "meta": {"status": "failure", "statuscode": 400, "message": "Recipient rejected"},
                        "data": {}
                    }
                })json",
                                            this};
            }

            if (request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == "DELETE") {
                ++cleanupRequests;
                return new FakePayloadReply{operation,
                                            request,
                                            R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 204, "message": "OK"},
                        "data": {}
                    }
                })json",
                                            this};
            }

            const auto statusCode = path.endsWith("/api/v1/share"_L1) ? 201 : 200;
            return new FakePayloadReply{operation,
                                        request,
                                        QString{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": %1, "message": "OK"},
                        "data": {"id": "share-1", "state": "draft"}
                    }
                })json"}
                                            .arg(statusCode)
                                            .toUtf8(),
                                        this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        QSignalSpy shareCreatedSpy{&controller, &SharingController::shareCreated};

        controller.createShareForRecipient("42"_L1, "user"_L1, "alice"_L1);

        QTRY_COMPARE(controller.shareCreationError(), "Recipient rejected"_L1);
        QTRY_COMPARE(cleanupRequests, 1);
        QVERIFY(controller.shares().isEmpty());
        QVERIFY(shareCreatedSpy.isEmpty());
        QVERIFY(!controller.creatingShare());
    }

    void sharingControllerReportsAddedRecipientAndUpdatesShare()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto recipientRequestBody = QJsonObject{};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *outgoingData) {
            const auto addingRecipient = request.url().path().endsWith("/recipient"_L1);
            if (addingRecipient && outgoingData) {
                if (!outgoingData->isOpen()) {
                    outgoingData->open(QIODevice::ReadOnly);
                }
                recipientRequestBody = QJsonDocument::fromJson(outgoingData->peek(outgoingData->bytesAvailable())).object();
                outgoingData->reset();
            }

            const auto response = addingRecipient ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": {
                            "id": "share-1",
                            "state": "active",
                            "recipients": [{
                                "class": "user-class",
                                "display_name": "Alice",
                                "value": "alice",
                                "instance": "cloud.example.com"
                            }]
                        }
                    }
                })json"}
                                                  : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{
                            "id": "share-1",
                            "state": "active",
                            "properties": [{
                                "class": "note-property",
                                "display_name": "Note to recipients",
                                "type": "string"
                            }],
                            "recipients": []
                        }]
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);

        auto addedShare = QPointer<Share>{};
        connect(&controller, &SharingController::recipientAdded, this, [&addedShare](Share *share) {
            addedShare = share;
        });
        controller.addRecipient(controller.shares().constFirst(), "user-class"_L1, "alice"_L1, "cloud.example.com"_L1);

        QTRY_VERIFY(addedShare);
        QCOMPARE(addedShare, controller.shares().constFirst());
        QCOMPARE(addedShare->recipients().size(), 1);
        QCOMPARE(addedShare->recipients().constFirst()->displayName(), "Alice"_L1);
        QCOMPARE(addedShare->recipients().constFirst()->instance(), std::optional<QString>{"cloud.example.com"_L1});
        QCOMPARE(addedShare->properties().size(), 1);
        QCOMPARE(addedShare->properties().constFirst()->displayName(), "Note to recipients"_L1);
        QCOMPARE(recipientRequestBody, (QJsonObject{{"class"_L1, "user-class"_L1}, {"value"_L1, "alice"_L1}, {"instance"_L1, "cloud.example.com"_L1}}));
    }

    void sharingControllerRemovesRecipientAndPreservesItsIdentity()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto removalQuery = QList<QPair<QString, QString>>{};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto removingRecipient = request.url().path().endsWith("/recipient"_L1)
                && (operation == QNetworkAccessManager::DeleteOperation || request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == "DELETE");
            if (removingRecipient) {
                removalQuery = QUrlQuery{request.url()}.queryItems();
                removalQuery.removeAll({"format"_L1, "json"_L1});
                std::ranges::sort(removalQuery);
            }

            const auto response = removingRecipient ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": {
                            "id": "share-1",
                            "state": "active",
                            "recipients": []
                        }
                    }
                })json"}
                                                    : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{
                            "id": "share-1",
                            "state": "active",
                            "recipients": [{
                                "class": "federated-user",
                                "display_name": "Alice",
                                "value": "alice",
                                "instance": "cloud.example.com"
                            }]
                        }]
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);

        QSignalSpy removedSpy{&controller, &SharingController::recipientRemoved};
        controller.removeRecipient(controller.shares().constFirst(), "federated-user"_L1, "alice"_L1, "cloud.example.com"_L1);

        QVERIFY(removedSpy.wait());
        QCOMPARE(controller.shares().constFirst()->recipients().size(), 0);
        QCOMPARE(removalQuery,
                 (QList<QPair<QString, QString>>{{"class"_L1, "federated-user"_L1}, {"instance"_L1, "cloud.example.com"_L1}, {"value"_L1, "alice"_L1}}));
    }

    void sharingControllerGeneratesAndAppliesRecipientSecret()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto secretRequestBody = QJsonObject{};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *outgoingData) {
            const auto path = request.url().path();
            const auto settingSecret = path.endsWith("/recipient/secret"_L1);
            if (settingSecret && outgoingData) {
                if (!outgoingData->isOpen()) {
                    outgoingData->open(QIODevice::ReadOnly);
                }
                secretRequestBody = QJsonDocument::fromJson(outgoingData->peek(outgoingData->bytesAvailable())).object();
                outgoingData->reset();
            }

            auto data = QJsonValue{QJsonArray{QJsonObject{
                {"id"_L1, "share-1"_L1},
                {"state"_L1, "active"_L1},
                {"recipients"_L1,
                 QJsonArray{QJsonObject{
                     {"class"_L1, "federated-user"_L1},
                     {"display_name"_L1, "Alice"_L1},
                     {"value"_L1, "alice"_L1},
                     {"instance"_L1, "cloud.example.com"_L1},
                     {"secret"_L1, QJsonObject{{"updatable"_L1, true}}},
                 }}},
            }}};
            if (path.endsWith("/secret"_L1) && !settingSecret) {
                data = "generated-secret"_L1;
            } else if (settingSecret) {
                data = QJsonObject{
                    {"id"_L1, "share-1"_L1},
                    {"state"_L1, "active"_L1},
                    {"recipients"_L1,
                     QJsonArray{QJsonObject{
                         {"class"_L1, "federated-user"_L1},
                         {"display_name"_L1, "Alice"_L1},
                         {"value"_L1, "alice"_L1},
                         {"instance"_L1, "cloud.example.com"_L1},
                         {"secret"_L1,
                          QJsonObject{
                              {"updatable"_L1, true},
                              {"url"_L1, "https://cloud.example.com/s/generated-secret"_L1},
                          }},
                     }}},
                };
            }

            const auto response = QJsonDocument{
                QJsonObject{
                    {"ocs"_L1,
                     QJsonObject{
                         {"meta"_L1,
                          QJsonObject{
                              {"status"_L1, "ok"_L1},
                              {"statuscode"_L1, 200},
                              {"message"_L1, "OK"_L1},
                          }},
                         {"data"_L1, data},
                     }},
                }}.toJson(QJsonDocument::Compact);
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);

        QSignalSpy updatedSpy{&controller, &SharingController::recipientSecretUpdated};
        controller.updateRecipientSecret(controller.shares().constFirst(), "federated-user"_L1, "alice"_L1, "cloud.example.com"_L1);

        QVERIFY(updatedSpy.wait());
        QCOMPARE(secretRequestBody,
                 (QJsonObject{{"class"_L1, "federated-user"_L1},
                              {"value"_L1, "alice"_L1},
                              {"secret"_L1, "generated-secret"_L1},
                              {"instance"_L1, "cloud.example.com"_L1}}));
        QCOMPARE(controller.shares().constFirst()->recipients().constFirst()->secretUrl(),
                 std::optional<QString>{"https://cloud.example.com/s/generated-secret"_L1});
    }

    void sharingControllerReportsRecipientOperationFailures()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto loadingShares = request.url().path().endsWith("/shares"_L1);
            const auto response = loadingShares ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{"id": "share-1", "state": "active"}]
                    }
                })json"}
                                                : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "failure", "statuscode": 400, "message": "Recipient rejected"},
                        "data": {}
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);

        QSignalSpy removalFailedSpy{&controller, &SharingController::recipientRemovalFailed};
        controller.removeRecipient(controller.shares().constFirst(), "user-class"_L1, "alice"_L1);
        QVERIFY(removalFailedSpy.wait());
        QCOMPARE(removalFailedSpy.constFirst().at(1).toString(), "Recipient rejected"_L1);

        QSignalSpy secretFailedSpy{&controller, &SharingController::recipientSecretUpdateFailed};
        controller.updateRecipientSecret(controller.shares().constFirst(), "user-class"_L1, "alice"_L1);
        QVERIFY(secretFailedSpy.wait());
        QCOMPARE(secretFailedSpy.constFirst().at(1).toString(), "Recipient rejected"_L1);
    }

    void sharingControllerSetsShareProperty()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto propertyRequestBody = QJsonObject{};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *outgoingData) {
            const auto settingProperty = request.url().path().endsWith("/property"_L1);
            if (settingProperty && outgoingData) {
                if (!outgoingData->isOpen()) {
                    outgoingData->open(QIODevice::ReadOnly);
                }
                propertyRequestBody = QJsonDocument::fromJson(outgoingData->peek(outgoingData->bytesAvailable())).object();
                outgoingData->reset();
            }

            const auto response = settingProperty ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": {
                            "id": "share-1",
                            "state": "active",
                            "properties": [{
                                "class": "note-property",
                                "display_name": "Note to recipients",
                                "type": "string",
                                "value": "Updated note"
                            }]
                        }
                    }
                })json"}
                                                  : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{
                            "id": "share-1",
                            "state": "active",
                            "properties": [{
                                "class": "note-property",
                                "display_name": "Note to recipients",
                                "type": "string",
                                "value": "Original note"
                            }]
                        }]
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);
        QCOMPARE(controller.shares().constFirst()->properties().constFirst()->value().toString(), "Original note"_L1);

        QSignalSpy propertyUpdatedSpy{&controller, &SharingController::propertyUpdated};
        controller.setProperty(controller.shares().constFirst(), "note-property"_L1, "Updated note"_L1);

        QTRY_COMPARE(propertyUpdatedSpy.size(), 1);
        QCOMPARE(controller.shares().constFirst()->properties().constFirst()->value().toString(), "Updated note"_L1);
        QCOMPARE(propertyRequestBody, (QJsonObject{{"class"_L1, "note-property"_L1}, {"value"_L1, "Updated note"_L1}}));
    }

    void sharingControllerReportsPermissionUpdateFailures()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto response = request.url().path().endsWith("/shares"_L1) ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{"id": "share-1", "state": "active"}]
                    }
                })json"}
                                                                              : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "failure", "statuscode": 400, "message": "Permission rejected"},
                        "data": {}
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);

        QSignalSpy permissionFailedSpy{&controller, &SharingController::permissionUpdateFailed};
        const auto share = controller.shares().constFirst();
        controller.setPermission(share, "permission-class"_L1, true);
        QTRY_COMPARE(permissionFailedSpy.size(), 1);
        QCOMPARE(permissionFailedSpy.constFirst().at(0).value<Share *>(), share);
        QCOMPARE(permissionFailedSpy.constFirst().at(1).toString(), "Permission rejected"_L1);

        controller.setPermissionPreset(share, "preset-class"_L1);
        QTRY_COMPARE(permissionFailedSpy.size(), 2);
        QCOMPARE(permissionFailedSpy.constLast().at(0).value<Share *>(), share);
        QCOMPARE(permissionFailedSpy.constLast().at(1).toString(), "Permission rejected"_L1);
    }

    void sharingControllerReportsPermissionNetworkFailures()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestCount = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            ++requestCount;
            if (requestCount == 1) {
                return static_cast<QNetworkReply *>(new FakePayloadReply{operation,
                                                                         request,
                                                                         R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{"id": "share-1", "state": "active"}]
                    }
                })json",
                                                                         this});
            }
            return static_cast<QNetworkReply *>(new FakeErrorReply{operation, request, this, 500});
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);

        QSignalSpy permissionFailedSpy{&controller, &SharingController::permissionUpdateFailed};
        const auto share = controller.shares().constFirst();
        controller.setPermission(share, "permission-class"_L1, true);
        QTRY_COMPARE(permissionFailedSpy.size(), 1);
        QVERIFY(!permissionFailedSpy.constFirst().at(1).toString().isEmpty());
    }

    void sharingControllerActivatesDraftShare()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto stateRequestBody = QJsonObject{};
        auto stateRequestVerb = QByteArray{};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *outgoingData) {
            const auto settingState = request.url().path().endsWith("/state"_L1);
            if (settingState) {
                stateRequestVerb = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
                if (stateRequestVerb.isEmpty() && operation == QNetworkAccessManager::PutOperation) {
                    stateRequestVerb = "PUT";
                }
                if (outgoingData) {
                    if (!outgoingData->isOpen()) {
                        outgoingData->open(QIODevice::ReadOnly);
                    }
                    stateRequestBody = QJsonDocument::fromJson(outgoingData->peek(outgoingData->bytesAvailable())).object();
                    outgoingData->reset();
                }
            }

            const auto response = settingState ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": {"id": "share-1", "state": "active"}
                    }
                })json"}
                                               : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{"id": "share-1", "state": "draft"}]
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);
        const auto share = controller.shares().constFirst();
        QCOMPARE(share->state(), Share::ShareState::Draft);

        QSignalSpy activatedSpy{&controller, &SharingController::shareActivated};
        QSignalSpy activationFailedSpy{&controller, &SharingController::shareActivationFailed};
        controller.activateShare(share);

        QTRY_COMPARE(activatedSpy.size(), 1);
        QVERIFY(activationFailedSpy.isEmpty());
        QCOMPARE(share->state(), Share::ShareState::Active);
        QCOMPARE(stateRequestVerb, "PUT");
        QCOMPARE(stateRequestBody, (QJsonObject{{"state"_L1, "active"_L1}}));

        controller.activateShare(share);
        QTest::qWait(10);
        QCOMPARE(activatedSpy.size(), 1);
    }

    void sharingControllerWaitsForDraftUpdatesBeforeActivation()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto stateRequests = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto path = request.url().path();
            if (path.endsWith("/property"_L1)) {
                return static_cast<QNetworkReply *>(new FakePayloadReply{operation,
                                                                         request,
                                                                         R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": {
                            "id": "share-1",
                            "state": "draft",
                            "properties": [{
                                "class": "note-property",
                                "display_name": "Note to recipients",
                                "type": "string",
                                "value": "Saved before activation"
                            }]
                        }
                    }
                })json",
                                                                         100,
                                                                         this});
            }

            if (path.endsWith("/state"_L1)) {
                ++stateRequests;
                return static_cast<QNetworkReply *>(new FakePayloadReply{operation,
                                                                         request,
                                                                         R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": {
                            "id": "share-1",
                            "state": "active",
                            "properties": [{
                                "class": "note-property",
                                "display_name": "Note to recipients",
                                "type": "string",
                                "value": "Saved before activation"
                            }]
                        }
                    }
                })json",
                                                                         this});
            }

            return static_cast<QNetworkReply *>(new FakePayloadReply{operation,
                                                                     request,
                                                                     R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{
                            "id": "share-1",
                            "state": "draft",
                            "properties": [{
                                "class": "note-property",
                                "display_name": "Note to recipients",
                                "type": "string"
                            }]
                        }]
                    }
                })json",
                                                                     this});
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);
        const auto share = controller.shares().constFirst();

        QSignalSpy activatedSpy{&controller, &SharingController::shareActivated};
        controller.setProperty(share, "note-property"_L1, "Saved before activation"_L1);
        controller.activateShare(share);

        QTest::qWait(20);
        QCOMPARE(stateRequests, 0);
        QCOMPARE(share->state(), Share::ShareState::Draft);

        QTRY_COMPARE(activatedSpy.size(), 1);
        QCOMPARE(stateRequests, 1);
        QCOMPARE(share->state(), Share::ShareState::Active);
        QCOMPARE(share->properties().constFirst()->value().toString(), "Saved before activation"_L1);
    }

    void sharingControllerDoesNotActivateAfterPendingDraftUpdateFails()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto stateRequests = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto path = request.url().path();
            if (path.endsWith("/property"_L1)) {
                return new FakePayloadReply{operation,
                                            request,
                                            R"json({
                    "ocs": {
                        "meta": {"status": "failure", "statuscode": 400, "message": "Note rejected"},
                        "data": {}
                    }
                })json",
                                            100,
                                            this};
            }

            if (path.endsWith("/state"_L1)) {
                ++stateRequests;
            }

            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{"id": "share-1", "state": "draft"}]
                    }
                })json",
                                        this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);
        const auto share = controller.shares().constFirst();

        QSignalSpy propertyFailedSpy{&controller, &SharingController::propertyUpdateFailed};
        QSignalSpy activationFailedSpy{&controller, &SharingController::shareActivationFailed};
        controller.setProperty(share, "note-property"_L1, "Rejected note"_L1);
        controller.activateShare(share);

        QTRY_COMPARE(propertyFailedSpy.size(), 1);
        QTRY_COMPARE(activationFailedSpy.size(), 1);
        QCOMPARE(stateRequests, 0);
        QCOMPARE(share->state(), Share::ShareState::Draft);
    }

    void sharingControllerReportsShareActivationFailure()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto settingState = request.url().path().endsWith("/state"_L1);
            const auto response = settingState ? QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "failure", "statuscode": 400, "message": "Share rejected"},
                        "data": {}
                    }
                })json"}
                                               : QByteArray{R"json({
                    "ocs": {
                        "meta": {"status": "ok", "statuscode": 200, "message": "OK"},
                        "data": [{"id": "share-1", "state": "draft"}]
                    }
                })json"};
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        controller.initialize("42"_L1);
        QTRY_COMPARE(controller.shares().size(), 1);
        const auto share = controller.shares().constFirst();

        QSignalSpy activatedSpy{&controller, &SharingController::shareActivated};
        QSignalSpy activationFailedSpy{&controller, &SharingController::shareActivationFailed};
        controller.activateShare(share);

        QTRY_COMPARE(activationFailedSpy.size(), 1);
        QVERIFY(activatedSpy.isEmpty());
        QCOMPARE(share->state(), Share::ShareState::Draft);
        QCOMPARE(activationFailedSpy.constFirst().at(0).value<Share *>(), share);
        QCOMPARE(activationFailedSpy.constFirst().at(1).toString(), "Share rejected"_L1);
    }

    void sharingControllerCleansUpShareWhenAttachingSourceFails()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestPaths = QStringList{};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto path = request.url().path();
            requestPaths.append(path);

            auto statusCode = 200;
            auto message = "OK"_L1;
            if (path.endsWith("/api/v1/share"_L1)) {
                statusCode = 201;
            } else if (path.endsWith("/source"_L1)) {
                statusCode = 400;
                message = "Source rejected"_L1;
            } else if (path.endsWith("/api/v1/share/share-1"_L1)) {
                statusCode = 204;
            }

            const auto response = QString{R"json({
                "ocs": {
                    "meta": {
                        "status": "%1",
                        "statuscode": %2,
                        "message": "%3"
                    },
                    "data": {
                        "id": "share-1",
                        "state": "draft"
                    }
                }
            })json"}
                                      .arg(statusCode >= 400 ? "failure"_L1 : "ok"_L1)
                                      .arg(statusCode)
                                      .arg(message)
                                      .toUtf8();
            return new FakePayloadReply{operation, request, response, this};
        });

        SharingController controller;
        controller.setAccount(fakeFolder.account());
        QSignalSpy sharesChangedSpy{&controller, &SharingController::sharesChanged};

        controller.createShareForRecipient("42"_L1, "user"_L1, "alice"_L1);

        QTRY_COMPARE(requestPaths.size(), 3);
        QVERIFY(!controller.creatingShare());
        QCOMPARE(controller.shareCreationError(), "Source rejected"_L1);
        QVERIFY(controller.shares().isEmpty());
        QVERIFY(sharesChangedSpy.isEmpty());
        QVERIFY(requestPaths.at(0).endsWith("/api/v1/share"_L1));
        QVERIFY(requestPaths.at(1).endsWith("/api/v1/share/share-1/source"_L1));
        QVERIFY(requestPaths.at(2).endsWith("/api/v1/share/share-1"_L1));
    }

    void operationSpecificStatusCodesAreEnforced()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                "ocs": {
                    "meta": {
                        "status": "ok",
                        "statuscode": 200,
                        "message": "OK"
                    },
                    "data": {}
                }
            })json",
                                        this};
        });

        const auto jobs = QList<UnifiedSharingRequest *>{
            new CreateShareJob{fakeFolder.account()},
            new DestroyShareJob{fakeFolder.account(), "share-1"_L1},
        };
        for (const auto job : jobs) {
            auto jobFinished = false;
            auto ocsError = false;
            connect(job, &UnifiedSharingRequest::jobFinished, this, [&](const QJsonDocument &, int) {
                jobFinished = true;
            });
            connect(job, &UnifiedSharingRequest::ocsError, this, [&](int, const QString &) {
                ocsError = true;
            });

            job->start();

            QTRY_VERIFY(ocsError);
            QVERIFY(!jobFinished);
        }
    }

    void typedJobsParseTheirResults()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto path = request.url().path();
            auto data = QByteArray{R"json({"id":"share-1","state":"active"})json"};
            if (path.endsWith("/recipients"_L1)) {
                data = R"json([{"label":"Alice"}])json";
            } else if (path.endsWith("/secret"_L1)) {
                data = R"json("generated-secret")json";
            } else if (path.endsWith("/shares"_L1)) {
                data = R"json([{"id":"share-1","state":"active"},{"id":"share-2","state":"draft"}])json";
            }

            auto statusCode = 200;
            const auto customVerb = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
            if ((operation == QNetworkAccessManager::DeleteOperation || customVerb == "DELETE") && !path.endsWith("/source"_L1)
                && !path.endsWith("/recipient"_L1)) {
                statusCode = 204;
            } else if (operation == QNetworkAccessManager::PostOperation && path.endsWith("/share"_L1)) {
                statusCode = 201;
            }
            const auto payload = QString{R"json({"ocs":{"meta":{"status":"ok","statuscode":%1,"message":"OK"},"data":%2}})json"}
                                     .arg(statusCode)
                                     .arg(QString::fromUtf8(data))
                                     .toUtf8();
            return new FakePayloadReply{operation, request, payload, this};
        });

        const auto account = fakeFolder.account();

        QPointer<Share> createdShare;
        const auto createJob = new CreateShareJob{account};
        connect(createJob, &CreateShareJob::shareCreated, this, [&](QPointer<Share> share) {
            createdShare = share;
        });
        createJob->start();
        QTRY_VERIFY(createdShare);
        QCOMPARE(createdShare->id(), "share-1"_L1);

        auto updateReceived = false;
        const auto updateJob = new SetPermissionJob{account, *createdShare, "permission-class"_L1, true};
        connect(updateJob, &UpdateShareJob::shareUpdated, this, [&](QPointer<Share> share) {
            updateReceived = share == createdShare;
        });
        updateJob->start();
        QTRY_VERIFY(updateReceived);
        QCOMPARE(createdShare->state(), Share::ShareState::Active);

        auto recipients = QJsonArray{};
        const auto searchJob = new SearchRecipientsJob{account, "ali"_L1, 0, 10};
        QCOMPARE(searchJob->timeoutMsec(), 10'000);
        connect(searchJob, &SearchRecipientsJob::recipientsFound, this, [&](const QJsonArray &result) {
            recipients = result;
        });
        searchJob->start();
        QTRY_COMPARE(recipients.size(), 1);
        QCOMPARE(recipients.at(0).toObject().value("label"_L1).toString(), "Alice"_L1);

        auto generatedSecret = QString{};
        const auto secretJob = new GenerateSecretJob{account};
        connect(secretJob, &GenerateSecretJob::secretGenerated, this, [&](const QString &secret) {
            generatedSecret = secret;
        });
        secretJob->start();
        QTRY_COMPARE(generatedSecret, "generated-secret"_L1);

        QPointer<Share> fetchedShare;
        const auto getShareJob = new GetShareJob{account, "share-1"_L1};
        connect(getShareJob, &GetShareJob::shareFetched, this, [&](QPointer<Share> share) {
            fetchedShare = share;
        });
        getShareJob->start();
        QTRY_VERIFY(fetchedShare);
        QCOMPARE(fetchedShare->id(), "share-1"_L1);

        auto fetchedShares = QList<QPointer<Share>>{};
        const auto getSharesJob = new GetSharesJob{account};
        connect(getSharesJob, &GetSharesJob::sharesFetched, this, [&](const QList<QPointer<Share>> &shares) {
            fetchedShares = shares;
        });
        getSharesJob->start();
        QTRY_COMPARE(fetchedShares.size(), 2);
        QCOMPARE(fetchedShares.at(0)->id(), "share-1"_L1);
        QCOMPARE(fetchedShares.at(1)->id(), "share-2"_L1);

        auto destroyed = false;
        const auto destroyJob = new DestroyShareJob{account, createdShare->id()};
        connect(destroyJob, &DestroyShareJob::jobFinished, this, [&](const QJsonDocument &, int) {
            destroyed = true;
        });
        destroyJob->start();
        QTRY_VERIFY(destroyed);

        delete createdShare;
        delete fetchedShare;
        qDeleteAll(fetchedShares);
    }

    void ocsErrorsAreSeparateFromSuccessfulResults()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                "ocs": {
                    "meta": {
                        "status": "failure",
                        "statuscode": 400,
                        "message": "Invalid permission"
                    },
                    "data": {
                        "id": "changed-share",
                        "state": "active"
                    }
                }
            })json",
                                        this};
        });

        auto jobFinished = false;
        auto ocsError = false;
        const auto job = new UnifiedSharingRequest{fakeFolder.account(), "/ocs/v2.php/apps/sharing/api/v1/share"_L1, "GET"_ba};
        connect(job, &UnifiedSharingRequest::jobFinished, this, [&](const QJsonDocument &, int) {
            jobFinished = true;
        });
        connect(job, &UnifiedSharingRequest::ocsError, this, [&](int, const QString &) {
            ocsError = true;
        });

        job->start();

        QTRY_VERIFY(ocsError);
        QVERIFY(!jobFinished);
    }

    void failedUpdatesDoNotMutateShares()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return new FakePayloadReply{operation,
                                        request,
                                        R"json({
                "ocs": {
                    "meta": {
                        "status": "failure",
                        "statuscode": 400,
                        "message": "Invalid update"
                    },
                    "data": {
                        "id": "changed-share",
                        "state": "active"
                    }
                }
            })json",
                                        this};
        });

        const auto account = fakeFolder.account();
        const auto share = Share::fromJson(QJsonDocument::fromJson(R"json({"ocs":{"data":{"id":"share-1","state":"draft"}}})json"), account);
        const auto jobs = QList<UpdateShareJob *>{
            new AddSourceJob{account, *share, "42"_L1},
            new RemoveSourceJob{account, *share, "42"_L1},
            new AddRecipientJob{account, *share, "recipient-class"_L1, "alice"_L1},
            new RemoveRecipientJob{account, *share, "recipient-class"_L1, "alice"_L1},
            new SetRecipientSecretJob{account, *share, "recipient-class"_L1, "alice"_L1, "secret"_L1},
            new SetPropertyJob{account, *share, "property-class"_L1, "value"_L1},
            new SetPermissionJob{account, *share, "permission-class"_L1, true},
            new SetPermissionPresetJob{account, *share, "preset-class"_L1},
            new SetShareStateJob{account, *share, Share::ShareState::Active},
        };

        for (const auto job : jobs) {
            auto shareUpdated = false;
            auto ocsError = false;
            connect(job, &UpdateShareJob::shareUpdated, this, [&](QPointer<Share>) {
                shareUpdated = true;
            });
            connect(job, &UpdateShareJob::ocsError, this, [&](int, const QString &) {
                ocsError = true;
            });

            job->start();

            QTRY_VERIFY(ocsError);
            QVERIFY(!shareUpdated);
            QCOMPARE(share->id(), "share-1"_L1);
            QCOMPARE(share->state(), Share::ShareState::Draft);
        }

        delete share;
    }

    void networkErrorsAreSeparateFromSuccessfulResults()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return new FakeErrorReply{operation, request, this, 500};
        });

        auto jobFinished = false;
        auto networkError = false;
        const auto job = new UnifiedSharingRequest{fakeFolder.account(), "/ocs/v2.php/apps/sharing/api/v1/share"_L1, "GET"_ba};
        connect(job, &UnifiedSharingRequest::jobFinished, this, [&](const QJsonDocument &, int) {
            jobFinished = true;
        });
        connect(job, &UnifiedSharingRequest::networkError, this, [&](QNetworkReply *) {
            networkError = true;
        });

        job->start();

        QTRY_VERIFY(networkError);
        QVERIFY(!jobFinished);
    }

    void timeoutsAreSeparateFromSuccessfulResults()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return new FakeHangingReply{operation, request, this};
        });

        auto jobFinished = false;
        auto networkError = false;
        auto timedOut = false;
        const auto job = new UnifiedSharingRequest{fakeFolder.account(), "/ocs/v2.php/apps/sharing/api/v1/share"_L1, "GET"_ba};
        job->setTimeout(10);
        connect(job, &UnifiedSharingRequest::jobFinished, this, [&](const QJsonDocument &, int) {
            jobFinished = true;
        });
        connect(job, &UnifiedSharingRequest::networkError, this, [&, job](QNetworkReply *) {
            networkError = true;
            timedOut = job->timedOut();
        });

        job->start();

        QTRY_VERIFY(networkError);
        QVERIFY(!jobFinished);
        QVERIFY(timedOut);
    }

    void recipientSearchIsDebouncedAndIgnoresStaleResults()
    {
        FakeFolder fakeFolder{{}, {}, {}, false};
        auto requestCount = 0;
        fakeFolder.setServerOverride([&](FakeQNAM::Operation operation, const QNetworkRequest &request, QIODevice *) {
            ++requestCount;
            const auto query = QUrlQuery{request.url()}.queryItemValue("query"_L1);
            const auto payload =
                QString{R"json({"ocs":{"meta":{"status":"ok","statuscode":200,"message":"OK"},"data":[{"display_name":"%1"}]}})json"}.arg(query).toUtf8();
            const auto delay = query == "old"_L1 ? 800 : FakePayloadReply::defaultDelay;
            return new FakePayloadReply{operation, request, payload, delay, this};
        });

        RecipientSearchModel model;
        model.setAccount(fakeFolder.account());
        QVERIFY(!model.fetchOngoing());
        model.setQuery("o"_L1);
        model.setQuery("ol"_L1);
        model.setQuery("old"_L1);

        QTest::qWait(200);
        QCOMPARE(requestCount, 0);
        QTRY_COMPARE_WITH_TIMEOUT(requestCount, 1, 500);
        QVERIFY(model.fetchOngoing());

        model.setQuery("new"_L1);
        QTRY_COMPARE_WITH_TIMEOUT(requestCount, 2, 500);
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 500);
        QTRY_VERIFY_WITH_TIMEOUT(!model.fetchOngoing(), 500);
        QCOMPARE(model.data(model.index(0), RecipientSearchModel::DisplayNameRole).toString(), "new"_L1);

        QTest::qWait(500);
        QCOMPARE(model.data(model.index(0), RecipientSearchModel::DisplayNameRole).toString(), "new"_L1);
    }
};

QTEST_GUILESS_MAIN(TestUnifiedSharing)

#include "testunifiedsharing.moc"
