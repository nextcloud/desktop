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

        verifyRequest(new SearchRecipientsJob{account, "ali"_L1, 10, 20, {"user-class"_L1, "group-class"_L1}},
                      "GET",
                      "/ocs/v2.php/apps/sharing/api/v1/recipients",
                      {{"query"_L1, "ali"_L1},
                       {"offset"_L1, "10"_L1},
                       {"limit"_L1, "20"_L1},
                       {"recipientTypeClasses%5B%5D"_L1, "user-class"_L1},
                       {"recipientTypeClasses%5B%5D"_L1, "group-class"_L1}});
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
                      {{"class"_L1, "OCA\\Files\\Sharing\\Source\\NodeShareSourceType"_L1}, {"value"_L1, "42"_L1}});
        verifyRequest(new RemoveSourceJob{account, *share, "42"_L1},
                      "DELETE",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1/source",
                      {{"class"_L1, "OCA\\Files\\Sharing\\Source\\NodeShareSourceType"_L1}, {"value"_L1, "42"_L1}});
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
        verifyRequest(new GetShareJob{account,
                                      "share-1"_L1,
                                      "secret"_L1,
                                      QJsonObject{{"argument-class"_L1, QJsonObject{{"key"_L1, "value"_L1}}}}},
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
        verifyRequest(new GetShareJob{account, "share-1"_L1},
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1");
        QCOMPARE(requestContentType, "application/x-www-form-urlencoded");
        verifyRequest(new UnifiedSharingRequest{account,
                                                "/ocs/v2.php/apps/sharing/api/v1/share/share-1"_L1,
                                                "POST"_ba,
                                                {.body = QJsonObject{}}},
                      "POST",
                      "/ocs/v2.php/apps/sharing/api/v1/share/share-1");
        QCOMPARE(requestContentType, "application/json");
        verifyRequest(new GetSharesJob{account},
                      "GET",
                      "/ocs/v2.php/apps/sharing/api/v1/shares",
                      {{"limit"_L1, "100"_L1}});

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
        model.setQuery("o"_L1);
        model.setQuery("ol"_L1);
        model.setQuery("old"_L1);

        QTest::qWait(200);
        QCOMPARE(requestCount, 0);
        QTRY_COMPARE_WITH_TIMEOUT(requestCount, 1, 500);

        model.setQuery("new"_L1);
        QTRY_COMPARE_WITH_TIMEOUT(requestCount, 2, 500);
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 500);
        QCOMPARE(model.data(model.index(0), RecipientSearchModel::DisplayNameRole).toString(), "new"_L1);

        QTest::qWait(500);
        QCOMPARE(model.data(model.index(0), RecipientSearchModel::DisplayNameRole).toString(), "new"_L1);
    }
};

QTEST_GUILESS_MAIN(TestUnifiedSharing)

#include "testunifiedsharing.moc"
