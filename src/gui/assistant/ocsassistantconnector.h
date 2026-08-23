/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"
#include <QJsonDocument>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

namespace OCC {

class JsonApiJob;
class AssistantApiJob;

class OcsAssistantConnector : public QObject
{
    Q_OBJECT
public:
    explicit OcsAssistantConnector(AccountPtr account, QObject *parent = nullptr);

    void fetchTaskTypes(quint64 requestGeneration);
    void fetchTasks(const QString &taskType, quint64 requestGeneration);
    void scheduleTask(const QString &input, const QString &taskType, const QStringList &history,
        quint64 requestGeneration,
        const QString &appId = QStringLiteral("assistant"),
        const QString &customId = QString());
    void deleteTask(qint64 taskId, quint64 requestGeneration);
    void cancelRequests();

signals:
    void taskTypesFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void tasksFetched(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void taskScheduled(quint64 requestGeneration, const QJsonDocument &json, int statusCode);
    void taskDeleted(quint64 requestGeneration, int statusCode);

private:
    void logIfError(const QString &context, int statusCode);

    AccountPtr _account;
    QPointer<JsonApiJob> _taskTypesJob;
    QPointer<JsonApiJob> _tasksJob;
    QPointer<AssistantApiJob> _scheduleJob;
    QPointer<JsonApiJob> _deleteJob;
};

} // namespace OCC
