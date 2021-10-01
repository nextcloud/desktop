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
#include <QDesktopServices>

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
    case ImagePlaceholderRole: {
        const auto resultInfo = _resultsCombined.at(index.row());

        if (resultInfo._categoryId.contains(QStringLiteral("message"))
            || resultInfo._categoryId.contains(QStringLiteral("talk"))) {
            return QStringLiteral("qrc:///client/theme/black/wizard-talk.svg");
        } else if (resultInfo._categoryId.contains(QStringLiteral("file"))) {
            return QStringLiteral("qrc:///client/theme/black/edit.svg");
        } else if (resultInfo._categoryId.contains(QStringLiteral("calendar"))) {
            return QStringLiteral("qrc:///client/theme/black/calendar.svg");
        } else if (resultInfo._categoryId.contains(QStringLiteral("mail"))) {
            return QStringLiteral("qrc:///client/theme/black/email.svg");
        } else if (resultInfo._categoryId.contains(QStringLiteral("comment"))) {
            return QStringLiteral("qrc:///client/theme/account.svg");
        }

        return QString();
    }
    case ImagesRole: {
        return _resultsCombined.at(index.row())._images;
    }
    case IconRole: {
        return _resultsCombined.at(index.row())._icon;
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
    case RoundedRole: {
        return _resultsCombined.at(index.row())._isRounded;
    }
    case TypeRole: {
        return _resultsCombined.at(index.row())._type;
    }
    case TypeAsStringRole: {
        return UnifiedSearchResult::typeAsString(_resultsCombined.at(index.row())._type);
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
    roles[ImagesRole] = "images";
    roles[ImagePlaceholderRole] = "imagePlaceholder";
    roles[TitleRole] = "resultTitle";
    roles[SublineRole] = "subline";
    roles[ResourceUrlRole] = "resourceUrl";
    roles[ThumbnailUrlRole] = "thumbnailUrl";
    roles[TypeRole] = "type";
    roles[TypeAsStringRole] = "typeAsString";
    roles[RoundedRole] = "isRounded";
    return roles;
}

QString UnifiedSearchResultsListModel::searchTerm() const
{
    return _searchTerm;
}

void UnifiedSearchResultsListModel::setSearchTerm(const QString &term)
{
    if (term == _searchTerm) {
        return;
    }

    _searchTerm = term;
    emit searchTermChanged();

    if (!_errorString.isEmpty()) {
        _errorString.clear();
        emit errorStringChanged();
    }

    disconnect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this,
        &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);

    if (_unifiedSearchTextEditingFinishedTimer.isActive()) {
        _unifiedSearchTextEditingFinishedTimer.stop();
    }

    if (!_searchTerm.isEmpty()) {
        _unifiedSearchTextEditingFinishedTimer.setInterval(800);
        connect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this,
            &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);
        _unifiedSearchTextEditingFinishedTimer.start();
    } else {
        for (auto &connection : _searchJobConnections) {
            if (connection) {
                QObject::disconnect(connection);
            }
        }

        const auto numSearchJobConections = _searchJobConnections.size();
        _searchJobConnections.clear();

        if (numSearchJobConections > 0) {
            emit isSearchInProgressChanged();
        }

        beginResetModel();
        _resultsCombined.clear();
        endResetModel();
    }
}

bool UnifiedSearchResultsListModel::isSearchInProgress() const
{
    return _searchJobConnections.size() > 0;
}

void UnifiedSearchResultsListModel::resultClicked(int resultIndex)
{
    if (resultIndex < 0 || resultIndex >= _resultsCombined.size()) {
        return;
    }

    if (isSearchInProgress()) {
        return;
    }

    const auto modelIndex = index(resultIndex);

    const auto categoryId = data(modelIndex, CategoryIdRole).toString();

    const auto foundProviderIt = std::find_if(std::begin(_providers), std::end(_providers),
        [&categoryId](const UnifiedSearchProvider &current) { return current._id == categoryId; });

    const auto providerInfo = foundProviderIt != std::end(_providers) ? *foundProviderIt : UnifiedSearchProvider();

    if (!providerInfo._id.isEmpty() && providerInfo._id == categoryId) {
        const auto type = data(modelIndex, TypeRole).toUInt();

        if (type == UnifiedSearchResult::Type::FetchMoreTrigger) {
            if (providerInfo._isPaginated) {
                // Load more items
                const auto providerFound = _providers.find(providerInfo._name);
                if (providerFound != _providers.end()) {
                    _currentFetchMoreInProgressCategoryId = providerInfo._id;
                    emit currentFetchMoreInProgressCategoryIdChanged();
                    startSearchForProvider(*providerFound, providerInfo._cursor);
                }
            }
        } else {
            const auto resourceUrl = QUrl(data(modelIndex, ResourceUrlRole).toString());
            if (resourceUrl.isValid()) {
                if (categoryId.contains("file")) {
                    const auto urlQuery = QUrlQuery(resourceUrl);
                    const auto dir = urlQuery.queryItemValue(QStringLiteral("dir"), QUrl::ComponentFormattingOption::FullyDecoded);
                    const auto fileName = urlQuery.queryItemValue(QStringLiteral("scrollto"), QUrl::ComponentFormattingOption::FullyDecoded);

                    if (!dir.isEmpty() && !fileName.isEmpty()) {
                        const QString relativePath = dir + QLatin1Char('/') + fileName;
                        if (!relativePath.isEmpty()) {
                            const auto localFiles = FolderMan::instance()->findFileInLocalFolders(QFileInfo(relativePath).path(), _accountState->account());

                            if (!localFiles.isEmpty()) {
                                QDesktopServices::openUrl(localFiles.constFirst());
                                return;
                            }
                        }
                    }
                }
                Utility::openBrowser(resourceUrl);
            }
        }
    }
}

void UnifiedSearchResultsListModel::slotSearchTermEditingFinished()
{
    disconnect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this,
        &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);

    if (_providers.isEmpty()) {
        auto *job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/search/providers"));
        QObject::connect(job, &JsonApiJob::jsonReceived, [&, this](const QJsonDocument &json, int statusCode) {
            if (statusCode != 200) {
                _errorString += tr("Failed to fetch search providers for '%1'. Error: %2").arg(_searchTerm).arg(job->errorString()) + QLatin1Char('\n');
                emit errorStringChanged();
                return;
            }
            const auto providerList = json.object()
                                          .value(QStringLiteral("ocs"))
                                          .toObject()
                                          .value(QStringLiteral("data"))
                                          .toVariant()
                                          .toList();

            for (const auto &provider : providerList) {
                const auto providerMap = provider.toMap();
                const auto id = providerMap[QStringLiteral("id")].toString();
                const auto name = providerMap[QStringLiteral("name")].toString();
                UnifiedSearchProvider newProvider;
                if (!name.isEmpty() && id != QStringLiteral("talk-message-current")) {
                    newProvider._name = name;
                    newProvider._id = id;
                    newProvider._order = providerMap[QStringLiteral("order")].toInt();
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

void UnifiedSearchResultsListModel::slotSearchForProviderFinished(const QJsonDocument &json, int statusCode)
{
    bool appendResults = false;

    if (const auto job = qobject_cast<JsonApiJob *>(sender())) {
        appendResults = job->property("appendResults").toBool();
        const auto providerId = job->property("providerId").toString();
        const auto numJobConnections = _searchJobConnections.size();
        _searchJobConnections.remove(providerId);

        if (numJobConnections != 0 && _searchJobConnections.size() == 0) {
            emit isSearchInProgressChanged();
        }

        if (!_currentFetchMoreInProgressCategoryId.isEmpty()) {
            _currentFetchMoreInProgressCategoryId.clear();
            emit currentFetchMoreInProgressCategoryIdChanged();
        }

        if (statusCode != 200) {
            _errorString += tr("Search has failed for '%1'. Error: %2").arg(_searchTerm).arg(job->errorString()) + QLatin1Char('\n');
            emit errorStringChanged();
            return;
        }
    }

    if (_searchTerm.isEmpty()) {
        return;
    }

    QList<UnifiedSearchResult> newEntries;

    const auto data = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
    if (!data.isEmpty()) {
        const auto dataMap = data.toVariantMap();
        const auto name = data.value(QStringLiteral("name")).toString();
        auto &providerForResults = _providers[name];
        const auto isPaginated = data.value(QStringLiteral("isPaginated")).toBool();
        const auto cursor = data.value(QStringLiteral("cursor")).toInt();
        const auto entries = data.value(QStringLiteral("entries")).toVariant().toList();

        if (!providerForResults._id.isEmpty() && !entries.isEmpty()) {
            providerForResults._isPaginated = isPaginated;
            providerForResults._cursor = cursor;

            if (providerForResults._pageSize == -1) {
                providerForResults._pageSize = cursor;
            }

            if (providerForResults._pageSize != -1 && entries.size() < providerForResults._pageSize) {
                // for some providers we are still getting a non-null cursor and isPaginated true even thought there are
                // no more results to paginate
                providerForResults._isPaginated = false;
            }

            for (const auto &entry : entries) {
                UnifiedSearchResult result;
                result._categoryId = providerForResults._id;
                result._order = providerForResults._order;
                result._categoryName = providerForResults._name;
                result._icon = entry.toMap().value(QStringLiteral("icon")).toString();

                if (result._icon.contains(QLatin1Char('/')) || result._thumbnailUrl.contains(QLatin1Char('\\'))) {
                    const QUrl urlForIcon(result._icon);

                    if (!urlForIcon.isValid() || urlForIcon.scheme().isEmpty()) {
                        if (const auto currentUser = UserModel::instance()->currentUser()) {
                            auto serverUrl = QUrl(currentUser->server(false));
                            // some icons may contain parameters after (?)
                            const QStringList iconPathSplitted = result._icon.contains(QLatin1Char('?'))
                                ? result._icon.split(QLatin1Char('?'))
                                : QStringList{result._icon};
                            serverUrl.setPath(iconPathSplitted[0]);
                            result._icon = serverUrl.toString();
                            if (iconPathSplitted.size() > 1) {
                                result._icon += QLatin1Char('?') + iconPathSplitted[1];
                            }
                        }
                    }
                } else {
                    const QUrl urlForIcon(result._icon);

                    if (!urlForIcon.isValid() || urlForIcon.scheme().isEmpty()) {
                        if (result._icon.contains(QStringLiteral("folder"))) {
                            result._icon = QStringLiteral(":/client/theme/black/folder.svg");
                        } else if (result._icon.contains(QStringLiteral("deck"))) {
                            result._icon = QStringLiteral(":/client/theme/black/deck.svg");
                        } else if (result._icon.contains(QStringLiteral("calendar"))) {
                            result._icon = QStringLiteral(":/client/theme/black/calendar.svg");
                        } else if (result._icon.contains(QStringLiteral("mail"))) {
                            result._icon = QStringLiteral(":/client/theme/black/email.svg");
                        }
                    }
                }

                result._isRounded = entry.toMap().value(QStringLiteral("rounded")).toBool();
                result._title = entry.toMap().value(QStringLiteral("title")).toString();
                result._subline = entry.toMap().value(QStringLiteral("subline")).toString();
                result._resourceUrl = entry.toMap().value(QStringLiteral("resourceUrl")).toString();
                result._thumbnailUrl = entry.toMap().value(QStringLiteral("thumbnailUrl")).toString();

                if (result._thumbnailUrl.contains(QLatin1Char('/'))
                    || result._thumbnailUrl.contains(QLatin1Char('\\'))) {
                    const QUrl urlForIcon(result._thumbnailUrl);

                    if (!urlForIcon.isValid() || urlForIcon.scheme().isEmpty()) {
                        if (const auto currentUser = UserModel::instance()->currentUser()) {
                            auto serverUrl = QUrl(currentUser->server(false));
                            // some icons may contain parameters after (?)
                            const QStringList thumbnailUrlSplitted = result._thumbnailUrl.contains(QLatin1Char('?'))
                                ? result._thumbnailUrl.split(QLatin1Char('?'))
                                : QStringList{result._thumbnailUrl};
                            serverUrl.setPath(thumbnailUrlSplitted[0]);
                            result._thumbnailUrl = serverUrl.toString();
                            if (thumbnailUrlSplitted.size() > 1) {
                                result._thumbnailUrl += QLatin1Char('?') + thumbnailUrlSplitted[1];
                            }
                        }
                    }
                }

                const QStringList listImages = {result._thumbnailUrl, result._icon};
                result._images = listImages.join(QLatin1Char(';'));

                newEntries.push_back(result);
            }

            if (appendResults) {
                appendResultsToProvider(providerForResults, newEntries);
            } else {
                combineResults(newEntries, providerForResults);
            }
        } else if (entries.isEmpty() && !providerForResults._id.isEmpty()) {
            // we may have received false pagination information from the server, such as, we expect more results
            // available via pagination, but, there are no more left, so, we need to stop paginating for this
            // provider

            providerForResults._isPaginated = false;

            if (appendResults) {
                appendResultsToProvider(providerForResults, {});
            }
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
    _resultsCombined.clear();
    endResetModel();

    for (const auto &provider : _providers) {
        startSearchForProvider(provider);
    }
}

void UnifiedSearchResultsListModel::startSearchForProvider(const UnifiedSearchProvider &provider, qint32 cursor)
{
    auto *job = new JsonApiJob(
        _accountState->account(), QLatin1String("ocs/v2.php/search/providers/%1/search").arg(provider._id));
    QUrlQuery params;
    params.addQueryItem(QStringLiteral("term"), _searchTerm);
    if (cursor > 0) {
        params.addQueryItem(QStringLiteral("cursor"), QString::number(cursor));
        job->setProperty("appendResults", true);
    }
    job->setProperty("providerId", provider._id);
    job->addQueryParams(params);
    const auto numSearchJobConnections = _searchJobConnections.size();
    _searchJobConnections.insert(provider._id,
        QObject::connect(
            job, &JsonApiJob::jsonReceived, this, &UnifiedSearchResultsListModel::slotSearchForProviderFinished));
    if (_searchJobConnections.size() > 0 && numSearchJobConnections == 0) {
        emit isSearchInProgressChanged();
    }
    job->start();
}

void UnifiedSearchResultsListModel::combineResults(const QList<UnifiedSearchResult> &newEntries, const UnifiedSearchProvider &provider)
{;
    auto newEntriesCopy = newEntries;

    const auto newEntriesOrder = newEntriesCopy.first()._order;
    const auto newEntriesName = newEntriesCopy.first()._categoryName;

    UnifiedSearchResult categorySeparator;
    categorySeparator._categoryId = newEntries.first()._categoryId;
    categorySeparator._categoryName = newEntriesName;
    categorySeparator._order = newEntriesOrder;
    categorySeparator._type = UnifiedSearchResult::Type::CategorySeparator;

    newEntriesCopy.push_front(categorySeparator);


    if (provider._cursor > 0 && provider._isPaginated) {
        UnifiedSearchResult fetchMoreTrigger;
        fetchMoreTrigger._categoryId = provider._id;
        fetchMoreTrigger._categoryName = provider._name;
        fetchMoreTrigger._order = newEntriesOrder;
        fetchMoreTrigger._type = UnifiedSearchResult::Type::FetchMoreTrigger;
        newEntriesCopy.push_back(fetchMoreTrigger);
    }


    if (_resultsCombined.isEmpty()) {
        beginInsertRows(QModelIndex(), 0, newEntriesCopy.size() - 1);
        _resultsCombined = newEntriesCopy;
        endInsertRows();
        return;
    }

    auto itToInsertTo = std::find_if(std::begin(_resultsCombined), std::end(_resultsCombined), [newEntriesOrder, newEntriesName](const UnifiedSearchResult &current) {
        if (current._order > newEntriesOrder) {
            return true;
        } else {
            if (current._order == newEntriesOrder) {
                return current._categoryName > newEntriesName;
            }

            return false;
        }
    });

    if (itToInsertTo != std::end(_resultsCombined)) {
        const auto first = itToInsertTo - std::begin(_resultsCombined);
        const auto last = first + newEntriesCopy.size() - 1;

        beginInsertRows(QModelIndex(), first, last);
        std::copy(std::begin(newEntriesCopy), std::end(newEntriesCopy), std::inserter(_resultsCombined, itToInsertTo));
        endInsertRows();
    } else {
        const auto first = _resultsCombined.size();
        const auto last = first + newEntriesCopy.size() - 1;

        beginInsertRows(QModelIndex(), first, last);
        std::copy(std::begin(newEntriesCopy), std::end(newEntriesCopy), std::back_inserter(_resultsCombined));
        endInsertRows();
    }
}

void UnifiedSearchResultsListModel::appendResultsToProvider(const UnifiedSearchProvider &provider, const QList<UnifiedSearchResult> &results)
{
    // Let's insert new results
    if (results.size() > 0) {
        const auto foundLastResultForProviderReverse = std::find_if(
            std::rbegin(_resultsCombined), std::rend(_resultsCombined), [&provider](const UnifiedSearchResult &result) {
                return result._categoryId == provider._id && result._type == UnifiedSearchResult::Type::Default;
            });

        if (foundLastResultForProviderReverse != std::rend(_resultsCombined)) {

            const auto numRowsToInsert = results.size();
            const auto foundLastResultForProvider = (foundLastResultForProviderReverse + 1).base();
            const auto first = foundLastResultForProvider + 1 - std::begin(_resultsCombined);
            const auto last = first + numRowsToInsert - 1;
            beginInsertRows(QModelIndex(), first, last);
            std::copy(std::begin(results), std::end(results),
                std::inserter(_resultsCombined, foundLastResultForProvider + 1));
            if (provider._isPaginated) {
                const auto foundLastResultForProviderReverse = std::find_if(std::rbegin(_resultsCombined),
                    std::rend(_resultsCombined), [&provider](const UnifiedSearchResult &result) {
                        return result._categoryId == provider._id
                            && result._type == UnifiedSearchResult::Type::FetchMoreTrigger;
                    });
            }
            endInsertRows();
            if (!provider._isPaginated) {
                // Let's remove the FetchMoreTrigger item if present
                const auto providerId = provider._id;
                const auto foudFetchMoreTriggerForProviderReverse = std::find_if(std::rbegin(_resultsCombined),
                    std::rend(_resultsCombined), [providerId](const UnifiedSearchResult &result) {
                        return result._categoryId == providerId
                            && result._type == UnifiedSearchResult::Type::FetchMoreTrigger;
                    });

                if (foudFetchMoreTriggerForProviderReverse != std::rend(_resultsCombined)) {
                    const auto foundFetchMoreTriggerForProvider = (foudFetchMoreTriggerForProviderReverse + 1).base();

                    if (foundFetchMoreTriggerForProvider != std::end(_resultsCombined)
                        && foundFetchMoreTriggerForProvider != std::begin(_resultsCombined)) {
                        Q_ASSERT(foundFetchMoreTriggerForProvider->_type == UnifiedSearchResult::Type::FetchMoreTrigger
                            && foundFetchMoreTriggerForProvider->_categoryId == providerId);
                        beginRemoveRows(QModelIndex(), foundFetchMoreTriggerForProvider - std::begin(_resultsCombined),
                            foundFetchMoreTriggerForProvider - std::begin(_resultsCombined));
                        _resultsCombined.erase(foundFetchMoreTriggerForProvider);
                        endRemoveRows();
                    }
                }
            }
        }
    }
}

}
