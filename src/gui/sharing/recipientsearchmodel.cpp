/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipientsearchmodel.h"

#include "account.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

RecipientSearchModel::RecipientSearchModel(QObject *parent)
    : QAbstractListModel{parent}
{}

int RecipientSearchModel::rowCount(const QModelIndex &parent) const
{
    return _searchResults.size();
}

QVariant RecipientSearchModel::data(const QModelIndex &index, int role) const
{
    const auto item = _searchResults.at(index.row());

    switch (role) {
    case TypeRole:
        return item.toObject().value("type"_L1).toString();
    case ValueRole:
        return item.toObject().value("value"_L1).toString();
    case DisplayNameRole:
        return item.toObject().value("display_name"_L1).toString();
    default:
        return {};
    }
}

QHash<int, QByteArray> RecipientSearchModel::roleNames() const
{
    return {
        { TypeRole, "type"_ba},
        { ValueRole, "value"_ba},
        { DisplayNameRole, "displayName"_ba},
    };
};

AccountState *RecipientSearchModel::accountState() const
{
    return _accountState;
}

void RecipientSearchModel::setAccountState(AccountState *accountState)
{
    if (_accountState == accountState) {
        return;
    }

    beginResetModel();
    _accountState = accountState;
    Q_EMIT accountStateChanged();
    endResetModel();
}

QString RecipientSearchModel::query() const
{
    return _query;
}

void RecipientSearchModel::setQuery(const QString &query)
{
    if (_query == query) {
        return;
    }

    qCritical() << "query set to" << query;
    _query = query;
    Q_EMIT queryChanged();

    if (_query.isEmpty()) {
        beginResetModel();
        _searchResults = {};
        endResetModel();
        return;
    }

    // TODO: start timer for search job
    auto job = _accountState->account()->sharing()->createSearchJob(_query, 0, 10, this);
    connect(job, &OCC::JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) -> void {
        beginResetModel();
        _searchResults = json.object().value("ocs"_L1).toObject().value("data"_L1).toArray();
        endResetModel();
    });
    job->start();
}
