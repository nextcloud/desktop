/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantclient.h"

#include "networkjobs.h"
#include "ocsassistantconnector.h"

#include <QJsonObject>
#include <QUrlQuery>

#include <utility>

using namespace Qt::StringLiterals;

namespace OCC {

AssistantClient::AssistantClient(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(std::move(account))
    , _taskConnector(new OcsAssistantConnector(_account, this))
{
    connect(_taskConnector, &OcsAssistantConnector::taskTypesFetched, this, &AssistantClient::taskTypesFetched);
    connect(_taskConnector, &OcsAssistantConnector::tasksFetched, this, &AssistantClient::tasksFetched);
    connect(_taskConnector, &OcsAssistantConnector::taskScheduled, this, &AssistantClient::taskScheduled);
    connect(_taskConnector, &OcsAssistantConnector::taskDeleted, this, &AssistantClient::taskDeleted);
}

void AssistantClient::fetchTaskTypes()
{
    _taskConnector->fetchTaskTypes();
}

void AssistantClient::fetchTasks(const QString &taskType)
{
    _taskConnector->fetchTasks(taskType);
}

void AssistantClient::scheduleTask(const QString &input, const QString &taskType)
{
    _taskConnector->scheduleTask(input, taskType, {});
}

void AssistantClient::deleteTask(qint64 taskId)
{
    _taskConnector->deleteTask(taskId);
}

void AssistantClient::fetchChatConversations()
{
    if (_chatConversationsJob) {
        return;
    }

    _chatConversationsJob = new JsonApiJob(_account, u"/ocs/v2.php/apps/assistant/chat/sessions"_s, this);
    connect(_chatConversationsJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        _chatConversationsJob = nullptr;
        emit chatConversationsFetched(json, statusCode);
    });
    _chatConversationsJob->start();
}

void AssistantClient::fetchChatMessages(qint64 conversationId)
{
    if (_chatMessagesJob) {
        return;
    }

    _chatMessagesJob = new JsonApiJob(_account, u"/ocs/v2.php/apps/assistant/chat/messages"_s, this);
    auto params = QUrlQuery{};
    params.addQueryItem(QStringLiteral("sessionId"), QString::number(conversationId));
    _chatMessagesJob->addQueryParams(params);
    connect(_chatMessagesJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        _chatMessagesJob = nullptr;
        emit chatMessagesFetched(json, statusCode);
    });
    _chatMessagesJob->start();
}

void AssistantClient::createChatConversation(const QString &title, qint64 timestamp)
{
    if (_createChatConversationJob) {
        return;
    }

    _createChatConversationJob = new JsonApiJob(_account, u"/ocs/v2.php/apps/assistant/chat/new_session"_s, this);
    _createChatConversationJob->setVerb(SimpleApiJob::Verb::Put);
    auto body = QJsonObject{
        {QStringLiteral("timestamp"), static_cast<double>(timestamp)},
    };
    if (!title.isEmpty()) {
        body.insert(QStringLiteral("title"), title);
    }
    _createChatConversationJob->setBody(QJsonDocument(body));
    connect(_createChatConversationJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        _createChatConversationJob = nullptr;
        emit chatConversationCreated(json, statusCode);
    });
    _createChatConversationJob->start();
}

void AssistantClient::createChatMessage(qint64 sessionId, const QString &role, const QString &content, qint64 timestamp, bool firstHumanMessage)
{
    if (_createChatMessageJob) {
        return;
    }

    _createChatMessageJob = new JsonApiJob(_account, u"/ocs/v2.php/apps/assistant/chat/new_message"_s, this);
    _createChatMessageJob->setVerb(SimpleApiJob::Verb::Put);
    const auto body = QJsonObject{
        {QStringLiteral("sessionId"), static_cast<double>(sessionId)},
        {QStringLiteral("role"), role},
        {QStringLiteral("content"), content},
        {QStringLiteral("timestamp"), static_cast<double>(timestamp)},
        {QStringLiteral("firstHumanMessage"), firstHumanMessage},
    };
    _createChatMessageJob->setBody(QJsonDocument(body));
    connect(_createChatMessageJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        _createChatMessageJob = nullptr;
        emit chatMessageCreated(json, statusCode);
    });
    _createChatMessageJob->start();
}

void AssistantClient::generateChatSession(qint64 conversationId)
{
    if (_generateChatSessionJob) {
        return;
    }

    _generateChatSessionJob = new JsonApiJob(_account, u"/ocs/v2.php/apps/assistant/chat/generate"_s, this);
    auto params = QUrlQuery{};
    params.addQueryItem(QStringLiteral("sessionId"), QString::number(conversationId));
    _generateChatSessionJob->addQueryParams(params);
    connect(_generateChatSessionJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        _generateChatSessionJob = nullptr;
        emit chatSessionGenerationStarted(json, statusCode);
    });
    _generateChatSessionJob->start();
}

void AssistantClient::checkChatGeneration(qint64 taskId, qint64 sessionId)
{
    if (_checkChatGenerationJob) {
        return;
    }

    _checkChatGenerationJob = new JsonApiJob(_account, u"/ocs/v2.php/apps/assistant/chat/check_generation"_s, this);
    auto params = QUrlQuery{};
    params.addQueryItem(QStringLiteral("taskId"), QString::number(taskId));
    params.addQueryItem(QStringLiteral("sessionId"), QString::number(sessionId));
    _checkChatGenerationJob->addQueryParams(params);
    connect(_checkChatGenerationJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        _checkChatGenerationJob = nullptr;
        emit chatGenerationChecked(json, statusCode);
    });
    _checkChatGenerationJob->start();
}

void AssistantClient::checkChatSession(qint64 sessionId)
{
    if (_checkChatSessionJob) {
        return;
    }

    _checkChatSessionJob = new JsonApiJob(_account, u"/ocs/v2.php/apps/assistant/chat/check_session"_s, this);
    auto params = QUrlQuery{};
    params.addQueryItem(QStringLiteral("sessionId"), QString::number(sessionId));
    _checkChatSessionJob->addQueryParams(params);
    connect(_checkChatSessionJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        _checkChatSessionJob = nullptr;
        emit chatSessionChecked(json, statusCode);
    });
    _checkChatSessionJob->start();
}

}
