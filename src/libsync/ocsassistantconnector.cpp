/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ocsassistantconnector.h"

#include "account.h"
#include "networkjobs.h"

#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace OCC {

namespace {

Q_LOGGING_CATEGORY(lcOcsAssistantConnector, "nextcloud.sync.ocsassistantconnector", QtInfoMsg)

const auto basePath = u"/ocs/v2.php/taskprocessing"_s;
const auto assistantSystemPrompt = QStringLiteral(
    "This is a conversation in a specific language between the user and you, Nextcloud Assistant. "
    "You are a kind, polite and helpful AI that helps the user to the best of its abilities. "
    "If you do not understand something, you will ask for clarification. Detect the language "
    "that the user is using. Make sure to use the same language in your response. Do not mention "
    "the language explicitly.");

int statusCodeFromJson(const QString &jsonStr, int fallback)
{
    if (jsonStr.contains("<?xml version=\"1.0\"?>"_L1)) {
        static const QRegularExpression xmlRegex("<statuscode>(\\d+)</statuscode>"_L1);
        const auto match = xmlRegex.match(jsonStr);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
        return fallback;
    }

    static const QRegularExpression jsonRegex(R"("statuscode":(\d+))");
    const auto match = jsonRegex.match(jsonStr);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }

    return fallback;
}

}

class AssistantApiJob : public SimpleApiJob
{
    Q_OBJECT
public:
    explicit AssistantApiJob(const AccountPtr &account, const QString &path, QObject *parent = nullptr)
        : SimpleApiJob(account, path, parent)
    {
    }

    void setFormBody(const QUrlQuery &query)
    {
        const auto body = query.toString(QUrl::FullyEncoded).toUtf8();
        setBody(body);
        request().setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    }

signals:
    void jsonReceived(const QJsonDocument &json, int statusCode);

protected:
    bool finished() override
    {
        qCInfo(lcOcsAssistantConnector) << "AssistantApiJob of" << reply()->request().url()
                                        << "FINISHED WITH STATUS" << replyStatusString();

        const auto httpStatusCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply()->error() != QNetworkReply::NoError) {
            qCWarning(lcOcsAssistantConnector) << "Network error:" << path() << errorString() << httpStatusCode;
            emit jsonReceived(QJsonDocument(), httpStatusCode);
            return true;
        }

        const QByteArray replyData = reply()->readAll();
        const auto jsonStr = QString::fromUtf8(replyData);
        const auto statusCode = statusCodeFromJson(jsonStr, httpStatusCode);

        QJsonParseError error{};
        auto json = QJsonDocument::fromJson(replyData, &error);
        if (error.error != QJsonParseError::NoError) {
            qCWarning(lcOcsAssistantConnector) << "Invalid JSON response:" << error.errorString();
        }

        emit jsonReceived(json, statusCode);
        return true;
    }
};

OcsAssistantConnector::OcsAssistantConnector(AccountPtr account, QObject *parent)
    : QObject(parent)
    , _account(std::move(account))
{
    Q_ASSERT(_account);
}

void OcsAssistantConnector::fetchTaskTypes()
{
    if (_taskTypesJob) {
        qCDebug(lcOcsAssistantConnector) << "Task types job already running.";
        return;
    }

    _taskTypesJob = new JsonApiJob(_account, basePath + u"/tasktypes"_s, this);
    connect(_taskTypesJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        qCInfo(lcOcsAssistantConnector).noquote() << statusCode << QString::fromUtf8(json.toJson(QJsonDocument::JsonFormat::Compact));
        emitIfError(QStringLiteral("taskTypes"), statusCode);
        emit taskTypesFetched(json, statusCode);
    });
    _taskTypesJob->start();
}

void OcsAssistantConnector::fetchTasks(const QString &taskType)
{
    if (_tasksJob) {
        qCDebug(lcOcsAssistantConnector) << "Tasks job already running.";
        return;
    }

    _tasksJob = new JsonApiJob(_account, u"ocs/v2.php/apps/assistant/api/v1/tasks"_s, this);
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("taskType"), taskType);
    _tasksJob->addQueryParams(params);
    connect(_tasksJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        qCInfo(lcOcsAssistantConnector).noquote() << statusCode << QString::fromUtf8(json.toJson(QJsonDocument::JsonFormat::Compact));
        emitIfError(QStringLiteral("tasks"), statusCode);
        emit tasksFetched(json, statusCode);
    });
    _tasksJob->start();
}

void OcsAssistantConnector::scheduleTask(const QString &input, const QString &taskType, const QStringList &history,
    const QString &appId, const QString &customId)
{
    if (_scheduleJob) {
        qCDebug(lcOcsAssistantConnector) << "Schedule job already running.";
        return;
    }

    _scheduleJob = new AssistantApiJob(_account, basePath + QStringLiteral("/schedule"), this);
    _scheduleJob->setVerb(SimpleApiJob::Verb::Post);

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    _scheduleJob->addQueryParams(params);

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("input[input]"), input);
    body.addQueryItem(QStringLiteral("input[system_prompt]"), assistantSystemPrompt);
    if (history.isEmpty()) {
        const QJsonObject firstHistoryEntry{
            {QStringLiteral("role"), QStringLiteral("human")},
            {QStringLiteral("content"), input},
        };
        body.addQueryItem(QStringLiteral("input[history][0]"), QString::fromUtf8(QJsonDocument(firstHistoryEntry).toJson(QJsonDocument::Compact)));
    } else {
        for (int index = 0; index < history.size(); ++index) {
            body.addQueryItem(QStringLiteral("input[history][%1]").arg(index), history.at(index));
        }
    }
    body.addQueryItem(QStringLiteral("type"), taskType);
    body.addQueryItem(QStringLiteral("appId"), appId);
    body.addQueryItem(QStringLiteral("customId"), customId);
    _scheduleJob->setFormBody(body);

    connect(_scheduleJob, &AssistantApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        qCInfo(lcOcsAssistantConnector).noquote() << statusCode << QString::fromUtf8(json.toJson(QJsonDocument::JsonFormat::Compact));
        emitIfError(QStringLiteral("schedule"), statusCode);
        emit taskScheduled(json, statusCode);
    });
    _scheduleJob->start();
}

void OcsAssistantConnector::deleteTask(qint64 taskId)
{
    if (_deleteJob) {
        qCDebug(lcOcsAssistantConnector) << "Delete task job already running.";
        return;
    }

    const auto path = QString{basePath + QStringLiteral("/task/") + QString::number(taskId)};
    _deleteJob = new JsonApiJob(_account, path, this);
    _deleteJob->setVerb(SimpleApiJob::Verb::Delete);
    connect(_deleteJob, &JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) {
        qCInfo(lcOcsAssistantConnector).noquote() << statusCode << QString::fromUtf8(json.toJson(QJsonDocument::JsonFormat::Compact));
        emitIfError(QStringLiteral("deleteTask"), statusCode);
        emit taskDeleted(statusCode);
    });
    _deleteJob->start();
}

void OcsAssistantConnector::emitIfError(const QString &context, int statusCode)
{
    if (statusCode < 200 || statusCode >= 300) {
        qCWarning(lcOcsAssistantConnector) << "Assistant request failed:" << context << "status" << statusCode;
        emit requestError(context, statusCode);
    }
}

} // namespace OCC

#include "ocsassistantconnector.moc"
