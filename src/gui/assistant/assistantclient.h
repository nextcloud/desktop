/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"

#include <QJsonDocument>
#include <QObject>
#include <QPointer>

namespace OCC
{

class JsonApiJob;
class OcsAssistantConnector;

/** @brief Sends Assistant task-processing and chat requests for one account. */
class AssistantClient : public QObject
{
    Q_OBJECT

public:
    /** @brief Creates a client for the given account. */
    explicit AssistantClient(AccountPtr account, QObject *parent = nullptr);

    /** @brief Requests the available task-processing types. */
    virtual void fetchTaskTypes(quint64 requestGeneration);
    /** @brief Requests tasks matching a task-processing type. */
    virtual void fetchTasks(const QString &taskType, quint64 requestGeneration);
    /** @brief Schedules a text input for a task-processing type. */
    virtual void scheduleTask(const QString &input, const QString &taskType, quint64 requestGeneration);
    /** @brief Deletes a task-processing task. */
    virtual void deleteTask(qint64 taskId, quint64 requestGeneration);

    /** @brief Requests the account's Assistant chat conversations. */
    virtual void fetchChatConversations(quint64 requestGeneration);
    /** @brief Requests the messages of a chat conversation. */
    virtual void fetchChatMessages(qint64 conversationId, quint64 requestGeneration);
    /** @brief Creates a chat conversation. */
    virtual void createChatConversation(const QString &title, qint64 timestamp, quint64 requestGeneration);
    /** @brief Adds a message to a chat conversation. */
    virtual void
    createChatMessage(qint64 sessionId, const QString &role, const QString &content, qint64 timestamp, bool firstHumanMessage, quint64 requestGeneration);
    /** @brief Starts response generation for a chat conversation. */
    virtual void generateChatSession(qint64 conversationId, quint64 requestGeneration);
    /** @brief Checks a running chat response-generation task. */
    virtual void checkChatGeneration(qint64 taskId, qint64 sessionId, quint64 requestGeneration);
    /** @brief Requests the current generation state of a chat conversation. */
    virtual void checkChatSession(qint64 sessionId, quint64 requestGeneration);
    /** @brief Cancels all active requests and suppresses their replies. */
    virtual void cancelRequests();

Q_SIGNALS:
    void taskTypesFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void tasksFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void taskScheduled(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void taskDeleted(quint64 requestGeneration, int statusCode);
    void chatConversationsFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void chatMessagesFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void chatConversationCreated(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void chatMessageCreated(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void chatSessionGenerationStarted(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void chatGenerationChecked(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void chatSessionChecked(quint64 requestGeneration, const QJsonDocument &json, int statusCode);

protected:
    /** @brief Creates a request-free client for a test double. */
    explicit AssistantClient(QObject *parent = nullptr);

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
