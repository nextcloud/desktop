/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistantconversationmodel.h"

#include "assistantutils.h"

#include <QJsonArray>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace OCC {

AssistantConversationModel::AssistantConversationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AssistantConversationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _items.size();
}

QVariant AssistantConversationModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const auto &item = _items.at(index.row());
    switch (role) {
    case ConversationIdRole:
        return item.conversationId;
    case TitleRole:
        return item.title;
    case TimestampRole:
        return item.timestamp;
    case SelectedRole:
        return item.selected;
    }
    return {};
}

QHash<int, QByteArray> AssistantConversationModel::roleNames() const
{
    static const auto roles = QHash<int, QByteArray>{
        {ConversationIdRole, "conversationId"},
        {TitleRole, "title"},
        {TimestampRole, "timestamp"},
        {SelectedRole, "selected"},
    };
    return roles;
}

void AssistantConversationModel::replaceFromResponse(const QJsonDocument &json, qint64 selectedConversationId)
{
    auto items = QList<Item>{};
    items.reserve(json.array().size());
    for (const auto &entry : json.array()) {
        items.append(itemFromJson(entry.toObject(), selectedConversationId));
    }

    beginResetModel();
    _items = std::move(items);
    endResetModel();
}

void AssistantConversationModel::prepend(const QJsonObject &conversation, qint64 selectedConversationId)
{
    beginInsertRows({}, 0, 0);
    _items.prepend(itemFromJson(conversation, selectedConversationId));
    endInsertRows();
}

void AssistantConversationModel::select(qint64 conversationId)
{
    for (auto row = 0; row < _items.size(); ++row) {
        auto &item = _items[row];
        const auto selected = item.conversationId == conversationId;
        if (item.selected == selected) {
            continue;
        }
        item.selected = selected;
        const auto changedIndex = index(row, 0);
        emit dataChanged(changedIndex, changedIndex, {SelectedRole});
    }
}

QString AssistantConversationModel::titleForConversation(qint64 conversationId) const
{
    const auto it = std::find_if(_items.cbegin(), _items.cend(), [conversationId](const Item &item) {
        return item.conversationId == conversationId;
    });
    return it == _items.cend() ? QString{} : it->title;
}

AssistantConversationModel::Item AssistantConversationModel::itemFromJson(const QJsonObject &conversation, qint64 selectedConversationId)
{
    const auto conversationId = AssistantUtils::jsonInteger(conversation.value("id"_L1));
    const auto timestamp = AssistantUtils::jsonInteger(conversation.value("timestamp"_L1), 0);
    auto title = conversation.value("title"_L1).toString();
    if (title.isEmpty()) {
        title = AssistantUtils::dateText(timestamp);
    }
    return {conversationId, title, timestamp, conversationId == selectedConversationId};
}

}
