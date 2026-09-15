/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "assistant/assistantclient.h"

class FakeAssistantClient final : public OCC::AssistantClient
{
public:
    explicit FakeAssistantClient(QObject *parent = nullptr);

    void fetchTaskTypes(quint64 requestGeneration) override;
    void fetchTasks(const QString &taskType, quint64 requestGeneration) override;
    void scheduleTask(const QString &input, const QString &taskType, quint64 requestGeneration) override;
    void deleteTask(qint64 taskId, quint64 requestGeneration) override;
    void fetchChatConversations(quint64 requestGeneration) override;
    void fetchChatMessages(qint64 conversationId, quint64 requestGeneration) override;
    void createChatConversation(const QString &title, qint64 timestamp, quint64 requestGeneration) override;
    void createChatMessage(qint64 sessionId, const QString &role, const QString &content, qint64 timestamp, bool firstHumanMessage, quint64 requestGeneration)
        override;
    void generateChatSession(qint64 conversationId, quint64 requestGeneration) override;
    void checkChatGeneration(qint64 taskId, qint64 sessionId, quint64 requestGeneration) override;
    void checkChatSession(qint64 sessionId, quint64 requestGeneration) override;
    void cancelRequests() override;

    void deliverTaskTypes(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 100);
    void deliverTasks(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 100);
    void deliverTaskScheduled(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 100);
    void deliverTaskDeleted(quint64 requestGeneration, int statusCode = 100);
    void deliverChatConversations(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 200);
    void deliverChatMessages(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 200);
    void deliverChatConversationCreated(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 200);
    void deliverChatMessageCreated(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 200);
    void deliverChatSessionGenerationStarted(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 200);
    void deliverChatGenerationCheck(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void deliverChatSessionCheck(quint64 requestGeneration, const QJsonDocument &json, int statusCode = 200);

    quint64 lastTaskTypesGeneration = 0;
    quint64 lastTasksGeneration = 0;
    quint64 lastTaskScheduleGeneration = 0;
    quint64 lastTaskDeleteGeneration = 0;
    quint64 lastChatConversationsGeneration = 0;
    quint64 lastChatMessagesGeneration = 0;
    quint64 lastChatConversationCreateGeneration = 0;
    quint64 lastChatMessageCreateGeneration = 0;
    quint64 lastChatSessionGeneration = 0;
    quint64 lastChatSessionCheckGeneration = 0;
    QString lastTaskType;
    QString lastTaskInput;
    QString lastChatConversationTitle;
    QString lastChatMessageRole;
    QString lastChatMessageContent;
    qint64 lastTaskId = -1;
    qint64 lastChatMessageSessionId = -1;
    qint64 lastGeneratedConversationId = -1;
    qint64 lastChatMessagesConversationId = -1;
    qint64 lastChatSessionCheckConversationId = -1;
    int fetchTaskTypesCount = 0;
    int fetchTasksCount = 0;
    int scheduleTaskCount = 0;
    int deleteTaskCount = 0;
    int fetchChatConversationsCount = 0;
    int createChatConversationCount = 0;
    int createChatMessageCount = 0;
    int generateChatSessionCount = 0;
    int chatSessionCheckCount = 0;
    int chatGenerationCheckCount = 0;
    int cancelRequestsCount = 0;
    bool lastChatMessageWasFirst = false;
    bool replyToGenerationChecksAsPending = false;
};
