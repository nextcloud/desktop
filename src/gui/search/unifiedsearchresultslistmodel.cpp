/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "unifiedsearchresultslistmodel.h"

#include "account.h"
#include "accountstate.h"
#include "folderman.h"
#include "guiutility.h"
#include "networkjobs.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>
#include <tuple>

using namespace Qt::StringLiterals;

namespace {
constexpr qsizetype aggregateResultsPerProvider = 3;
constexpr auto requestResultsPerProvider = 10;

QString imagePlaceholderUrlForProviderId(const QString &providerId, const bool darkMode)
{
    const auto colorIconPath = darkMode ? QStringLiteral(":/client/theme/white/") : QStringLiteral(":/client/theme/black/");
    if (providerId.contains("message"_L1, Qt::CaseInsensitive)
        || providerId.contains("talk"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("wizard-talk.svg");
    } else if (providerId.contains("file"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("edit.svg");
    } else if (providerId.contains("deck"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("deck.svg");
    } else if (providerId.contains("calendar"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("calendar.svg");
    } else if (providerId.contains("mail"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("email.svg");
    } else if (providerId.contains("comment"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("comment.svg");
    }

    return colorIconPath % QStringLiteral("change.svg");
}

QString localIconPathFromIconPrefix(const QString &iconNameWithPrefix, const bool darkMode)
{
    const auto colorIconPath = darkMode ? QStringLiteral(":/client/theme/white/") : QStringLiteral(":/client/theme/black/");
    if (iconNameWithPrefix.contains("message"_L1, Qt::CaseInsensitive)
        || iconNameWithPrefix.contains("talk"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("wizard-talk.svg");
    } else if (iconNameWithPrefix.contains("folder"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("folder.svg");
    } else if (iconNameWithPrefix.contains("deck"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("deck.svg");
    } else if (iconNameWithPrefix.contains("contacts"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("wizard-groupware.svg");
    } else if (iconNameWithPrefix.contains("calendar"_L1, Qt::CaseInsensitive)) {
        return colorIconPath % QStringLiteral("calendar.svg");
    } else if (iconNameWithPrefix.contains("mail"_L1, Qt::CaseInsensitive)) {
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
        const auto parts = defaultIconName.split(u'-');
        if (parts.size() > 1) {
            const auto themedIcon = colorIconPath + parts[1] + QStringLiteral(".svg");
            if (QFile::exists(themedIcon)) {
                return themedIcon;
            }
            const auto icon = QStringLiteral(":/client/theme/") + parts[1] + QStringLiteral(".svg");
            if (QFile::exists(icon)) {
                return icon;
            }
        }
        return localIconPathFromIconPrefix(defaultIconName, darkMode);
    }

    return colorIconPath % QStringLiteral("change.svg");
}

QString absoluteServerResource(const QString &resource, const QUrl &serverUrl)
{
    if (resource.isEmpty()) {
        return {};
    }

    const auto url = QUrl(resource);
    if (!url.isRelative() && !url.scheme().isEmpty()) {
        return resource;
    }

    auto absoluteUrl = serverUrl;
    if (resource.startsWith(u'/') || resource.startsWith(u'\\')) {
        const auto queryPosition = resource.indexOf(u'?');
        absoluteUrl.setPath(queryPosition < 0 ? resource : resource.left(queryPosition));
        if (queryPosition >= 0) {
            absoluteUrl.setQuery(resource.mid(queryPosition + 1));
        }
    } else {
        absoluteUrl = serverUrl.resolved(QUrl(resource));
    }
    return absoluteUrl.toString();
}

QString normalizedIcon(const QString &icon, const QUrl &serverUrl, const bool darkMode)
{
    if (icon.isEmpty()) {
        return {};
    }
    if (icon.startsWith(QStringLiteral(":/"))) {
        return icon;
    }
    if (icon.startsWith(u'/') || icon.startsWith(u'\\')) {
        return absoluteServerResource(icon, serverUrl);
    }
    return iconUrlForDefaultIconName(icon, darkMode);
}

std::pair<QString, bool> iconsFromThumbnailAndFallbackIcon(const QString &thumbnailUrl,
                                                           const QString &fallbackIcon,
                                                           const QUrl &serverUrl,
                                                           const bool darkMode)
{
    const auto thumbnail = absoluteServerResource(thumbnailUrl, serverUrl);
    const auto icon = normalizedIcon(fallbackIcon, serverUrl, darkMode);
    auto icons = QStringList{};
    if (!thumbnail.isEmpty()) {
        icons.push_back(thumbnail);
    }
    if (!icon.isEmpty()) {
        icons.push_back(icon);
    }
    return {icons.join(u';'), !thumbnail.isEmpty()};
}

QString navigationAppIconForResult(const OCC::AccountState *accountState,
                                   const QString &providerId,
                                   const QString &resultTitle,
                                   const bool darkMode)
{
    if (!accountState || providerId != QStringLiteral("settings_apps")) {
        return {};
    }

    const auto apps = accountState->appList();
    const auto appIt = std::find_if(apps.cbegin(), apps.cend(), [&resultTitle](const auto *app) {
        return app && app->name().compare(resultTitle, Qt::CaseInsensitive) == 0;
    });
    if (appIt == apps.cend() || (*appIt)->iconUrl().isEmpty()) {
        return {};
    }

    return (*appIt)->iconUrl().toString()
        % (darkMode ? QStringLiteral("/white") : QStringLiteral("/black"));
}
}

namespace OCC {
Q_LOGGING_CATEGORY(lcUnifiedSearch, "nextcloud.gui.unifiedsearch", QtInfoMsg)

UnifiedSearchResultsListModel::UnifiedSearchResultsListModel(AccountState *accountState,
                                                             QObject *parent,
                                                             int debounceInterval,
                                                             int revealInterval)
    : QAbstractListModel(parent)
    , _accountState(accountState)
{
    _debounceTimer.setSingleShot(true);
    _debounceTimer.setInterval(debounceInterval);
    _revealTimer.setSingleShot(true);
    _revealTimer.setInterval(revealInterval);

    connect(&_debounceTimer, &QTimer::timeout, this, [this] {
        if (_providersReady) {
            startSearch();
        } else {
            _pendingSearch = true;
            if (!_providersLoading) {
                discoverProviders();
            }
        }
        setWaitingForSearchTermEditEnd(false);
    });
    connect(&_revealTimer, &QTimer::timeout, this, &UnifiedSearchResultsListModel::closeRevealWindow);

    connect(this, &UnifiedSearchResultsListModel::isSearchInProgressChanged, this, &UnifiedSearchResultsListModel::searchStateChanged);
    connect(this, &UnifiedSearchResultsListModel::isSearchInProgressChanged, this, &UnifiedSearchResultsListModel::showConnectedServicesActionChanged);
    connect(this, &UnifiedSearchResultsListModel::searchTermChanged, this, &UnifiedSearchResultsListModel::searchStateChanged);
    connect(this, &UnifiedSearchResultsListModel::searchTermChanged, this, &UnifiedSearchResultsListModel::showConnectedServicesActionChanged);
    connect(this, &UnifiedSearchResultsListModel::errorStringChanged, this, &UnifiedSearchResultsListModel::searchStateChanged);
    connect(this, &UnifiedSearchResultsListModel::waitingForSearchTermEditEndChanged, this, &UnifiedSearchResultsListModel::searchStateChanged);
    connect(this, &QAbstractListModel::modelReset, this, &UnifiedSearchResultsListModel::searchStateChanged);
    connect(this, &UnifiedSearchResultsListModel::currentFetchMoreInProgressProviderIdChanged, this, &UnifiedSearchResultsListModel::canEditSearchChanged);
    connect(this, &UnifiedSearchResultsListModel::providersChanged, this, &UnifiedSearchResultsListModel::showConnectedServicesActionChanged);
    connect(this, &UnifiedSearchResultsListModel::viewModeChanged, this, &UnifiedSearchResultsListModel::showConnectedServicesActionChanged);

    _lastKnownConnected = isAccountConnected();
    if (_accountState) {
        connect(_accountState, &AccountState::isConnectedChanged, this, [this] {
            const auto connected = isAccountConnected();
            emit canEditSearchChanged();
            if (_lastKnownConnected && !connected) {
                const auto wasInProgress = isSearchInProgress();
                ++_queryGeneration;
                abortSearchJobs();
                abortProviderDiscovery();
                _debounceTimer.stop();
                setWaitingForSearchTermEditEnd(false);
                resetProviderRuntime();
                setProvidersReady(false);
                rebuildProjection();
                setErrorString(tr("Search is unavailable while this account is offline."));
                updateProgressSignals(wasInProgress);
            } else if (!_lastKnownConnected && connected) {
                setErrorString({});
                _pendingSearch = hasSearchTerm();
                discoverProviders();
            }
            _lastKnownConnected = connected;
        });
    }

    if (isAccountConnected()) {
        QTimer::singleShot(0, this, &UnifiedSearchResultsListModel::discoverProviders);
    } else {
        setErrorString(tr("Search is unavailable while this account is offline."));
    }
}

UnifiedSearchResultsListModel::~UnifiedSearchResultsListModel()
{
    abortSearchJobs();
    abortProviderDiscovery();
}

QVariant UnifiedSearchResultsListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const auto &result = _results.at(index.row());
    switch (role) {
    case ProviderNameRole:
        return result._providerName;
    case ProviderIdRole:
        return result._providerId;
    case ProviderIconRole:
        return result._providerIcon;
    case DarkImagePlaceholderRole:
        return imagePlaceholderUrlForProviderId(result._providerId, true);
    case LightImagePlaceholderRole:
        return imagePlaceholderUrlForProviderId(result._providerId, false);
    case DarkIconsRole:
        return result._darkIcons;
    case LightIconsRole:
        return result._lightIcons;
    case DarkIconsIsThumbnailRole:
        return result._darkIconsIsThumbnail;
    case LightIconsIsThumbnailRole:
        return result._lightIconsIsThumbnail;
    case TitleRole:
        return result._title;
    case SublineRole:
        return result._subline;
    case ResourceUrlRole:
        return result._resourceUrl;
    case RoundedRole:
        return result._isRounded;
    case TypeRole:
        return static_cast<int>(result._type);
    case TypeAsStringRole:
        return UnifiedSearchResult::typeAsString(result._type);
    case StableKeyRole:
        return result._stableKey;
    case SelectedRole:
        return result._isSelected;
    case SelectableRole:
        return result._isSelectable;
    case PartialMatchRole:
        return result._isPartialMatch;
    case HasOverflowRole:
        return result._hasOverflow;
    case LoadingRole:
        return result._isLoading;
    }
    return {};
}

int UnifiedSearchResultsListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : _results.size();
}

QHash<int, QByteArray> UnifiedSearchResultsListModel::roleNames() const
{
    static const auto roles = [this] {
        auto result = QAbstractListModel::roleNames();
        result[ProviderNameRole] = "providerName";
        result[ProviderIdRole] = "providerId";
        result[ProviderIconRole] = "providerIcon";
        result[DarkIconsRole] = "darkIcons";
        result[LightIconsRole] = "lightIcons";
        result[DarkIconsIsThumbnailRole] = "darkIconsIsThumbnail";
        result[LightIconsIsThumbnailRole] = "lightIconsIsThumbnail";
        result[DarkImagePlaceholderRole] = "darkImagePlaceholder";
        result[LightImagePlaceholderRole] = "lightImagePlaceholder";
        result[TitleRole] = "resultTitle";
        result[SublineRole] = "subline";
        result[ResourceUrlRole] = "resourceUrlRole";
        result[TypeRole] = "type";
        result[TypeAsStringRole] = "typeAsString";
        result[RoundedRole] = "isRounded";
        result[StableKeyRole] = "stableKey";
        result[SelectedRole] = "isSelected";
        result[SelectableRole] = "isSelectable";
        result[PartialMatchRole] = "isPartialMatch";
        result[HasOverflowRole] = "hasOverflow";
        result[LoadingRole] = "isLoading";
        return result;
    }();
    return roles;
}

bool UnifiedSearchResultsListModel::isSearchInProgress() const
{
    return _inFlightSearchRequests > 0 || (_providersLoading && hasSearchTerm());
}

QString UnifiedSearchResultsListModel::currentFetchMoreInProgressProviderId() const { return _currentFetchMoreInProgressProviderId; }
QString UnifiedSearchResultsListModel::searchTerm() const { return _searchTerm; }
QString UnifiedSearchResultsListModel::errorString() const { return _errorString; }
bool UnifiedSearchResultsListModel::waitingForSearchTermEditEnd() const { return _waitingForSearchTermEditEnd; }
bool UnifiedSearchResultsListModel::isFetchMoreInProgress() const { return !_currentFetchMoreInProgressProviderId.isEmpty(); }
bool UnifiedSearchResultsListModel::hasSearchTerm() const { return !_searchTerm.trimmed().isEmpty(); }
bool UnifiedSearchResultsListModel::hasSearchError() const { return !_errorString.isEmpty(); }
bool UnifiedSearchResultsListModel::canEditSearch() const { return _accountState && _accountState->account() && isAccountConnected(); }
bool UnifiedSearchResultsListModel::isAccountConnected() const { return _accountState && _accountState->isConnected(); }

UnifiedSearchResultsListModel::SearchState UnifiedSearchResultsListModel::searchState() const
{
    if (!_results.isEmpty()) {
        return SearchState::Results;
    }
    if (hasSearchError() && !isSearchInProgress()) {
        return SearchState::SearchError;
    }
    if (!hasSearchTerm()) {
        return SearchState::Placeholder;
    }
    if (_waitingForSearchTermEditEnd || isSearchInProgress()) {
        return SearchState::Skeleton;
    }
    return SearchState::NothingFound;
}

QVariantList UnifiedSearchResultsListModel::providers() const
{
    QVariantList result;
    for (const auto &providerId : _providerOrder) {
        const auto providerIt = _providers.constFind(providerId);
        if (providerIt == _providers.cend()) {
            continue;
        }
        const auto &provider = providerIt.value();
        result.push_back(QVariantMap{
            {QStringLiteral("id"), provider.id},
            {QStringLiteral("name"), provider.name},
            {QStringLiteral("icon"), providerIcon(provider, false)},
            {QStringLiteral("selected"), _selectedProviderIds.contains(provider.id)},
            {QStringLiteral("external"), provider.isExternalProvider},
        });
    }
    return result;
}

QVariantList UnifiedSearchResultsListModel::activeFilters() const
{
    QVariantList result;
    for (const auto &providerId : _selectedProviderIds) {
        const auto providerIt = _providers.constFind(providerId);
        if (providerIt == _providers.cend()) {
            continue;
        }
        const auto &provider = providerIt.value();
        result.push_back(QVariantMap{{QStringLiteral("type"), QStringLiteral("provider")},
                                     {QStringLiteral("id"), providerId},
                                     {QStringLiteral("label"), provider.name},
                                     {QStringLiteral("icon"), providerIcon(provider, false)}});
    }
    if (_since.isValid() && _until.isValid()) {
        result.push_back(QVariantMap{{QStringLiteral("type"), QStringLiteral("date")},
                                     {QStringLiteral("id"), QStringLiteral("date")},
                                     {QStringLiteral("label"), _dateLabel}});
    }
    if (!_personId.isEmpty()) {
        result.push_back(QVariantMap{{QStringLiteral("type"), QStringLiteral("person")},
                                     {QStringLiteral("id"), _personId},
                                     {QStringLiteral("label"), _personName},
                                     {QStringLiteral("icon"), _personAvatarUrl}});
    }
    return result;
}

bool UnifiedSearchResultsListModel::providersReady() const { return _providersReady; }

bool UnifiedSearchResultsListModel::dateFilterAvailable() const
{
    return std::any_of(_providers.cbegin(), _providers.cend(), [](const auto &provider) {
        return provider.filters.contains(QStringLiteral("since")) && provider.filters.contains(QStringLiteral("until"));
    });
}

bool UnifiedSearchResultsListModel::peopleFilterAvailable() const
{
    return std::any_of(_providers.cbegin(), _providers.cend(), [](const auto &provider) {
        return provider.filters.contains(QStringLiteral("person"));
    });
}

bool UnifiedSearchResultsListModel::hasExternalProviders() const
{
    return std::any_of(_providers.cbegin(), _providers.cend(), [](const auto &provider) { return provider.isExternalProvider; });
}

bool UnifiedSearchResultsListModel::externalProvidersEnabled() const { return _externalProvidersEnabled; }

bool UnifiedSearchResultsListModel::showConnectedServicesAction() const
{
    return _viewMode == ViewMode::Aggregate && hasSearchTerm() && hasExternalProviders() && !isSearchInProgress();
}

bool UnifiedSearchResultsListModel::hasPartialFailure() const { return _hasPartialFailure; }
UnifiedSearchResultsListModel::ViewMode UnifiedSearchResultsListModel::viewMode() const { return _viewMode; }

QString UnifiedSearchResultsListModel::detailProviderName() const
{
    const auto providerIt = _providers.constFind(_detailProviderId);
    return providerIt == _providers.cend() ? QString() : providerIt->name;
}

int UnifiedSearchResultsListModel::selectedRow() const { return _selectedRow; }
QString UnifiedSearchResultsListModel::accessibilityStatus() const { return _accessibilityStatus; }
AccountState *UnifiedSearchResultsListModel::accountState() const { return _accountState; }

void UnifiedSearchResultsListModel::setSearchTerm(const QString &term)
{
    if (_searchTerm == term) {
        return;
    }
    _searchTerm = term;
    emit searchTermChanged();
    scheduleSearch();
}

void UnifiedSearchResultsListModel::discoverProviders()
{
    if (!_accountState || !_accountState->account()) {
        setErrorString(tr("Failed to fetch search providers."));
        return;
    }
    if (!isAccountConnected()) {
        setErrorString(tr("Search is unavailable while this account is offline."));
        return;
    }

    const auto wasInProgress = isSearchInProgress();
    abortProviderDiscovery();
    _providersLoading = true;
    setProvidersReady(false);
    const auto generation = ++_providerGeneration;

    auto *const job = new JsonApiJob(_accountState->account(), QStringLiteral("ocs/v2.php/search/providers"));
    _providerDiscoveryJob = job;
    connect(job, &JsonApiJob::jsonReceived, this, [this, generation, job](const QJsonDocument &json, const int statusCode) {
        providerDiscoveryFinished(json, statusCode, generation, job);
    });
    connect(job, &QObject::destroyed, this, [this, job] {
        if (_providerDiscoveryJob == job) {
            _providerDiscoveryJob.clear();
        }
    });
    job->start();
    updateProgressSignals(wasInProgress);
}

void UnifiedSearchResultsListModel::providerDiscoveryFinished(const QJsonDocument &json,
                                                               int statusCode,
                                                               quint64 generation,
                                                               QObject *jobObject)
{
    if (generation != _providerGeneration || jobObject != _providerDiscoveryJob) {
        return;
    }
    const auto wasInProgress = isSearchInProgress();
    _providerDiscoveryJob.clear();
    _providersLoading = false;
    _providers.clear();
    _providerOrder.clear();

    if (statusCode != 200) {
        if (!_selectedProviderIds.isEmpty()) {
            _selectedProviderIds.clear();
            emit activeFiltersChanged();
        }
        setErrorString(tr("Failed to fetch search providers."));
        emit providersChanged();
        updateProgressSignals(wasInProgress);
        return;
    }

    const auto providerList = json.object().value("ocs"_L1).toObject().value("data"_L1).toArray();
    QVector<QString> appOrder;
    QHash<QString, QVector<QString>> providersByApp;
    auto discoveryIndex = 0;
    for (const auto &providerValue : providerList) {
        const auto providerObject = providerValue.toObject();
        UnifiedSearchProvider provider;
        provider.id = providerObject.value("id"_L1).toString();
        provider.name = providerObject.value("name"_L1).toString();
        if (provider.id.isEmpty() || provider.name.isEmpty()) {
            continue;
        }
        provider.appId = providerObject.value("appId"_L1).toString(provider.id);
        provider.icon = providerObject.value("icon"_L1).toString();
        provider.order = providerObject.contains("order"_L1)
            ? providerObject.value("order"_L1).toInt(std::numeric_limits<int>::max())
            : std::numeric_limits<int>::max();
        provider.discoveryIndex = discoveryIndex++;
        const auto externalValue = providerObject.value("isExternalProvider"_L1);
        provider.isExternalProvider = externalValue.isBool()
            ? externalValue.toBool()
            : externalValue.toString().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0;
        const auto filtersValue = providerObject.value("filters"_L1);
        if (filtersValue.isUndefined() || filtersValue.isNull()) {
            provider.filters.insert(QStringLiteral("term"));
        } else if (filtersValue.isArray()) {
            for (const auto &filter : filtersValue.toArray()) {
                provider.filters.insert(filter.toString());
            }
            if (provider.filters.isEmpty()) provider.filters.insert(QStringLiteral("term"));
        } else {
            const auto filters = filtersValue.toObject();
            for (auto it = filters.constBegin(); it != filters.constEnd(); ++it) {
                provider.filters.insert(it.key());
            }
            if (provider.filters.isEmpty()) provider.filters.insert(QStringLiteral("term"));
        }
        _providers.insert(provider.id, provider);
        if (!providersByApp.contains(provider.appId)) {
            appOrder.push_back(provider.appId);
        }
        providersByApp[provider.appId].push_back(provider.id);
    }
    const auto providerLess = [this](const QString &leftId, const QString &rightId) {
        const auto &left = _providers[leftId];
        const auto &right = _providers[rightId];
        return std::tie(left.order, left.discoveryIndex) < std::tie(right.order, right.discoveryIndex);
    };
    for (auto it = providersByApp.begin(); it != providersByApp.end(); ++it) {
        std::stable_sort(it->begin(), it->end(), providerLess);
    }
    std::stable_sort(appOrder.begin(), appOrder.end(), [&providersByApp, &providerLess](const QString &leftApp, const QString &rightApp) {
        return providerLess(providersByApp[leftApp].constFirst(), providersByApp[rightApp].constFirst());
    });
    for (const auto &appId : appOrder) {
        _providerOrder.append(providersByApp.value(appId));
    }

    const auto previousSelectedProviderIds = _selectedProviderIds;
    _selectedProviderIds.removeIf([this](const auto &providerId) {
        return !_providers.contains(providerId);
    });
    if (_selectedProviderIds != previousSelectedProviderIds) {
        emit activeFiltersChanged();
    }

    setProvidersReady(!_providers.isEmpty());
    setErrorString(_providersReady ? QString() : tr("No search providers are available."));
    emit providersChanged();
    updateProgressSignals(wasInProgress);

    if (_pendingSearch && hasSearchTerm() && _providersReady) {
        _pendingSearch = false;
        startSearch();
    }
}

void UnifiedSearchResultsListModel::scheduleSearch()
{
    const auto wasInProgress = isSearchInProgress();
    ++_queryGeneration;
    abortSearchJobs();
    _revealTimer.stop();
    _debounceTimer.stop();
    _pendingSearch = false;
    _revealWindowClosed = false;
    resetProviderRuntime();
    setErrorString({});
    if (_viewMode != ViewMode::Aggregate) {
        _viewMode = ViewMode::Aggregate;
        _detailProviderId.clear();
        emit viewModeChanged();
    }
    _aggregateSelectedStableKey.clear();
    rebuildProjection();

    if (hasSearchTerm()) {
        setWaitingForSearchTermEditEnd(true);
        _debounceTimer.start();
    } else {
        setWaitingForSearchTermEditEnd(false);
        setAccessibilityStatus(tr("Search cleared"));
    }
    updateProgressSignals(wasInProgress);
}

void UnifiedSearchResultsListModel::startSearch()
{
    if (!_accountState || !_accountState->account() || !isAccountConnected() || !hasSearchTerm() || !_providersReady) {
        return;
    }
    const auto wasInProgress = isSearchInProgress();
    abortSearchJobs();
    _revealOrder.clear();
    _revealWindowClosed = false;
    resetProviderRuntime();
    setErrorString({});

    auto requestCount = 0;
    for (const auto &providerId : _providerOrder) {
        auto &provider = _providers[providerId];
        if (!providerIsApplicable(provider)) {
            continue;
        }
        provider.partialMatch = !providerSupportsAllContentFilters(provider);
        provider.status = ProviderStatus::Loading;
        ++requestCount;
    }
    if (requestCount == 0) {
        rebuildProjection();
        updateAccessibilityStatus();
        updateProgressSignals(wasInProgress);
        return;
    }

    _revealTimer.start();
    for (const auto &providerId : _providerOrder) {
        if (_providers[providerId].status == ProviderStatus::Loading) {
            startSearchForProvider(providerId);
        }
    }
    setAccessibilityStatus(tr("Searching"));
    updateProgressSignals(wasInProgress);
}

void UnifiedSearchResultsListModel::startSearchForProvider(const QString &providerId, bool pagination)
{
    if (!_accountState || !_accountState->account() || !_providers.contains(providerId)) {
        return;
    }
    auto &provider = _providers[providerId];
    const auto generation = _queryGeneration;
    auto *const job = new JsonApiJob(_accountState->account(),
        QStringLiteral("ocs/v2.php/search/providers/%1/search").arg(QString::fromUtf8(QUrl::toPercentEncoding(providerId))));
    job->addQueryParams(queryForProvider(provider, pagination));
    connect(job, &JsonApiJob::jsonReceived, this,
        [this, providerId, pagination, generation, job](const QJsonDocument &json, const int statusCode) {
            providerSearchFinished(json, statusCode, providerId, pagination, generation, job);
        });
    trackSearchJob(job);
    if (pagination) {
        provider.paging = true;
        provider.loadMoreFailed = false;
        _currentFetchMoreInProgressProviderId = providerId;
        emit currentFetchMoreInProgressProviderIdChanged();
        if (!updateDetailPagingState(provider)) {
            rebuildProjection();
        }
    }
    job->start();
}

void UnifiedSearchResultsListModel::providerSearchFinished(const QJsonDocument &json,
                                                            int statusCode,
                                                            const QString &providerId,
                                                            bool pagination,
                                                            quint64 generation,
                                                            QObject *jobObject)
{
    const auto wasInProgress = isSearchInProgress();
    untrackSearchJob(jobObject);
    if (generation != _queryGeneration || !_providers.contains(providerId)) {
        updateProgressSignals(wasInProgress);
        return;
    }

    auto &provider = _providers[providerId];
    const auto previousEntryCount = provider.entries.size();
    if (pagination) {
        provider.paging = false;
        if (_currentFetchMoreInProgressProviderId == providerId) {
            _currentFetchMoreInProgressProviderId.clear();
            emit currentFetchMoreInProgressProviderIdChanged();
        }
    }

    if (statusCode != 200) {
        if (pagination) {
            provider.loadMoreFailed = true;
        } else {
            provider.status = ProviderStatus::Failed;
        }
    } else {
        const auto data = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject();
        auto parsedEntries = parseEntries(data.value("entries"_L1).toArray(), provider,
            pagination ? provider.entries.size() : 0);
        const auto pageHasMore = !parsedEntries.isEmpty()
            && data.value("isPaginated"_L1).toBool(false)
            && !data.value("cursor"_L1).isNull()
            && !data.value("cursor"_L1).isUndefined();
        if (pagination) {
            provider.entries.append(std::move(parsedEntries));
            provider.hasMore = pageHasMore;
        } else {
            provider.entries = std::move(parsedEntries);
            provider.status = ProviderStatus::Blocked;
            provider.hasMore = pageHasMore;
        }
        provider.cursor = data.value("cursor"_L1);
        provider.loadMoreFailed = false;
    }

    if (!pagination) {
        promoteReadyProviders();
        updateAggregateState();
    } else if (!updateDetailProjectionAfterPagination(provider, previousEntryCount)) {
        rebuildProjection();
    }
    updateAccessibilityStatus();
    updateProgressSignals(wasInProgress);
}

QVector<UnifiedSearchResult> UnifiedSearchResultsListModel::parseEntries(const QJsonArray &entries,
                                                                         const UnifiedSearchProvider &provider,
                                                                         int firstEntryIndex) const
{
    QVector<UnifiedSearchResult> parsedEntries;
    const auto account = _accountState ? _accountState->account() : AccountPtr();
    const auto accountUrl = account ? account->url() : QUrl();
    auto entryIndex = firstEntryIndex;
    for (const auto &entryValue : entries) {
        if (parsedEntries.size() >= requestResultsPerProvider) {
            break;
        }
        const auto entry = entryValue.toObject();
        if (entry.isEmpty()) {
            continue;
        }
        UnifiedSearchResult result;
        result._providerId = provider.id;
        result._providerName = provider.name;
        result._order = provider.order;
        result._isRounded = entry.value("rounded"_L1).toBool(false);
        result._title = entry.value("title"_L1).toString();
        result._subline = entry.value("subline"_L1).toString();
        result._resourceUrl = openableResourceUrl(QUrl(entry.value("resourceUrl"_L1).toString()), accountUrl);
        const auto entryIcon = entry.value("icon"_L1).toString();
        const auto darkNavigationAppIcon = navigationAppIconForResult(_accountState, provider.id, result._title, true);
        const auto lightNavigationAppIcon = navigationAppIconForResult(_accountState, provider.id, result._title, false);
        const auto darkFallbackIcon = !darkNavigationAppIcon.isEmpty()
            ? darkNavigationAppIcon
            : (entryIcon.isEmpty() ? providerIcon(provider, true) : entryIcon);
        const auto lightFallbackIcon = !lightNavigationAppIcon.isEmpty()
            ? lightNavigationAppIcon
            : (entryIcon.isEmpty() ? providerIcon(provider, false) : entryIcon);
        const auto darkIcons = iconsFromThumbnailAndFallbackIcon(entry.value("thumbnailUrl"_L1).toString(), darkFallbackIcon, accountUrl, true);
        const auto lightIcons = iconsFromThumbnailAndFallbackIcon(entry.value("thumbnailUrl"_L1).toString(), lightFallbackIcon, accountUrl, false);
        result._darkIcons = darkIcons.first;
        result._lightIcons = lightIcons.first;
        result._darkIconsIsThumbnail = darkIcons.second;
        result._lightIconsIsThumbnail = lightIcons.second;
        result._providerIcon = providerIcon(provider, false);
        result._isSelectable = true;
        result._stableKey = stableKeyForResult(provider.id, result, entryIndex++);
        parsedEntries.push_back(result);
    }
    return parsedEntries;
}

void UnifiedSearchResultsListModel::promoteReadyProviders()
{
    auto unresolvedPredecessor = false;
    auto changed = false;
    for (const auto &providerId : _providerOrder) {
        auto &provider = _providers[providerId];
        if (!providerIsApplicable(provider)) {
            continue;
        }
        if (provider.status == ProviderStatus::Loading) {
            unresolvedPredecessor = true;
        } else if (provider.status == ProviderStatus::Blocked) {
            if (_revealWindowClosed || !unresolvedPredecessor) {
                provider.status = ProviderStatus::Loaded;
                if (!_revealOrder.contains(providerId)) {
                    _revealOrder.push_back(providerId);
                }
                changed = true;
            } else {
                unresolvedPredecessor = true;
            }
        }
    }
    if (changed) {
        rebuildProjection();
    }
}

void UnifiedSearchResultsListModel::closeRevealWindow()
{
    _revealWindowClosed = true;
    promoteReadyProviders();
}

void UnifiedSearchResultsListModel::rebuildProjection()
{
    if (_selectedRow >= 0 && _selectedRow < _results.size() && _results[_selectedRow]._isSelectable) {
        _selectedStableKey = _results[_selectedRow]._stableKey;
    }

    QVector<UnifiedSearchResult> projection;
    if (_viewMode == ViewMode::ProviderDetail && _providers.contains(_detailProviderId)) {
        const auto &provider = _providers[_detailProviderId];
        projection = provider.entries;
        if (provider.loadMoreFailed || provider.hasMore) {
            projection.push_back(pagingRowForProvider(provider));
        }
    } else {
        QSet<QString> filteredResourceUrls;
        for (const auto &providerId : _revealOrder) {
            const auto &provider = _providers[providerId];
            if (provider.partialMatch) {
                continue;
            }
            for (const auto &entry : provider.entries) {
                if (!entry._resourceUrl.isEmpty()) {
                    filteredResourceUrls.insert(entry._resourceUrl.toString());
                }
            }
            appendProviderProjection(provider, false, {}, projection);
        }

        auto partialHeaderAdded = false;
        for (const auto &providerId : _revealOrder) {
            const auto &provider = _providers[providerId];
            if (!provider.partialMatch || provider.entries.isEmpty()) {
                continue;
            }
            const auto hasVisibleEntry = std::any_of(provider.entries.cbegin(), provider.entries.cend(),
                [&filteredResourceUrls](const auto &entry) {
                    return entry._resourceUrl.isEmpty() || !filteredResourceUrls.contains(entry._resourceUrl.toString());
                });
            if (!hasVisibleEntry) {
                continue;
            }
            if (!partialHeaderAdded) {
                UnifiedSearchResult header;
                header._type = UnifiedSearchResult::Type::PartialMatchesHeader;
                header._title = tr("Partial matches");
                header._stableKey = QStringLiteral("partial-matches");
                projection.push_back(header);
                partialHeaderAdded = true;
            }
            appendProviderProjection(provider, true, filteredResourceUrls, projection);
        }
    }

    auto selectedRow = -1;
    for (auto row = 0; row < projection.size(); ++row) {
        if (!projection[row]._isSelectable) {
            continue;
        }
        if (selectedRow < 0) {
            selectedRow = row;
        }
        if (!_selectedStableKey.isEmpty() && projection[row]._stableKey == _selectedStableKey) {
            selectedRow = row;
            break;
        }
    }
    for (auto row = 0; row < projection.size(); ++row) {
        projection[row]._isSelected = row == selectedRow;
    }

    beginResetModel();
    _results = std::move(projection);
    endResetModel();
    setSelectedRow(selectedRow);
    if (selectedRow >= 0) {
        _selectedStableKey = _results[selectedRow]._stableKey;
    } else {
        _selectedStableKey.clear();
    }
}

UnifiedSearchResult UnifiedSearchResultsListModel::pagingRowForProvider(const UnifiedSearchProvider &provider) const
{
    UnifiedSearchResult pagingRow;
    pagingRow._providerId = provider.id;
    pagingRow._providerName = provider.name;
    pagingRow._type = provider.loadMoreFailed
        ? UnifiedSearchResult::Type::RetryFetchMoreTrigger
        : UnifiedSearchResult::Type::FetchMoreTrigger;
    pagingRow._isLoading = provider.paging;
    pagingRow._stableKey = QStringLiteral("paging:%1").arg(provider.id);
    return pagingRow;
}

bool UnifiedSearchResultsListModel::updateDetailPagingState(const UnifiedSearchProvider &provider)
{
    if (_viewMode != ViewMode::ProviderDetail || _detailProviderId != provider.id || _results.isEmpty()) {
        return false;
    }
    const auto row = _results.size() - 1;
    auto &pagingRow = _results[row];
    if (pagingRow._providerId != provider.id
        || (pagingRow._type != UnifiedSearchResult::Type::FetchMoreTrigger
            && pagingRow._type != UnifiedSearchResult::Type::RetryFetchMoreTrigger)) {
        return false;
    }
    pagingRow = pagingRowForProvider(provider);
    emit dataChanged(index(row), index(row), {TypeRole, TypeAsStringRole, LoadingRole});
    return true;
}

bool UnifiedSearchResultsListModel::updateDetailProjectionAfterPagination(const UnifiedSearchProvider &provider,
                                                                           const int previousEntryCount)
{
    if (_viewMode != ViewMode::ProviderDetail || _detailProviderId != provider.id
        || previousEntryCount < 0 || provider.entries.size() < previousEntryCount
        || _results.size() != previousEntryCount + 1) {
        return false;
    }
    const auto pagingRowIndex = _results.size() - 1;
    if (_results[pagingRowIndex]._providerId != provider.id
        || (_results[pagingRowIndex]._type != UnifiedSearchResult::Type::FetchMoreTrigger
            && _results[pagingRowIndex]._type != UnifiedSearchResult::Type::RetryFetchMoreTrigger)) {
        return false;
    }

    if (provider.loadMoreFailed) {
        _results[pagingRowIndex] = pagingRowForProvider(provider);
        emit dataChanged(index(pagingRowIndex), index(pagingRowIndex));
        return true;
    }

    const auto appendedCount = provider.entries.size() - previousEntryCount;
    if (appendedCount == 0) {
        beginRemoveRows({}, pagingRowIndex, pagingRowIndex);
        _results.removeLast();
        endRemoveRows();
        return true;
    }

    _results[pagingRowIndex] = provider.entries[previousEntryCount];
    emit dataChanged(index(pagingRowIndex), index(pagingRowIndex));

    QVector<UnifiedSearchResult> tail;
    tail.reserve(appendedCount);
    for (auto entryIndex = previousEntryCount + 1; entryIndex < provider.entries.size(); ++entryIndex) {
        tail.push_back(provider.entries[entryIndex]);
    }
    if (provider.hasMore) {
        tail.push_back(pagingRowForProvider(provider));
    }
    if (!tail.isEmpty()) {
        const auto firstRow = _results.size();
        const auto lastRow = firstRow + tail.size() - 1;
        beginInsertRows({}, firstRow, lastRow);
        _results.append(tail);
        endInsertRows();
    }
    return true;
}

void UnifiedSearchResultsListModel::appendProviderProjection(const UnifiedSearchProvider &provider,
                                                              bool partial,
                                                              const QSet<QString> &filteredResourceUrls,
                                                              QVector<UnifiedSearchResult> &projection) const
{
    QVector<UnifiedSearchResult> visibleEntries;
    visibleEntries.reserve(aggregateResultsPerProvider + 1);
    for (const auto &entry : provider.entries) {
        if (partial && !entry._resourceUrl.isEmpty() && filteredResourceUrls.contains(entry._resourceUrl.toString())) {
            continue;
        }
        auto visibleEntry = entry;
        visibleEntry._isPartialMatch = partial;
        visibleEntries.push_back(visibleEntry);
        if (visibleEntries.size() > aggregateResultsPerProvider) {
            break;
        }
    }
    if (visibleEntries.isEmpty()) {
        return;
    }

    UnifiedSearchResult header;
    header._providerId = provider.id;
    header._providerName = provider.name;
    header._providerIcon = visibleEntries.constFirst()._providerIcon;
    header._title = provider.name;
    header._hasOverflow = visibleEntries.size() > aggregateResultsPerProvider || provider.hasMore;
    header._isPartialMatch = partial;
    header._type = UnifiedSearchResult::Type::ProviderHeader;
    header._stableKey = QStringLiteral("header:%1:%2").arg(partial ? QStringLiteral("partial") : QStringLiteral("full"), provider.id);
    projection.push_back(header);

    const auto visibleCount = std::min(aggregateResultsPerProvider, visibleEntries.size());
    for (auto index = 0; index < visibleCount; ++index) {
        projection.push_back(visibleEntries[index]);
    }
}

void UnifiedSearchResultsListModel::resetProviderRuntime()
{
    for (auto it = _providers.begin(); it != _providers.end(); ++it) {
        it->status = ProviderStatus::Idle;
        it->entries.clear();
        it->cursor = {};
        it->hasMore = false;
        it->loadMoreFailed = false;
        it->paging = false;
        it->partialMatch = false;
    }
    _revealOrder.clear();
    if (_hasPartialFailure) {
        _hasPartialFailure = false;
        emit hasPartialFailureChanged();
    }
    if (!_currentFetchMoreInProgressProviderId.isEmpty()) {
        _currentFetchMoreInProgressProviderId.clear();
        emit currentFetchMoreInProgressProviderIdChanged();
    }
}

void UnifiedSearchResultsListModel::abortSearchJobs()
{
    const auto hadJobs = !_activeSearchJobs.isEmpty() || _inFlightSearchRequests > 0;
    for (const auto &jobObject : std::as_const(_activeSearchJobs)) {
        if (!jobObject) {
            continue;
        }
        disconnect(jobObject, nullptr, this, nullptr);
        if (const auto job = qobject_cast<JsonApiJob *>(jobObject.data()); job && job->reply() && job->reply()->isRunning()) {
            job->reply()->abort();
        }
        jobObject->deleteLater();
    }
    _activeSearchJobs.clear();
    _inFlightSearchRequests = 0;
    if (hadJobs) {
        emit isSearchInProgressChanged();
    }
}

void UnifiedSearchResultsListModel::abortProviderDiscovery()
{
    if (!_providerDiscoveryJob) {
        _providersLoading = false;
        return;
    }
    disconnect(_providerDiscoveryJob, nullptr, this, nullptr);
    if (const auto job = qobject_cast<JsonApiJob *>(_providerDiscoveryJob.data()); job && job->reply() && job->reply()->isRunning()) {
        job->reply()->abort();
    }
    _providerDiscoveryJob->deleteLater();
    _providerDiscoveryJob.clear();
    _providersLoading = false;
}

void UnifiedSearchResultsListModel::trackSearchJob(QObject *jobObject)
{
    const auto wasInProgress = isSearchInProgress();
    _activeSearchJobs.push_back(jobObject);
    ++_inFlightSearchRequests;
    connect(jobObject, &QObject::destroyed, this, [this, jobObject] { untrackSearchJob(jobObject); });
    updateProgressSignals(wasInProgress);
}

void UnifiedSearchResultsListModel::untrackSearchJob(QObject *jobObject)
{
    const auto it = std::find_if(_activeSearchJobs.begin(), _activeSearchJobs.end(), [jobObject](const auto &candidate) {
        return candidate == jobObject;
    });
    if (it == _activeSearchJobs.end()) {
        return;
    }
    _activeSearchJobs.erase(it);
    _inFlightSearchRequests = std::max(0, _inFlightSearchRequests - 1);
}

void UnifiedSearchResultsListModel::updateProgressSignals(bool wasInProgress)
{
    if (wasInProgress != isSearchInProgress()) {
        emit isSearchInProgressChanged();
    }
}

void UnifiedSearchResultsListModel::updateAggregateState()
{
    auto applicableCount = 0;
    auto failedCount = 0;
    auto settledCount = 0;
    for (const auto &providerId : _providerOrder) {
        const auto &provider = _providers[providerId];
        if (!providerIsApplicable(provider)) {
            continue;
        }
        ++applicableCount;
        if (provider.status == ProviderStatus::Failed) {
            ++failedCount;
            ++settledCount;
        } else if (provider.status == ProviderStatus::Loaded || provider.status == ProviderStatus::Blocked) {
            ++settledCount;
        }
    }

    const auto partialFailure = failedCount > 0 && failedCount < applicableCount;
    if (_hasPartialFailure != partialFailure) {
        _hasPartialFailure = partialFailure;
        emit hasPartialFailureChanged();
    }
    if (applicableCount > 0 && failedCount == applicableCount) {
        setErrorString(tr("Search failed for all available sources. Please try again."));
    } else if (settledCount == applicableCount) {
        setErrorString({});
        _revealTimer.stop();
        promoteReadyProviders();
    }
    emit showConnectedServicesActionChanged();
}

void UnifiedSearchResultsListModel::updateAccessibilityStatus()
{
    if (isSearchInProgress()) {
        setAccessibilityStatus(tr("Searching"));
        return;
    }
    auto resultCount = 0;
    for (const auto &result : _results) {
        if (result._type == UnifiedSearchResult::Type::Default) {
            ++resultCount;
        }
    }
    if (_viewMode == ViewMode::ProviderDetail) {
        setAccessibilityStatus(tr("%1 results in %2").arg(resultCount).arg(detailProviderName()));
    } else if (resultCount == 0) {
        setAccessibilityStatus(tr("No matching results"));
    } else if (_hasPartialFailure) {
        setAccessibilityStatus(tr("%1 results. Some sources are unavailable.").arg(resultCount));
    } else {
        setAccessibilityStatus(tr("%1 results").arg(resultCount));
    }
}

void UnifiedSearchResultsListModel::restartForFilterChange()
{
    emit activeFiltersChanged();
    emit providersChanged();
    if (_viewMode != ViewMode::Aggregate) {
        _viewMode = ViewMode::Aggregate;
        _detailProviderId.clear();
        emit viewModeChanged();
    }
    if (hasSearchTerm()) {
        scheduleSearch();
    }
}

void UnifiedSearchResultsListModel::setErrorString(const QString &error)
{
    if (_errorString == error) {
        return;
    }
    _errorString = error;
    emit errorStringChanged();
}

void UnifiedSearchResultsListModel::setProvidersReady(const bool ready)
{
    if (_providersReady == ready) {
        return;
    }
    _providersReady = ready;
    emit providersReadyChanged();
}

void UnifiedSearchResultsListModel::setWaitingForSearchTermEditEnd(bool waiting)
{
    if (_waitingForSearchTermEditEnd == waiting) {
        return;
    }
    const auto wasInProgress = isSearchInProgress();
    _waitingForSearchTermEditEnd = waiting;
    emit waitingForSearchTermEditEndChanged();
    updateProgressSignals(wasInProgress);
}

void UnifiedSearchResultsListModel::setSelectedRow(int row)
{
    if (_selectedRow == row) {
        return;
    }
    _selectedRow = row;
    emit selectedRowChanged();
}

void UnifiedSearchResultsListModel::setAccessibilityStatus(const QString &status)
{
    if (_accessibilityStatus == status) {
        return;
    }
    _accessibilityStatus = status;
    emit accessibilityStatusChanged();
}

bool UnifiedSearchResultsListModel::providerIsApplicable(const UnifiedSearchProvider &provider) const
{
    if (!_selectedProviderIds.isEmpty()) {
        return _selectedProviderIds.contains(provider.id);
    }
    return !provider.isExternalProvider || _externalProvidersEnabled;
}

bool UnifiedSearchResultsListModel::providerSupportsAllContentFilters(const UnifiedSearchProvider &provider) const
{
    const auto supportsDate = !_since.isValid() || (provider.filters.contains(QStringLiteral("since")) && provider.filters.contains(QStringLiteral("until")));
    const auto supportsPerson = _personId.isEmpty() || provider.filters.contains(QStringLiteral("person"));
    return supportsDate && supportsPerson;
}

QString UnifiedSearchResultsListModel::providerIcon(const UnifiedSearchProvider &provider, const bool darkMode) const
{
    if (_accountState) {
        if (const auto app = _accountState->findApp(provider.appId); app && !app->iconUrl().isEmpty()) {
            return app->iconUrl().toString();
        }
    }

    const auto account = _accountState ? _accountState->account() : AccountPtr();
    return normalizedIcon(provider.icon, account ? account->url() : QUrl(), darkMode);
}

QUrlQuery UnifiedSearchResultsListModel::queryForProvider(const UnifiedSearchProvider &provider, bool pagination) const
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), _searchTerm);
    query.addQueryItem(QStringLiteral("limit"), QString::number(requestResultsPerProvider));
    if (_since.isValid() && provider.filters.contains(QStringLiteral("since"))) {
        query.addQueryItem(QStringLiteral("since"), _since.toUTC().toString(Qt::ISODateWithMs));
    }
    if (_until.isValid() && provider.filters.contains(QStringLiteral("until"))) {
        query.addQueryItem(QStringLiteral("until"), _until.toUTC().toString(Qt::ISODateWithMs));
    }
    if (!_personId.isEmpty() && provider.filters.contains(QStringLiteral("person"))) {
        query.addQueryItem(QStringLiteral("person"), _personId);
    }
    if (pagination && !provider.cursor.isNull() && !provider.cursor.isUndefined()) {
        query.addQueryItem(QStringLiteral("cursor"), provider.cursor.toVariant().toString());
    }
    return query;
}

QString UnifiedSearchResultsListModel::stableKeyForResult(const QString &providerId,
                                                           const UnifiedSearchResult &result,
                                                           int entryIndex)
{
    return QStringLiteral("result:%1:%2:%3").arg(providerId, result._resourceUrl.toString(), QString::number(entryIndex));
}

QUrl UnifiedSearchResultsListModel::openableResourceUrl(const QUrl &resourceUrl, const QUrl &accountUrl)
{
    return resourceUrl.isRelative() ? accountUrl.resolved(resourceUrl) : resourceUrl;
}

void UnifiedSearchResultsListModel::resultClicked(const QString &providerId, const QUrl &resourceUrl) const
{
    const QUrlQuery urlQuery{resourceUrl};
    const auto dir = urlQuery.queryItemValue(QStringLiteral("dir"), QUrl::FullyDecoded);
    const auto fileName = urlQuery.queryItemValue(QStringLiteral("scrollto"), QUrl::FullyDecoded);
    if (providerId.contains("file"_L1, Qt::CaseInsensitive) && !dir.isEmpty() && !fileName.isEmpty()) {
        if (!_accountState || !_accountState->account()) {
            return;
        }
        const auto relativePath = dir + u'/' + fileName;
        const auto localFiles = FolderMan::instance()->findFileInLocalFolders(QFileInfo(relativePath).path(), _accountState->account());
        if (!localFiles.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(localFiles.constFirst()));
            return;
        }
    }
    Utility::openBrowser(resourceUrl);
}

void UnifiedSearchResultsListModel::fetchMoreTriggerClicked(const QString &providerId) { loadMore(providerId); }

void UnifiedSearchResultsListModel::toggleProviderFilter(const QString &providerId)
{
    if (!_providers.contains(providerId)) {
        return;
    }
    if (_selectedProviderIds.contains(providerId)) {
        _selectedProviderIds.removeAll(providerId);
    } else {
        _selectedProviderIds.push_back(providerId);
    }
    restartForFilterChange();
}

void UnifiedSearchResultsListModel::clearTypeFilters()
{
    if (_selectedProviderIds.isEmpty()) {
        return;
    }
    _selectedProviderIds.clear();
    restartForFilterChange();
}

void UnifiedSearchResultsListModel::setDatePreset(const QString &preset)
{
    const auto today = QDate::currentDate();
    auto first = today;
    auto last = today;
    if (preset == QStringLiteral("today")) {
        _dateLabel = tr("Today");
    } else if (preset == QStringLiteral("7days") || preset == QStringLiteral("last7days")) {
        first = today.addDays(-6);
        _dateLabel = tr("Last 7 days");
    } else if (preset == QStringLiteral("30days") || preset == QStringLiteral("last30days")) {
        first = today.addDays(-29);
        _dateLabel = tr("Last 30 days");
    } else if (preset == QStringLiteral("thisyear")) {
        first = QDate(today.year(), 1, 1);
        last = QDate(today.year(), 12, 31);
        _dateLabel = tr("This year");
    } else if (preset == QStringLiteral("lastyear")) {
        first = QDate(today.year() - 1, 1, 1);
        last = QDate(today.year() - 1, 12, 31);
        _dateLabel = tr("Last year");
    } else {
        return;
    }
    const auto zone = QTimeZone::systemTimeZone();
    _since = QDateTime(first, QTime(0, 0), zone);
    _until = QDateTime(last, QTime(23, 59, 59, 999), zone);
    restartForFilterChange();
}

bool UnifiedSearchResultsListModel::setCustomDateRange(const QString &sinceDate, const QString &untilDate)
{
    const auto first = QDate::fromString(sinceDate, Qt::ISODate);
    const auto last = QDate::fromString(untilDate, Qt::ISODate);
    if (!first.isValid() || !last.isValid() || last < first) {
        return false;
    }
    const auto zone = QTimeZone::systemTimeZone();
    _since = QDateTime(first, QTime(0, 0), zone);
    _until = QDateTime(last, QTime(23, 59, 59, 999), zone);
    _dateLabel = tr("%1 – %2").arg(QLocale().toString(first, QLocale::ShortFormat), QLocale().toString(last, QLocale::ShortFormat));
    restartForFilterChange();
    return true;
}

void UnifiedSearchResultsListModel::clearDateFilter()
{
    if (!_since.isValid() && !_until.isValid()) {
        return;
    }
    _since = {};
    _until = {};
    _dateLabel.clear();
    restartForFilterChange();
}

void UnifiedSearchResultsListModel::setPersonFilter(const QString &userId, const QString &displayName, const QString &avatarUrl)
{
    if (userId.isEmpty()) {
        return;
    }
    _personId = userId;
    _personName = displayName;
    _personAvatarUrl = avatarUrl;
    restartForFilterChange();
}

void UnifiedSearchResultsListModel::clearPersonFilter()
{
    if (_personId.isEmpty()) {
        return;
    }
    _personId.clear();
    _personName.clear();
    _personAvatarUrl.clear();
    restartForFilterChange();
}

void UnifiedSearchResultsListModel::removeFilter(const QString &type, const QString &id)
{
    if (type == QStringLiteral("provider")) {
        if (_selectedProviderIds.removeAll(id) > 0) {
            restartForFilterChange();
        }
    } else if (type == QStringLiteral("date")) {
        clearDateFilter();
    } else if (type == QStringLiteral("person")) {
        clearPersonFilter();
    }
}

void UnifiedSearchResultsListModel::setExternalProvidersEnabled(bool enabled)
{
    if (_externalProvidersEnabled == enabled) {
        return;
    }
    _externalProvidersEnabled = enabled;
    emit externalProvidersEnabledChanged();
    if (hasSearchTerm()) {
        scheduleSearch();
    }
}

void UnifiedSearchResultsListModel::openProviderDetail(const QString &providerId)
{
    const auto providerIt = _providers.constFind(providerId);
    if (providerIt == _providers.cend()
        || (providerIt->entries.size() <= aggregateResultsPerProvider && !providerIt->hasMore)) {
        return;
    }
    _aggregateSelectedStableKey = _selectedStableKey;
    _detailProviderId = providerId;
    _viewMode = ViewMode::ProviderDetail;
    _selectedStableKey.clear();
    emit viewModeChanged();
    rebuildProjection();
    updateAccessibilityStatus();
}

void UnifiedSearchResultsListModel::closeProviderDetail()
{
    if (_viewMode == ViewMode::Aggregate) {
        return;
    }
    _viewMode = ViewMode::Aggregate;
    _detailProviderId.clear();
    _selectedStableKey = _aggregateSelectedStableKey;
    _aggregateSelectedStableKey.clear();
    emit viewModeChanged();
    rebuildProjection();
    updateAccessibilityStatus();
}

void UnifiedSearchResultsListModel::loadMore(const QString &providerId)
{
    if (_viewMode != ViewMode::ProviderDetail || _detailProviderId != providerId || !_providers.contains(providerId)) {
        return;
    }
    const auto &provider = _providers[providerId];
    if (provider.paging || (!provider.hasMore && !provider.loadMoreFailed)) {
        return;
    }
    startSearchForProvider(providerId, true);
}

void UnifiedSearchResultsListModel::retryLoadMore(const QString &providerId) { loadMore(providerId); }

void UnifiedSearchResultsListModel::retryFailedProviders()
{
    if (!hasSearchTerm()) {
        return;
    }
    _revealWindowClosed = true;
    setErrorString({});
    for (const auto &providerId : _providerOrder) {
        auto &provider = _providers[providerId];
        if (provider.status == ProviderStatus::Failed && providerIsApplicable(provider)) {
            provider.status = ProviderStatus::Loading;
            startSearchForProvider(providerId);
        }
    }
    if (_hasPartialFailure) {
        _hasPartialFailure = false;
        emit hasPartialFailureChanged();
    }
}

void UnifiedSearchResultsListModel::retry()
{
    if (!_providersReady) {
        discoverProviders();
    } else if (hasSearchTerm()) {
        scheduleSearch();
    }
}

void UnifiedSearchResultsListModel::moveSelection(SelectionDirection direction)
{
    QVector<int> selectableRows;
    for (auto row = 0; row < _results.size(); ++row) {
        if (_results[row]._isSelectable) {
            selectableRows.push_back(row);
        }
    }
    if (selectableRows.isEmpty()) {
        return;
    }

    auto newRow = selectableRows.constFirst();
    const auto currentPosition = selectableRows.indexOf(_selectedRow);
    switch (direction) {
    case SelectionDirection::Previous:
        newRow = currentPosition <= 0 ? selectableRows.constFirst() : selectableRows[currentPosition - 1];
        break;
    case SelectionDirection::Next:
        newRow = currentPosition < 0 || currentPosition >= selectableRows.size() - 1
            ? selectableRows.constLast()
            : selectableRows[currentPosition + 1];
        break;
    case SelectionDirection::First:
        newRow = selectableRows.constFirst();
        break;
    case SelectionDirection::Last:
        newRow = selectableRows.constLast();
        break;
    }
    if (newRow == _selectedRow) {
        return;
    }
    const auto previousRow = _selectedRow;
    if (previousRow >= 0 && previousRow < _results.size()) {
        _results[previousRow]._isSelected = false;
    }
    _results[newRow]._isSelected = true;
    _selectedStableKey = _results[newRow]._stableKey;
    setSelectedRow(newRow);
    if (previousRow >= 0) {
        emit dataChanged(index(previousRow), index(previousRow), {SelectedRole});
    }
    emit dataChanged(index(newRow), index(newRow), {SelectedRole});
}

void UnifiedSearchResultsListModel::activateSelected() const
{
    if (_selectedRow < 0 || _selectedRow >= _results.size()) {
        return;
    }
    const auto &result = _results[_selectedRow];
    if (result._isSelectable) {
        resultClicked(result._providerId, result._resourceUrl);
    }
}
}
