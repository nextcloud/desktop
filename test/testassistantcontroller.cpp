/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "assistant/assistantcontroller.h"
#include "assistant/fakeassistantclient.h"
#include "syncenginetestutils.h"
#include "testhelper.h"
#include <QUrlQuery>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtTest>

using namespace OCC;
using namespace Qt::StringLiterals;

namespace
{

QJsonDocument ocsResponse(const QJsonValue &data)
{
    return QJsonDocument{QJsonObject{
        {QStringLiteral("ocs"), QJsonObject{{QStringLiteral("data"), data}}},
    }};
}

QJsonObject message(qint64 id, qint64 conversationId, const QString &role, const QString &content)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("sessionId"), conversationId},
        {QStringLiteral("role"), role},
        {QStringLiteral("content"), content},
        {QStringLiteral("timestamp"), 1},
    };
}

QJsonDocument humanMessageResponse(qint64 conversationId, bool enveloped = false)
{
    const auto messages = QJsonArray{message(1, conversationId, QStringLiteral("human"), QStringLiteral("Question"))};
    return enveloped ? ocsResponse(messages) : QJsonDocument{messages};
}

QJsonDocument chatTaskTypesResponse()
{
    const auto chatType = QJsonObject{
        {QStringLiteral("name"), QStringLiteral("Chat")},
        {QStringLiteral("description"), QStringLiteral("A conversation")},
    };
    return ocsResponse(QJsonObject{
        {QStringLiteral("types"), QJsonObject{{QStringLiteral("core:text2text:chat"), chatType}}},
    });
}

QJsonDocument conversationsResponse(std::initializer_list<QJsonObject> conversations)
{
    auto array = QJsonArray{};
    for (const auto &conversation : conversations) {
        array.append(conversation);
    }
    return ocsResponse(array);
}

QJsonObject conversation(qint64 id, const QString &title)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("timestamp"), 1},
    };
}

QJsonDocument completedTaskResponse(qint64 taskId, const QString &taskType, const QString &input)
{
    const auto task = QJsonObject{
        {QStringLiteral("id"), taskId},
        {QStringLiteral("type"), taskType},
        {QStringLiteral("appId"), QStringLiteral("assistant")},
        {QStringLiteral("input"), QJsonObject{{QStringLiteral("input"), input}}},
        {QStringLiteral("output"), QJsonObject{{QStringLiteral("output"), QStringLiteral("Done")}}},
        {QStringLiteral("status"), QStringLiteral("STATUS_SUCCESSFUL")},
    };
    return ocsResponse(QJsonObject{{QStringLiteral("tasks"), QJsonArray{task}}});
}

struct ControllerFixture {
    ControllerFixture()
        : account(Account::create())
        , accountState(new FakeAccountState(account))
        , client(new FakeAssistantClient)
        , controller(accountState, client, nullptr)
    {
        account->setCapabilities({
            {QStringLiteral("assistant"),
             QVariantMap{
                 {QStringLiteral("enabled"), true},
                 {QStringLiteral("version"), QStringLiteral("1.0.9")},
             }},
        });
    }

    void loadConversations()
    {
        controller.loadData();
        client->deliverTaskTypes(client->lastTaskTypesGeneration, chatTaskTypesResponse());
        client->deliverChatConversations(client->lastChatConversationsGeneration,
                                         conversationsResponse({conversation(42, u"Current"_s), conversation(43, u"Other"_s)}));
        controller.selectChatConversation(42);
        client->deliverChatMessages(client->lastChatMessagesGeneration, humanMessageResponse(42));
        client->deliverChatSessionCheck(client->lastChatSessionCheckGeneration, ocsResponse(QJsonObject{}));
    }

    AccountPtr account;
    AccountStatePtr accountState;
    FakeAssistantClient *client;
    AssistantController controller;
};

}

class TestAssistantController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void conversationDeletionSendsDeleteRequest_data()
    {
        QTest::addColumn<int>("httpStatus");
        QTest::addColumn<QByteArray>("payload");
        QTest::addColumn<int>("expectedStatus");
        QTest::addColumn<bool>("validJson");
        QTest::newRow("raw-success") << 200 << QByteArray("[]") << 0 << true;
        QTest::newRow("ocs-success") << 200 << QByteArray(R"({"ocs":{"meta":{"statuscode":100},"data":[]}})") << 100 << true;
        QTest::newRow("server-error") << 500 << QByteArray("{}") << 500 << false;
        QTest::newRow("malformed") << 200 << QByteArray("not json") << 0 << false;
    }

    void conversationDeletionSendsDeleteRequest()
    {
        QFETCH(int, httpStatus);
        QFETCH(QByteArray, payload);
        QFETCH(int, expectedStatus);
        QFETCH(bool, validJson);
        const auto account = Account::create();
        account->setUrl(QUrl(u"https://cloud.example"_s));
        const auto network = new FakeQNAM({});
        account->setCredentials(new FakeCredentials{network});
        auto requests = QList<QNetworkRequest>{};
        network->setOverride([&requests, network, httpStatus, payload](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            requests.append(request);
            if (operation != QNetworkAccessManager::DeleteOperation) {
                return new FakeErrorReply(operation, request, network, 405);
            }
            if (httpStatus != 200) {
                return new FakeErrorReply(operation, request, network, httpStatus);
            }
            return new FakePayloadReply(operation, request, payload, network);
        });
        auto client = AssistantClient(account);
        auto received = QSignalSpy(&client, &AssistantClient::chatConversationDeleted);
        client.deleteChatConversation(42, 3);
        client.deleteChatConversation(43, 4);
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(requests.size(), 1);
        QCOMPARE(requests.first().url().path(), u"/ocs/v2.php/apps/assistant/chat/delete_session"_s);
        QCOMPARE(QUrlQuery(requests.first().url()).queryItemValue(u"sessionId"_s), u"42"_s);
        QCOMPARE(received.first().at(0).toULongLong(), 3);
        QCOMPARE(received.first().at(1).toLongLong(), 42);
        QCOMPARE(!qvariant_cast<QJsonDocument>(received.first().at(2)).isNull(), validJson);
        QCOMPARE(received.first().at(3).toInt(), expectedStatus);
    }

    void conversationDeletionUpdatesOnlyAfterSuccess_data()
    {
        QTest::addColumn<qint64>("deletedId");
        QTest::newRow("active") << qint64{42};
        QTest::newRow("other") << qint64{43};
    }

    void conversationDeletionUpdatesOnlyAfterSuccess()
    {
        QFETCH(qint64, deletedId);
        auto fixture = ControllerFixture{};
        fixture.loadConversations();
        fixture.controller.deleteChatConversation(deletedId);
        const auto generation = fixture.client->lastChatConversationDeleteGeneration;
        QVERIFY(fixture.controller.requestInProgress());
        QCOMPARE(fixture.controller.chatConversations()->rowCount(), 2);
        QCOMPARE(fixture.controller.messages()->rowCount(), 1);
        fixture.controller.deleteChatConversation(deletedId);
        QCOMPARE(fixture.client->deleteChatConversationCount, 1);
        fixture.client->deliverChatConversationDeleted(generation, deletedId, QJsonDocument{QJsonArray{}}, 0);
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(fixture.controller.error().isEmpty());
        QCOMPARE(fixture.controller.chatConversations()->rowCount(), 1);
        QCOMPARE(fixture.controller.selectedChatConversationId(), deletedId == 42 ? -1 : 42);
        QCOMPARE(fixture.controller.selectedChatConversationTitle(), deletedId == 42 ? QString{} : u"Current"_s);
        QCOMPARE(fixture.controller.messages()->rowCount(), deletedId == 42 ? 0 : 1);
        QCOMPARE(fixture.controller.showRetryResponseGeneration(), deletedId != 42);

        const auto remainingId = deletedId == 42 ? 43 : 42;
        fixture.controller.deleteChatConversation(remainingId);
        fixture.client->deliverChatConversationDeleted(fixture.client->lastChatConversationDeleteGeneration, remainingId, ocsResponse(QJsonArray{}));
        QCOMPARE(fixture.controller.chatConversations()->rowCount(), 0);
        QCOMPARE(fixture.controller.selectedChatConversationId(), -1);
    }

    void conversationDeletionFailurePreservesChat_data()
    {
        QTest::addColumn<int>("status");
        QTest::newRow("forbidden") << 403;
        QTest::newRow("server-error") << 500;
        QTest::newRow("network-error-or-timeout") << 0;
        QTest::newRow("malformed-success") << 200;
    }

    void conversationDeletionFailurePreservesChat()
    {
        QFETCH(int, status);
        auto fixture = ControllerFixture{};
        fixture.loadConversations();
        fixture.controller.deleteChatConversation(42);
        fixture.client->deliverChatConversationDeleted(fixture.client->lastChatConversationDeleteGeneration, 42, {}, status);
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.error().isEmpty());
        QCOMPARE(fixture.controller.chatConversations()->rowCount(), 2);
        QCOMPARE(fixture.controller.selectedChatConversationId(), 42);
        QCOMPARE(fixture.controller.messages()->rowCount(), 1);
        QVERIFY(fixture.controller.showRetryResponseGeneration());
        fixture.controller.deleteChatConversation(42);
        QCOMPARE(fixture.client->deleteChatConversationCount, 2);
    }

    void conversationDeletionRejectsInvalidAndStaleRequests()
    {
        auto fixture = ControllerFixture{};
        fixture.loadConversations();
        for (const auto id : {-1, 0, 99}) {
            fixture.controller.deleteChatConversation(id);
        }
        QCOMPARE(fixture.client->deleteChatConversationCount, 0);
        fixture.controller.selectChatConversation(42);
        fixture.controller.deleteChatConversation(42);
        QCOMPARE(fixture.client->deleteChatConversationCount, 0);
        fixture.client->deliverChatMessages(fixture.client->lastChatMessagesGeneration, humanMessageResponse(42));
        fixture.client->deliverChatSessionCheck(fixture.client->lastChatSessionCheckGeneration, ocsResponse(QJsonObject{}));
        fixture.controller.deleteChatConversation(42);
        const auto generation = fixture.client->lastChatConversationDeleteGeneration;
        const auto cancellations = fixture.client->cancelRequestsCount;
        fixture.controller.startNewChat();
        QVERIFY(fixture.client->cancelRequestsCount > cancellations);
        fixture.client->deliverChatConversationDeleted(generation, 42, ocsResponse(QJsonArray{}));
        QCOMPARE(fixture.controller.chatConversations()->rowCount(), 2);
        QCOMPARE(fixture.controller.selectedChatConversationId(), -1);
        QVERIFY(!fixture.controller.requestInProgress());
    }

    void actionDecisionsUseGenerationQuery_data()
    {
        QTest::addColumn<int>("decision");
        QTest::newRow("ordinary-message") << -1;
        QTest::newRow("cancel") << 0;
        QTest::newRow("confirm") << 1;
    }

    void actionDecisionsUseGenerationQuery()
    {
        using namespace Qt::StringLiterals;
        QFETCH(int, decision);
        const auto account = Account::create();
        account->setUrl(QUrl(u"https://cloud.example"_s));
        const auto network = new FakeQNAM({});
        account->setCredentials(new FakeCredentials{network});
        auto requests = QList<QNetworkRequest>{};
        network->setOverride([&requests, network](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            requests.append(request);
            return new FakeJsonReply(operation, request, network, 200, QJsonDocument{QJsonObject{{u"taskId"_s, 7}}});
        });
        auto client = AssistantClient(account);
        auto received = QSignalSpy(&client, &AssistantClient::chatSessionGenerationStarted);
        client.generateChatSession(42, 3, decision);
        QTRY_COMPARE(received.size(), 1);
        QCOMPARE(requests.size(), 1);
        QCOMPARE(requests.first().url().path(), u"/ocs/v2.php/apps/assistant/chat/generate"_s);
        const auto query = QUrlQuery(requests.first().url());
        QCOMPARE(query.queryItemValue(u"sessionId"_s), u"42"_s);
        QCOMPARE(query.hasQueryItem(u"agencyConfirm"_s), decision >= 0);
        if (decision >= 0) {
            QCOMPARE(query.queryItemValue(u"agencyConfirm"_s), QString::number(decision));
        }
    }

    void pendingActionsWorkflow_data()
    {
        QTest::addColumn<bool>("confirm");
        QTest::newRow("confirm") << true;
        QTest::newRow("cancel") << false;
    }

    void pendingActionsWorkflow()
    {
        using namespace Qt::StringLiterals;
        QFETCH(bool, confirm);
        ControllerFixture fixture;
        fixture.controller.selectTaskType(u"core:text2text:chat"_s);
        fixture.controller.selectChatConversation(42);
        auto generation = fixture.client->lastChatMessagesGeneration;
        fixture.client->deliverChatMessages(generation, humanMessageResponse(42));
        const auto actions = QJsonArray{QJsonObject{{u"name"_s, u"Schedule a calendar event"_s},
                                                    {u"args"_s, QJsonObject{{u"title"_s, u"Roadmap"_s}, {u"attendees"_s, QJsonArray{u"Alice"_s}}}},
                                                    {u"destructive"_s, false},
                                                    {u"impulse_radius"_s, u"group"_s}}};
        fixture.client->deliverChatSessionCheck(generation, ocsResponse(QJsonObject{{u"sessionAgencyPendingActions"_s, actions}}));
        QCOMPARE(fixture.controller.pendingActions(), actions.toVariantList());
        QVERIFY(!fixture.controller.showRetryResponseGeneration());
        fixture.controller.answerPendingActions(confirm);
        QCOMPARE(fixture.client->createChatMessageCount, 1);
        QCOMPARE(fixture.client->lastChatMessageRole, u"human"_s);
        QVERIFY(fixture.client->lastChatMessageContent.isEmpty());
        QVERIFY(!fixture.client->lastChatMessageWasFirst);
        fixture.controller.answerPendingActions(!confirm);
        QCOMPARE(fixture.client->createChatMessageCount, 1);
        generation = fixture.client->lastChatMessageCreateGeneration;
        fixture.client->deliverChatMessageCreated(generation, QJsonDocument{}, 500);
        QVERIFY(!fixture.controller.requestInProgress());
        QCOMPARE(fixture.controller.pendingActions(), actions.toVariantList());
        fixture.controller.answerPendingActions(confirm);
        generation = fixture.client->lastChatMessageCreateGeneration;
        fixture.client->deliverChatMessageCreated(generation, QJsonDocument{message(2, 42, u"human"_s, {})});
        QCOMPARE(fixture.client->lastAgencyConfirm, confirm ? 1 : 0);
        QCOMPARE(fixture.client->lastGeneratedConversationId, 42);
        QVERIFY(fixture.controller.pendingActions().isEmpty());
        fixture.client->deliverChatSessionGenerationStarted(generation, QJsonDocument{QJsonObject{{u"taskId"_s, 9}}});
        auto reply = message(3, 42, u"assistant"_s, u"Next action"_s);
        reply.insert(u"sessionAgencyPendingActions"_s, actions);
        fixture.client->deliverChatGenerationCheck(generation, ocsResponse(reply), 200);
        QCOMPARE(fixture.controller.pendingActions(), actions.toVariantList());
        QVERIFY(!fixture.controller.requestInProgress());
        fixture.controller.selectChatConversation(43);
        QVERIFY(fixture.controller.pendingActions().isEmpty());
        fixture.client->deliverChatGenerationCheck(generation, ocsResponse(reply), 200);
        QVERIFY(fixture.controller.pendingActions().isEmpty());
        fixture.controller.startNewChat();
        fixture.controller.answerPendingActions(true);
        QCOMPARE(fixture.client->generateChatSessionCount, 1);
    }

    void actionFailureDoesNotReplayDecision_data()
    {
        QTest::addColumn<bool>("timeout");
        QTest::newRow("server-error") << false;
        QTest::newRow("poll-timeout") << true;
    }

    void actionFailureDoesNotReplayDecision()
    {
        using namespace Qt::StringLiterals;
        QFETCH(bool, timeout);
        ControllerFixture fixture;
        fixture.controller.selectTaskType(u"core:text2text:chat"_s);
        fixture.controller.selectChatConversation(42);
        auto generation = fixture.client->lastChatMessagesGeneration;
        fixture.client->deliverChatMessages(generation, humanMessageResponse(42));
        const auto actions = QJsonArray{QJsonObject{{u"name"_s, u"Create event"_s}, {u"args"_s, QJsonObject{}}}};
        fixture.client->deliverChatSessionCheck(generation, ocsResponse(QJsonObject{{u"sessionAgencyPendingActions"_s, actions}}));
        fixture.controller.answerPendingActions(true);
        generation = fixture.client->lastChatMessageCreateGeneration;
        fixture.client->deliverChatMessageCreated(generation, QJsonDocument{message(2, 42, u"human"_s, {})});
        if (timeout) {
            fixture.client->deliverChatSessionGenerationStarted(generation, QJsonDocument{QJsonObject{{u"taskId"_s, 9}}});
            fixture.client->replyToGenerationChecksAsPending = true;
            for (auto i = 0; i < 31; ++i) {
                QVERIFY(QMetaObject::invokeMethod(&fixture.controller, "pollChatGeneration", Qt::DirectConnection));
            }
        } else {
            fixture.client->deliverChatSessionGenerationStarted(generation, QJsonDocument{}, 500);
        }
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.thinking());
        QVERIFY(!fixture.controller.showRetryResponseGeneration());
        QVERIFY(!fixture.controller.error().isEmpty());
        QCOMPARE(fixture.client->generateChatSessionCount, 1);
        fixture.controller.loadData();
        generation = fixture.client->lastTaskTypesGeneration;
        fixture.client->deliverTaskTypes(generation, chatTaskTypesResponse());
        fixture.client->deliverChatConversations(generation, conversationsResponse({conversation(42, u"Conversation"_s)}));
        QCOMPARE(fixture.client->lastChatMessagesGeneration, generation);
        QCOMPARE(fixture.client->lastChatMessagesConversationId, 42);
        QVERIFY(fixture.controller.requestInProgress());
        fixture.client->deliverChatMessages(generation, humanMessageResponse(42));
        QCOMPARE(fixture.client->lastChatSessionCheckGeneration, generation);
        QCOMPARE(fixture.client->lastChatSessionCheckConversationId, 42);
        QVERIFY(fixture.controller.requestInProgress());
        fixture.client->deliverChatSessionCheck(generation, ocsResponse(QJsonObject{{u"sessionAgencyPendingActions"_s, actions}}));
        QCOMPARE(fixture.controller.pendingActions(), actions.toVariantList());
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(fixture.controller.error().isEmpty());
        QCOMPARE(fixture.client->createChatMessageCount, 1);
        QCOMPARE(fixture.client->generateChatSessionCount, 1);
        fixture.controller.submitQuestion(u"Change the title"_s);
        generation = fixture.client->lastChatMessageCreateGeneration;
        fixture.client->deliverChatMessageCreated(generation, QJsonDocument{message(3, 42, u"human"_s, u"Change the title"_s)});
        QCOMPARE(fixture.client->lastAgencyConfirm, -1);
        QVERIFY(fixture.controller.pendingActions().isEmpty());
    }

    void actionDecisionsStayHiddenAfterReload_data()
    {
        QTest::addColumn<bool>("confirm");
        QTest::addColumn<bool>("responseLost");
        QTest::newRow("confirm") << true << false;
        QTest::newRow("cancel") << false << false;
        QTest::newRow("confirm-response-lost") << true << true;
        QTest::newRow("cancel-response-lost") << false << true;
    }

    void actionDecisionsStayHiddenAfterReload()
    {
        using namespace Qt::StringLiterals;
        QFETCH(bool, confirm);
        QFETCH(bool, responseLost);
        ControllerFixture fixture;
        fixture.controller.selectTaskType(u"core:text2text:chat"_s);
        fixture.controller.selectChatConversation(42);
        auto generation = fixture.client->lastChatMessagesGeneration;
        const auto question = message(1, 42, u"human"_s, u"Create an event"_s);
        const auto proposal = message(2, 42, u"assistant"_s, u"Please confirm the event"_s);
        const auto decision = message(3, 42, u"human"_s, {});
        const auto result = message(4, 42, u"assistant"_s, confirm ? u"Event created"_s : u"Event cancelled"_s);
        const auto actions = QJsonArray{QJsonObject{{u"name"_s, u"Create event"_s}, {u"args"_s, QJsonObject{}}}};
        fixture.client->deliverChatMessages(generation, QJsonDocument{QJsonArray{question, proposal}});
        fixture.client->deliverChatSessionCheck(generation, ocsResponse(QJsonObject{{u"sessionAgencyPendingActions"_s, actions}}));

        fixture.controller.answerPendingActions(confirm);
        generation = fixture.client->lastChatMessageCreateGeneration;
        fixture.client->deliverChatMessageCreated(generation, QJsonDocument{decision});
        QCOMPARE(fixture.client->lastAgencyConfirm, confirm ? 1 : 0);
        QCOMPARE(fixture.controller.messages()->rowCount(), 2);
        fixture.client->deliverChatSessionGenerationStarted(generation, QJsonDocument{QJsonObject{{u"taskId"_s, 9}}});
        fixture.client->deliverChatGenerationCheck(generation, responseLost ? QJsonDocument{} : QJsonDocument{result}, responseLost ? 500 : 200);
        QCOMPARE(fixture.controller.messages()->rowCount(), responseLost ? 2 : 3);
        QCOMPARE(fixture.controller.error().isEmpty(), !responseLost);

        fixture.controller.loadData();
        generation = fixture.client->lastTaskTypesGeneration;
        fixture.client->deliverTaskTypes(generation, chatTaskTypesResponse());
        fixture.client->deliverChatConversations(generation, conversationsResponse({conversation(42, u"Conversation"_s)}));
        QCOMPARE(fixture.client->lastChatMessagesGeneration, generation);
        fixture.client->deliverChatMessages(generation, QJsonDocument{QJsonArray{question, proposal, decision, result}});
        QCOMPARE(fixture.client->lastChatSessionCheckGeneration, generation);
        fixture.client->deliverChatSessionCheck(generation, ocsResponse(QJsonObject{}));

        const auto messages = fixture.controller.messages();
        QCOMPARE(messages->rowCount(), 3);
        QCOMPARE(messages->data(messages->index(2, 0), AssistantMessageModel::MessageIdRole).toLongLong(), 4);
        QCOMPARE(messages->data(messages->index(2, 0), AssistantMessageModel::TextRole).toString(), result.value("content"_L1).toString());
        QVERIFY(fixture.controller.pendingActions().isEmpty());
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.thinking());
        QVERIFY(!fixture.controller.showRetryResponseGeneration());
        QVERIFY(fixture.controller.error().isEmpty());
        QCOMPARE(fixture.client->createChatMessageCount, 1);
        QCOMPARE(fixture.client->generateChatSessionCount, 1);
    }

    void chatLinksResolveAgainstAccountAndRejectNonWebSchemes()
    {
        using namespace Qt::StringLiterals;
        ControllerFixture fixture;
        fixture.account->setUrl(QUrl(u"https://cloud.example/nextcloud"_s));
        QCOMPARE(fixture.controller.chatLinkUrl(u"index.php/f/12"_s), QUrl(u"https://cloud.example/nextcloud/index.php/f/12"_s));
        QCOMPARE(fixture.controller.chatLinkUrl(u"/nextcloud/index.php/f/12"_s), QUrl(u"https://cloud.example/nextcloud/index.php/f/12"_s));
        QCOMPARE(fixture.controller.chatLinkUrl(u"https://example.org/document.pdf"_s), QUrl(u"https://example.org/document.pdf"_s));
        for (const auto &link : {u"file:///etc/passwd"_s, u"javascript:alert(1)"_s, u"https://user:password@example.org"_s, u""_s}) {
            QVERIFY(fixture.controller.chatLinkUrl(link).isEmpty());
        }
        QVERIFY(!fixture.controller.createAttachment({{u"fileId"_s, -1}}, &fixture.controller));
        QVERIFY(!fixture.controller.createAttachment({{u"fileId"_s, 12}}, nullptr));
    }

    void loadDataLoadsChatTypeAndWrappedConversations()
    {
        ControllerFixture fixture;

        fixture.controller.loadData();

        QCOMPARE(fixture.client->fetchTaskTypesCount, 1);
        QVERIFY(fixture.controller.requestInProgress());

        const auto requestGeneration = fixture.client->lastTaskTypesGeneration;
        fixture.client->deliverTaskTypes(requestGeneration, chatTaskTypesResponse());

        QCOMPARE(fixture.controller.taskTypes()->rowCount(), 1);
        QCOMPARE(fixture.controller.selectedTaskTypeId(), QStringLiteral("core:text2text:chat"));
        QCOMPARE(fixture.controller.selectedTaskTypeName(), QStringLiteral("Chat"));
        QCOMPARE(fixture.client->fetchChatConversationsCount, 1);

        const auto conversations = QJsonArray{
            QJsonObject{
                {QStringLiteral("id"), 42},
                {QStringLiteral("title"), QStringLiteral("Conversation")},
                {QStringLiteral("timestamp"), 1},
            },
        };
        fixture.client->deliverChatConversations(requestGeneration, ocsResponse(conversations));

        QCOMPARE(fixture.controller.chatConversations()->rowCount(), 1);
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(fixture.controller.error().isEmpty());
    }

    void loadDataRejectsMalformedTaskTypes()
    {
        ControllerFixture fixture;

        fixture.controller.loadData();
        fixture.client->deliverTaskTypes(fixture.client->lastTaskTypesGeneration, ocsResponse(QJsonObject{}));

        QCOMPARE(fixture.controller.taskTypes()->rowCount(), 0);
        QVERIFY(fixture.controller.selectedTaskTypeId().isEmpty());
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.error().isEmpty());
        QCOMPARE(fixture.client->fetchChatConversationsCount, 0);

        fixture.controller.submitQuestion(QStringLiteral("Question"));
        QCOMPARE(fixture.client->fetchTaskTypesCount, 2);
        fixture.client->deliverTaskTypes(fixture.client->lastTaskTypesGeneration, ocsResponse(QJsonObject{}));

        QVERIFY(fixture.controller.selectedTaskTypeId().isEmpty());
        QCOMPARE(fixture.client->createChatConversationCount, 0);
        QCOMPARE(fixture.client->scheduleTaskCount, 0);
        QVERIFY(!fixture.controller.requestInProgress());
    }

    void wrappedMessagesUpdateConversationState()
    {
        ControllerFixture fixture;

        fixture.controller.loadData();
        const auto loadGeneration = fixture.client->lastTaskTypesGeneration;
        fixture.client->deliverTaskTypes(loadGeneration, chatTaskTypesResponse());
        fixture.client->deliverChatConversations(loadGeneration, conversationsResponse({conversation(42, QStringLiteral("Original title"))}));

        fixture.controller.selectChatConversation(42);
        const auto requestGeneration = fixture.client->lastChatMessagesGeneration;
        fixture.client->deliverChatMessages(requestGeneration, humanMessageResponse(42, true));

        QCOMPARE(fixture.controller.messages()->rowCount(), 1);
        QCOMPARE(fixture.client->chatSessionCheckCount, 1);

        fixture.client->deliverChatSessionCheck(requestGeneration,
                                                ocsResponse(QJsonObject{{QStringLiteral("sessionTitle"), QStringLiteral("Renamed conversation")}}));

        QCOMPARE(fixture.controller.selectedChatConversationTitle(), QStringLiteral("Renamed conversation"));
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(fixture.controller.showRetryResponseGeneration());

        fixture.controller.startNewChat();
        fixture.controller.selectChatConversation(42);
        QCOMPARE(fixture.controller.selectedChatConversationTitle(), QStringLiteral("Renamed conversation"));
    }

    void reloadClearsConversationMissingFromServer()
    {
        ControllerFixture fixture;

        fixture.controller.loadData();
        const auto initialGeneration = fixture.client->lastTaskTypesGeneration;
        fixture.client->deliverTaskTypes(initialGeneration, chatTaskTypesResponse());
        fixture.client->deliverChatConversations(initialGeneration, conversationsResponse({conversation(42, QStringLiteral("Conversation"))}));
        fixture.controller.selectChatConversation(42);
        fixture.client->deliverChatMessages(fixture.client->lastChatMessagesGeneration, humanMessageResponse(42, true));
        fixture.client->deliverChatSessionCheck(fixture.client->lastChatSessionCheckGeneration, ocsResponse(QJsonObject{}));

        QCOMPARE(fixture.controller.messages()->rowCount(), 1);
        QCOMPARE(fixture.controller.selectedChatConversationId(), 42);

        fixture.controller.loadData();
        const auto reloadGeneration = fixture.client->lastTaskTypesGeneration;
        fixture.client->deliverTaskTypes(reloadGeneration, chatTaskTypesResponse());
        fixture.client->deliverChatConversations(reloadGeneration, conversationsResponse({}));

        QCOMPARE(fixture.controller.selectedChatConversationId(), -1);
        QVERIFY(fixture.controller.selectedChatConversationTitle().isEmpty());
        QCOMPARE(fixture.controller.messages()->rowCount(), 0);
        QVERIFY(!fixture.controller.showRetryResponseGeneration());
    }

    void selectingConversationClearsPreviousError()
    {
        ControllerFixture fixture;

        fixture.controller.selectChatConversation(41);
        fixture.client->deliverChatMessages(fixture.client->lastChatMessagesGeneration, {}, 500);
        QVERIFY(!fixture.controller.error().isEmpty());

        fixture.controller.selectChatConversation(42);

        QVERIFY(fixture.controller.error().isEmpty());
        QCOMPARE(fixture.client->lastChatMessagesConversationId, 42);
    }

    void wrappedResponsesCompleteNewChatWorkflow()
    {
        ControllerFixture fixture;

        fixture.controller.submitQuestion(QStringLiteral(" Question "));

        QCOMPARE(fixture.client->createChatConversationCount, 1);
        QCOMPARE(fixture.client->lastChatConversationTitle, QStringLiteral("Question"));
        const auto requestGeneration = fixture.client->lastChatConversationCreateGeneration;

        const auto session = QJsonObject{
            {QStringLiteral("id"), 42},
            {QStringLiteral("title"), QStringLiteral("Question")},
            {QStringLiteral("timestamp"), 1},
        };
        fixture.client->deliverChatConversationCreated(requestGeneration, ocsResponse(QJsonObject{{QStringLiteral("session"), session}}));

        QCOMPARE(fixture.controller.selectedChatConversationId(), 42);
        QCOMPARE(fixture.client->createChatMessageCount, 1);
        QCOMPARE(fixture.client->lastChatMessageSessionId, 42);
        QCOMPARE(fixture.client->lastChatMessageRole, QStringLiteral("human"));
        QCOMPARE(fixture.client->lastChatMessageContent, QStringLiteral("Question"));
        QVERIFY(fixture.client->lastChatMessageWasFirst);

        fixture.client->deliverChatMessageCreated(requestGeneration, ocsResponse(message(1, 42, QStringLiteral("human"), QStringLiteral("Question"))));

        QCOMPARE(fixture.controller.messages()->rowCount(), 1);
        QCOMPARE(fixture.client->generateChatSessionCount, 1);
        QCOMPARE(fixture.client->lastGeneratedConversationId, 42);

        fixture.client->deliverChatSessionGenerationStarted(requestGeneration, ocsResponse(QJsonObject{{QStringLiteral("taskId"), 99}}));
        QVERIFY(fixture.controller.requestInProgress());
        QVERIFY(fixture.controller.thinking());

        fixture.client->deliverChatGenerationCheck(requestGeneration, ocsResponse(message(2, 42, QStringLiteral("assistant"), QStringLiteral("Answer"))), 200);

        QCOMPARE(fixture.controller.messages()->rowCount(), 2);
        const auto answerIndex = fixture.controller.messages()->index(1, 0);
        QCOMPARE(fixture.controller.messages()->data(answerIndex, AssistantMessageModel::TextRole).toString(), QStringLiteral("Answer"));
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.thinking());
        QVERIFY(fixture.controller.error().isEmpty());
    }

    void nonChatTaskWorkflowSchedulesAndLoadsResult()
    {
        ControllerFixture fixture;
        const auto taskType = QStringLiteral("core:text2text:text_generation");

        fixture.controller.selectTaskType(taskType);
        QCOMPARE(fixture.client->fetchTasksCount, 1);
        fixture.client->deliverTasks(fixture.client->lastTasksGeneration, ocsResponse(QJsonObject{{QStringLiteral("tasks"), QJsonArray{}}}));

        fixture.controller.submitQuestion(QStringLiteral(" Summarize this "));
        QCOMPARE(fixture.client->scheduleTaskCount, 1);
        QCOMPARE(fixture.client->lastTaskInput, QStringLiteral("Summarize this"));
        QCOMPARE(fixture.client->lastTaskType, taskType);

        const auto requestGeneration = fixture.client->lastTaskScheduleGeneration;
        const auto scheduledTask = QJsonObject{{QStringLiteral("task"), QJsonObject{{QStringLiteral("id"), 7}}}};
        fixture.client->deliverTaskScheduled(requestGeneration, ocsResponse(scheduledTask));
        QCOMPARE(fixture.client->fetchTasksCount, 2);

        fixture.client->deliverTasks(requestGeneration, completedTaskResponse(7, taskType, QStringLiteral("Summarize this")));

        QCOMPARE(fixture.controller.tasks()->rowCount(), 1);
        const auto taskIndex = fixture.controller.tasks()->index(0, 0);
        QCOMPARE(fixture.controller.tasks()->data(taskIndex, AssistantTaskModel::OutputRole).toString(), QStringLiteral("Done"));
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(fixture.controller.error().isEmpty());
    }

    void clearRejectsLateMessageReply()
    {
        ControllerFixture fixture;
        fixture.controller.selectChatConversation(41);
        const auto clearedGeneration = fixture.client->lastChatMessagesGeneration;

        fixture.controller.clear();
        fixture.client->deliverChatMessages(clearedGeneration, humanMessageResponse(41));

        QCOMPARE(fixture.controller.messages()->rowCount(), 0);
        QCOMPARE(fixture.client->chatSessionCheckCount, 0);
        QCOMPARE(fixture.controller.selectedChatConversationId(), -1);
    }

    void newerConversationRejectsSupersededReply()
    {
        ControllerFixture fixture;
        fixture.controller.selectChatConversation(41);
        const auto supersededGeneration = fixture.client->lastChatMessagesGeneration;
        fixture.controller.selectChatConversation(42);
        const auto currentGeneration = fixture.client->lastChatMessagesGeneration;

        fixture.client->deliverChatMessages(supersededGeneration, humanMessageResponse(41));
        QCOMPARE(fixture.controller.messages()->rowCount(), 0);
        QCOMPARE(fixture.client->chatSessionCheckCount, 0);

        fixture.client->deliverChatMessages(currentGeneration, humanMessageResponse(42));
        QCOMPARE(fixture.controller.messages()->rowCount(), 1);
        QCOMPARE(fixture.client->chatSessionCheckCount, 1);
        QCOMPARE(fixture.client->lastChatSessionCheckConversationId, 42);
        QCOMPARE(fixture.client->lastChatSessionCheckGeneration, currentGeneration);
    }

    void chatGenerationPollingStopsAtLimit()
    {
        ControllerFixture fixture;
        fixture.controller.selectChatConversation(42);
        const auto requestGeneration = fixture.client->lastChatMessagesGeneration;
        fixture.client->deliverChatMessages(requestGeneration, humanMessageResponse(42));
        fixture.client->deliverChatSessionCheck(requestGeneration, QJsonDocument{QJsonObject{{QStringLiteral("messageTaskId"), 99}}});
        fixture.client->replyToGenerationChecksAsPending = true;

        for (auto attempt = 0; attempt < 31; ++attempt) {
            QVERIFY(QMetaObject::invokeMethod(&fixture.controller, "pollChatGeneration", Qt::DirectConnection));
        }

        QCOMPARE(fixture.client->chatGenerationCheckCount, 30);
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.thinking());
        QVERIFY(fixture.controller.showRetryResponseGeneration());
        QVERIFY(!fixture.controller.error().isEmpty());
    }

    void pendingChatGenerationKeepsRequestActive()
    {
        ControllerFixture fixture;
        fixture.controller.selectChatConversation(42);
        const auto requestGeneration = fixture.client->lastChatMessagesGeneration;
        fixture.client->deliverChatMessages(requestGeneration, humanMessageResponse(42));
        fixture.client->deliverChatSessionCheck(requestGeneration, QJsonDocument{QJsonObject{{QStringLiteral("messageTaskId"), 99}}});

        fixture.client->deliverChatGenerationCheck(requestGeneration, {}, 417);

        QVERIFY(fixture.controller.requestInProgress());
        QVERIFY(fixture.controller.thinking());
        QVERIFY(fixture.controller.error().isEmpty());
    }

    void unexpectedChatGenerationStatusFailsRequest()
    {
        ControllerFixture fixture;
        fixture.controller.selectChatConversation(42);
        const auto requestGeneration = fixture.client->lastChatMessagesGeneration;
        fixture.client->deliverChatMessages(requestGeneration, humanMessageResponse(42));
        fixture.client->deliverChatSessionCheck(requestGeneration, QJsonDocument{QJsonObject{{QStringLiteral("messageTaskId"), 99}}});

        fixture.client->deliverChatGenerationCheck(requestGeneration, {}, 418);

        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.thinking());
        QVERIFY(!fixture.controller.error().isEmpty());
    }

    void successfulGenerationRequiresAssistantMessage_data()
    {
        QTest::addColumn<QJsonDocument>("response");

        QTest::newRow("empty-object") << ocsResponse(QJsonObject{});
        QTest::newRow("human-message") << ocsResponse(message(2, 42, QStringLiteral("human"), QStringLiteral("Not an assistant response")));
        QTest::newRow("missing-content") << ocsResponse(QJsonObject{{QStringLiteral("id"), 2}, {QStringLiteral("role"), QStringLiteral("assistant")}});
    }

    void successfulGenerationRequiresAssistantMessage()
    {
        QFETCH(QJsonDocument, response);
        ControllerFixture fixture;
        fixture.controller.selectChatConversation(42);
        const auto requestGeneration = fixture.client->lastChatMessagesGeneration;
        fixture.client->deliverChatMessages(requestGeneration, humanMessageResponse(42));
        fixture.client->deliverChatSessionCheck(requestGeneration, ocsResponse(QJsonObject{{QStringLiteral("messageTaskId"), 99}}));

        fixture.client->deliverChatGenerationCheck(requestGeneration, response, 200);

        QCOMPARE(fixture.controller.messages()->rowCount(), 1);
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.thinking());
        QVERIFY(fixture.controller.showRetryResponseGeneration());
        QVERIFY(!fixture.controller.error().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestAssistantController)
#include "testassistantcontroller.moc"
