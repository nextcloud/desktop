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
    ++fetchTaskTypesCount;
    lastTaskTypesGeneration = requestGeneration;
}

void FakeAssistantClient::fetchTasks(const QString &taskType, quint64 requestGeneration)
{
    ++fetchTasksCount;
    lastTaskType = taskType;
    lastTasksGeneration = requestGeneration;
}

void FakeAssistantClient::scheduleTask(const QString &input, const QString &taskType, quint64 requestGeneration)
{
    ++scheduleTaskCount;
    lastTaskInput = input;
    lastTaskType = taskType;
    lastTaskScheduleGeneration = requestGeneration;
}

void FakeAssistantClient::deleteTask(qint64 taskId, quint64 requestGeneration)
{
    ++deleteTaskCount;
    lastTaskId = taskId;
    lastTaskDeleteGeneration = requestGeneration;
}

void FakeAssistantClient::fetchChatConversations(quint64 requestGeneration)
{
    ++fetchChatConversationsCount;
    lastChatConversationsGeneration = requestGeneration;
}

void FakeAssistantClient::fetchChatMessages(qint64 conversationId, quint64 requestGeneration)
{
    lastChatMessagesConversationId = conversationId;
    lastChatMessagesGeneration = requestGeneration;
}

void FakeAssistantClient::createChatConversation(const QString &title, qint64 timestamp, quint64 requestGeneration)
{
    ++createChatConversationCount;
    lastChatConversationTitle = title;
    lastChatConversationCreateGeneration = requestGeneration;
    Q_UNUSED(timestamp)
}

void FakeAssistantClient::createChatMessage(qint64 sessionId,
                                            const QString &role,
                                            const QString &content,
                                            qint64 timestamp,
                                            bool firstHumanMessage,
                                            quint64 requestGeneration)
{
    ++createChatMessageCount;
    lastChatMessageSessionId = sessionId;
    lastChatMessageRole = role;
    lastChatMessageContent = content;
    lastChatMessageWasFirst = firstHumanMessage;
    lastChatMessageCreateGeneration = requestGeneration;
    Q_UNUSED(timestamp)
}

void FakeAssistantClient::generateChatSession(qint64 conversationId, quint64 requestGeneration)
{
    ++generateChatSessionCount;
    lastGeneratedConversationId = conversationId;
    lastChatSessionGeneration = requestGeneration;
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

void FakeAssistantClient::deliverTaskTypes(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT taskTypesFetched(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverTasks(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT tasksFetched(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverTaskScheduled(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT taskScheduled(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverTaskDeleted(quint64 requestGeneration, int statusCode)
{
    Q_EMIT taskDeleted(requestGeneration, statusCode);
}

void FakeAssistantClient::deliverChatConversations(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatConversationsFetched(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverChatMessages(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatMessagesFetched(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverChatConversationCreated(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatConversationCreated(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverChatMessageCreated(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatMessageCreated(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverChatSessionGenerationStarted(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatSessionGenerationStarted(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverChatGenerationCheck(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatGenerationChecked(requestGeneration, json, statusCode);
}

void FakeAssistantClient::deliverChatSessionCheck(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    Q_EMIT chatSessionChecked(requestGeneration, json, statusCode);
}
