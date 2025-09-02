/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "endpointmodel.h"
#include "account.h"

namespace OCC {

EndpointModel::EndpointModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void EndpointModel::parseEndpoints()
{
    if (!_accountState->isConnected()) {
        return;
    }

    const auto elementsList = _accountState->account()->capabilities().declarativeUiEndpoints();
    for (const auto &element : elementsList) {
        const auto elementMap = element.toMap();
        const auto type = elementMap.value("type").toString(); // context-menu, create-new
        const auto endpoints = elementMap.value("endpoints").toList();
        for (const auto &endpoint : endpoints) {
            const auto element = endpoint.toMap();
            _endpoints.append({element.value("type").toString(),
                               element.value("name").toString(),
                               element.value("url").toString()});
        }
    }

    Q_EMIT endpointModelChanged();
}

QVariant EndpointModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    const auto row = index.row();
    switch (role) {
    case EndpointTypeRole:
        return _endpoints.at(row).type; //context-menu, create-new
    case EndpointNameRole:
        return _endpoints.at(row).name; // Deck board
    case EndpointUrlRole:
        return _endpoints.at(row).url; // /ocs/v2.php/apps/declarativetest/newDeckBoard
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
    roles[EndpointTypeRole] = "type";
    roles[EndpointNameRole] = "name";
    roles[EndpointUrlRole] = "url";

    return roles;
}

void EndpointModel::setAccountState(AccountState *accountState)
{
    if (accountState == nullptr) {
        return;
    }

    if (accountState == _accountState) {
        return;
    }

    _accountState = accountState;
    parseEndpoints();
    Q_EMIT accountStateChanged();
}

void EndpointModel::setLocalPath(const QString &localPath)
{
    if (localPath.isEmpty()) {
        return;
    }

    if (localPath == _localPath) {
        return;
    }

    _localPath = localPath;
    Q_EMIT localPathChanged();
}

AccountState *EndpointModel::accountState() const
{
    return _accountState;
}

QString EndpointModel::localPath() const
{
    return _localPath;
}

} // namespace OCC
