/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipientsearchmodel.h"

#include <QJsonObject>
#include <QLoggingCategory>

#include "searchrecipientsjob.h"
#include "recipienticonutils.h"

Q_LOGGING_CATEGORY(lcSharingRecipientShareModel, "nextcloud.gui.sharing.recipientsearchmodel", QtInfoMsg)

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

namespace
{
constexpr auto searchDelayMsec = 300;
}

RecipientSearchModel::RecipientSearchModel(QObject *parent)
    : QAbstractListModel{parent}
{
    _searchTimer.setSingleShot(true);
    _searchTimer.setInterval(searchDelayMsec);
    connect(&_searchTimer, &QTimer::timeout, this, &RecipientSearchModel::search);
}

int RecipientSearchModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _searchResults.size();
}

QVariant RecipientSearchModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid));

    const auto item = _searchResults.at(index.row()).toObject();
    const auto icon = item.value("icon"_L1).toObject();

    switch (role) {
    case TypeRole:
        return item.value("class"_L1).toString();
    case ValueRole:
        return item.value("value"_L1).toString();
    case DisplayNameRole:
        return item.value("display_name"_L1).toString();
    case InstanceRole:
        return item.value("instance"_L1).toVariant();
    case IconSvgUrlRole:
        return RecipientIconUtils::svgDataUrl(icon.value("svg"_L1).toString());
    case IconLightRole:
        return icon.value("light"_L1).toString();
    case IconDarkRole:
        return icon.value("dark"_L1).toString();
    default:
        return {};
    }
}

QHash<int, QByteArray> RecipientSearchModel::roleNames() const
{
    return {
        {TypeRole, "type"_ba},
        {ValueRole, "value"_ba},
        {DisplayNameRole, "displayName"_ba},
        {InstanceRole, "instance"_ba},
        {IconSvgUrlRole, "iconSvgUrl"_ba},
        {IconLightRole, "iconLight"_ba},
        {IconDarkRole, "iconDark"_ba},
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
    _searchTimer.stop();
    _account = account;
    _searchResults = {};
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
        _searchTimer.stop();
        beginResetModel();
        _searchResults = {};
        endResetModel();
        return;
    }

    _searchTimer.start();
}

QString RecipientSearchModel::shareId() const
{
    return _shareId;
}

void RecipientSearchModel::setShareId(const QString &shareId)
{
    if (_shareId == shareId) {
        return;
    }

    _shareId = shareId;
    beginResetModel();
    _searchResults = {};
    endResetModel();
    Q_EMIT shareIdChanged();
    if (!_query.isEmpty()) {
        _searchTimer.start();
    }
}

void RecipientSearchModel::search()
{
    const auto query = _query;
    const auto account = _account;
    const auto shareId = _shareId;
    const auto job = new SearchRecipientsJob{account,
                                             query,
                                             0,
                                             10,
                                             {},
                                             shareId.isEmpty() ? std::nullopt : std::optional{shareId}};
    connect(job, &SearchRecipientsJob::recipientsFound, this, [this, account, query, shareId](const QJsonArray &recipients) {
        if (_account != account || _query != query || _shareId != shareId) {
            return;
        }

        beginResetModel();
        _searchResults = recipients;
        endResetModel();
    });
    job->start();
}
