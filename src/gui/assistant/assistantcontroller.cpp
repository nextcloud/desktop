/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantcontroller.h"

#include "account.h"
#include "assistantclient.h"
#include "assistantutils.h"

#include <QDateTime>
#include <QJsonObject>
#include <QLoggingCategory>

#include <chrono>
#include <utility>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

namespace OCC {

namespace {

Q_LOGGING_CATEGORY(lcAssistantController, "nextcloud.gui.assistant", QtInfoMsg)

constexpr auto chatTaskTypeId = "core:text2text:chat"_L1;
constexpr auto successMinStatusCode = 200;
constexpr auto successMaxStatusCode = 300;

bool statusSuccess(int statusCode)
{
    return statusCode == 100 || (statusCode >= successMinStatusCode && statusCode < successMaxStatusCode);
}

bool chatStatusSuccess(const QJsonDocument &json, int statusCode)
{
    return statusSuccess(statusCode) || (statusCode == 0 && !json.isNull());
}

qint64 taskIdFromSchedule(const QJsonDocument &json)
{
    const auto task = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject().value("task"_L1).toObject();
    return AssistantUtils::jsonInteger(task.value("id"_L1));
}

}

AssistantController::AssistantController(AccountStatePtr accountState, QObject *parent)
    : QObject(parent)
    , _accountState(std::move(accountState))
    , _account(_accountState->account())
    , _client(new AssistantClient(_account, this))
{
    Q_ASSERT(_accountState);
    Q_ASSERT(_account);

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
    connect(&_chatPollTimer, &QTimer::timeout, this, [this] {
        if (_chatMessageTaskId > 0 && _selectedChatConversationId > 0) {
            _client->checkChatGeneration(_chatMessageTaskId, _selectedChatConversationId);
        }
    });
}

AssistantTaskTypeModel *AssistantController::taskTypes()
{
    return &_taskTypes;
}

AssistantTaskModel *AssistantController::tasks()
{
    return &_tasks;
}

AssistantConversationModel *AssistantController::chatConversations()
{
    return &_chatConversations;
}

AssistantMessageModel *AssistantController::messages()
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
    return _taskType == chatTaskTypeId;
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
        emit errorChanged();
        return;
    }

    _error.clear();
    emit errorChanged();
    setRequestInProgress(true);
    _client->fetchTaskTypes();
}

void AssistantController::selectTaskType(const QString &taskTypeId)
{
    if (_taskType == taskTypeId) {
        if (selectedTaskTypeIsChat()) {
            loadChatConversations();
        } else {
            refreshTasks();
        }
        return;
    }

    _taskType = taskTypeId;
    updateSelectedTypeMetadata();
    _response.clear();
    _error.clear();
    emit responseChanged();
    emit errorChanged();

    if (selectedTaskTypeIsChat()) {
        _tasks.clear();
        loadChatConversations();
        return;
    }

    _messages.clear();
    refreshTasks();
}

void AssistantController::refreshTasks()
{
    if (_taskType.isEmpty() || selectedTaskTypeIsChat()) {
        return;
    }

    setRequestInProgress(true);
    _client->fetchTasks(_taskType);
}

void AssistantController::deleteTask(qint64 taskId)
{
    if (taskId <= 0) {
        return;
    }
    setRequestInProgress(true);
    _client->deleteTask(taskId);
}

void AssistantController::retryTask(qint64 taskId)
{
    scheduleSelectedTask(_tasks.inputForTask(taskId));
}

void AssistantController::selectChatConversation(qint64 conversationId)
{
    _selectedChatConversationId = conversationId;
    _chatConversations.select(conversationId);
    _selectedChatConversationTitle = _chatConversations.titleForConversation(conversationId);
    emit selectedChatConversationChanged();

    _messages.clear();
    setShowRetryResponseGeneration(false);
    _chatMessageTaskId = -1;
    _chatPollTimer.stop();
    setThinking(false);

    if (conversationId > 0) {
        loadChatMessages(conversationId);
    }
}

void AssistantController::startNewChat()
{
    _selectedChatConversationId = -1;
    _selectedChatConversationTitle.clear();
    _chatConversations.select(-1);
    _messages.clear();
    _response.clear();
    _error.clear();
    _chatMessageTaskId = -1;
    _chatPollTimer.stop();
    setRequestInProgress(false);
    setThinking(false);
    setShowRetryResponseGeneration(false);
    emit selectedChatConversationChanged();
    emit responseChanged();
    emit errorChanged();
}

void AssistantController::retryResponseGeneration()
{
    if (_selectedChatConversationId > 0) {
        startChatGeneration(_selectedChatConversationId);
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
        emit errorChanged();
        return;
    }
    if (_requestInProgress) {
        _error = tr("Assistant is already processing a request.");
        emit errorChanged();
        return;
    }

    _question = trimmedQuestion;
    _error.clear();
    _response.clear();
    emit questionChanged();
    emit errorChanged();
    emit responseChanged();

    if (_taskType.isEmpty()) {
        _pendingQuestion = trimmedQuestion;
        setRequestInProgress(true);
        _client->fetchTaskTypes();
        return;
    }
    if (selectedTaskTypeIsChat()) {
        submitChatMessage(trimmedQuestion);
        return;
    }
    scheduleSelectedTask(trimmedQuestion);
}

void AssistantController::clear()
{
    _taskPollTimer.stop();
    _chatPollTimer.stop();
    _taskId = -1;
    _chatMessageTaskId = -1;
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
    emit questionChanged();
    emit responseChanged();
    emit errorChanged();
    emit selectedChatConversationChanged();
}

void AssistantController::pollTasks()
{
    if (_taskType.isEmpty()) {
        _taskPollTimer.stop();
        return;
    }
    if (_taskPollAttempts >= _maxTaskPollAttempts) {
        _taskPollTimer.stop();
        _taskId = -1;
        setRequestInProgress(false);
        if (_response.isEmpty()) {
            _response = tr("No response yet. Please try again later.");
            emit responseChanged();
        }
        return;
    }

    ++_taskPollAttempts;
    _client->fetchTasks(_taskType);
}

void AssistantController::slotTaskTypesFetched(const QJsonDocument &json, int statusCode)
{
    if (!statusSuccess(statusCode)) {
        requestFailed(QStringLiteral("taskTypes"), statusCode);
        return;
    }

    _taskTypes.replaceFromResponse(json);
    if (_taskTypes.rowCount() == 0) {
        _error = tr("No supported assistant task types were returned.");
        emit errorChanged();
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
            submitChatMessage(pendingQuestion);
        } else {
            scheduleSelectedTask(pendingQuestion);
        }
        return;
    }

    if (selectedTaskTypeIsChat()) {
        loadChatConversations();
    } else {
        refreshTasks();
    }
}

void AssistantController::slotTasksFetched(const QJsonDocument &json, int statusCode)
{
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
    emit responseChanged();
    setRequestInProgress(false);
}

void AssistantController::slotTaskScheduled(const QJsonDocument &json, int statusCode)
{
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
    emit responseChanged();
    _taskPollAttempts = 0;
    _client->fetchTasks(_taskType);
}

void AssistantController::slotTaskDeleted(int statusCode)
{
    if (!statusSuccess(statusCode)) {
        requestFailed(QStringLiteral("deleteTask"), statusCode);
        return;
    }
    refreshTasks();
}

void AssistantController::slotChatConversationsFetched(const QJsonDocument &json, int statusCode)
{
    if (!chatStatusSuccess(json, statusCode)) {
        requestFailed(QStringLiteral("chatConversations"), statusCode);
        return;
    }
    _chatConversations.replaceFromResponse(json, _selectedChatConversationId);
    setRequestInProgress(false);
}

void AssistantController::slotChatMessagesFetched(const QJsonDocument &json, int statusCode)
{
    if (!chatStatusSuccess(json, statusCode)) {
        requestFailed(QStringLiteral("chatMessages"), statusCode);
        return;
    }
    _messages.replaceFromResponse(json);
    if (_selectedChatConversationId > 0) {
        _client->checkChatSession(_selectedChatConversationId);
        return;
    }
    setRequestInProgress(false);
}

void AssistantController::slotChatConversationCreated(const QJsonDocument &json, int statusCode)
{
    if (!chatStatusSuccess(json, statusCode)) {
        requestFailed(QStringLiteral("createChatConversation"), statusCode);
        return;
    }

    const auto conversationObject = json.object().value("session"_L1).toObject();
    const auto conversationId = AssistantUtils::jsonInteger(conversationObject.value("id"_L1));
    if (conversationId <= 0) {
        requestFailed(QStringLiteral("createChatConversation"), statusCode);
        return;
    }

    _selectedChatConversationId = conversationId;
    _chatConversations.prepend(conversationObject, conversationId);
    _selectedChatConversationTitle = _chatConversations.titleForConversation(conversationId);
    emit selectedChatConversationChanged();

    const auto pendingMessage = std::exchange(_pendingChatMessage, {});
    if (!pendingMessage.isEmpty()) {
        submitChatMessage(pendingMessage);
    } else {
        setRequestInProgress(false);
    }
}

void AssistantController::slotChatMessageCreated(const QJsonDocument &json, int statusCode)
{
    if (!chatStatusSuccess(json, statusCode)) {
        requestFailed(QStringLiteral("createChatMessage"), statusCode);
        return;
    }
    _messages.append(json.object());
    startChatGeneration(_selectedChatConversationId);
}

void AssistantController::slotChatSessionGenerationStarted(const QJsonDocument &json, int statusCode)
{
    if (!chatStatusSuccess(json, statusCode)) {
        requestFailed(QStringLiteral("generateChatSession"), statusCode);
        return;
    }

    _chatMessageTaskId = AssistantUtils::jsonInteger(json.object().value("taskId"_L1));
    if (_chatMessageTaskId <= 0) {
        requestFailed(QStringLiteral("generateChatSession"), statusCode);
        return;
    }

    setThinking(true);
    if (!_chatPollTimer.isActive()) {
        _chatPollTimer.start();
    }
}

void AssistantController::slotChatGenerationChecked(const QJsonDocument &json, int statusCode)
{
    if (statusCode == 417) {
        return;
    }
    if (!chatStatusSuccess(json, statusCode)) {
        requestFailed(QStringLiteral("checkChatGeneration"), statusCode);
        return;
    }

    if (json.object().value("role"_L1).toString() == "assistant"_L1) {
        _messages.append(json.object());
    }
    _chatMessageTaskId = -1;
    _chatPollTimer.stop();
    setThinking(false);
    setRequestInProgress(false);
}

void AssistantController::slotChatSessionChecked(const QJsonDocument &json, int statusCode)
{
    if (!chatStatusSuccess(json, statusCode)) {
        requestFailed(QStringLiteral("checkChatSession"), statusCode);
        return;
    }

    const auto session = json.object();
    const auto sessionTitle = session.value("sessionTitle"_L1).toString();
    if (!sessionTitle.isEmpty() && _selectedChatConversationTitle != sessionTitle) {
        _selectedChatConversationTitle = sessionTitle;
        emit selectedChatConversationChanged();
    }

    _chatMessageTaskId = AssistantUtils::jsonInteger(session.value("messageTaskId"_L1),
        AssistantUtils::jsonInteger(session.value("taskId"_L1)));
    if (_chatMessageTaskId > 0) {
        setThinking(true);
        setRequestInProgress(true);
        if (!_chatPollTimer.isActive()) {
            _chatPollTimer.start();
        }
        return;
    }

    setShowRetryResponseGeneration(_messages.lastMessageIsHuman());
    setRequestInProgress(false);
}

void AssistantController::submitChatMessage(const QString &message)
{
    setShowRetryResponseGeneration(false);
    setRequestInProgress(true);
    if (_selectedChatConversationId <= 0) {
        _pendingChatMessage = message;
        _client->createChatConversation(message, QDateTime::currentSecsSinceEpoch());
        return;
    }
    _client->createChatMessage(_selectedChatConversationId,
        QStringLiteral("human"),
        message,
        QDateTime::currentSecsSinceEpoch(),
        _messages.rowCount() == 0);
}

void AssistantController::scheduleSelectedTask(const QString &input)
{
    const auto trimmedInput = input.trimmed();
    if (trimmedInput.isEmpty() || _taskType.isEmpty()) {
        return;
    }

    _question = trimmedInput;
    _response = tr("Scheduling assistant task…");
    _error.clear();
    emit questionChanged();
    emit responseChanged();
    emit errorChanged();
    setRequestInProgress(true);
    _client->scheduleTask(trimmedInput, _taskType);
}

void AssistantController::setRequestInProgress(bool inProgress)
{
    if (_requestInProgress == inProgress) {
        return;
    }
    _requestInProgress = inProgress;
    emit requestInProgressChanged();
}

void AssistantController::setThinking(bool thinking)
{
    if (_thinking == thinking) {
        return;
    }
    _thinking = thinking;
    emit thinkingChanged();
}

void AssistantController::setShowRetryResponseGeneration(bool show)
{
    if (_showRetryResponseGeneration == show) {
        return;
    }
    _showRetryResponseGeneration = show;
    emit showRetryResponseGenerationChanged();
}

void AssistantController::updateSelectedTypeMetadata()
{
    _taskTypeName = _taskTypes.nameForType(_taskType);
    _taskTypeDescription = _taskTypes.descriptionForType(_taskType);
    emit selectedTaskTypeChanged();
}

void AssistantController::loadChatConversations()
{
    setRequestInProgress(true);
    _client->fetchChatConversations();
}

void AssistantController::loadChatMessages(qint64 conversationId)
{
    setRequestInProgress(true);
    _client->fetchChatMessages(conversationId);
}

void AssistantController::startChatGeneration(qint64 conversationId)
{
    setRequestInProgress(true);
    setThinking(true);
    setShowRetryResponseGeneration(false);
    _client->generateChatSession(conversationId);
}

void AssistantController::requestFailed(const QString &context, int statusCode)
{
    _taskPollTimer.stop();
    _chatPollTimer.stop();
    _taskId = -1;
    _chatMessageTaskId = -1;
    setRequestInProgress(false);
    setThinking(false);
    _error = tr("Assistant request failed (%1).").arg(statusCode);
    emit errorChanged();
    qCWarning(lcAssistantController) << "Assistant request error:" << context << statusCode;
}

}
