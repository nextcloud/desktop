/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantmessagemodel.h"

#include "assistantutils.h"

#include <QJsonArray>

#include <utility>

using namespace Qt::StringLiterals;

namespace OCC {

AssistantMessageModel::AssistantMessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AssistantMessageModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _items.size();
}

QVariant AssistantMessageModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const auto &item = _items.at(index.row());
    switch (role) {
    case MessageIdRole:
        return item.messageId;
    case SessionIdRole:
        return item.sessionId;
    case MessageRole:
        return item.role;
    case TextRole:
        return item.text;
    case TimestampRole:
        return item.timestamp;
    case DateTextRole:
        return item.dateText;
    }
    return {};
}

QHash<int, QByteArray> AssistantMessageModel::roleNames() const
{
    static const auto roles = QHash<int, QByteArray>{
        {MessageIdRole, "messageId"},
        {SessionIdRole, "sessionId"},
        {MessageRole, "messageRole"},
        {TextRole, "messageText"},
        {TimestampRole, "timestamp"},
        {DateTextRole, "dateText"},
    };
    return roles;
}

void AssistantMessageModel::replaceFromResponse(const QJsonDocument &json)
{
    auto items = QList<Item>{};
    items.reserve(json.array().size());
    for (const auto &entry : json.array()) {
        items.append(itemFromJson(entry.toObject()));
    }

    beginResetModel();
    _items = std::move(items);
    endResetModel();
}

void AssistantMessageModel::append(const QJsonObject &message)
{
    const auto row = _items.size();
    beginInsertRows({}, row, row);
    _items.append(itemFromJson(message));
    endInsertRows();
}

void AssistantMessageModel::clear()
{
    if (_items.isEmpty()) {
        return;
    }
    beginResetModel();
    _items.clear();
    endResetModel();
}

bool AssistantMessageModel::lastMessageIsHuman() const
{
    return !_items.isEmpty() && _items.constLast().role == "user"_L1;
}

AssistantMessageModel::Item AssistantMessageModel::itemFromJson(const QJsonObject &message)
{
    const auto timestamp = AssistantUtils::jsonInteger(message.value("timestamp"_L1), 0);
    auto role = message.value("role"_L1).toString();
    if (role == "human"_L1) {
        role = QStringLiteral("user");
    }
    return {
        AssistantUtils::jsonInteger(message.value("id"_L1)),
        AssistantUtils::jsonInteger(message.value("session_id"_L1), AssistantUtils::jsonInteger(message.value("sessionId"_L1))),
        role,
        message.value("content"_L1).toString(),
        timestamp,
        AssistantUtils::dateText(timestamp),
    };
}

}
