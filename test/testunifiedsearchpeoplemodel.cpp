/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/search/unifiedsearchpeoplemodel.h"

#include "account.h"
#include "accountstate.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

#include <QAbstractItemModelTester>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QUrlQuery>

#include <memory>

namespace {
const auto response = QByteArrayLiteral(R"({"ocs":{"meta":{"status":"ok","statuscode":200,"message":"OK"},"data":{"exact":{"users":[{"label":"Ada Lovelace","value":{"shareWith":"ada","shareType":0}}]},"users":[{"label":"Ada duplicate","value":{"shareWith":"ada","shareType":0}},{"label":"Alan Turing","value":{"shareWith":"alan","shareType":0}}]}}})");
const auto errorResponse = QByteArrayLiteral(R"({"ocs":{"meta":{"status":"failure","statuscode":500,"message":"Failure"},"data":{}}})");
}

class TestUnifiedSearchPeopleModel : public QObject
{
    Q_OBJECT

private slots:
    void emptyQueryShowsSignedInUserWithoutRequest()
    {
        auto qnam = std::unique_ptr<FakeQNAM>(new FakeQNAM({}));
        auto account = OCC::Account::create();
        account->setCredentials(new FakeCredentials(qnam.get()));
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.test/")));
        account->setDavUser(QStringLiteral("current-user"));
        auto state = std::make_unique<FakeAccountState>(account);
        auto requestCount = 0;
        qnam->setOverride([&](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) {
            ++requestCount;
            return static_cast<QNetworkReply *>(new FakePayloadReply(operation, request, response, qnam.get()));
        });

        OCC::UnifiedSearchPeopleModel model(nullptr, 0);
        QAbstractItemModelTester tester(&model);
        model.setAccountState(state.get());

        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 1000);
        QCOMPARE(requestCount, 0);
        QCOMPARE(model.data(model.index(0), OCC::UnifiedSearchPeopleModel::UserIdRole).toString(), QStringLiteral("current-user"));
        QVERIFY(model.errorString().isEmpty());
    }

    void parsesUsersAndUsesDocumentedParameters()
    {
        auto qnam = std::unique_ptr<FakeQNAM>(new FakeQNAM({}));
        auto account = OCC::Account::create();
        account->setCredentials(new FakeCredentials(qnam.get()));
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.test/")));
        account->setDavUser(QStringLiteral("me"));
        auto state = std::make_unique<FakeAccountState>(account);
        QUrl requestedUrl;
        qnam->setOverride([&](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) {
            requestedUrl = request.url();
            return static_cast<QNetworkReply *>(new FakePayloadReply(operation, request, response, qnam.get()));
        });

        OCC::UnifiedSearchPeopleModel model(nullptr, 0);
        QAbstractItemModelTester tester(&model);
        model.setAccountState(state.get());
        model.setSearchTerm(QStringLiteral("a"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy() && model.rowCount() == 2, 1000);

        QCOMPARE(requestedUrl.path(), QStringLiteral("/ocs/v2.php/apps/files_sharing/api/v1/sharees"));
        const QUrlQuery query(requestedUrl);
        QCOMPARE(query.queryItemValue(QStringLiteral("shareType")), QStringLiteral("0"));
        QCOMPARE(query.queryItemValue(QStringLiteral("lookup")), QStringLiteral("false"));
        QCOMPARE(query.queryItemValue(QStringLiteral("page")), QStringLiteral("1"));
        QCOMPARE(query.queryItemValue(QStringLiteral("perPage")), QStringLiteral("50"));
        QCOMPARE(model.data(model.index(0), OCC::UnifiedSearchPeopleModel::UserIdRole).toString(), QStringLiteral("ada"));
        QCOMPARE(model.data(model.index(1), OCC::UnifiedSearchPeopleModel::UserIdRole).toString(), QStringLiteral("alan"));
        QCOMPARE(model.data(model.index(0), OCC::UnifiedSearchPeopleModel::AvatarUrlRole).toString(),
            QStringLiteral("https://cloud.example.test/index.php/avatar/ada/64"));
    }

    void injectsMatchingSignedInUser()
    {
        auto qnam = std::unique_ptr<FakeQNAM>(new FakeQNAM({}));
        auto account = OCC::Account::create();
        account->setCredentials(new FakeCredentials(qnam.get()));
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.test/")));
        account->setDavUser(QStringLiteral("current-user"));
        auto state = std::make_unique<FakeAccountState>(account);
        qnam->setOverride([&](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return static_cast<QNetworkReply *>(new FakePayloadReply(operation, request, QByteArrayLiteral(R"({"ocs":{"meta":{"status":"ok","statuscode":200,"message":"OK"},"data":{"exact":{"users":[]},"users":[]}}})"), qnam.get()));
        });
        OCC::UnifiedSearchPeopleModel model(nullptr, 0);
        model.setAccountState(state.get());
        model.setSearchTerm(QStringLiteral("current"));
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 1000);
        QCOMPARE(model.data(model.index(0), OCC::UnifiedSearchPeopleModel::UserIdRole).toString(), QStringLiteral("current-user"));
    }

    void queryReplacementAndFailureClearOldPeople()
    {
        auto qnam = std::unique_ptr<FakeQNAM>(new FakeQNAM({}));
        auto account = OCC::Account::create();
        account->setCredentials(new FakeCredentials(qnam.get()));
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.test/")));
        account->setDavUser(QStringLiteral("me"));
        auto state = std::make_unique<FakeAccountState>(account);
        qnam->setOverride([&](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) {
            const auto query = QUrlQuery(request.url()).queryItemValue(QStringLiteral("search"));
            if (query == QStringLiteral("broken")) {
                return static_cast<QNetworkReply *>(new FakeErrorReply(operation, request, qnam.get(), 500, errorResponse));
            }
            return static_cast<QNetworkReply *>(new FakePayloadReply(operation, request, response, qnam.get()));
        });

        OCC::UnifiedSearchPeopleModel model(nullptr, 0);
        QAbstractItemModelTester tester(&model);
        model.setAccountState(state.get());
        model.setSearchTerm(QStringLiteral("a"));
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 2, 1000);

        model.setSearchTerm(QStringLiteral("broken"));
        QCOMPARE(model.rowCount(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy() && !model.errorString().isEmpty(), 1000);
        QCOMPARE(model.rowCount(), 0);
    }

    void accountDestructionClearsPeopleAndPointer()
    {
        auto qnam = std::unique_ptr<FakeQNAM>(new FakeQNAM({}));
        auto account = OCC::Account::create();
        account->setCredentials(new FakeCredentials(qnam.get()));
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.test/")));
        account->setDavUser(QStringLiteral("current-user"));
        auto state = std::make_unique<FakeAccountState>(account);

        OCC::UnifiedSearchPeopleModel model(nullptr, 0);
        QAbstractItemModelTester tester(&model);
        model.setAccountState(state.get());
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 1000);

        state->setStateForTesting(OCC::AccountState::Disconnected);
        QVERIFY(QMetaObject::invokeMethod(state.get(), "isConnectedChanged", Qt::DirectConnection));
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.errorString().isEmpty());

        state.reset();
        QCOMPARE(model.accountState(), nullptr);
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.errorString().isEmpty());
    }

    void capsNonConformingResponses()
    {
        auto qnam = std::unique_ptr<FakeQNAM>(new FakeQNAM({}));
        auto account = OCC::Account::create();
        account->setCredentials(new FakeCredentials(qnam.get()));
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.test/")));
        auto state = std::make_unique<FakeAccountState>(account);

        auto users = QJsonArray();
        for (auto index = 0; index < 75; ++index) {
            users.push_back(QJsonObject{
                {QStringLiteral("label"), QStringLiteral("User %1").arg(index)},
                {QStringLiteral("value"), QJsonObject{{QStringLiteral("shareWith"), QStringLiteral("user-%1").arg(index)}}},
            });
        }
        const auto oversizedResponse = QJsonDocument(QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{
                {QStringLiteral("meta"), QJsonObject{{QStringLiteral("statuscode"), 200}}},
                {QStringLiteral("data"), QJsonObject{
                    {QStringLiteral("exact"), QJsonObject{{QStringLiteral("users"), QJsonArray()}}},
                    {QStringLiteral("users"), users},
                }},
            }},
        }).toJson(QJsonDocument::Compact);
        qnam->setOverride([&](QNetworkAccessManager::Operation operation, const QNetworkRequest &request, QIODevice *) {
            return static_cast<QNetworkReply *>(new FakePayloadReply(operation, request, oversizedResponse, qnam.get()));
        });

        OCC::UnifiedSearchPeopleModel model(nullptr, 0);
        model.setAccountState(state.get());
        model.setSearchTerm(QStringLiteral("user"));
        QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 50, 1000);
    }
};

QTEST_MAIN(TestUnifiedSearchPeopleModel)
#include "testunifiedsearchpeoplemodel.moc"
