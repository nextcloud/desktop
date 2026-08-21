/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountstate.h"
#include "unifiedsearchresult.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>

#include <limits>

namespace OCC {

/**
 * @brief Account-scoped presentation model for Nextcloud Unified Search.
 * @ingroup gui
 *
 * The model owns provider discovery, concurrent searches, stable provider
 * reveal order, filters, aggregate/detail projections and keyboard selection.
 */
class UnifiedSearchResultsListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool isSearchInProgress READ isSearchInProgress NOTIFY isSearchInProgressChanged)
    Q_PROPERTY(QString currentFetchMoreInProgressProviderId READ currentFetchMoreInProgressProviderId NOTIFY currentFetchMoreInProgressProviderIdChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm NOTIFY searchTermChanged)
    Q_PROPERTY(bool waitingForSearchTermEditEnd READ waitingForSearchTermEditEnd NOTIFY waitingForSearchTermEditEndChanged)
    Q_PROPERTY(bool isFetchMoreInProgress READ isFetchMoreInProgress NOTIFY currentFetchMoreInProgressProviderIdChanged)
    Q_PROPERTY(bool hasSearchTerm READ hasSearchTerm NOTIFY searchTermChanged)
    Q_PROPERTY(bool hasSearchError READ hasSearchError NOTIFY errorStringChanged)
    Q_PROPERTY(bool canEditSearch READ canEditSearch NOTIFY canEditSearchChanged)
    Q_PROPERTY(bool isAccountConnected READ isAccountConnected NOTIFY canEditSearchChanged)
    Q_PROPERTY(SearchState searchState READ searchState NOTIFY searchStateChanged)
    Q_PROPERTY(QVariantList providers READ providers NOTIFY providersChanged)
    Q_PROPERTY(QVariantList activeFilters READ activeFilters NOTIFY activeFiltersChanged)
    Q_PROPERTY(bool providersReady READ providersReady NOTIFY providersReadyChanged)
    Q_PROPERTY(bool dateFilterAvailable READ dateFilterAvailable NOTIFY providersChanged)
    Q_PROPERTY(bool peopleFilterAvailable READ peopleFilterAvailable NOTIFY providersChanged)
    Q_PROPERTY(bool hasExternalProviders READ hasExternalProviders NOTIFY providersChanged)
    Q_PROPERTY(bool externalProvidersEnabled READ externalProvidersEnabled NOTIFY externalProvidersEnabledChanged)
    Q_PROPERTY(bool showConnectedServicesAction READ showConnectedServicesAction NOTIFY showConnectedServicesActionChanged)
    Q_PROPERTY(bool hasPartialFailure READ hasPartialFailure NOTIFY hasPartialFailureChanged)
    Q_PROPERTY(ViewMode viewMode READ viewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QString detailProviderName READ detailProviderName NOTIFY viewModeChanged)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY selectedRowChanged)
    Q_PROPERTY(QString accessibilityStatus READ accessibilityStatus NOTIFY accessibilityStatusChanged)
    Q_PROPERTY(AccountState *accountState READ accountState CONSTANT)

public:
    enum class SearchState {
        None,
        Placeholder,
        Skeleton,
        NothingFound,
        Results,
        SearchError,
    };
    Q_ENUM(SearchState)

    enum class ViewMode {
        Aggregate,
        ProviderDetail,
    };
    Q_ENUM(ViewMode)

    enum class SelectionDirection {
        Previous,
        Next,
        First,
        Last,
    };
    Q_ENUM(SelectionDirection)

    enum class ResultType {
        Default = static_cast<int>(UnifiedSearchResult::Type::Default),
        ProviderHeader = static_cast<int>(UnifiedSearchResult::Type::ProviderHeader),
        PartialMatchesHeader = static_cast<int>(UnifiedSearchResult::Type::PartialMatchesHeader),
        FetchMoreTrigger = static_cast<int>(UnifiedSearchResult::Type::FetchMoreTrigger),
        RetryFetchMoreTrigger = static_cast<int>(UnifiedSearchResult::Type::RetryFetchMoreTrigger),
    };
    Q_ENUM(ResultType)

    enum DataRole {
        ProviderNameRole = Qt::UserRole + 1,
        ProviderIdRole,
        ProviderIconRole,
        DarkImagePlaceholderRole,
        LightImagePlaceholderRole,
        DarkIconsRole,
        LightIconsRole,
        DarkIconsIsThumbnailRole,
        LightIconsIsThumbnailRole,
        TitleRole,
        SublineRole,
        ResourceUrlRole,
        RoundedRole,
        TypeRole,
        TypeAsStringRole,
        StableKeyRole,
        SelectedRole,
        SelectableRole,
        PartialMatchRole,
        HasOverflowRole,
        LoadingRole,
    };

    explicit UnifiedSearchResultsListModel(AccountState *accountState,
                                            QObject *parent = nullptr,
                                            int debounceInterval = 300,
                                            int revealInterval = 1000);
    ~UnifiedSearchResultsListModel() override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool isSearchInProgress() const;
    [[nodiscard]] QString currentFetchMoreInProgressProviderId() const;
    [[nodiscard]] QString searchTerm() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] bool waitingForSearchTermEditEnd() const;
    [[nodiscard]] bool isFetchMoreInProgress() const;
    [[nodiscard]] bool hasSearchTerm() const;
    [[nodiscard]] bool hasSearchError() const;
    [[nodiscard]] bool canEditSearch() const;
    [[nodiscard]] bool isAccountConnected() const;
    [[nodiscard]] SearchState searchState() const;
    [[nodiscard]] QVariantList providers() const;
    [[nodiscard]] QVariantList activeFilters() const;
    [[nodiscard]] bool providersReady() const;
    [[nodiscard]] bool dateFilterAvailable() const;
    [[nodiscard]] bool peopleFilterAvailable() const;
    [[nodiscard]] bool hasExternalProviders() const;
    [[nodiscard]] bool externalProvidersEnabled() const;
    [[nodiscard]] bool showConnectedServicesAction() const;
    [[nodiscard]] bool hasPartialFailure() const;
    [[nodiscard]] ViewMode viewMode() const;
    [[nodiscard]] QString detailProviderName() const;
    [[nodiscard]] int selectedRow() const;
    [[nodiscard]] QString accessibilityStatus() const;
    [[nodiscard]] AccountState *accountState() const;

    Q_INVOKABLE void resultClicked(const QString &providerId, const QUrl &resourceUrl) const;
    Q_INVOKABLE void fetchMoreTriggerClicked(const QString &providerId);
    Q_INVOKABLE void toggleProviderFilter(const QString &providerId);
    Q_INVOKABLE void clearTypeFilters();
    Q_INVOKABLE void setDatePreset(const QString &preset);
    Q_INVOKABLE bool setCustomDateRange(const QString &sinceDate, const QString &untilDate);
    Q_INVOKABLE void clearDateFilter();
    Q_INVOKABLE void setPersonFilter(const QString &userId, const QString &displayName, const QString &avatarUrl = {});
    Q_INVOKABLE void clearPersonFilter();
    Q_INVOKABLE void removeFilter(const QString &type, const QString &id = {});
    Q_INVOKABLE void setExternalProvidersEnabled(bool enabled);
    Q_INVOKABLE void openProviderDetail(const QString &providerId);
    Q_INVOKABLE void closeProviderDetail();
    Q_INVOKABLE void loadMore(const QString &providerId);
    Q_INVOKABLE void retryLoadMore(const QString &providerId);
    Q_INVOKABLE void retryFailedProviders();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void moveSelection(SelectionDirection direction);
    Q_INVOKABLE void activateSelected() const;

signals:
    void currentFetchMoreInProgressProviderIdChanged();
    void isSearchInProgressChanged();
    void errorStringChanged();
    void searchTermChanged();
    void waitingForSearchTermEditEndChanged();
    void canEditSearchChanged();
    void searchStateChanged();
    void providersChanged();
    void providersReadyChanged();
    void activeFiltersChanged();
    void externalProvidersEnabledChanged();
    void showConnectedServicesActionChanged();
    void hasPartialFailureChanged();
    void viewModeChanged();
    void selectedRowChanged();
    void accessibilityStatusChanged();

public slots:
    void setSearchTerm(const QString &term);

private:
    enum class ProviderStatus {
        Idle,
        Loading,
        Blocked,
        Loaded,
        Failed,
    };

    struct UnifiedSearchProvider
    {
        QString id;
        QString appId;
        QString name;
        QString icon;
        QSet<QString> filters;
        int order = std::numeric_limits<int>::max();
        int discoveryIndex = -1;
        bool isExternalProvider = false;

        ProviderStatus status = ProviderStatus::Idle;
        QVector<UnifiedSearchResult> entries;
        QJsonValue cursor;
        bool hasMore = false;
        bool loadMoreFailed = false;
        bool paging = false;
        bool partialMatch = false;
    };

    void discoverProviders();
    void providerDiscoveryFinished(const QJsonDocument &json, int statusCode, quint64 generation, QObject *jobObject);
    void scheduleSearch();
    void startSearch();
    void startSearchForProvider(const QString &providerId, bool pagination = false);
    void providerSearchFinished(const QJsonDocument &json,
                                int statusCode,
                                const QString &providerId,
                                bool pagination,
                                quint64 generation,
                                QObject *jobObject);
    [[nodiscard]] QVector<UnifiedSearchResult> parseEntries(const QJsonArray &entries,
                                                            const UnifiedSearchProvider &provider,
                                                            int firstEntryIndex = 0) const;
    void promoteReadyProviders();
    void closeRevealWindow();
    void rebuildProjection();
    [[nodiscard]] UnifiedSearchResult pagingRowForProvider(const UnifiedSearchProvider &provider) const;
    [[nodiscard]] bool updateDetailProjectionAfterPagination(const UnifiedSearchProvider &provider, int previousEntryCount);
    [[nodiscard]] bool updateDetailPagingState(const UnifiedSearchProvider &provider);
    void appendProviderProjection(const UnifiedSearchProvider &provider,
                                  bool partial,
                                  const QSet<QString> &filteredResourceUrls,
                                  QVector<UnifiedSearchResult> &projection) const;
    void resetProviderRuntime();
    void abortSearchJobs();
    void abortProviderDiscovery();
    void trackSearchJob(QObject *jobObject);
    void untrackSearchJob(QObject *jobObject);
    void updateProgressSignals(bool wasInProgress);
    void updateAggregateState();
    void updateAccessibilityStatus();
    void restartForFilterChange();
    void setErrorString(const QString &error);
    void setProvidersReady(bool ready);
    void setWaitingForSearchTermEditEnd(bool waiting);
    void setSelectedRow(int row);
    void setAccessibilityStatus(const QString &status);
    [[nodiscard]] bool providerIsApplicable(const UnifiedSearchProvider &provider) const;
    [[nodiscard]] bool providerSupportsAllContentFilters(const UnifiedSearchProvider &provider) const;
    [[nodiscard]] QString providerIcon(const UnifiedSearchProvider &provider, bool darkMode) const;
    [[nodiscard]] QUrlQuery queryForProvider(const UnifiedSearchProvider &provider, bool pagination) const;
    [[nodiscard]] static QString stableKeyForResult(const QString &providerId,
                                                    const UnifiedSearchResult &result,
                                                    int entryIndex);
    [[nodiscard]] static QUrl openableResourceUrl(const QUrl &resourceUrl, const QUrl &accountUrl);

    QHash<QString, UnifiedSearchProvider> _providers;
    QVector<QString> _providerOrder;
    QVector<QString> _revealOrder;
    QVector<UnifiedSearchResult> _results;
    QVector<QPointer<QObject>> _activeSearchJobs;
    QPointer<QObject> _providerDiscoveryJob;

    QString _searchTerm;
    QString _errorString;
    QString _currentFetchMoreInProgressProviderId;
    QStringList _selectedProviderIds;
    QDateTime _since;
    QDateTime _until;
    QString _dateLabel;
    QString _personId;
    QString _personName;
    QString _personAvatarUrl;
    QString _detailProviderId;
    QString _selectedStableKey;
    QString _aggregateSelectedStableKey;
    QString _accessibilityStatus;

    bool _waitingForSearchTermEditEnd = false;
    bool _providersLoading = false;
    bool _providersReady = false;
    bool _pendingSearch = false;
    bool _revealWindowClosed = false;
    bool _externalProvidersEnabled = false;
    bool _hasPartialFailure = false;
    bool _lastKnownConnected = false;
    int _inFlightSearchRequests = 0;
    int _selectedRow = -1;
    quint64 _queryGeneration = 0;
    quint64 _providerGeneration = 0;
    ViewMode _viewMode = ViewMode::Aggregate;

    QTimer _debounceTimer;
    QTimer _revealTimer;
    AccountState *_accountState = nullptr;
};
}
