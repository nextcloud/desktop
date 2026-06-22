/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"
#include "owncloudlib.h"

#include <QJsonDocument>
#include <QObject>
#include <QPointer>
#include <QString>

namespace OCC {

class JsonApiJob;
class AssistantApiJob;

class OWNCLOUDSYNC_EXPORT OcsAssistantConnector : public QObject
{
    Q_OBJECT
public:
    explicit OcsAssistantConnector(AccountPtr account, QObject *parent = nullptr);

    void fetchTaskTypes();
    void fetchTasks(const QString &taskType);
    void scheduleTask(const QString &input, const QString &taskType, const QStringList &history,
        const QString &appId = QStringLiteral("assistant"),
        const QString &customId = QString());
    void deleteTask(qint64 taskId);

signals:
    void taskTypesFetched(const QJsonDocument &json, int statusCode);
    void tasksFetched(const QJsonDocument &json, int statusCode);
    void taskScheduled(const QJsonDocument &json, int statusCode);
    void taskDeleted(int statusCode);
    void requestError(const QString &context, int statusCode);

private:
    void emitIfError(const QString &context, int statusCode);

    AccountPtr _account;
    QPointer<JsonApiJob> _taskTypesJob;
    QPointer<JsonApiJob> _tasksJob;
    QPointer<AssistantApiJob> _scheduleJob;
    QPointer<JsonApiJob> _deleteJob;
};

} // namespace OCC
