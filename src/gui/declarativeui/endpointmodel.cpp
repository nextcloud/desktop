/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "endpointmodel.h"
#include "networkjobs.h"
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
                               element.value("url").toString(),
                               element.value("desktop_icon").toString(),
                               element.value("filter").toString(),
                               element.value("parameter").toString()});
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
    case EndpointIconRole:
        return _endpoints.at(row).icon; // zip
    case EndpointFilterRole:
        return _endpoints.at(row).filter; // image/
    case EndpointParameterRole:
        return _endpoints.at(row).parameter; // fileId
    case EndpointVerbRole:
        return _endpoints.at(row).verb; // POST, GET
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
    roles[EndpointIconRole] = "icon";
    roles[EndpointFilterRole] = "filter";
    roles[EndpointParameterRole] = "parameter";
    roles[EndpointVerbRole] = "verb";

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

void EndpointModel::createRequest(const int row)
{
    if (!_accountState) {
        return;
    }

    auto job = new JsonApiJob(_accountState->account(),
                                _endpoints.at(row).url,
                                this);
    connect(job, &JsonApiJob::jsonReceived,
            this, &EndpointModel::processRequest);
    QUrlQuery params;
    params.addQueryItem(_endpoints.at(row).parameter, 0); //fileId
    job->addQueryParams(params);
    job->setVerb(SimpleApiJob::Verb::Post); //fixit _endpoints.at(row).verb
    job->start();
}

void EndpointModel::processRequest(const QJsonDocument &json)
{
    const auto root = json.object().value(QStringLiteral("root")).toObject();
    if (root.empty()) {
        return;
    }
    const auto orientation = root.value(QStringLiteral("orientation")).toString();
    const auto rows = root.value(QStringLiteral("rows")).toArray();
    if (rows.empty()) {
        return;
    }

    for (const auto &rowValue : rows) {
        const auto row = rowValue.toObject();
        const auto children = row.value("children").toArray();

        for (const auto &childValue : children) {
            const auto child = childValue.toObject();
            _response.name = child.value(QStringLiteral("element")).toString();
            _response.type = child.value(QStringLiteral("type")).toString();
            _response.label = child.value(QStringLiteral("label")).toString();
            _response.url = _accountState->account()->url().toString() +
                child.value(QStringLiteral("url")).toString();
            _response.text = child.value(QStringLiteral("text")).toString();
        }
    }
}

} // namespace OCC
