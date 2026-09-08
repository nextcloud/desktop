/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantcontroller.h"

#include "account.h"
#include "assistantclient.h"
#include "assistantutils.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>

#include <chrono>
#include <utility>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

namespace OCC
{

namespace
{

Q_LOGGING_CATEGORY(lcAssistantController, "nextcloud.gui.assistant", QtInfoMsg)

constexpr auto assistantControllerChatTaskTypeId = "core:text2text:chat"_L1;
constexpr auto successMinStatusCode = 200;
constexpr auto successMaxStatusCode = 300;
constexpr auto chatGenerationPendingStatusCode = 417;

enum class ChatReplyStatus {
    Success,
    Pending,
    Failure,
};

bool statusSuccess(int statusCode)
{
    return statusCode == 100 || (statusCode >= successMinStatusCode && statusCode < successMaxStatusCode);
}

ChatReplyStatus chatReplyStatus(const QJsonDocument &json, int statusCode)
{
    if (statusCode == chatGenerationPendingStatusCode) {
        return ChatReplyStatus::Pending;
    }
    if (statusSuccess(statusCode) || (statusCode == 0 && !json.isNull())) {
        return ChatReplyStatus::Success;
    }
    return ChatReplyStatus::Failure;
}

qint64 taskIdFromSchedule(const QJsonDocument &json)
{
    const auto task = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject().value("task"_L1).toObject();
    return AssistantUtils::jsonInteger(task.value("id"_L1));
}

QJsonDocument chatDataFromResponse(const QJsonDocument &json)
{
    if (!json.isObject()) {
        return json;
    }

    const auto data = json.object().value("ocs"_L1).toObject().value("data"_L1);
    if (data.isArray()) {
        return QJsonDocument{data.toArray()};
    }
    if (data.isObject()) {
        return QJsonDocument{data.toObject()};
    }
    return json;
}
}

AssistantController::AssistantController(const AccountStatePtr &accountState, QObject *parent)
    : AssistantController(accountState, nullptr, parent)
{
}

AssistantController::AssistantController(const AccountStatePtr &accountState, AssistantClient *client, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
    , _account(_accountState ? _accountState->account() : AccountPtr{})
    , _client(client)
{
    Q_ASSERT(_accountState);
    Q_ASSERT(_account);

    if (!_client) {
        _client = new AssistantClient(_account, this);
    } else {
        _client->setParent(this);
    }

    _taskType = assistantControllerChatTaskTypeId;

    connect(_accountState.data(), &AccountState::isConnectedChanged, this, &AssistantController::accountConnectedChanged);
    connect(_account.data(), &Account::capabilitiesChanged, this, &AssistantController::assistantEnabledChanged);
    connect(_client, &AssistantClient::taskTypesFetched, this, &AssistantController::slotTaskTypesFetched);
    connect(_client, &AssistantClient::tasksFetched, this, &AssistantController::slotTasksFetched);
    connect(_client, &AssistantClient::taskScheduled, this, &AssistantController::slotTaskScheduled);
    connect(_client, &AssistantClient::taskDeleted, this, &AssistantController::slotTaskDeleted);
    connect(_client, &AssistantClient::chatConversationsFetched, this, &AssistantController::slotChatConversationsFetched);
    connect(_client, &AssistantClient::chatMessagesFetched, this, &AssistantController::slotChatMessagesFetched);
    connect(_client, &AssistantClient::chatConversationCreated, this, &AssistantController::slotChatConversationCreated);
    connect(_client, &AssistantClient::chatMessageCreated, this, &AssistantController::slotChatMessageCreated);
    connect(_client, &AssistantClient::chatSessionGenerationStarted, this, &AssistantController::slotChatSessionGenerationStarted);
    connect(_client, &AssistantClient::chatGenerationChecked, this, &AssistantController::slotChatGenerationChecked);
    connect(_client, &AssistantClient::chatSessionChecked, this, &AssistantController::slotChatSessionChecked);

    _taskPollTimer.setInterval(2s);
    _taskPollTimer.setSingleShot(false);
    connect(&_taskPollTimer, &QTimer::timeout, this, &AssistantController::pollTasks);

    _chatPollTimer.setInterval(4s);
    _chatPollTimer.setSingleShot(false);
    connect(&_chatPollTimer, &QTimer::timeout, this, &AssistantController::pollChatGeneration);
}

QAbstractItemModel *AssistantController::taskTypes()
{
    return &_taskTypes;
}

QAbstractItemModel *AssistantController::tasks()
{
    return &_tasks;
}

QAbstractItemModel *AssistantController::chatConversations()
{
    return &_chatConversations;
}

QAbstractItemModel *AssistantController::messages()
{
    return &_messages;
}

QString AssistantController::question() const
{
    return _question;
}

QString AssistantController::response() const
{
    return _response;
}

QString AssistantController::error() const
{
    return _error;
}

bool AssistantController::requestInProgress() const
{
    return _requestInProgress;
}

bool AssistantController::assistantEnabled() const
{
    return _account->capabilities().ncAssistantEnabled();
}

bool AssistantController::accountConnected() const
{
    return _accountState->isConnected();
}

QString AssistantController::selectedTaskTypeId() const
{
    return _taskType;
}

QString AssistantController::selectedTaskTypeName() const
{
    return _taskTypeName;
}

QString AssistantController::selectedTaskTypeDescription() const
{
    return _taskTypeDescription;
}

bool AssistantController::selectedTaskTypeIsChat() const
{
    return _taskType == assistantControllerChatTaskTypeId;
}

qint64 AssistantController::selectedChatConversationId() const
{
    return _selectedChatConversationId;
}

QString AssistantController::selectedChatConversationTitle() const
{
    return _selectedChatConversationTitle;
}

bool AssistantController::thinking() const
{
    return _thinking;
}

bool AssistantController::showRetryResponseGeneration() const
{
    return _showRetryResponseGeneration;
}

void AssistantController::loadData()
{
    if (!_account->capabilities().ncAssistantEnabled()) {
        _error = tr("Assistant is not available for this account.");
        Q_EMIT errorChanged();
        return;
    }

    _error.clear();
    Q_EMIT errorChanged();
    setRequestInProgress(true);
    _client->fetchTaskTypes(beginRequest());
}

void AssistantController::selectTaskType(const QString &taskTypeId)
{
    const auto requestGeneration = beginRequest();
    if (_taskType == taskTypeId) {
        if (selectedTaskTypeIsChat()) {
            loadChatConversations(requestGeneration);
        } else {
            refreshTasks(requestGeneration);
        }
        return;
    }

    _taskType = taskTypeId;
    updateSelectedTypeMetadata();
    _response.clear();
    _error.clear();
    Q_EMIT responseChanged();
    Q_EMIT errorChanged();

    if (selectedTaskTypeIsChat()) {
        _tasks.clear();
        loadChatConversations(requestGeneration);
        return;
    }

    _messages.clear();
    refreshTasks(requestGeneration);
}

void AssistantController::refreshTasks()
{
    refreshTasks(beginRequest());
}

void AssistantController::deleteTask(qint64 taskId)
{
    if (taskId <= 0) {
        return;
    }
    setRequestInProgress(true);
    _client->deleteTask(taskId, beginRequest());
}

void AssistantController::retryTask(qint64 taskId)
{
    const auto input = _tasks.inputForTask(taskId);
    if (!input.trimmed().isEmpty()) {
        scheduleSelectedTask(input, beginRequest());
    }
}

void AssistantController::selectChatConversation(qint64 conversationId)
{
    const auto requestGeneration = beginRequest();
    _error.clear();
    Q_EMIT errorChanged();
    _selectedChatConversationId = conversationId;
    _chatConversations.select(conversationId);
    _selectedChatConversationTitle = _chatConversations.titleForConversation(conversationId);
    Q_EMIT selectedChatConversationChanged();

    _messages.clear();
    setShowRetryResponseGeneration(false);
    _chatMessageTaskId = -1;
    _chatPollAttempts = 0;
    _chatPollTimer.stop();
    setThinking(false);

    if (conversationId > 0) {
        loadChatMessages(conversationId, requestGeneration);
    }
}

void AssistantController::startNewChat()
{
    beginRequest();
    _selectedChatConversationId = -1;
    _selectedChatConversationTitle.clear();
    _chatConversations.select(-1);
    _messages.clear();
    _response.clear();
    _error.clear();
    _chatMessageTaskId = -1;
    _chatPollAttempts = 0;
    _chatPollTimer.stop();
    setRequestInProgress(false);
    setThinking(false);
    setShowRetryResponseGeneration(false);
    Q_EMIT selectedChatConversationChanged();
    Q_EMIT responseChanged();
    Q_EMIT errorChanged();
}

void AssistantController::retryResponseGeneration()
{
    if (_selectedChatConversationId > 0) {
        startChatGeneration(_selectedChatConversationId, beginRequest());
    }
}

void AssistantController::submitQuestion(const QString &question)
{
    const auto trimmedQuestion = question.trimmed();
    if (trimmedQuestion.isEmpty()) {
        return;
    }
    if (!_account->capabilities().ncAssistantEnabled()) {
        _error = tr("Assistant is not available for this account.");
        Q_EMIT errorChanged();
        return;
    }
    if (_requestInProgress) {
        _error = tr("Assistant is already processing a request.");
        Q_EMIT errorChanged();
        return;
    }

    _question = trimmedQuestion;
    _error.clear();
    _response.clear();
    Q_EMIT questionChanged();
    Q_EMIT errorChanged();
    Q_EMIT responseChanged();

    const auto requestGeneration = beginRequest();
    if (_taskType.isEmpty()) {
        _pendingQuestion = trimmedQuestion;
        setRequestInProgress(true);
        _client->fetchTaskTypes(requestGeneration);
        return;
    }
    if (selectedTaskTypeIsChat()) {
        submitChatMessage(trimmedQuestion, requestGeneration);
        return;
    }
    scheduleSelectedTask(trimmedQuestion, requestGeneration);
}

void AssistantController::clear()
{
    beginRequest();
    _taskPollTimer.stop();
    _chatPollTimer.stop();
    _taskId = -1;
    _chatMessageTaskId = -1;
    _chatPollAttempts = 0;
    _pendingChatMessage.clear();
    _pendingQuestion.clear();
    _selectedChatConversationId = -1;
    _selectedChatConversationTitle.clear();
    _chatConversations.select(-1);
    _question.clear();
    _response.clear();
    _error.clear();
    _messages.clear();
    setRequestInProgress(false);
    setThinking(false);
    setShowRetryResponseGeneration(false);
    Q_EMIT questionChanged();
    Q_EMIT responseChanged();
    Q_EMIT errorChanged();
    Q_EMIT selectedChatConversationChanged();
}

quint64 AssistantController::beginRequest()
{
    ++_requestGeneration;
    _client->cancelRequests();
    _taskPollTimer.stop();
    _chatPollTimer.stop();
    _taskId = -1;
    _chatMessageTaskId = -1;
    _taskPollAttempts = 0;
    _chatPollAttempts = 0;
    _pendingQuestion.clear();
    _pendingChatMessage.clear();
    setThinking(false);
    setShowRetryResponseGeneration(false);
    return _requestGeneration;
}

void AssistantController::refreshTasks(quint64 requestGeneration)
{
    if (_taskType.isEmpty() || selectedTaskTypeIsChat()) {
        return;
    }

    setRequestInProgress(true);
    _client->fetchTasks(_taskType, requestGeneration);
}

void AssistantController::pollTasks()
{
    if (_taskType.isEmpty()) {
        _taskPollTimer.stop();
        return;
    }
    if (_taskPollAttempts >= _maxTaskPollAttempts) {
        beginRequest();
        setRequestInProgress(false);
        if (_response.isEmpty()) {
            _response = tr("No response yet. Please try again later.");
            Q_EMIT responseChanged();
        }
        return;
    }

    ++_taskPollAttempts;
    _client->fetchTasks(_taskType, _requestGeneration);
}

void AssistantController::pollChatGeneration()
{
    if (_chatMessageTaskId <= 0 || _selectedChatConversationId <= 0) {
        _chatPollTimer.stop();
        return;
    }
    if (_chatPollAttempts >= _maxChatPollAttempts) {
        beginRequest();
        setRequestInProgress(false);
        setShowRetryResponseGeneration(_messages.lastMessageIsHuman());
        _error = tr("No response yet. Please try again later.");
        Q_EMIT errorChanged();
        return;
    }

    ++_chatPollAttempts;
    _client->checkChatGeneration(_chatMessageTaskId, _selectedChatConversationId, _requestGeneration);
}

void AssistantController::slotTaskTypesFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    if (!statusSuccess(statusCode)) {
        requestFailed(QStringLiteral("taskTypes"), statusCode);
        return;
    }

    _taskTypes.replaceFromResponse(json);
    if (_taskTypes.rowCount() == 0) {
        _taskType.clear();
        updateSelectedTypeMetadata();
        _tasks.clear();
        _chatConversations.clear();
        _messages.clear();
        _selectedChatConversationId = -1;
        _selectedChatConversationTitle.clear();
        Q_EMIT selectedChatConversationChanged();
        _error = tr("No supported assistant task types were returned.");
        Q_EMIT errorChanged();
        setRequestInProgress(false);
        return;
    }

    if (!_taskTypes.contains(_taskType)) {
        _taskType = _taskTypes.firstTypeId();
    }
    updateSelectedTypeMetadata();

    if (!_pendingQuestion.isEmpty()) {
        const auto pendingQuestion = std::exchange(_pendingQuestion, {});
        if (selectedTaskTypeIsChat()) {
            submitChatMessage(pendingQuestion, requestGeneration);
        } else {
            scheduleSelectedTask(pendingQuestion, requestGeneration);
        }
        return;
    }

    if (selectedTaskTypeIsChat()) {
        loadChatConversations(requestGeneration);
    } else {
        refreshTasks(requestGeneration);
    }
}

void AssistantController::slotTasksFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    if (!statusSuccess(statusCode)) {
        requestFailed(QStringLiteral("tasks"), statusCode);
        return;
    }

    _tasks.replaceFromResponse(json, _taskType);

    if (_taskId > 0) {
        const auto runningState = _tasks.runningState(_taskId);
        const auto keepPolling = !runningState.has_value() || runningState.value();
        if (runningState.has_value() && !runningState.value()) {
            _taskId = -1;
        }

        if (keepPolling) {
            if (!_taskPollTimer.isActive()) {
                _taskPollAttempts = 0;
                _taskPollTimer.start();
            }
            return;
        }
    }

    _taskPollTimer.stop();
    _response.clear();
    Q_EMIT responseChanged();
    setRequestInProgress(false);
}

void AssistantController::slotTaskScheduled(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    if (!statusSuccess(statusCode)) {
        requestFailed(QStringLiteral("schedule"), statusCode);
        return;
    }

    _taskId = taskIdFromSchedule(json);
    if (_taskId <= 0) {
        requestFailed(QStringLiteral("schedule"), statusCode);
        return;
    }
    _response = tr("Assistant task scheduled.");
    Q_EMIT responseChanged();
    _taskPollAttempts = 0;
    _client->fetchTasks(_taskType, requestGeneration);
}

void AssistantController::slotTaskDeleted(quint64 requestGeneration, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    if (!statusSuccess(statusCode)) {
        requestFailed(QStringLiteral("deleteTask"), statusCode);
        return;
    }
    refreshTasks(requestGeneration);
}

void AssistantController::slotChatConversationsFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    const auto response = chatDataFromResponse(json);
    if (chatReplyStatus(response, statusCode) != ChatReplyStatus::Success) {
        requestFailed(QStringLiteral("chatConversations"), statusCode);
        return;
    }
    _chatConversations.replaceFromResponse(response, _selectedChatConversationId);
    if (_selectedChatConversationId > 0 && !_chatConversations.contains(_selectedChatConversationId)) {
        _selectedChatConversationId = -1;
        _selectedChatConversationTitle.clear();
        _messages.clear();
        setShowRetryResponseGeneration(false);
        Q_EMIT selectedChatConversationChanged();
    }
    setRequestInProgress(false);
}

void AssistantController::slotChatMessagesFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    const auto response = chatDataFromResponse(json);
    if (chatReplyStatus(response, statusCode) != ChatReplyStatus::Success) {
        requestFailed(QStringLiteral("chatMessages"), statusCode);
        return;
    }
    _messages.replaceFromResponse(response);
    if (_selectedChatConversationId > 0) {
        _client->checkChatSession(_selectedChatConversationId, requestGeneration);
        return;
    }
    setRequestInProgress(false);
}

void AssistantController::slotChatConversationCreated(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    const auto response = chatDataFromResponse(json);
    if (chatReplyStatus(response, statusCode) != ChatReplyStatus::Success) {
        requestFailed(QStringLiteral("createChatConversation"), statusCode);
        return;
    }

    const auto conversationObject = response.object().value("session"_L1).toObject();
    const auto conversationId = AssistantUtils::jsonInteger(conversationObject.value("id"_L1));
    if (conversationId <= 0) {
        requestFailed(QStringLiteral("createChatConversation"), statusCode);
        return;
    }

    _selectedChatConversationId = conversationId;
    _chatConversations.prepend(conversationObject, conversationId);
    _selectedChatConversationTitle = _chatConversations.titleForConversation(conversationId);
    Q_EMIT selectedChatConversationChanged();

    const auto pendingMessage = std::exchange(_pendingChatMessage, {});
    if (!pendingMessage.isEmpty()) {
        submitChatMessage(pendingMessage, requestGeneration);
    } else {
        setRequestInProgress(false);
    }
}

void AssistantController::slotChatMessageCreated(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    const auto response = chatDataFromResponse(json);
    if (chatReplyStatus(response, statusCode) != ChatReplyStatus::Success) {
        requestFailed(QStringLiteral("createChatMessage"), statusCode);
        return;
    }
    _messages.append(response.object());
    startChatGeneration(_selectedChatConversationId, requestGeneration);
}

void AssistantController::slotChatSessionGenerationStarted(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    const auto response = chatDataFromResponse(json);
    if (chatReplyStatus(response, statusCode) != ChatReplyStatus::Success) {
        requestFailed(QStringLiteral("generateChatSession"), statusCode);
        return;
    }

    _chatMessageTaskId = AssistantUtils::jsonInteger(response.object().value("taskId"_L1));
    if (_chatMessageTaskId <= 0) {
        requestFailed(QStringLiteral("generateChatSession"), statusCode);
        return;
    }

    setThinking(true);
    _chatPollAttempts = 0;
    if (!_chatPollTimer.isActive()) {
        _chatPollTimer.start();
    }
}

void AssistantController::slotChatGenerationChecked(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    const auto response = chatDataFromResponse(json);
    const auto replyStatus = chatReplyStatus(response, statusCode);
    if (replyStatus == ChatReplyStatus::Failure) {
        requestFailed(QStringLiteral("checkChatGeneration"), statusCode);
        return;
    }
    if (replyStatus == ChatReplyStatus::Pending) {
        return;
    }

    const auto message = response.object();
    if (message.value("role"_L1).toString() != "assistant"_L1 || !message.value("content"_L1).isString()) {
        requestFailed(QStringLiteral("checkChatGenerationResponse"), statusCode);
        setShowRetryResponseGeneration(_messages.lastMessageIsHuman());
        return;
    }
    _messages.append(message);
    _chatMessageTaskId = -1;
    _chatPollTimer.stop();
    setThinking(false);
    setRequestInProgress(false);
}

void AssistantController::slotChatSessionChecked(quint64 requestGeneration, const QJsonDocument &json, int statusCode)
{
    if (requestGeneration != _requestGeneration) {
        return;
    }
    const auto response = chatDataFromResponse(json);
    if (chatReplyStatus(response, statusCode) != ChatReplyStatus::Success) {
        requestFailed(QStringLiteral("checkChatSession"), statusCode);
        return;
    }

    const auto session = response.object();
    const auto sessionTitle = session.value("sessionTitle"_L1).toString();
    if (!sessionTitle.isEmpty() && _selectedChatConversationTitle != sessionTitle) {
        _selectedChatConversationTitle = sessionTitle;
        _chatConversations.updateTitle(_selectedChatConversationId, sessionTitle);
        Q_EMIT selectedChatConversationChanged();
    }

    _chatMessageTaskId = AssistantUtils::jsonInteger(session.value("messageTaskId"_L1), AssistantUtils::jsonInteger(session.value("taskId"_L1)));
    if (_chatMessageTaskId > 0) {
        setThinking(true);
        setRequestInProgress(true);
        _chatPollAttempts = 0;
        if (!_chatPollTimer.isActive()) {
            _chatPollTimer.start();
        }
        return;
    }

    setShowRetryResponseGeneration(_messages.lastMessageIsHuman());
    setRequestInProgress(false);
}

void AssistantController::submitChatMessage(const QString &message, quint64 requestGeneration)
{
    setShowRetryResponseGeneration(false);
    setRequestInProgress(true);
    if (_selectedChatConversationId <= 0) {
        _pendingChatMessage = message;
        _client->createChatConversation(message, QDateTime::currentSecsSinceEpoch(), requestGeneration);
        return;
    }
    _client->createChatMessage(_selectedChatConversationId,
                               QStringLiteral("human"),
                               message,
                               QDateTime::currentSecsSinceEpoch(),
                               _messages.rowCount() == 0,
                               requestGeneration);
}

void AssistantController::scheduleSelectedTask(const QString &input, quint64 requestGeneration)
{
    const auto trimmedInput = input.trimmed();
    if (trimmedInput.isEmpty() || _taskType.isEmpty()) {
        return;
    }

    _question = trimmedInput;
    _response = tr("Scheduling assistant task…");
    _error.clear();
    Q_EMIT questionChanged();
    Q_EMIT responseChanged();
    Q_EMIT errorChanged();
    setRequestInProgress(true);
    _client->scheduleTask(trimmedInput, _taskType, requestGeneration);
}

void AssistantController::setRequestInProgress(bool inProgress)
{
    if (_requestInProgress == inProgress) {
        return;
    }
    _requestInProgress = inProgress;
    Q_EMIT requestInProgressChanged();
}

void AssistantController::setThinking(bool thinking)
{
    if (_thinking == thinking) {
        return;
    }
    _thinking = thinking;
    Q_EMIT thinkingChanged();
}

void AssistantController::setShowRetryResponseGeneration(bool show)
{
    if (_showRetryResponseGeneration == show) {
        return;
    }
    _showRetryResponseGeneration = show;
    Q_EMIT showRetryResponseGenerationChanged();
}

void AssistantController::updateSelectedTypeMetadata()
{
    _taskTypeName = _taskTypes.nameForType(_taskType);
    _taskTypeDescription = _taskTypes.descriptionForType(_taskType);
    Q_EMIT selectedTaskTypeChanged();
}

void AssistantController::loadChatConversations(quint64 requestGeneration)
{
    setRequestInProgress(true);
    _client->fetchChatConversations(requestGeneration);
}

void AssistantController::loadChatMessages(qint64 conversationId, quint64 requestGeneration)
{
    setRequestInProgress(true);
    _client->fetchChatMessages(conversationId, requestGeneration);
}

void AssistantController::startChatGeneration(qint64 conversationId, quint64 requestGeneration)
{
    _error.clear();
    Q_EMIT errorChanged();
    setRequestInProgress(true);
    setThinking(true);
    setShowRetryResponseGeneration(false);
    _client->generateChatSession(conversationId, requestGeneration);
}

void AssistantController::requestFailed(const QString &context, int statusCode)
{
    beginRequest();
    setRequestInProgress(false);
    _error = tr("Assistant request failed (%1).").arg(statusCode);
    Q_EMIT errorChanged();
    qCWarning(lcAssistantController) << "Assistant request error:" << context << statusCode;
}

}
