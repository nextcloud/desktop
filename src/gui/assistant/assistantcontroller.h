/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountstate.h"
#include "assistantconversationmodel.h"
#include "assistantmessagemodel.h"
#include "assistanttaskmodel.h"
#include "assistanttasktypemodel.h"

#include <QJsonDocument>
#include <QObject>
#include <QTimer>

namespace OCC {

class AssistantClient;

/** @brief Owns the Assistant state and workflows exposed to QML for one account. */
class AssistantController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AssistantTaskTypeModel *taskTypes READ taskTypes CONSTANT)
    Q_PROPERTY(AssistantTaskModel *tasks READ tasks CONSTANT)
    Q_PROPERTY(AssistantConversationModel *chatConversations READ chatConversations CONSTANT)
    Q_PROPERTY(AssistantMessageModel *messages READ messages CONSTANT)
    Q_PROPERTY(QString question READ question NOTIFY questionChanged)
    Q_PROPERTY(QString response READ response NOTIFY responseChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool requestInProgress READ requestInProgress NOTIFY requestInProgressChanged)
    Q_PROPERTY(bool assistantEnabled READ assistantEnabled NOTIFY assistantEnabledChanged)
    Q_PROPERTY(bool accountConnected READ accountConnected NOTIFY accountConnectedChanged)
    Q_PROPERTY(QString selectedTaskTypeId READ selectedTaskTypeId NOTIFY selectedTaskTypeChanged)
    Q_PROPERTY(QString selectedTaskTypeName READ selectedTaskTypeName NOTIFY selectedTaskTypeChanged)
    Q_PROPERTY(QString selectedTaskTypeDescription READ selectedTaskTypeDescription NOTIFY selectedTaskTypeChanged)
    Q_PROPERTY(bool selectedTaskTypeIsChat READ selectedTaskTypeIsChat NOTIFY selectedTaskTypeChanged)
    Q_PROPERTY(qint64 selectedChatConversationId READ selectedChatConversationId NOTIFY selectedChatConversationChanged)
    Q_PROPERTY(QString selectedChatConversationTitle READ selectedChatConversationTitle NOTIFY selectedChatConversationChanged)
    Q_PROPERTY(bool thinking READ thinking NOTIFY thinkingChanged)
    Q_PROPERTY(bool showRetryResponseGeneration READ showRetryResponseGeneration NOTIFY showRetryResponseGenerationChanged)

public:
    /** @brief Creates a controller for the given account. */
    explicit AssistantController(AccountStatePtr accountState, QObject *parent = nullptr);

    /** @brief Returns the available task-type model. */
    [[nodiscard]] AssistantTaskTypeModel *taskTypes();
    /** @brief Returns the task model for the selected type. */
    [[nodiscard]] AssistantTaskModel *tasks();
    /** @brief Returns the chat-conversation model. */
    [[nodiscard]] AssistantConversationModel *chatConversations();
    /** @brief Returns the selected conversation's message model. */
    [[nodiscard]] AssistantMessageModel *messages();
    /** @brief Returns the most recently submitted input. */
    [[nodiscard]] QString question() const;
    /** @brief Returns transient request status text. */
    [[nodiscard]] QString response() const;
    /** @brief Returns the last user-visible request error. */
    [[nodiscard]] QString error() const;
    /** @brief Returns whether an Assistant request is active. */
    [[nodiscard]] bool requestInProgress() const;
    /** @brief Returns whether the account advertises Assistant support. */
    [[nodiscard]] bool assistantEnabled() const;
    /** @brief Returns whether the account is connected. */
    [[nodiscard]] bool accountConnected() const;
    /** @brief Returns the selected task type identifier. */
    [[nodiscard]] QString selectedTaskTypeId() const;
    /** @brief Returns the selected task type name. */
    [[nodiscard]] QString selectedTaskTypeName() const;
    /** @brief Returns the selected task type description. */
    [[nodiscard]] QString selectedTaskTypeDescription() const;
    /** @brief Returns whether the selected type uses the chat workflow. */
    [[nodiscard]] bool selectedTaskTypeIsChat() const;
    /** @brief Returns the selected chat conversation identifier. */
    [[nodiscard]] qint64 selectedChatConversationId() const;
    /** @brief Returns the selected chat conversation title. */
    [[nodiscard]] QString selectedChatConversationTitle() const;
    /** @brief Returns whether a chat response is being generated. */
    [[nodiscard]] bool thinking() const;
    /** @brief Returns whether retrying chat response generation is available. */
    [[nodiscard]] bool showRetryResponseGeneration() const;

    /** @brief Loads task types and data for the selected type. */
    Q_INVOKABLE void loadData();
    /** @brief Selects a task type and loads its data. */
    Q_INVOKABLE void selectTaskType(const QString &taskTypeId);
    /** @brief Reloads tasks for the selected non-chat type. */
    Q_INVOKABLE void refreshTasks();
    /** @brief Deletes a task and reloads the task list. */
    Q_INVOKABLE void deleteTask(qint64 taskId);
    /** @brief Schedules a new task using an existing task's input. */
    Q_INVOKABLE void retryTask(qint64 taskId);
    /** @brief Selects a chat conversation and loads its messages. */
    Q_INVOKABLE void selectChatConversation(qint64 conversationId);
    /** @brief Clears the selected conversation for a new chat. */
    Q_INVOKABLE void startNewChat();
    /** @brief Restarts response generation for the selected conversation. */
    Q_INVOKABLE void retryResponseGeneration();
    /** @brief Submits text to the selected task or chat workflow. */
    Q_INVOKABLE void submitQuestion(const QString &question);
    /** @brief Clears transient Assistant and conversation state. */
    Q_INVOKABLE void clear();

signals:
    void questionChanged();
    void responseChanged();
    void errorChanged();
    void requestInProgressChanged();
    void assistantEnabledChanged();
    void accountConnectedChanged();
    void selectedTaskTypeChanged();
    void selectedChatConversationChanged();
    void thinkingChanged();
    void showRetryResponseGenerationChanged();

private slots:
    void pollTasks();
    void slotTaskTypesFetched(const QJsonDocument &json, int statusCode);
    void slotTasksFetched(const QJsonDocument &json, int statusCode);
    void slotTaskScheduled(const QJsonDocument &json, int statusCode);
    void slotTaskDeleted(int statusCode);
    void slotChatConversationsFetched(const QJsonDocument &json, int statusCode);
    void slotChatMessagesFetched(const QJsonDocument &json, int statusCode);
    void slotChatConversationCreated(const QJsonDocument &json, int statusCode);
    void slotChatMessageCreated(const QJsonDocument &json, int statusCode);
    void slotChatSessionGenerationStarted(const QJsonDocument &json, int statusCode);
    void slotChatGenerationChecked(const QJsonDocument &json, int statusCode);
    void slotChatSessionChecked(const QJsonDocument &json, int statusCode);

private:
    void submitChatMessage(const QString &message);
    void scheduleSelectedTask(const QString &input);
    void setRequestInProgress(bool inProgress);
    void setThinking(bool thinking);
    void setShowRetryResponseGeneration(bool show);
    void updateSelectedTypeMetadata();
    void loadChatConversations();
    void loadChatMessages(qint64 conversationId);
    void startChatGeneration(qint64 conversationId);
    void requestFailed(const QString &context, int statusCode);

    AccountStatePtr _accountState;
    AccountPtr _account;
    AssistantClient *_client = nullptr;
    AssistantTaskTypeModel _taskTypes;
    AssistantTaskModel _tasks;
    AssistantConversationModel _chatConversations;
    AssistantMessageModel _messages;
    QTimer _taskPollTimer;
    QTimer _chatPollTimer;
    int _taskPollAttempts = 0;
    int _maxTaskPollAttempts = 60;
    qint64 _taskId = -1;
    QString _taskType;
    QString _taskTypeName;
    QString _taskTypeDescription;
    QString _question;
    QString _pendingQuestion;
    QString _response;
    QString _error;
    qint64 _selectedChatConversationId = -1;
    QString _selectedChatConversationTitle;
    qint64 _chatMessageTaskId = -1;
    QString _pendingChatMessage;
    bool _requestInProgress = false;
    bool _thinking = false;
    bool _showRetryResponseGeneration = false;

    Q_DISABLE_COPY_MOVE(AssistantController)
};

}
