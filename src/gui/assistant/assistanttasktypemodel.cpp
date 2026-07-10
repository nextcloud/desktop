/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "assistanttasktypemodel.h"

#include <QJsonObject>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace OCC {

namespace {

constexpr auto chatTaskTypeId = "core:text2text:chat"_L1;

bool isSingleTextTask(const QJsonObject &inputShape, const QJsonObject &outputShape)
{
    if (inputShape.size() != 1 || outputShape.size() != 1) {
        return false;
    }

    const auto inputType = inputShape.constBegin().value().toObject().value("type"_L1).toString();
    const auto outputType = outputShape.constBegin().value().toObject().value("type"_L1).toString();
    return inputType == "Text"_L1 && outputType == "Text"_L1;
}

}

AssistantTaskTypeModel::AssistantTaskTypeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AssistantTaskTypeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _items.size();
}

QVariant AssistantTaskTypeModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const auto &item = _items.at(index.row());
    switch (role) {
    case TypeIdRole:
        return item.typeId;
    case NameRole:
        return item.name;
    case DescriptionRole:
        return item.description;
    case IsChatRole:
        return item.isChat;
    }
    return {};
}

QHash<int, QByteArray> AssistantTaskTypeModel::roleNames() const
{
    static const auto roles = QHash<int, QByteArray>{
        {TypeIdRole, "typeId"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {IsChatRole, "isChat"},
    };
    return roles;
}

void AssistantTaskTypeModel::replaceFromResponse(const QJsonDocument &json)
{
    auto items = QList<Item>{};
    const auto types = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject().value("types"_L1).toObject();
    for (const auto &typeId : types.keys()) {
        const auto typeObject = types.value(typeId).toObject();
        const auto inputShape = typeObject.value("inputShape"_L1).toObject();
        const auto outputShape = typeObject.value("outputShape"_L1).toObject();
        const auto isChat = typeId == chatTaskTypeId;
        const auto isTranslate = typeId.contains("translate"_L1, Qt::CaseInsensitive);
        if (!isChat && !isTranslate && !isSingleTextTask(inputShape, outputShape)) {
            continue;
        }

        auto name = typeObject.value("name"_L1).toString();
        if (name.isEmpty()) {
            name = isChat ? tr("Chat") : typeId;
        }
        items.append({typeId, name, typeObject.value("description"_L1).toString(), isChat});
    }

    std::sort(items.begin(), items.end(), [](const Item &left, const Item &right) {
        if (left.isChat != right.isChat) {
            return left.isChat;
        }
        return left.name.localeAwareCompare(right.name) < 0;
    });

    beginResetModel();
    _items = std::move(items);
    endResetModel();
}

bool AssistantTaskTypeModel::contains(const QString &typeId) const
{
    return std::any_of(_items.cbegin(), _items.cend(), [&typeId](const Item &item) {
        return item.typeId == typeId;
    });
}

QString AssistantTaskTypeModel::firstTypeId() const
{
    return _items.isEmpty() ? QString{} : _items.constFirst().typeId;
}

QString AssistantTaskTypeModel::nameForType(const QString &typeId) const
{
    const auto it = std::find_if(_items.cbegin(), _items.cend(), [&typeId](const Item &item) {
        return item.typeId == typeId;
    });
    return it == _items.cend() ? QString{} : it->name;
}

QString AssistantTaskTypeModel::descriptionForType(const QString &typeId) const
{
    const auto it = std::find_if(_items.cbegin(), _items.cend(), [&typeId](const Item &item) {
        return item.typeId == typeId;
    });
    return it == _items.cend() ? QString{} : it->description;
}

}
