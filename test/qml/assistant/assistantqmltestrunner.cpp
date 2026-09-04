/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "assistant/assistantcontroller.h"
#include "assistant/fakeassistantclient.h"
#include "testhelper.h"

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

#include <memory>

namespace
{

QJsonDocument ocsResponse(const QJsonValue &data)
{
    return QJsonDocument{QJsonObject{
        {QStringLiteral("ocs"), QJsonObject{{QStringLiteral("data"), data}}},
    }};
}

QJsonDocument chatTaskTypesResponse()
{
    return ocsResponse(QJsonObject{
        {QStringLiteral("types"),
         QJsonObject{
             {QStringLiteral("core:text2text:chat"),
              QJsonObject{
                  {QStringLiteral("name"), QStringLiteral("Chat")},
                  {QStringLiteral("description"), QStringLiteral("A conversation")},
              }},
         }},
    });
}

QJsonObject conversation()
{
    return {
        {QStringLiteral("id"), 42},
        {QStringLiteral("title"), QStringLiteral("Conversation")},
        {QStringLiteral("timestamp"), 1},
    };
}

QJsonObject message()
{
    return {
        {QStringLiteral("id"), 1},
        {QStringLiteral("sessionId"), 42},
        {QStringLiteral("role"), QStringLiteral("human")},
        {QStringLiteral("content"), QStringLiteral("Question")},
        {QStringLiteral("timestamp"), 1},
    };
}

QJsonDocument completedTaskResponse()
{
    const auto task = QJsonObject{
        {QStringLiteral("id"), 7},
        {QStringLiteral("type"), QStringLiteral("core:text2text:summarize")},
        {QStringLiteral("appId"), QStringLiteral("assistant")},
        {QStringLiteral("input"), QJsonObject{{QStringLiteral("input"), QStringLiteral("Question")}}},
        {QStringLiteral("output"), QJsonObject{{QStringLiteral("output"), QStringLiteral("Answer")}}},
        {QStringLiteral("status"), QStringLiteral("STATUS_SUCCESSFUL")},
    };
    return ocsResponse(QJsonObject{{QStringLiteral("tasks"), QJsonArray{task}}});
}

}

class AssistantQmlTestSetup final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(OCC::AssistantController *controller READ controller NOTIFY controllerChanged)
    Q_PROPERTY(int scheduleTaskCount READ scheduleTaskCount NOTIFY clientStateChanged)
    Q_PROPERTY(int deleteTaskCount READ deleteTaskCount NOTIFY clientStateChanged)
    Q_PROPERTY(qint64 lastDeletedTaskId READ lastDeletedTaskId NOTIFY clientStateChanged)

public:
    AssistantQmlTestSetup()
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        Q_INIT_RESOURCE(assistant);

        qmlRegisterUncreatableType<QAbstractItemModel>("com.nextcloud.desktopclient", 1, 0, "QAbstractItemModel", "Owned by the Assistant controller");
        qmlRegisterUncreatableType<OCC::AssistantController>("com.nextcloud.desktopclient", 1, 0, "AssistantController", "Owned by the Assistant test setup");
        reset();
    }

    [[nodiscard]] OCC::AssistantController *controller() const
    {
        return _controller.get();
    }

    [[nodiscard]] int scheduleTaskCount() const
    {
        return _client->scheduleTaskCount;
    }

    [[nodiscard]] int deleteTaskCount() const
    {
        return _client->deleteTaskCount;
    }

    [[nodiscard]] qint64 lastDeletedTaskId() const
    {
        return _client->lastTaskId;
    }

    Q_INVOKABLE void reset()
    {
        _controller.reset();
        _accountState.reset();
        _account.reset();

        _account = OCC::Account::create();
        _account->setCapabilities({
            {QStringLiteral("assistant"),
             QVariantMap{
                 {QStringLiteral("enabled"), true},
                 {QStringLiteral("version"), QStringLiteral("1.0.9")},
             }},
        });
        _accountState.reset(new FakeAccountState(_account));
        _client = new FakeAssistantClient;
        _controller = std::make_unique<OCC::AssistantController>(_accountState, _client, nullptr);
        Q_EMIT controllerChanged();
        Q_EMIT clientStateChanged();
    }

    Q_INVOKABLE void completeChatLoad()
    {
        const auto requestGeneration = _client->lastTaskTypesGeneration;
        _client->deliverTaskTypes(requestGeneration, chatTaskTypesResponse());
        _client->deliverChatConversations(requestGeneration, ocsResponse(QJsonArray{conversation()}));
    }

    Q_INVOKABLE void completeEmptyTaskTypes()
    {
        _client->deliverTaskTypes(_client->lastTaskTypesGeneration, ocsResponse(QJsonObject{}));
    }

    Q_INVOKABLE void selectConversationAndCompleteMessages()
    {
        _controller->selectChatConversation(42);
        const auto requestGeneration = _client->lastChatMessagesGeneration;
        _client->deliverChatMessages(requestGeneration, ocsResponse(QJsonArray{message()}));
        _client->deliverChatSessionCheck(requestGeneration, ocsResponse(QJsonObject{}));
    }

    Q_INVOKABLE void seedTask()
    {
        _controller->selectTaskType(QStringLiteral("core:text2text:summarize"));
        _client->deliverTasks(_client->lastTasksGeneration, completedTaskResponse());
    }

public Q_SLOTS:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->addImportPath(QCoreApplication::applicationDirPath());
        engine->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
        engine->addImportPath(QStringLiteral("qrc:/qml/theme"));
        engine->rootContext()->setContextProperty(QStringLiteral("assistantTestSetup"), this);
    }

Q_SIGNALS:
    void controllerChanged();
    void clientStateChanged();

private:
    OCC::AccountPtr _account;
    OCC::AccountStatePtr _accountState;
    FakeAssistantClient *_client = nullptr;
    std::unique_ptr<OCC::AssistantController> _controller;
};

QUICK_TEST_MAIN_WITH_SETUP(assistant, AssistantQmlTestSetup)

#include "assistantqmltestrunner.moc"
