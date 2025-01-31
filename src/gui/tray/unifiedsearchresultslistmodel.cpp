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

#include "unifiedsearchresultslistmodel.h"

#include "account.h"
#include "accountstate.h"
#include "guiutility.h"
#include "folderman.h"
#include "networkjobs.h"

#include <algorithm>

#include <QAbstractListModel>
#include <QDesktopServices>

namespace {
QString imagePlaceholderUrlForProviderId(const QString &providerId, const bool darkMode)
{
    const auto colorIconPath = darkMode ? QStringLiteral(":/client/theme/white/") : QStringLiteral(":/client/theme/black/");
    if (providerId.contains(QStringLiteral("message"), Qt::CaseInsensitive)
        || providerId.contains(QStringLiteral("talk"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("wizard-talk.svg");
    } else if (providerId.contains(QStringLiteral("file"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("edit.svg");
    } else if (providerId.contains(QStringLiteral("deck"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("deck.svg");
    } else if (providerId.contains(QStringLiteral("calendar"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("calendar.svg");
    } else if (providerId.contains(QStringLiteral("mail"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("email.svg");
    } else if (providerId.contains(QStringLiteral("comment"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("comment.svg");
    }

    return colorIconPath % QStringLiteral("change.svg");
}

QString localIconPathFromIconPrefix(const QString &iconNameWithPrefix, const bool darkMode)
{
    const auto colorIconPath = darkMode ? QStringLiteral(":/client/theme/white/") : QStringLiteral(":/client/theme/black/");
    if (iconNameWithPrefix.contains(QStringLiteral("message"), Qt::CaseInsensitive)
        || iconNameWithPrefix.contains(QStringLiteral("talk"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("wizard-talk.svg");
    } else if (iconNameWithPrefix.contains(QStringLiteral("folder"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("folder.svg");
    } else if (iconNameWithPrefix.contains(QStringLiteral("deck"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("deck.svg");
    } else if (iconNameWithPrefix.contains(QStringLiteral("contacts"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("wizard-groupware.svg");
    } else if (iconNameWithPrefix.contains(QStringLiteral("calendar"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("calendar.svg");
    } else if (iconNameWithPrefix.contains(QStringLiteral("mail"), Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("email.svg");
    }

    return colorIconPath % QStringLiteral("change.svg");
}

QString iconUrlForDefaultIconName(const QString &defaultIconName, const bool darkMode)
{
    const QUrl urlForIcon{defaultIconName};

    if (urlForIcon.isValid() && !urlForIcon.scheme().isEmpty()) {
        return defaultIconName;
    }
    
    const auto colorIconPath = darkMode ? QStringLiteral(":/client/theme/white/") : QStringLiteral(":/client/theme/black/");

    if (defaultIconName.startsWith(QStringLiteral("icon-"))) {
        const auto parts = defaultIconName.split(QLatin1Char('-'));

        if (parts.size() > 1) {
            const QString blackOrWhiteIconFilePath = colorIconPath + parts[1] + QStringLiteral(".svg");

            if (QFile::exists(blackOrWhiteIconFilePath)) {
                return blackOrWhiteIconFilePath;
            }

            const QString iconFilePath = QStringLiteral(":/client/theme/") + parts[1] + QStringLiteral(".svg");

            if (QFile::exists(iconFilePath)) {
                return iconFilePath;
            }
        }

        const auto iconNameFromIconPrefix = localIconPathFromIconPrefix(defaultIconName, darkMode);

        if (!iconNameFromIconPrefix.isEmpty()) {
            return iconNameFromIconPrefix;
        }
    }

    return colorIconPath % QStringLiteral("change.svg");
}

QString generateUrlForThumbnail(const QString &thumbnailUrl, const QUrl &serverUrl)
{
    auto serverUrlCopy = serverUrl;
    auto thumbnailUrlCopy = thumbnailUrl;

    if (thumbnailUrlCopy.startsWith(QLatin1Char('/')) || thumbnailUrlCopy.startsWith(QLatin1Char('\\'))) {
        // relative image resource URL, just needs some concatenation with current server URL
        // some icons may contain parameters after (?)
        const QStringList thumbnailUrlCopySplitted = thumbnailUrlCopy.contains(QLatin1Char('?'))
            ? thumbnailUrlCopy.split(QLatin1Char('?'), Qt::SkipEmptyParts)
            : QStringList{thumbnailUrlCopy};
        Q_ASSERT(!thumbnailUrlCopySplitted.isEmpty());
        serverUrlCopy.setPath(thumbnailUrlCopySplitted[0]);
        thumbnailUrlCopy = serverUrlCopy.toString();
        if (thumbnailUrlCopySplitted.size() > 1) {
            thumbnailUrlCopy += QLatin1Char('?') + thumbnailUrlCopySplitted[1];
        }
    }

    return thumbnailUrlCopy;
}

QString generateUrlForIcon(const QString &fallbackIcon, const QUrl &serverUrl, const bool darkMode)
{
    auto serverUrlCopy = serverUrl;

    auto fallbackIconCopy = fallbackIcon;

    if (fallbackIconCopy.startsWith(QLatin1Char('/')) || fallbackIconCopy.startsWith(QLatin1Char('\\'))) {
        // relative image resource URL, just needs some concatenation with current server URL
        // some icons may contain parameters after (?)
        const QStringList fallbackIconPathSplitted =
            fallbackIconCopy.contains(QLatin1Char('?')) ? fallbackIconCopy.split(QLatin1Char('?')) : QStringList{fallbackIconCopy};
        Q_ASSERT(!fallbackIconPathSplitted.isEmpty());
        serverUrlCopy.setPath(fallbackIconPathSplitted[0]);
        fallbackIconCopy = serverUrlCopy.toString();
        if (fallbackIconPathSplitted.size() > 1) {
            fallbackIconCopy += QLatin1Char('?') + fallbackIconPathSplitted[1];
        }
    } else if (!fallbackIconCopy.isEmpty()) {
        // could be one of names for standard icons (e.g. icon-mail)
        const auto defaultIconUrl = iconUrlForDefaultIconName(fallbackIconCopy, darkMode);
        if (!defaultIconUrl.isEmpty()) {
            fallbackIconCopy = defaultIconUrl;
        }
    }

    return fallbackIconCopy;
}

// Return image URL and whether it is a thumbnail or not
std::pair<QString, bool> iconsFromThumbnailAndFallbackIcon(const QString &thumbnailUrl, const QString &fallbackIcon, const QUrl &serverUrl, const bool darkMode)
{
    if (thumbnailUrl.isEmpty() && fallbackIcon.isEmpty()) {
        return {};
    }

    if (serverUrl.isEmpty()) {
        const QStringList listImages = {thumbnailUrl, fallbackIcon};
        return {listImages.join(QLatin1Char(';')), false};
    }

    const auto urlForThumbnail = generateUrlForThumbnail(thumbnailUrl, serverUrl);
    const auto urlForFallbackIcon = generateUrlForIcon(fallbackIcon, serverUrl, darkMode);

    qDebug() << "SEARCH" << urlForThumbnail << urlForFallbackIcon;

    if (urlForThumbnail.isEmpty() && !urlForFallbackIcon.isEmpty()) {
        return {urlForFallbackIcon, false};
    }

    if (!urlForThumbnail.isEmpty() && urlForFallbackIcon.isEmpty()) {
        return {urlForThumbnail, true};
    }

    const QStringList listImages{urlForThumbnail, urlForFallbackIcon};
    return {listImages.join(QLatin1Char(';')), true};
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
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));

    switch (role) {
    case ProviderNameRole:
        return _results.at(index.row())._providerName;
    case ProviderIdRole: 
        return _results.at(index.row())._providerId;
    case DarkImagePlaceholderRole:
        return imagePlaceholderUrlForProviderId(_results.at(index.row())._providerId, true);
    case LightImagePlaceholderRole:
        return imagePlaceholderUrlForProviderId(_results.at(index.row())._providerId, false);
    case DarkIconsRole:
        return _results.at(index.row())._darkIcons;
    case LightIconsRole:
        return _results.at(index.row())._lightIcons;
    case DarkIconsIsThumbnailRole:
        return _results.at(index.row())._darkIconsIsThumbnail;
    case LightIconsIsThumbnailRole:
        return _results.at(index.row())._lightIconsIsThumbnail;
    case TitleRole:
        return _results.at(index.row())._title;
    case SublineRole:
        return _results.at(index.row())._subline;
    case ResourceUrlRole:
        return _results.at(index.row())._resourceUrl;
    case RoundedRole:
        return _results.at(index.row())._isRounded;
    case TypeRole:
        return _results.at(index.row())._type;
    case TypeAsStringRole:
        return UnifiedSearchResult::typeAsString(_results.at(index.row())._type);
    }

    return {};
}

int UnifiedSearchResultsListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _results.size();
}

QHash<int, QByteArray> UnifiedSearchResultsListModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles[ProviderNameRole] = "providerName";
    roles[ProviderIdRole] = "providerId";
    roles[DarkIconsRole] = "darkIcons";
    roles[LightIconsRole] = "lightIcons";
    roles[DarkIconsIsThumbnailRole] = "darkIconsIsThumbnail";
    roles[LightIconsIsThumbnailRole] = "lightIconsIsThumbnail";
    roles[DarkImagePlaceholderRole] = "darkImagePlaceholder";
    roles[LightImagePlaceholderRole] = "lightImagePlaceholder";
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

QString UnifiedSearchResultsListModel::errorString() const
{
    return _errorString;
}

QString UnifiedSearchResultsListModel::currentFetchMoreInProgressProviderId() const
{
    return _currentFetchMoreInProgressProviderId;
}

bool UnifiedSearchResultsListModel::waitingForSearchTermEditEnd() const
{
    return _waitingForSearchTermEditEnd;
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

    disconnectAndClearSearchJobs();

    clearCurrentFetchMoreInProgressProviderId();

    disconnect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this,
        &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);

    if (_unifiedSearchTextEditingFinishedTimer.isActive()) {
        _unifiedSearchTextEditingFinishedTimer.stop();
        _waitingForSearchTermEditEnd = false;
        emit waitingForSearchTermEditEndChanged();
    }

    if (!_searchTerm.isEmpty()) {
        _unifiedSearchTextEditingFinishedTimer.setInterval(searchTermEditingFinishedSearchStartDelay);
        connect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this,
            &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);
        _unifiedSearchTextEditingFinishedTimer.start();
        _waitingForSearchTermEditEnd = true;
        emit waitingForSearchTermEditEndChanged();
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

void UnifiedSearchResultsListModel::resultClicked(const QString &providerId, const QUrl &resourceUrl) const
{
    const QUrlQuery urlQuery{resourceUrl};
    const auto dir = urlQuery.queryItemValue(QStringLiteral("dir"), QUrl::ComponentFormattingOption::FullyDecoded);
    const auto fileName =
        urlQuery.queryItemValue(QStringLiteral("scrollto"), QUrl::ComponentFormattingOption::FullyDecoded);

    if (providerId.contains(QStringLiteral("file"), Qt::CaseInsensitive) && !dir.isEmpty() && !fileName.isEmpty()) {
        if (!_accountState || !_accountState->account()) {
            return;
        }

        const QString relativePath = dir + QLatin1Char('/') + fileName;
        const auto localFiles =
            FolderMan::instance()->findFileInLocalFolders(QFileInfo(relativePath).path(), _accountState->account());

        if (!localFiles.isEmpty()) {
            qCInfo(lcUnifiedSearch) << "Opening file:" << localFiles.constFirst();
            QDesktopServices::openUrl(QUrl::fromLocalFile(localFiles.constFirst()));
            return;
        }
    }
    Utility::openBrowser(resourceUrl);
}

void UnifiedSearchResultsListModel::fetchMoreTriggerClicked(const QString &providerId)
{
    if (isSearchInProgress() || !_currentFetchMoreInProgressProviderId.isEmpty()) {
        return;
    }

    const auto providerInfo = _providers.value(providerId, {});

    if (!providerInfo._id.isEmpty() && providerInfo._id == providerId && providerInfo._isPaginated) {
        // Load more items
        _currentFetchMoreInProgressProviderId = providerId;
        emit currentFetchMoreInProgressProviderIdChanged();
        startSearchForProvider(providerId, providerInfo._cursor);
    }
}

void UnifiedSearchResultsListModel::slotSearchTermEditingFinished()
{
    _waitingForSearchTermEditEnd = false;
    emit waitingForSearchTermEditEndChanged();

    disconnect(&_unifiedSearchTextEditingFinishedTimer, &QTimer::timeout, this,
        &UnifiedSearchResultsListModel::slotSearchTermEditingFinished);

    if (!_accountState || !_accountState->account()) {
        qCCritical(lcUnifiedSearch) << QStringLiteral("Account state is invalid. Could not start search!");
        return;
    }

    if (_providers.isEmpty()) {
        auto job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/search/providers"));
        QObject::connect(job, &JsonApiJob::jsonReceived, this, &UnifiedSearchResultsListModel::slotFetchProvidersFinished);
        job->start();
    } else {
        startSearch();
    }
}

void UnifiedSearchResultsListModel::slotFetchProvidersFinished(const QJsonDocument &json, int statusCode)
{
    const auto job = qobject_cast<JsonApiJob *>(sender());

    if (!job) {
        qCCritical(lcUnifiedSearch) << QStringLiteral("Failed to fetch providers.").arg(_searchTerm);
        _errorString += tr("Failed to fetch providers.") + QLatin1Char('\n');
        emit errorStringChanged();
        return;
    }
    
    if (statusCode != 200) {
        qCCritical(lcUnifiedSearch) << QStringLiteral("%1: Failed to fetch search providers for '%2'. Error: %3")
                                           .arg(statusCode)
                                           .arg(_searchTerm)
                                           .arg(job->errorString());
        _errorString +=
            tr("Failed to fetch search providers for '%1'. Error: %2").arg(_searchTerm).arg(job->errorString())
            + QLatin1Char('\n');
        emit errorStringChanged();
        return;
    }
    const auto providerList =
        json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toVariant().toList();

    for (const auto &provider : providerList) {
        const auto providerMap = provider.toMap();
        const auto id = providerMap[QStringLiteral("id")].toString();
        const auto name = providerMap[QStringLiteral("name")].toString();
        if (!name.isEmpty() && id != QStringLiteral("talk-message-current")) {
            UnifiedSearchProvider newProvider;
            newProvider._name = name;
            newProvider._id = id;
            newProvider._order = providerMap[QStringLiteral("order")].toInt();
            _providers.insert(newProvider._id, newProvider);
        }
    }

    if (!_providers.empty()) {
        startSearch();
    }
}

void UnifiedSearchResultsListModel::slotSearchForProviderFinished(const QJsonDocument &json, int statusCode)
{
    Q_ASSERT(_accountState && _accountState->account());

    const auto job = qobject_cast<JsonApiJob *>(sender());

    if (!job) {
        qCCritical(lcUnifiedSearch) << QStringLiteral("Search has failed for '%2'.").arg(_searchTerm);
        _errorString += tr("Search has failed for '%2'.").arg(_searchTerm) + QLatin1Char('\n');
        emit errorStringChanged();
        return;
    }

    const auto providerId = job->property("providerId").toString();
    
    if (providerId.isEmpty()) {
        return;
    }

    if (!_searchJobConnections.isEmpty()) {
        _searchJobConnections.remove(providerId);

        if (_searchJobConnections.isEmpty()) {
            emit isSearchInProgressChanged();
        }
    }

    if (providerId == _currentFetchMoreInProgressProviderId) {
        clearCurrentFetchMoreInProgressProviderId();
    }

    if (statusCode != 200) {
        qCCritical(lcUnifiedSearch) << QStringLiteral("%1: Search has failed for '%2'. Error: %3")
                                           .arg(statusCode)
                                           .arg(_searchTerm)
                                           .arg(job->errorString());
        _errorString +=
            tr("Search has failed for '%1'. Error: %2").arg(_searchTerm).arg(job->errorString()) + QLatin1Char('\n');
        emit errorStringChanged();
        return;
    }

    const auto data = json.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
    if (!data.isEmpty()) {
        parseResultsForProvider(data, providerId, job->property("appendResults").toBool());
    }
}

void UnifiedSearchResultsListModel::startSearch()
{
    Q_ASSERT(_accountState && _accountState->account());

    disconnectAndClearSearchJobs();

    if (!_accountState || !_accountState->account()) {
        return;
    }

    if (!_results.isEmpty()) {
        beginResetModel();
        _results.clear();
        endResetModel();
    }

    for (const auto &provider : std::as_const(_providers)) {
        startSearchForProvider(provider._id);
    }
}

void UnifiedSearchResultsListModel::startSearchForProvider(const QString &providerId, qint32 cursor)
{
    Q_ASSERT(_accountState && _accountState->account());

    if (!_accountState || !_accountState->account()) {
        return;
    }

    auto job = new JsonApiJob(_accountState->account(),
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

void UnifiedSearchResultsListModel::parseResultsForProvider(const QJsonObject &data, const QString &providerId, bool fetchedMore)
{
    const auto cursor = data.value(QStringLiteral("cursor")).toInt();
    const auto entries = data.value(QStringLiteral("entries")).toVariant().toList();

    auto &provider = _providers[providerId];

    if (provider._id.isEmpty() && fetchedMore) {
        _providers.remove(providerId);
        return;
    }

    if (entries.isEmpty()) {
        // we may have received false pagination information from the server, such as, we expect more
        // results available via pagination, but, there are no more left, so, we need to stop paginating for
        // this provider
        provider._isPaginated = false;

        if (fetchedMore) {
            removeFetchMoreTrigger(provider._id);
        }

        return;
    }

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

    QVector<UnifiedSearchResult> newEntries;

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

        const auto resourceUrl = entryMap.value(QStringLiteral("resourceUrl")).toUrl();
        const auto accountUrl = (_accountState && _accountState->account()) ? _accountState->account()->url() : QUrl();

        result._resourceUrl = openableResourceUrl(resourceUrl, accountUrl);
        const auto darkIconsData = iconsFromThumbnailAndFallbackIcon(entryMap.value(QStringLiteral("thumbnailUrl")).toString(),
                                                                     entryMap.value(QStringLiteral("icon")).toString(), accountUrl, true);
        const auto lightIconsData = iconsFromThumbnailAndFallbackIcon(entryMap.value(QStringLiteral("thumbnailUrl")).toString(),
                                                                      entryMap.value(QStringLiteral("icon")).toString(), accountUrl, false);
        result._darkIcons = darkIconsData.first;
        result._lightIcons = lightIconsData.first;
        result._darkIconsIsThumbnail = darkIconsData.second;
        result._lightIconsIsThumbnail = lightIconsData.second;

        newEntries.push_back(result);
    }

    if (fetchedMore) {
        appendResultsToProvider(newEntries, provider);
    } else {
        appendResults(newEntries, provider);
    }
}

QUrl UnifiedSearchResultsListModel::openableResourceUrl(const QUrl &resourceUrl, const QUrl &accountUrl)
{
    if (!resourceUrl.isRelative()) {
        return resourceUrl;
    }

    QUrl finalResourceUrl(accountUrl);
    finalResourceUrl.setPath(resourceUrl.toString());
    return finalResourceUrl;
}

void UnifiedSearchResultsListModel::appendResults(QVector<UnifiedSearchResult> results, const UnifiedSearchProvider &provider)
{
    if (provider._cursor > 0 && provider._isPaginated) {
        UnifiedSearchResult fetchMoreTrigger;
        fetchMoreTrigger._providerId = provider._id;
        fetchMoreTrigger._providerName = provider._name;
        fetchMoreTrigger._order = provider._order;
        fetchMoreTrigger._type = UnifiedSearchResult::Type::FetchMoreTrigger;
        results.push_back(fetchMoreTrigger);
    }


    if (_results.isEmpty()) {
        beginInsertRows({}, 0, results.size() - 1);
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

    const auto first = static_cast<int>(std::distance(std::begin(_results), itToInsertTo));
    const auto last = first + results.size() - 1;

    beginInsertRows({}, first, last);
    std::copy(std::begin(results), std::end(results), std::inserter(_results, itToInsertTo));
    endInsertRows();
}

void UnifiedSearchResultsListModel::appendResultsToProvider(const QVector<UnifiedSearchResult> &results, const UnifiedSearchProvider &provider)
{
    if (results.isEmpty()) {
        return;
    }

    const auto providerId = provider._id;
    /* we need to find the last result that is not a fetch-more-trigger or category-separator for the current
       provider */
    const auto itLastResultForProviderReverse =
        std::find_if(std::rbegin(_results), std::rend(_results), [&providerId](const UnifiedSearchResult &result) {
            return result._providerId == providerId && result._type == UnifiedSearchResult::Type::Default;
        });

    if (itLastResultForProviderReverse != std::rend(_results)) {
        // #1 Insert rows
        // convert reverse_iterator to iterator
        const auto itLastResultForProvider = (itLastResultForProviderReverse + 1).base();
        const auto first = static_cast<int>(std::distance(std::begin(_results), itLastResultForProvider + 1));
        const auto last = first + results.size() - 1;
        beginInsertRows({}, first, last);
        std::copy(std::begin(results), std::end(results), std::inserter(_results, itLastResultForProvider + 1));
        endInsertRows();

        // #2 Remove the FetchMoreTrigger item if there are no more results to load for this provider
        if (!provider._isPaginated) {
            removeFetchMoreTrigger(providerId);
        }
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
            const auto eraseIndex = static_cast<int>(std::distance(std::begin(_results), itFetchMoreTriggerForProvider));
            Q_ASSERT(eraseIndex >= 0 && eraseIndex < static_cast<int>(_results.size()));
            beginRemoveRows({}, eraseIndex, eraseIndex);
            _results.erase(itFetchMoreTriggerForProvider);
            endRemoveRows();
        }
    }
}

void UnifiedSearchResultsListModel::disconnectAndClearSearchJobs()
{
    for (const auto &connection : std::as_const(_searchJobConnections)) {
        if (connection) {
            QObject::disconnect(connection);
        }
    }

    if (!_searchJobConnections.isEmpty()) {
        _searchJobConnections.clear();
        emit isSearchInProgressChanged();
    }
}

void UnifiedSearchResultsListModel::clearCurrentFetchMoreInProgressProviderId()
{
    if (!_currentFetchMoreInProgressProviderId.isEmpty()) {
        _currentFetchMoreInProgressProviderId.clear();
        emit currentFetchMoreInProgressProviderIdChanged();
    }
}

}
