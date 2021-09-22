/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "UnifiedSearchResultsListModel.h"

#include "account.h"
#include "accountstate.h"
#include "guiutility.h"
#include "networkjobs.h"

#include <algorithm>

#include "UserModel.h"

#include <QAbstractListModel>

namespace OCC {

UnifiedSearchResultsListModel::UnifiedSearchResultsListModel(AccountState *accountState, QObject *parent)
    : QAbstractListModel(parent)
    , _accountState(accountState)
{
}

QVariant UnifiedSearchResultsListModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= _resultsCombined.size()) {
        return QVariant();
    }

    switch (role) {
    case CategoryNameRole: {
        return _resultsCombined.at(index.row())._categoryName;
    }
    case CategoryIdRole: {
        return _resultsCombined.at(index.row())._categoryId;
    }
    case IconRole: {
        const auto resultInfo = _resultsCombined.at(index.row());

        if (!resultInfo._icon.isEmpty()) {
            if (resultInfo._icon.contains(QStringLiteral("/"))) {
                const QUrl urlForIcon(resultInfo._icon);

                if (!urlForIcon.isValid() || urlForIcon.scheme().isEmpty()) {
                    if (const auto currentUser = UserModel::instance()->currentUser()) {
                        return QString(currentUser->server(false) + resultInfo._icon);
                    }
                }
            }

            if (resultInfo._icon.contains(QStringLiteral("folder"))) {
                return QStringLiteral(":/client/theme/black/folder.svg");
            } else if (resultInfo._icon.contains(QStringLiteral("deck"))) {
                return QStringLiteral(":/client/theme/black/deck.svg");
            } else if (resultInfo._icon.contains(QStringLiteral("calendar"))) {
                return QStringLiteral(":/client/theme/black/calendar.svg");
            } else if (resultInfo._icon.contains(QStringLiteral("mail"))) {
                return QStringLiteral(":/client/theme/black/email.svg");
            }
        }

        return resultInfo._icon;
    }
    case TitleRole: {
        return _resultsCombined.at(index.row())._title;
    }
    case SublineRole: {
        return _resultsCombined.at(index.row())._subline;
    }
    case ThumbnailUrlRole: {
        const auto resultInfo = _resultsCombined.at(index.row());

        if (resultInfo._thumbnailUrl.contains(QStringLiteral("/"))) {
            const QUrl urlForIcon(resultInfo._thumbnailUrl);

            if (!urlForIcon.isValid() || urlForIcon.scheme().isEmpty()) {
                if (const auto currentUser = UserModel::instance()->currentUser()) {
                    return QString(currentUser->server(false) + resultInfo._thumbnailUrl);
                }
            }
        }

        return resultInfo._thumbnailUrl;
    }
    case ResourceUrlRole: {
        return _resultsCombined.at(index.row())._resourceUrl;
    }
    case TypeRole: {
        return _resultsCombined.at(index.row())._type;
    }
    }

    return QVariant();
}

int UnifiedSearchResultsListModel::rowCount(const QModelIndex &) const
{
    return _resultsCombined.size();
}

QHash<int, QByteArray> UnifiedSearchResultsListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[CategoryNameRole] = "categoryName";
    roles[CategoryIdRole] = "categoryId";
    roles[IconRole] = "icon";
    roles[TitleRole] = "resultTitle";
    roles[SublineRole] = "subline";
    roles[ResourceUrlRole] = "resourceUrl";
    roles[ThumbnailUrlRole] = "thumbnailUrl";
    roles[TypeRole] = "type";
    return roles;
}

void UnifiedSearchResultsListModel::setSearchTerm(const QString &term)
{
    if (term == _searchTerm) {
        return;
    }

    disconnect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this, &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);

    if (_unifiedSearchTextEditingFinishedTimer.isActive()) {
        _unifiedSearchTextEditingFinishedTimer.stop();
    }

    _searchTerm = term;

    if (!searchTerm().isEmpty()) {
        _unifiedSearchTextEditingFinishedTimer.setInterval(400);
        connect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this, &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);
        _unifiedSearchTextEditingFinishedTimer.start();
    } else {
        for (auto& connection : _searchJobConnections) {
            if (connection) {
                QObject::disconnect(connection);
            }
        }

        _searchJobConnections.clear();

        beginResetModel();
        _resultsByCategory.clear();
        _resultsCombined.clear();
        endResetModel();
    }
}

QString UnifiedSearchResultsListModel::searchTerm() const
{
    return _searchTerm;
}

void UnifiedSearchResultsListModel::resultClicked(int resultIndex)
{
    if (resultIndex < 0 || resultIndex >= _resultsCombined.size()) {
        return;
    }

    const auto modelIndex = index(resultIndex);

    const auto categoryId = data(modelIndex, CategoryIdRole).toString();
    const auto categoryInfo = _resultsByCategory.value(categoryId, UnifiedSearchResultCategory());

    if (!categoryInfo._id.isEmpty() && categoryInfo._id == categoryId) {
        const auto type = data(modelIndex, TypeRole).toUInt();

        if (type == UnifiedSearchResult::Type::FetchMoreTrigger) {
            if (categoryInfo._isPaginated) {
                // Load more items
                const auto providerFound = _providers.find(categoryInfo._name);
                if (providerFound != _providers.end()) {
                    startSearchForProvider(*providerFound, categoryInfo._cursor);
                }
            }
        } else {
            const auto resourceUrl = QUrl(data(modelIndex, ResourceUrlRole).toString());
            if (resourceUrl.isValid()) {
                Utility::openBrowser(resourceUrl);
            }
        }
    }
}

void UnifiedSearchResultsListModel::slotSearchTermEditingFinished()
{
    disconnect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this, &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);
    QString term = searchTerm();

    if (_providers.isEmpty()) {
        auto *job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/search/providers"));
        QObject::connect(job, &JsonApiJob::jsonReceived, [&, this](const QJsonDocument &json) {
            const auto providerList = json.object().value("ocs").toObject().value("data").toVariant().toList();

            for (const auto& provider : providerList) {
                const auto providerMap = provider.toMap();
                UnifiedSearchProvider newProvider;
                newProvider._name = providerMap["name"].toString();
                if (!newProvider._name.isEmpty()) {
                    newProvider._id = providerMap["id"].toString();
                    newProvider._order = providerMap["order"].toInt();
                    _providers.insert(newProvider._name, newProvider);
                }
            }

            if (!_providers.empty()) {
                startSearch();
            }
        });
        job->start();
    } else {
        startSearch();
    }
}

void UnifiedSearchResultsListModel::slotSearchForProviderFinished(const QJsonDocument &json)
{
    if (searchTerm().isEmpty()) {
        return;
    }

    const auto data = json.object().value("ocs").toObject().value("data").toObject();
    if (!data.isEmpty()) {
        const auto dataMap = data.toVariantMap();
        const auto name = data.value("name").toString();
        const auto providerForResults = _providers.find(name);
        const auto isPaginated = data.value("isPaginated").toBool();
        const auto cursor = data.value("cursor").toInt();
        const auto entries = data.value("entries").toVariant().toList();

        if (providerForResults != _providers.end() && !entries.isEmpty()) {
            UnifiedSearchResultCategory &category = _resultsByCategory[(*providerForResults)._id];

            category._id = (*providerForResults)._id;
            category._name = (*providerForResults)._name;
            category._order = (*providerForResults)._order;
            category._isPaginated = isPaginated;
            category._cursor = cursor;

            for (const auto &entry : entries) {
                UnifiedSearchResult result;
                result._categoryId = category._id;
                result._categoryName = category._name;
                result._icon = entry.toMap()["icon"].toString();
                result._order = category._order;
                result._title = entry.toMap()["title"].toString();
                result._subline = entry.toMap()["subline"].toString();
                result._resourceUrl = entry.toMap()["resourceUrl"].toString();
                result._thumbnailUrl = entry.toMap()["thumbnailUrl"].toString();
                category._results.push_back(result);
            }

            combineResults();
        }
    }
}

void UnifiedSearchResultsListModel::startSearch()
{
    for (auto &connection : _searchJobConnections) {
        if (connection) {
            QObject::disconnect(connection);
        }
    }

    beginResetModel();
    _resultsByCategory.clear();
    _resultsCombined.clear();
    endResetModel();

    for (const auto& provider : _providers) {
        startSearchForProvider(provider);
    }
}

void UnifiedSearchResultsListModel::startSearchForProvider(const UnifiedSearchProvider &provider, qint32 cursor)
{
    auto *job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/search/providers/%1/search").arg(provider._id));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("term"), searchTerm());
    if (cursor > 0) {
        params.addQueryItem("cursor", QString::number(cursor));
    }
    job->addQueryParams(params);
    _searchJobConnections.push_back(QObject::connect(job, &JsonApiJob::jsonReceived, this, &UnifiedSearchResultsListModel::slotSearchForProviderFinished));
    job->start();
}
void UnifiedSearchResultsListModel::combineResults()
{
    QList<UnifiedSearchResult> resultsCombined;
    for (const auto &category : _resultsByCategory) {
        if (category._results.isEmpty()) {
            continue;
        }
        UnifiedSearchResult categorySeparator;
        categorySeparator._categoryId = category._id;
        categorySeparator._categoryName = category._name;
        categorySeparator._type = UnifiedSearchResult::Type::CategorySeparator;
        resultsCombined.push_back(categorySeparator);

        resultsCombined.append(category._results);

        if (category._cursor > 0) {
            UnifiedSearchResult fetchMoreTrigger;
            fetchMoreTrigger._categoryId = category._id;
            fetchMoreTrigger._categoryName = category._name;
            fetchMoreTrigger._type = UnifiedSearchResult::Type::FetchMoreTrigger;
            resultsCombined.push_back(fetchMoreTrigger);
        }
    }
    beginResetModel();
    _resultsCombined.clear();
    endResetModel();

    beginInsertRows(QModelIndex(), 0, resultsCombined.size() - 1);
    _resultsCombined = resultsCombined;
    endInsertRows();
}

}
