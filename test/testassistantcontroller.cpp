/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "assistant/assistantcontroller.h"
#include "assistant/assistantmodule.h"
#include "assistant/fakeassistantclient.h"
#include "testhelper.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using namespace OCC;

namespace {

QJsonDocument humanMessageResponse(qint64 conversationId)
{
    return QJsonDocument{QJsonArray{
        QJsonObject{
            {QStringLiteral("id"), 1},
            {QStringLiteral("sessionId"), conversationId},
            {QStringLiteral("role"), QStringLiteral("human")},
            {QStringLiteral("content"), QStringLiteral("Question")},
            {QStringLiteral("timestamp"), 1},
        },
    }};
}

struct ControllerFixture
{
    ControllerFixture()
        : account(Account::create())
        , accountState(new FakeAccountState(account))
        , client(new FakeAssistantClient)
        , controller(accountState, client, nullptr)
    {
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

private slots:
    void assistantResourceIsRegistered()
    {
        AssistantModule::registerQmlTypes();

        QVERIFY(QFile::exists(QStringLiteral(":/qml/src/gui/assistant/qml/AssistantWindow.qml")));
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
        fixture.client->deliverChatSessionCheck(requestGeneration,
            QJsonDocument{QJsonObject{{QStringLiteral("messageTaskId"), 99}}});
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
};

QTEST_GUILESS_MAIN(TestAssistantController)
#include "testassistantcontroller.moc"
