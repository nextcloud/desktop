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
#include "folderman.h"
#include "networkjobs.h"

#include <algorithm>

#include <QAbstractListModel>
#include <QDesktopServices>

namespace {
static QString imagePlaceholderUrlForProviderId(const QString &providerId)
{
    if (providerId.contains(QStringLiteral("message"), Qt::CaseInsensitive)
        || providerId.contains(QStringLiteral("talk"), Qt::CaseInsensitive)) {
        return QStringLiteral("qrc:///client/theme/black/wizard-talk.svg");
    } else if (providerId.contains(QStringLiteral("file"), Qt::CaseInsensitive)) {
        return QStringLiteral("qrc:///client/theme/black/edit.svg");
    } else if (providerId.contains(QStringLiteral("deck"), Qt::CaseInsensitive)) {
        return QStringLiteral("qrc:///client/theme/black/deck.svg");
    } else if (providerId.contains(QStringLiteral("calendar"), Qt::CaseInsensitive)) {
        return QStringLiteral("qrc:///client/theme/black/calendar.svg");
    } else if (providerId.contains(QStringLiteral("mail"), Qt::CaseInsensitive)) {
        return QStringLiteral("qrc:///client/theme/black/email.svg");
    } else if (providerId.contains(QStringLiteral("comment"), Qt::CaseInsensitive)) {
        return QStringLiteral("qrc:///client/theme/black/comment.svg");
    }

    return QStringLiteral("qrc:///client/theme/change.svg");
}

static QString iconUrlForDefaultIconName(const QString &defaultIconName)
{
    const QUrl urlForIcon(defaultIconName);

    if (urlForIcon.isValid() && !urlForIcon.scheme().isEmpty()) {
        return defaultIconName;
    }
    
    if (defaultIconName.startsWith(QStringLiteral("icon-"))) {
        const auto parts = defaultIconName.split(QLatin1Char('-'));

        if (parts.size() > 1) {
            const QString iconFilePath = QStringLiteral(":/client/theme/") + parts[1] + QStringLiteral(".svg");

            if (QFile(iconFilePath).exists()) {
                return iconFilePath;
            }

            const QString blackIconFilePath = QStringLiteral(":/client/theme/black/") + parts[1] + QStringLiteral(".svg");

            if (QFile(blackIconFilePath).exists()) {
                return blackIconFilePath;
            }
        }

        if (defaultIconName.contains(QStringLiteral("message"), Qt::CaseInsensitive)
            || defaultIconName.contains(QStringLiteral("talk"), Qt::CaseInsensitive)) {
            return QStringLiteral(":/client/theme/black/wizard-talk.svg");
        } else if (defaultIconName.contains(QStringLiteral("folder"), Qt::CaseInsensitive)) {
            return QStringLiteral(":/client/theme/black/folder.svg");
        } else if (defaultIconName.contains(QStringLiteral("deck"), Qt::CaseInsensitive)) {
            return QStringLiteral(":/client/theme/black/deck.svg");
        } else if (defaultIconName.contains(QStringLiteral("contacts"), Qt::CaseInsensitive)) {
            return QStringLiteral(":/client/theme/black/wizard-groupware.svg");
        } else if (defaultIconName.contains(QStringLiteral("calendar"), Qt::CaseInsensitive)) {
            return QStringLiteral(":/client/theme/black/calendar.svg");
        } else if (defaultIconName.contains(QStringLiteral("mail"), Qt::CaseInsensitive)) {
            return QStringLiteral(":/client/theme/black/email.svg");
        }
    }

    return QStringLiteral(":/client/theme/change.svg");
}

static QString iconsFromThumbnailAndFallbackIcon(QString thumbnailUrl, QString fallackIcon, QUrl serverUrl)
{
    if (thumbnailUrl.isEmpty() && fallackIcon.isEmpty()) {
        return QStringLiteral("");
    }

    const QUrl urlForThumbnail(thumbnailUrl);
    if ((thumbnailUrl.startsWith(QLatin1Char('/')) || thumbnailUrl.startsWith(QLatin1Char('\\')))
        && (!urlForThumbnail.isValid() || urlForThumbnail.scheme().isEmpty())) {
        // relative image resource URL, just needs some concatenation with current server URL
        // some icons may contain parameters after (?)
        const QStringList thumbnailUrlSplitted =
            thumbnailUrl.contains(QLatin1Char('?')) ? thumbnailUrl.split(QLatin1Char('?')) : QStringList{thumbnailUrl};
        serverUrl.setPath(thumbnailUrlSplitted[0]);
        thumbnailUrl = serverUrl.toString();
        if (thumbnailUrlSplitted.size() > 1) {
            thumbnailUrl += QLatin1Char('?') + thumbnailUrlSplitted[1];
        }
    }

    const QUrl urlForIcon(fallackIcon);
    if ((fallackIcon.startsWith(QLatin1Char('/')) || fallackIcon.startsWith(QLatin1Char('\\')))
        && (!urlForIcon.isValid() || urlForIcon.scheme().isEmpty())) {
        // relative image resource URL, just needs some concatenation with current server URL
        // some icons may contain parameters after (?)
        const QStringList iconPathSplitted =
            fallackIcon.contains(QLatin1Char('?')) ? fallackIcon.split(QLatin1Char('?')) : QStringList{fallackIcon};
        serverUrl.setPath(iconPathSplitted[0]);
        fallackIcon = serverUrl.toString();
        if (iconPathSplitted.size() > 1) {
            fallackIcon += QLatin1Char('?') + iconPathSplitted[1];
        }
    } else if (!fallackIcon.isEmpty()) {
        // could be one of names for standard icons (e.g. icon-mail)
        const auto defaultIconUrl = iconUrlForDefaultIconName(fallackIcon);
        if (!defaultIconUrl.isEmpty()) {
            fallackIcon = defaultIconUrl;
        }
    }

    if (thumbnailUrl.isEmpty() && !fallackIcon.isEmpty()) {
        return fallackIcon;
    }

    if (!thumbnailUrl.isEmpty() && fallackIcon.isEmpty()) {
        return thumbnailUrl;
    }

    const QStringList listImages = {thumbnailUrl, fallackIcon};
    return listImages.join(QLatin1Char(';'));
}

constexpr int searchTermEditingFinishedSearchStartDelay = 800;

// server-side bug of returning the cursor > 0 and isPaginated == 'true', using '5' as it is done on Android client's end now
constexpr int minimumEntresNumberToShowLoadMore = 5;
}
namespace OCC {

Q_LOGGING_CATEGORY(lcUnifiedSearch, "nextcloud.gui.unifiedsearch", QtInfoMsg)

UnifiedSearchResultsListModel::UnifiedSearchResultsListModel(AccountState *accountState, QObject *parent)
    : QAbstractListModel(parent)
    , _accountState(accountState)
{
}

QVariant UnifiedSearchResultsListModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= _results.size()) {
        return QVariant();
    }

    switch (role) {
    case ProviderNameRole: {
        return _results.at(index.row())._providerName;
    }
    case ProviderIdRole: {
        return _results.at(index.row())._providerId;
    }
    case ImagePlaceholderRole: {
        return imagePlaceholderUrlForProviderId(_results.at(index.row())._providerId);
    }
    case IconsRole: {
        return _results.at(index.row())._icons;
    }
    case TitleRole: {
        return _results.at(index.row())._title;
    }
    case SublineRole: {
        return _results.at(index.row())._subline;
    }
    case ResourceUrlRole: {
        return _results.at(index.row())._resourceUrl;
    }
    case RoundedRole: {
        return _results.at(index.row())._isRounded;
    }
    case TypeRole: {
        return _results.at(index.row())._type;
    }
    case TypeAsStringRole: {
        return UnifiedSearchResult::typeAsString(_results.at(index.row())._type);
    }
    }

    return QVariant();
}

int UnifiedSearchResultsListModel::rowCount(const QModelIndex &) const
{
    return _results.size();
}

QHash<int, QByteArray> UnifiedSearchResultsListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ProviderNameRole] = "providerName";
    roles[ProviderIdRole] = "providerId";
    roles[IconsRole] = "icons";
    roles[ImagePlaceholderRole] = "imagePlaceholder";
    roles[TitleRole] = "resultTitle";
    roles[SublineRole] = "subline";
    roles[ResourceUrlRole] = "resourceUrlRole";
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
        _unifiedSearchTextEditingFinishedTimer.setInterval(searchTermEditingFinishedSearchStartDelay);
        connect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this,
            &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);
        _unifiedSearchTextEditingFinishedTimer.start();
    } else {
        if (!_searchJobConnections.isEmpty()) {
            for (const auto &connection : _searchJobConnections) {
                if (connection) {
                    QObject::disconnect(connection);
                }
            }

            _searchJobConnections.clear();
            emit isSearchInProgressChanged();
        }
    }

    if (!_results.isEmpty()) {
        beginResetModel();
        _results.clear();
        endResetModel();
    }
}

bool UnifiedSearchResultsListModel::isSearchInProgress() const
{
    return !_searchJobConnections.isEmpty();
}

void UnifiedSearchResultsListModel::resultClicked(const QString &providerId, const QUrl &resourceUrl)
{
    if (providerId.isEmpty() || resourceUrl.isEmpty()) {
        return;
    }

    if (resourceUrl.isValid()) {
        if (providerId.contains(QStringLiteral("file"), Qt::CaseInsensitive)) {
            if (!_accountState || !_accountState->account()) {
                return;
            }

            const auto urlQuery = QUrlQuery(resourceUrl);
            const auto dir = urlQuery.queryItemValue(QStringLiteral("dir"), QUrl::ComponentFormattingOption::FullyDecoded);
            const auto fileName = urlQuery.queryItemValue(QStringLiteral("scrollto"), QUrl::ComponentFormattingOption::FullyDecoded);

            if (!dir.isEmpty() && !fileName.isEmpty()) {
                const QString relativePath = dir + QLatin1Char('/') + fileName;
                if (!relativePath.isEmpty()) {
                    const auto localFiles = FolderMan::instance()->findFileInLocalFolders(
                        QFileInfo(relativePath).path(), _accountState->account());

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

void UnifiedSearchResultsListModel::fetchMoreTriggerClicked(const QString &providerId)
{
    if (isSearchInProgress() || !_currentFetchMoreInProgressProviderId.isEmpty()) {
        return;
    }

    const auto providerInfo = _providers.value(providerId, {});

    if (!providerInfo._id.isEmpty() && providerInfo._id == providerId) {
        if (providerInfo._isPaginated) {
            // Load more items
            _currentFetchMoreInProgressProviderId = providerId;
            emit currentFetchMoreInProgressProviderIdChanged();
            startSearchForProvider(providerId, providerInfo._cursor);
        }
    }
}

void UnifiedSearchResultsListModel::slotSearchTermEditingFinished()
{
    disconnect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this,
        &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);

    if (!_accountState || !_accountState->account()) {
        qCCritical(lcUnifiedSearch) << QString("Account state is invalid. Could not start search!");
        return;
    }

    if (_providers.isEmpty()) {
        auto *job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/search/providers"));
        QObject::connect(job, &JsonApiJob::jsonReceived, [&, this](const QJsonDocument &json, int statusCode) {
            if (statusCode != 200) {
                qCCritical(lcUnifiedSearch) << QString("%1: Failed to fetch search providers for '%2'. Error: %3")
                                                   .arg(statusCode)
                                                   .arg(_searchTerm)
                                                   .arg(job->errorString());
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
                    _providers.insert(newProvider._id, newProvider);
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
    Q_ASSERT(_accountState && _accountState->account());
    bool appendToProvider = false;

    if (const auto job = qobject_cast<JsonApiJob *>(sender())) {
        appendToProvider = job->property("appendResults").toBool();
        const auto providerId = job->property("providerId").toString();

        if (!_searchJobConnections.isEmpty()) {
            _searchJobConnections.remove(providerId);

            if (_searchJobConnections.isEmpty()) {
                emit isSearchInProgressChanged();
            }
        }

        if (!_currentFetchMoreInProgressProviderId.isEmpty()) {
            _currentFetchMoreInProgressProviderId.clear();
            emit currentFetchMoreInProgressProviderIdChanged();
        }

        if (statusCode != 200) {
            qCCritical(lcUnifiedSearch) << QString("%1: Search has failed for '%2'. Error: %3")
                                               .arg(statusCode)
                                               .arg(_searchTerm)
                                               .arg(job->errorString());
            _errorString += tr("Search has failed for '%1'. Error: %2").arg(_searchTerm).arg(job->errorString())
                + QLatin1Char('\n');
            emit errorStringChanged();
            return;
        }

        if (_searchTerm.isEmpty()) {
            return;
        }

        if (providerId.isEmpty()) {
            return;
        }

        QList<UnifiedSearchResult> newEntries;

        const auto data = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
        if (!data.isEmpty()) {
            const auto name = data.value(QStringLiteral("name")).toString();
            const auto cursor = data.value(QStringLiteral("cursor")).toInt();
            const auto entries = data.value(QStringLiteral("entries")).toVariant().toList();

            auto &provider = _providers[providerId];

            if (!provider._id.isEmpty()) {
                if (!entries.isEmpty()) {
                    provider._isPaginated = data.value(QStringLiteral("isPaginated")).toBool();
                    provider._cursor = cursor;

                    if (provider._pageSize == -1) {
                        provider._pageSize = cursor;
                    }

                    if ((provider._pageSize != -1 && entries.size() < provider._pageSize)
                        || entries.size() < minimumEntresNumberToShowLoadMore) {
                        // for some providers we are still getting a non-null cursor and isPaginated true even thought
                        // there are no more results to paginate
                        provider._isPaginated = false;
                    }

                    for (const auto &entry : entries) {
                        const auto entryMap = entry.toMap();
                        if (entryMap.isEmpty()) {
                            continue;
                        }
                        UnifiedSearchResult result;
                        result._providerId = provider._id;
                        result._order = provider._order;
                        result._providerName = provider._name;
                        result._isRounded = entryMap.value(QStringLiteral("rounded")).toBool();
                        result._title = entryMap.value(QStringLiteral("title")).toString();
                        result._subline = entryMap.value(QStringLiteral("subline")).toString();
                        const auto resourceUrl = entryMap.value(QStringLiteral("resourceUrl")).toString();
                        result._resourceUrl = [this, &resourceUrl]() {
                            QUrl result(resourceUrl);
                            if (result.scheme().isEmpty()) {
                                result = _accountState->account()->url();
                                result.setPath(resourceUrl);
                            }
                            return result;
                        }();
                        result._icons =
                            iconsFromThumbnailAndFallbackIcon(entryMap.value(QStringLiteral("thumbnailUrl")).toString(),
                                entryMap.value(QStringLiteral("icon")).toString(), _accountState->account()->url());

                        newEntries.push_back(result);
                    }

                    if (appendToProvider) {
                        appendResultsToProvider(newEntries, provider);
                    } else {
                        appendResults(newEntries, provider);
                    }
                } else {
                    // we may have received false pagination information from the server, such as, we expect more
                    // results available via pagination, but, there are no more left, so, we need to stop paginating for
                    // this provider
                    provider._isPaginated = false;

                    if (appendToProvider) {
                        appendResultsToProvider({}, provider);
                    }
                }
            } else {
                _providers.remove(providerId);
            }
        }
    }
}

void UnifiedSearchResultsListModel::startSearch()
{
    Q_ASSERT(_accountState && _accountState->account());

    for (auto &connection : _searchJobConnections) {
        if (connection) {
            QObject::disconnect(connection);
        }
    }

    if (_accountState && _accountState->account()) {

        if (!_results.isEmpty()) {
            beginResetModel();
            _results.clear();
            endResetModel();
        }

        for (const auto &provider : _providers) {
            startSearchForProvider(provider._id);
        }
    }
}

void UnifiedSearchResultsListModel::startSearchForProvider(const QString &providerId, qint32 cursor)
{
    Q_ASSERT(_accountState && _accountState->account());

    if (!_accountState || !_accountState->account()) {
        return;
    }

    auto *job = new JsonApiJob(_accountState->account(),
        QLatin1String("ocs/v2.php/search/providers/%1/search").arg(providerId));

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("term"), _searchTerm);
    if (cursor > 0) {
        params.addQueryItem(QStringLiteral("cursor"), QString::number(cursor));
        job->setProperty("appendResults", true);
    }
    job->setProperty("providerId", providerId);
    job->addQueryParams(params);
    const auto wasSearchInProgress = isSearchInProgress();
    _searchJobConnections.insert(providerId,
        QObject::connect(
            job, &JsonApiJob::jsonReceived, this, &UnifiedSearchResultsListModel::slotSearchForProviderFinished));
    if (isSearchInProgress() && !wasSearchInProgress) {
        emit isSearchInProgressChanged();
    }
    job->start();
}

void UnifiedSearchResultsListModel::appendResults(
    QList<UnifiedSearchResult> results, const UnifiedSearchProvider &provider)
{
    UnifiedSearchResult categorySeparator;
    categorySeparator._providerId = results.first()._providerId;
    categorySeparator._providerName = provider._name;
    categorySeparator._order = provider._order;
    categorySeparator._type = UnifiedSearchResult::Type::CategorySeparator;

    results.push_front(categorySeparator);


    if (provider._cursor > 0 && provider._isPaginated) {
        UnifiedSearchResult fetchMoreTrigger;
        fetchMoreTrigger._providerId = provider._id;
        fetchMoreTrigger._providerName = provider._name;
        fetchMoreTrigger._order = provider._order;
        fetchMoreTrigger._type = UnifiedSearchResult::Type::FetchMoreTrigger;
        results.push_back(fetchMoreTrigger);
    }


    if (_results.isEmpty()) {
        beginInsertRows(QModelIndex(), 0, results.size() - 1);
        _results = results;
        endInsertRows();
        return;
    }

    // insertion is done with sorting (first -> by order, then -> by name)
    const auto itToInsertTo = std::find_if(std::begin(_results), std::end(_results),
        [&provider](const UnifiedSearchResult &current) {
            // insert before other results of higher order when possible
            if (current._order > provider._order) {
                return true;
            } else {
                if (current._order == provider._order) {
                    // insert before results of higher QString value when possible
                    return current._providerName > provider._name;
                }

                return false;
            }
        });

    if (itToInsertTo != std::end(_results)) {
        // insert before other results of lower sort order
        const auto first = itToInsertTo - std::begin(_results);
        const auto last = first + results.size() - 1;

        beginInsertRows(QModelIndex(), first, last);
        std::copy(std::begin(results), std::end(results), std::inserter(_results, itToInsertTo));
        endInsertRows();
    } else {
        // current results are of lower sort order, just append them
        const auto first = _results.size();
        const auto last = first + results.size() - 1;

        beginInsertRows(QModelIndex(), first, last);
        std::copy(std::begin(results), std::end(results), std::back_inserter(_results));
        endInsertRows();
    }
}

void UnifiedSearchResultsListModel::appendResultsToProvider(
    const QList<UnifiedSearchResult> &results, const UnifiedSearchProvider &provider)
{
    if (results.size() > 0) {
        const auto providerId = provider._id;
        /* we need to find the last result that is not a fetch-more-trigger or category-separator for the current
           provider */
        const auto itLastResultForProviderReverse = std::find_if(
            std::rbegin(_results),
            std::rend(_results),
            [&providerId](const UnifiedSearchResult &result) {
                return result._providerId == providerId && result._type == UnifiedSearchResult::Type::Default;
            });

        if (itLastResultForProviderReverse != std::rend(_results)) {
            // #1 Insert rows
            const auto numRowsToInsert = results.size();
            // convert reverse_iterator to iterator
            const auto itLastResultForProvider = (itLastResultForProviderReverse + 1).base();
            const auto first = itLastResultForProvider + 1 - std::begin(_results);
            const auto last = first + numRowsToInsert - 1;
            beginInsertRows(QModelIndex(), first, last);
            std::copy(std::begin(results), std::end(results), std::inserter(_results, itLastResultForProvider + 1));
            endInsertRows();

            // #2 Remove the FetchMoreTrigger item if there are no more results to load for this provider
            if (!provider._isPaginated) {
                removeFetchMoreTrigger(providerId);
            }
        }
    } else {
        removeFetchMoreTrigger(provider._id);
    }
}

void UnifiedSearchResultsListModel::removeFetchMoreTrigger(const QString &providerId)
{
    const auto itFetchMoreTriggerForProviderReverse = std::find_if(
        std::rbegin(_results),
        std::rend(_results),
        [providerId](const UnifiedSearchResult &result) {
            return result._providerId == providerId && result._type == UnifiedSearchResult::Type::FetchMoreTrigger;
        });

    if (itFetchMoreTriggerForProviderReverse != std::rend(_results)) {
        // convert reverse_iterator to iterator
        const auto itFetchMoreTriggerForProvider = (itFetchMoreTriggerForProviderReverse + 1).base();

        if (itFetchMoreTriggerForProvider != std::end(_results)
            && itFetchMoreTriggerForProvider != std::begin(_results)) {
            const auto eraseIndex = itFetchMoreTriggerForProvider - std::begin(_results);
            Q_ASSERT(eraseIndex >= 0 && eraseIndex < _results.size());
            beginRemoveRows(QModelIndex(), eraseIndex, eraseIndex);
            _results.erase(itFetchMoreTriggerForProvider);
            endRemoveRows();
        }
    }
}

}
