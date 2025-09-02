/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "endpointmodel.h"

namespace OCC {

EndpointModel::EndpointModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

EndpointModel::EndpointType EndpointModel::stringToEnum(const QString &type)
{
    if (type == QStringLiteral("context-menu")) {
        return EndpointType::ContextMenuRole;
    }

    if (type == QStringLiteral("create-new")) {
        return EndpointType::CreateMenuRole;
    }

    return {};
}

void EndpointModel::parseElements(const QVariantList &elementsList)
{
    for (const auto &element : elementsList) {
        const auto elementMap = element.toMap();
        const auto type = elementMap.value("type").toString();
        const auto endpoints = elementMap.value("endpoints").toList();
        for (const auto &endpoint : endpoints) {
            const auto element = endpoint.toMap();
            _endpoints.append({stringToEnum(type),
                               element.value("name").toString(),
                               element.value("url").toString()});
        }
    }
}

QVariant EndpointModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    switch (role) {
    case EndpointTypeRole:
        return _endpoints.at(index.row()).type;
    case EndpointNameRole:
        return _endpoints.at(index.row()).name;
    case EndpointUrlRole:
        return _endpoints.at(index.row()).url;
    }

    return {};
}

int EndpointModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _endpoints.size();
}

QHash<int, QByteArray> EndpointModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[EndpointTypeRole] = "endpointType";
    roles[EndpointNameRole] = "endpointName";
    roles[EndpointUrlRole] = "endpointUrl";

    return roles;
}

} // namespace OCC
