/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "assistant/assistantcontroller.h"
#include "assistant/fakeassistantclient.h"
#include "testhelper.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtTest>

using namespace OCC;

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
    void assistantResourceIsRegistered()
    {
        Q_INIT_RESOURCE(assistant);

        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantWindow.qml")));
        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantConversationPicker.qml")));
        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantConversationDelegate.qml")));
        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantMessageDelegate.qml")));
        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantMessageList.qml")));
        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantTaskList.qml")));
        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantTaskDelegate.qml")));
        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantDeleteTaskDialog.qml")));
        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantTaskTypeSelector.qml")));
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
        QVERIFY(!fixture.controller.requestInProgress());
        QVERIFY(!fixture.controller.error().isEmpty());
        QCOMPARE(fixture.client->fetchChatConversationsCount, 0);
    }

    void wrappedMessagesUpdateConversationState()
    {
        ControllerFixture fixture;

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
};

QTEST_GUILESS_MAIN(TestAssistantController)
#include "testassistantcontroller.moc"
