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

#include <QAbstractListModel>

namespace OCC {

UnifiedSearchResultsListModel::UnifiedSearchResultsListModel(AccountState *accountState, QObject *parent)
    : QAbstractListModel(parent)
    , _accountState(accountState)
{
    beginInsertRows(QModelIndex(), 0, 5);
    UnifiedSearchResult filesCategorySeparator;
    filesCategorySeparator._categoryId = "files";
    filesCategorySeparator._categoryName = "Files";
    filesCategorySeparator._isCategorySeparator = true;
    _resultsCombined.push_back(filesCategorySeparator);

    UnifiedSearchResult fakeFileResult;
    fakeFileResult._title = "Fake file result";
    fakeFileResult._subline = "Subline for Fake file result";
    fakeFileResult._categoryId = "files";
    fakeFileResult._categoryName = "Files";

    UnifiedSearchResult fetchMoreFileResultsTrigger;
    fetchMoreFileResultsTrigger._categoryId = "files";
    fetchMoreFileResultsTrigger._isFetchMoreTrigger = true;

    _resultsCombined.push_back(fakeFileResult);
    _resultsCombined.push_back(fetchMoreFileResultsTrigger);

    UnifiedSearchResult talkMessagesCategorySeparator;
    talkMessagesCategorySeparator._categoryId = "talk_messages";
    talkMessagesCategorySeparator._categoryName = "Messages";
    talkMessagesCategorySeparator._isCategorySeparator = true;
    _resultsCombined.push_back(talkMessagesCategorySeparator);

    UnifiedSearchResult fakeTalkMessagesResult;
    fakeTalkMessagesResult._title = "Fake Talk messages result";
    fakeTalkMessagesResult._subline = "Subline for Fake Talk messages result";
    fakeTalkMessagesResult._categoryId = "talk_messages";
    fakeTalkMessagesResult._categoryName = "Messages";

    UnifiedSearchResult fetchMoreTalkMessagesTrigger;
    fetchMoreTalkMessagesTrigger._categoryId = "talk_messages";
    fetchMoreTalkMessagesTrigger._isFetchMoreTrigger = true;

    _resultsCombined.push_back(fakeTalkMessagesResult);
    _resultsCombined.push_back(fetchMoreTalkMessagesTrigger);
    endInsertRows();
}

UnifiedSearchResultsListModel::~UnifiedSearchResultsListModel()
{
    int a = 5;
    a = 6;
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
    case TitleRole: {
        return _resultsCombined.at(index.row())._title;
    }
    case SublineRole: {
        return _resultsCombined.at(index.row())._subline;
    }
    case ThumbnailUrlRole: {
        return _resultsCombined.at(index.row())._thumbnailUrl;
    }
    case ResourceUrlRole: {
        return _resultsCombined.at(index.row())._resourceUrl;
    }
    case IsFetchMoreTrigger: {
        return _resultsCombined.at(index.row())._isFetchMoreTrigger;
    }
    case IsCategorySeparator: {
        return _resultsCombined.at(index.row())._isCategorySeparator;
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
    roles[TitleRole] = "resultTitle";
    roles[SublineRole] = "subline";
    roles[ResourceUrlRole] = "resourceUrl";
    roles[ThumbnailUrlRole] = "thumbnailUrl";
    roles[IsFetchMoreTrigger] = "isFetchMoreTrigger";
    roles[IsCategorySeparator] = "isCategorySeparator";
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
        const auto isFetchMoreTrigger = data(modelIndex, IsFetchMoreTrigger).toBool();

        if (isFetchMoreTrigger) {
            if (categoryInfo._isPaginated) {
                // Load more items
                const auto providerFound = _providers.find(categoryInfo._name);
                if (providerFound != _providers.end()) {
                    startSearchForProvider(*providerFound, categoryInfo._cursor * 2);
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

void UnifiedSearchResultsListModel::startSearch()
{
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
        params.addQueryItem("cursor", QString(cursor));
    }
    job->addQueryParams(params);
    QObject::connect(job, &JsonApiJob::jsonReceived, [&, provider](const QJsonDocument &json) {
        const auto data = json.object().value("ocs").toObject().value("data").toObject();
        if (!data.isEmpty()) {
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
    });
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
        categorySeparator._isCategorySeparator = true;
        resultsCombined.push_back(categorySeparator);

        resultsCombined.append(category._results);

        UnifiedSearchResult fetchMoreTrigger;
        fetchMoreTrigger._categoryId = category._id;
        fetchMoreTrigger._categoryName = category._name;
        fetchMoreTrigger._isFetchMoreTrigger = true;
        resultsCombined.push_back(fetchMoreTrigger);
    }
    beginResetModel();
    _resultsCombined.clear();
    endResetModel();

    beginInsertRows(QModelIndex(), 0, resultsCombined.size() - 1);
    _resultsCombined = resultsCombined;
    endInsertRows();
}

}
