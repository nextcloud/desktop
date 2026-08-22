/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistanttaskmodel.h"

#include "assistantutils.h"

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace OCC {

namespace {

QJsonArray tasksFromResponse(const QJsonDocument &json)
{
    const auto data = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject();
    if (data.value("tasks"_L1).isArray()) {
        return data.value("tasks"_L1).toArray();
    }
    if (data.value("task"_L1).isObject()) {
        auto tasks = QJsonArray{};
        tasks.append(data.value("task"_L1));
        return tasks;
    }
    return {};
}

}

AssistantTaskModel::AssistantTaskModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AssistantTaskModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _items.size();
}

QVariant AssistantTaskModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const auto &item = _items.at(index.row());
    switch (role) {
    case TaskIdRole:
        return item.taskId;
    case TypeRole:
        return item.type;
    case AppIdRole:
        return item.appId;
    case InputRole:
        return item.input;
    case OutputRole:
        return item.output;
    case StatusRole:
        return item.status;
    case StatusTextRole:
        return item.statusText;
    case DateTextRole:
        return item.dateText;
    }
    return {};
}

QHash<int, QByteArray> AssistantTaskModel::roleNames() const
{
    static const auto roles = QHash<int, QByteArray>{
        {TaskIdRole, "taskId"},
        {TypeRole, "taskType"},
        {AppIdRole, "appId"},
        {InputRole, "input"},
        {OutputRole, "output"},
        {StatusRole, "status"},
        {StatusTextRole, "statusText"},
        {DateTextRole, "dateText"},
    };
    return roles;
}

void AssistantTaskModel::replaceFromResponse(const QJsonDocument &json, const QString &taskType)
{
    auto items = QList<Item>{};
    for (const auto &entry : tasksFromResponse(json)) {
        const auto task = entry.toObject();
        const auto appId = task.value("appId"_L1).toString();
        if (!appId.isEmpty() && appId != "assistant"_L1) {
            continue;
        }
        const auto type = task.value("type"_L1).toString();
        if (type != taskType) {
            continue;
        }

        const auto status = AssistantUtils::statusString(task.value("status"_L1));
        auto statusText = tr("Unknown");
        if (status == "1"_L1 || status == "STATUS_SCHEDULED"_L1) {
            statusText = tr("Scheduled");
        } else if (status == "2"_L1 || status == "STATUS_RUNNING"_L1) {
            statusText = tr("In progress");
        } else if (status == "3"_L1 || status == "STATUS_SUCCESSFUL"_L1) {
            statusText = tr("Completed");
        } else if (status == "4"_L1 || status == "STATUS_FAILED"_L1) {
            statusText = tr("Failed");
        }

        const auto timestamp = AssistantUtils::jsonInteger(task.value("lastUpdated"_L1),
            AssistantUtils::jsonInteger(task.value("completionExpectedAt"_L1),
                AssistantUtils::jsonInteger(task.value("scheduledAt"_L1), 0)));
        const auto statusValue = task.value("status"_L1);
        items.append({
            AssistantUtils::jsonInteger(task.value("id"_L1)),
            type,
            appId,
            AssistantUtils::textFromValue(task.value("input"_L1)),
            AssistantUtils::textFromValue(task.value("output"_L1)),
            status,
            statusText,
            AssistantUtils::dateText(timestamp),
            AssistantUtils::taskStillRunning(statusValue),
        });
    }

    beginResetModel();
    _items = std::move(items);
    endResetModel();
}

void AssistantTaskModel::clear()
{
    if (_items.isEmpty()) {
        return;
    }
    beginResetModel();
    _items.clear();
    endResetModel();
}

QString AssistantTaskModel::inputForTask(qint64 taskId) const
{
    const auto it = std::find_if(_items.cbegin(), _items.cend(), [taskId](const Item &item) {
        return item.taskId == taskId;
    });
    return it == _items.cend() ? QString{} : it->input;
}

std::optional<bool> AssistantTaskModel::runningState(qint64 taskId) const
{
    const auto it = std::find_if(_items.cbegin(), _items.cend(), [taskId](const Item &item) {
        return item.taskId == taskId;
    });
    if (it == _items.cend()) {
        return std::nullopt;
    }
    return it->running;
}

}
