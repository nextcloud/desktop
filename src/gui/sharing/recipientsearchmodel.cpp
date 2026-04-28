/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipientsearchmodel.h"

#include <QLoggingCategory>
#include <QJsonObject>

#include "ocssharingjob.h"

Q_LOGGING_CATEGORY(lcSharingRecipientShareModel, "nextcloud.gui.sharing.recipientsearchmodel", QtInfoMsg)

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
        return item.toObject().value("class"_L1).toString();
    case ValueRole:
        return item.toObject().value("value"_L1).toString();
    case DisplayNameRole:
        return item.toObject().value("display_name"_L1).toString();
    case IconRole:
        return "image://tray-image-provider/%1"_L1.arg(item.toObject().value("icon"_L1).toObject().value("light").toString());
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
        { IconRole, "iconUrl"_ba},
    };
};

AccountPtr RecipientSearchModel::account() const
{
    return _account;
}

void RecipientSearchModel::setAccount(AccountPtr account)
{
    if (_account == account) {
        return;
    }

    beginResetModel();
    _account = account;
    Q_EMIT accountChanged();
    endResetModel();
}

QString RecipientSearchModel::query() const
{
    return _query;
}

void RecipientSearchModel::setQuery(const QString &query)
{
    if (!_account) {
        return;
    }

    if (_query == query) {
        return;
    }

    qCDebug(lcSharingRecipientShareModel) << "query set to" << query;
    _query = query;
    Q_EMIT queryChanged();

    if (_query.isEmpty()) {
        beginResetModel();
        _searchResults = {};
        endResetModel();
        return;
    }

    // TODO: start timer for search job
    auto job = new OcsSharingJob(_account);
    connect(job, &OcsSharingJob::jobFinished, this, [this](const QJsonDocument &json, int statusCode) -> void {
        beginResetModel();
        _searchResults = json.object().value("ocs"_L1).toObject().value("data"_L1).toArray();
        endResetModel();
    });
    job->searchRecipients(query, 0, 10);
}
