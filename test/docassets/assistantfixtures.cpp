/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantfixtures.h"
#include "assistant/assistantcontroller.h"
#include "assistant/fakeassistantclient.h"
#include "capturescene.h"
#include "fixtureaccount.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace OCC::DocAssets
{
namespace
{
QJsonDocument ocs(const QJsonValue &data)
{
    return QJsonDocument(QJsonObject{{"ocs", QJsonObject{{"data", data}}}});
}
constexpr auto fixtureTimestamp = 1789639200;
}
bool prepareAssistant(CaptureScene &scene)
{
    prepareAccount(scene);
    auto *client = new FakeAssistantClient;
    auto *controller = new AssistantController(scene.accountState, client, &scene);
    scene.source = QUrl(QStringLiteral("qrc:/qml/src/gui/assistant/qml/AssistantWindow.qml"));
    scene.properties = {{QStringLiteral("assistantController"), QVariant::fromValue<QObject *>(controller)},
                        {QStringLiteral("accountName"), QStringLiteral("Alex Morgan")},
                        {QStringLiteral("accountServer"), QStringLiteral("cloud.example.com")},
                        {QStringLiteral("accountAvatar"), QStringLiteral("qrc:/client/theme/black/user.svg")}};
    scene.ready = [controller, client, supplied = false](QString *failure) mutable {
        if (!supplied && client->fetchTaskTypesCount > 0) {
            supplied = true;
            const auto types = QJsonObject{{"core:text2text:chat", QJsonObject{{"name", "Chat"}, {"description", "A conversation"}}}};
            client->deliverTaskTypes(client->lastTaskTypesGeneration, ocs(QJsonObject{{"types", types}}));
            client->deliverChatConversations(client->lastChatConversationsGeneration,
                                             ocs(QJsonArray{QJsonObject{{"id", 42}, {"title", "Project planning"}, {"timestamp", fixtureTimestamp}},
                                                            QJsonObject{{"id", 43}, {"title", "Team meeting notes"}, {"timestamp", fixtureTimestamp}}}));
            controller->selectChatConversation(42);
            client->deliverChatMessages(
                client->lastChatMessagesGeneration,
                ocs(QJsonArray{
                    QJsonObject{{"id", 1},
                                {"sessionId", 42},
                                {"role", "human"},
                                {"content", "What should we cover in our project review?"},
                                {"timestamp", fixtureTimestamp}},
                    QJsonObject{
                        {"id", 2},
                        {"sessionId", 42},
                        {"role", "assistant"},
                        {"content",
                         "Review the milestones, confirm responsibilities, and agree on the next steps. Share the updated plan with the team afterwards."},
                        {"timestamp", fixtureTimestamp + 1}}}));
            client->deliverChatSessionCheck(client->lastChatSessionCheckGeneration, ocs(QJsonObject{}));
        }
        if (!controller->error().isEmpty()) {
            *failure = controller->error();
        }
        return supplied && !controller->requestInProgress() && !controller->thinking() && controller->messages()->rowCount() == 2;
    };
    return true;
}
}
