/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"

#include <QJsonDocument>
#include <QObject>
#include <QPointer>

namespace OCC {

class JsonApiJob;
class OcsAssistantConnector;

/** @brief Sends Assistant task-processing and chat requests for one account. */
class AssistantClient final : public QObject
{
    Q_OBJECT

public:
    /** @brief Creates a client for the given account. */
    explicit AssistantClient(AccountPtr account, QObject *parent = nullptr);

    /** @brief Requests the available task-processing types. */
    void fetchTaskTypes();
    /** @brief Requests tasks matching a task-processing type. */
    void fetchTasks(const QString &taskType);
    /** @brief Schedules a text input for a task-processing type. */
    void scheduleTask(const QString &input, const QString &taskType);
    /** @brief Deletes a task-processing task. */
    void deleteTask(qint64 taskId);

    /** @brief Requests the account's Assistant chat conversations. */
    void fetchChatConversations();
    /** @brief Requests the messages of a chat conversation. */
    void fetchChatMessages(qint64 conversationId);
    /** @brief Creates a chat conversation. */
    void createChatConversation(const QString &title, qint64 timestamp);
    /** @brief Adds a message to a chat conversation. */
    void createChatMessage(qint64 sessionId, const QString &role, const QString &content, qint64 timestamp, bool firstHumanMessage);
    /** @brief Starts response generation for a chat conversation. */
    void generateChatSession(qint64 conversationId);
    /** @brief Checks a running chat response-generation task. */
    void checkChatGeneration(qint64 taskId, qint64 sessionId);
    /** @brief Requests the current generation state of a chat conversation. */
    void checkChatSession(qint64 sessionId);

signals:
    void taskTypesFetched(const QJsonDocument &json, int statusCode);
    void tasksFetched(const QJsonDocument &json, int statusCode);
    void taskScheduled(const QJsonDocument &json, int statusCode);
    void taskDeleted(int statusCode);
    void chatConversationsFetched(const QJsonDocument &json, int statusCode);
    void chatMessagesFetched(const QJsonDocument &json, int statusCode);
    void chatConversationCreated(const QJsonDocument &json, int statusCode);
    void chatMessageCreated(const QJsonDocument &json, int statusCode);
    void chatSessionGenerationStarted(const QJsonDocument &json, int statusCode);
    void chatGenerationChecked(const QJsonDocument &json, int statusCode);
    void chatSessionChecked(const QJsonDocument &json, int statusCode);

private:
    AccountPtr _account;
    OcsAssistantConnector *_taskConnector = nullptr;
    QPointer<JsonApiJob> _chatConversationsJob;
    QPointer<JsonApiJob> _chatMessagesJob;
    QPointer<JsonApiJob> _createChatConversationJob;
    QPointer<JsonApiJob> _createChatMessageJob;
    QPointer<JsonApiJob> _generateChatSessionJob;
    QPointer<JsonApiJob> _checkChatGenerationJob;
    QPointer<JsonApiJob> _checkChatSessionJob;

    Q_DISABLE_COPY_MOVE(AssistantClient)
};

}
