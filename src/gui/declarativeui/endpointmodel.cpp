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

    const auto declarativeUiMap = _accountState->account()->capabilities().declarativeUiEndpoints();
    for (auto declarativeUiApp : std::as_const(declarativeUiMap)) {
        const auto contextMenuMap = declarativeUiApp.toMap();
        for (const auto &contextMenuItem : contextMenuMap) {
            const auto contextMenuList = contextMenuItem.toList();
            for (const auto &contextMenuMap : contextMenuList) {
                const auto contextMenu = contextMenuMap.toMap();
                _endpoints.append({contextMenu.value("type").toString(),
                                   _accountState->account()->url().toString()
                                       + contextMenu.value("icon").toString(),
                                   contextMenu.value("name").toString(),
                                   contextMenu.value("url").toString(),
                                   contextMenu.value("method").toString(),
                                   contextMenu.value("mimetypeFilters").toString(),
                                   contextMenu.value("params").toString()});
            }
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
    case EndpointIconRole:
        return _endpoints.at(row).icon; // deck.svg
    case EndpointNameRole:
        return _endpoints.at(row).name; // Convert file
    case EndpointUrlRole:
        return _endpoints.at(row).url; // /ocs/v2.php/apps/declarativetest/newDeckBoard
    case EndpointMethodRole:
        return _endpoints.at(row).method; // GET
    case EndpointMimetypeFiltersRole:
        return _endpoints.at(row).mimetypeFilters; // image
    case EndpointParamsRole:
        return _endpoints.at(row).params; // filePath
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
    roles[EndpointIconRole] = "icon";
    roles[EndpointNameRole] = "name";
    roles[EndpointUrlRole] = "url";
    roles[EndpointMethodRole] = "method";
    roles[EndpointMimetypeFiltersRole] = "mimeTypeFilters";
    roles[EndpointParamsRole] = "params";

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

void EndpointModel::setResponse(const Response &response)
{
    _response = response;
    Q_EMIT responseChanged();
}

AccountState *EndpointModel::accountState() const
{
    return _accountState;
}

QString EndpointModel::localPath() const
{
    return _localPath;
}


QString EndpointModel::label() const
{
    return _response.label;
}

void EndpointModel::setLabel(const QString &label)
{
    _response.label = label;
}

QString EndpointModel::url() const
{
    return _response.url;
}

void EndpointModel::setUrl(const QString &url)
{
    _response.url = url;
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
    params.addQueryItem(_endpoints.at(row).params, 0); //fileId
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
            _response.label = child.value(QStringLiteral("element")).toString();
            _response.url = _accountState->account()->url().toString() +
                child.value(QStringLiteral("url")).toString();
        }
    }

    Q_EMIT responseChanged();
}

} // namespace OCC
