/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ocsassistantconnector.h"

#include "account.h"
#include "assistantapijob.h"
#include "networkjobs.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QUrlQuery>

#include <utility>

using namespace Qt::StringLiterals;

namespace OCC
{

namespace
{

Q_LOGGING_CATEGORY(lcOcsAssistantConnector, "nextcloud.gui.assistant.connector", QtInfoMsg)

constexpr auto basePath = "/ocs/v2.php/taskprocessing"_L1;
constexpr auto assistantChatTaskTypeId = "core:text2text:chat"_L1;
constexpr auto assistantSystemPrompt =
    "This is a conversation in a specific language between the user and you, Nextcloud Assistant. "
    "You are a kind, polite and helpful AI that helps the user to the best of its abilities. "
    "If you do not understand something, you will ask for clarification. Detect the language "
    "that the user is using. Make sure to use the same language in your response. Do not mention "
    "the language explicitly."_L1;

}

OcsAssistantConnector::OcsAssistantConnector(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(std::move(account))
{
    Q_ASSERT(_account);
}

void OcsAssistantConnector::fetchTaskTypes(quint64 requestGeneration)
{
    if (_taskTypesJob) {
        qCDebug(lcOcsAssistantConnector) << "Task types job already running.";
        return;
    }

    _taskTypesJob = new JsonApiJob(_account, QString{basePath} + u"/tasktypes"_s, this);
    connect(_taskTypesJob, &JsonApiJob::jsonReceived, this, [this, requestGeneration](const QJsonDocument &json, int statusCode) {
        _taskTypesJob = nullptr;
        qCInfo(lcOcsAssistantConnector).noquote() << statusCode << QString::fromUtf8(json.toJson(QJsonDocument::JsonFormat::Compact));
        logIfError(QStringLiteral("taskTypes"), statusCode);
        Q_EMIT taskTypesFetched(requestGeneration, json, statusCode);
    });
    _taskTypesJob->start();
}

void OcsAssistantConnector::fetchTasks(const QString &taskType, quint64 requestGeneration)
{
    if (_tasksJob) {
        qCDebug(lcOcsAssistantConnector) << "Tasks job already running.";
        return;
    }

    _tasksJob = new JsonApiJob(_account, QString{basePath} + u"/tasks"_s, this);
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("taskType"), taskType);
    _tasksJob->addQueryParams(params);
    connect(_tasksJob, &JsonApiJob::jsonReceived, this, [this, requestGeneration](const QJsonDocument &json, int statusCode) {
        _tasksJob = nullptr;
        qCInfo(lcOcsAssistantConnector).noquote() << statusCode << QString::fromUtf8(json.toJson(QJsonDocument::JsonFormat::Compact));
        logIfError(QStringLiteral("tasks"), statusCode);
        Q_EMIT tasksFetched(requestGeneration, json, statusCode);
    });
    _tasksJob->start();
}

void OcsAssistantConnector::scheduleTask(const QString &input,
                                         const QString &taskType,
                                         const QStringList &history,
                                         quint64 requestGeneration,
                                         const QString &appId,
                                         const QString &customId)
{
    if (_scheduleJob) {
        qCDebug(lcOcsAssistantConnector) << "Schedule job already running.";
        return;
    }

    _scheduleJob = new AssistantApiJob(_account, QString{basePath} + QStringLiteral("/schedule"), this);
    _scheduleJob->setVerb(SimpleApiJob::Verb::Post);

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    _scheduleJob->addQueryParams(params);

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("input[input]"), input);
    if (taskType == assistantChatTaskTypeId) {
        body.addQueryItem(QStringLiteral("input[system_prompt]"), QString{assistantSystemPrompt});
        if (history.isEmpty()) {
            const QJsonObject firstHistoryEntry = {
                {QStringLiteral("role"), QStringLiteral("human")},
                {QStringLiteral("content"), input},
            };
            body.addQueryItem(QStringLiteral("input[history][0]"), QString::fromUtf8(QJsonDocument(firstHistoryEntry).toJson(QJsonDocument::Compact)));
        } else {
            for (auto index = 0; index < history.size(); ++index) {
                body.addQueryItem(QStringLiteral("input[history][%1]").arg(index), history.at(index));
            }
        }
    }
    body.addQueryItem(QStringLiteral("type"), taskType);
    body.addQueryItem(QStringLiteral("appId"), appId);
    body.addQueryItem(QStringLiteral("customId"), customId);
    _scheduleJob->setFormBody(body);

    connect(_scheduleJob, &AssistantApiJob::jsonReceived, this, [this, requestGeneration](const QJsonDocument &json, int statusCode) {
        _scheduleJob = nullptr;
        qCInfo(lcOcsAssistantConnector).noquote() << statusCode << QString::fromUtf8(json.toJson(QJsonDocument::JsonFormat::Compact));
        logIfError(QStringLiteral("schedule"), statusCode);
        Q_EMIT taskScheduled(requestGeneration, json, statusCode);
    });
    _scheduleJob->start();
}

void OcsAssistantConnector::deleteTask(qint64 taskId, quint64 requestGeneration)
{
    if (_deleteJob) {
        qCDebug(lcOcsAssistantConnector) << "Delete task job already running.";
        return;
    }

    const auto path = QString{basePath}.append("/task/"_L1).append(QString::number(taskId));
    _deleteJob = new JsonApiJob(_account, path, this);
    _deleteJob->setVerb(SimpleApiJob::Verb::Delete);
    connect(_deleteJob, &JsonApiJob::jsonReceived, this, [this, requestGeneration](const QJsonDocument &json, int statusCode) {
        _deleteJob = nullptr;
        qCInfo(lcOcsAssistantConnector).noquote() << statusCode << QString::fromUtf8(json.toJson(QJsonDocument::JsonFormat::Compact));
        logIfError(QStringLiteral("deleteTask"), statusCode);
        Q_EMIT taskDeleted(requestGeneration, statusCode);
    });
    _deleteJob->start();
}

void OcsAssistantConnector::cancelRequests()
{
    const auto cancelJob = [this](auto &job) {
        if (!job) {
            return;
        }
        job->disconnect(this);
        job->deleteLater();
        job = nullptr;
    };
    cancelJob(_taskTypesJob);
    cancelJob(_tasksJob);
    cancelJob(_scheduleJob);
    cancelJob(_deleteJob);
}

void OcsAssistantConnector::logIfError(const QString &context, int statusCode)
{
    if (statusCode != 100 && (statusCode < 200 || statusCode >= 300)) {
        qCWarning(lcOcsAssistantConnector) << "Assistant request failed:" << context << "status" << statusCode;
    }
}

} // namespace OCC
