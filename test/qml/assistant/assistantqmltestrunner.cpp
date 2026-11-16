/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "assistant/assistantcontroller.h"
#include "assistant/assistantimageprovider.h"
#include "assistant/fakeassistantclient.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

#include <QBuffer>
#include <QImage>
#include <QUrlQuery>

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
    Q_PROPERTY(int deleteConversationCount READ deleteConversationCount NOTIFY clientStateChanged)
    Q_PROPERTY(qint64 lastDeletedConversationId READ lastDeletedConversationId NOTIFY clientStateChanged)

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

    [[nodiscard]] int deleteConversationCount() const
    {
        return _client->deleteChatConversationCount;
    }

    [[nodiscard]] qint64 lastDeletedConversationId() const
    {
        return _client->lastDeletedConversationId;
    }

    Q_INVOKABLE void completeConversationDeletion(bool success)
    {
        _client->deliverChatConversationDeleted(_client->lastChatConversationDeleteGeneration,
                                                _client->lastDeletedConversationId,
                                                success ? ocsResponse(QJsonArray{}) : QJsonDocument{},
                                                success ? 200 : 500);
        Q_EMIT clientStateChanged();
    }

    Q_INVOKABLE void reset()
    {
        _controller.reset();
        _accountState.reset();
        _account.reset();

        _account = OCC::Account::create();
        using namespace Qt::StringLiterals;
        _account->setUrl(QUrl(u"https://cloud.example"_s));
        const auto network = new FakeQNAM({});
        network->setOverride([network](auto operation, const auto &request, QIODevice *) -> QNetworkReply * {
            const auto path = request.url().path();
            if (path.endsWith("/file/14/info"_L1)) {
                return new FakeErrorReply(operation, request, network, 404);
            }
            if (path.endsWith("/info"_L1)) {
                const auto name = path.contains("/12/"_L1) ? u"Picture.png"_s : u"Archive.zip"_s;
                const auto json = QJsonDocument{QJsonObject{{u"ocs"_s,
                                                             QJsonObject{
                                                                 {u"meta"_s, QJsonObject{{u"statuscode"_s, 100}}},
                                                                 {u"data"_s, QJsonObject{{u"name"_s, name}}},
                                                             }}}};
                return new FakeJsonReply(operation, request, network, 200, json);
            }
            if (QUrlQuery(request.url()).queryItemValue(u"fileId"_s) == "12"_L1 || path.endsWith("/task/1221224/output-file/14/preview"_L1)) {
                auto image = QImage{QSize{2, 2}, QImage::Format_RGB32};
                image.fill(Qt::red);
                auto buffer = QBuffer{};
                buffer.open(QIODevice::WriteOnly);
                image.save(&buffer, "PNG");
                return new FakePayloadReply(operation, request, buffer.data(), network);
            }
            return new FakeErrorReply(operation, request, network, 404);
        });
        _account->setCredentials(new FakeCredentials{network});
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

    Q_INVOKABLE void seedPendingActions(bool malformed = false)
    {
        using namespace Qt::StringLiterals;
        selectConversationAndCompleteMessages();
        const auto action = malformed
            ? QJsonObject{}
            : QJsonObject{{u"name"_s, u"Schedule a calendar event"_s},
                          {u"args"_s, QJsonObject{{u"title"_s, u"Roadmap"_s}, {u"details"_s, QJsonObject{{u"notes"_s, u"Long notes\n"_s.repeated(100)}}}}},
                          {u"destructive"_s, false},
                          {u"impulse_radius"_s, u"group"_s}};
        _client->deliverChatSessionCheck(_client->lastChatMessagesGeneration, ocsResponse(QJsonObject{{u"sessionAgencyPendingActions"_s, QJsonArray{action}}}));
    }

    Q_INVOKABLE int pendingDecision() const
    {
        return _client->lastAgencyConfirm;
    }

    Q_INVOKABLE void completeActionReply()
    {
        auto reply = message();
        reply.insert(QStringLiteral("content"), QString());
        _client->deliverChatMessageCreated(_client->lastChatMessageCreateGeneration, ocsResponse(reply));
    }

    Q_INVOKABLE void seedLongConversation()
    {
        using namespace Qt::StringLiterals;
        auto session = conversation();
        session.insert(u"title"_s, u"Long conversation title\n"_s.repeated(40));
        _client->deliverChatConversations(_client->lastTaskTypesGeneration, ocsResponse(QJsonArray{session}));
        _controller->selectChatConversation(42);
        auto reply = message();
        reply.insert(u"role"_s, u"assistant"_s);
        reply.insert(u"content"_s, u"A long response.\n\n"_s.repeated(100));
        const auto generation = _client->lastChatMessagesGeneration;
        _client->deliverChatMessages(generation, ocsResponse(QJsonArray{reply}));
        _client->deliverChatSessionCheck(generation, ocsResponse(QJsonObject{}));
    }

    Q_INVOKABLE void seedAttachments()
    {
        using namespace Qt::StringLiterals;
        _controller->selectChatConversation(42);
        auto imageMessage = message();
        imageMessage.insert(u"role"_s, u"assistant"_s);
        imageMessage.insert(u"content"_s, u""_s);
        imageMessage.insert(u"attachments"_s, QJsonArray{QJsonObject{{u"file_id"_s, 12}, {u"type"_s, u"File"_s}}});
        auto fileMessage = imageMessage;
        fileMessage.insert(u"id"_s, 2);
        fileMessage.insert(u"attachments"_s, QJsonArray{QJsonObject{{u"file_id"_s, 13}, {u"type"_s, u"File"_s}}});
        const auto generation = _client->lastChatMessagesGeneration;
        _client->deliverChatMessages(generation, ocsResponse(QJsonArray{imageMessage, fileMessage}));
        _client->deliverChatSessionCheck(generation, ocsResponse(QJsonObject{}));
    }

    Q_INVOKABLE void seedTask()
    {
        _controller->selectTaskType(QStringLiteral("core:text2text:summarize"));
        _client->deliverTasks(_client->lastTasksGeneration, completedTaskResponse());
    }

public Q_SLOTS:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        using namespace Qt::StringLiterals;
        engine->addImageProvider("assistant-attachments"_L1, new OCC::AssistantImageProvider);
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
