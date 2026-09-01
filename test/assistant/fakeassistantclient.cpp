/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fakeassistantclient.h"

FakeAssistantClient::FakeAssistantClient(QObject *parent)
    : AssistantClient(parent)
{
}

void FakeAssistantClient::fetchTaskTypes(quint64 requestGeneration)
{
    Q_UNUSED(requestGeneration)
}

void FakeAssistantClient::fetchTasks(const QString &taskType, quint64 requestGeneration)
{
    Q_UNUSED(taskType)
    Q_UNUSED(requestGeneration)
}

void FakeAssistantClient::scheduleTask(const QString &input, const QString &taskType, quint64 requestGeneration)
{
    Q_UNUSED(input)
    Q_UNUSED(taskType)
    Q_UNUSED(requestGeneration)
}

void FakeAssistantClient::deleteTask(qint64 taskId, quint64 requestGeneration)
{
    Q_UNUSED(taskId)
    Q_UNUSED(requestGeneration)
}

void FakeAssistantClient::fetchChatConversations(quint64 requestGeneration)
{
    Q_UNUSED(requestGeneration)
}

void FakeAssistantClient::fetchChatMessages(qint64 conversationId, quint64 requestGeneration)
{
    lastChatMessagesConversationId = conversationId;
    lastChatMessagesGeneration = requestGeneration;
}

void FakeAssistantClient::createChatConversation(const QString &title, qint64 timestamp, quint64 requestGeneration)
{
    Q_UNUSED(title)
    Q_UNUSED(timestamp)
    Q_UNUSED(requestGeneration)
}

void FakeAssistantClient::createChatMessage(qint64 sessionId,
                                            const QString &role,
                                            const QString &content,
                                            qint64 timestamp,
                                            bool firstHumanMessage,
                                            quint64 requestGeneration)
{
    Q_UNUSED(sessionId)
    Q_UNUSED(role)
    Q_UNUSED(content)
    Q_UNUSED(timestamp)
    Q_UNUSED(firstHumanMessage)
    Q_UNUSED(requestGeneration)
}

void FakeAssistantClient::generateChatSession(qint64 conversationId, quint64 requestGeneration)
{
    Q_UNUSED(conversationId)
    Q_UNUSED(requestGeneration)
}

void FakeAssistantClient::checkChatGeneration(qint64 taskId, qint64 sessionId, quint64 requestGeneration)
{
    Q_UNUSED(taskId)
    Q_UNUSED(sessionId)
    ++chatGenerationCheckCount;
    if (replyToGenerationChecksAsPending) {
        Q_EMIT chatGenerationChecked(requestGeneration, {}, 417);
    }
}

void FakeAssistantClient::checkChatSession(qint64 sessionId, quint64 requestGeneration)
{
    ++chatSessionCheckCount;
    lastChatSessionCheckConversationId = sessionId;
    lastChatSessionCheckGeneration = requestGeneration;
}

void FakeAssistantClient::cancelRequests()
{
    ++cancelRequestsCount;
}

void FakeAssistantClient::deliverChatMessages(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatMessagesFetched(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverChatGenerationCheck(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatGenerationChecked(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverChatSessionCheck(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatSessionChecked(requestGeneration, json, statusCode);
}
