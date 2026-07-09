/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "unifiedsearchresult.h"

#include <limits>

#include <QtCore>

namespace OCC {
class AccountState;

/**
 * @brief The UnifiedSearchResultsListModel
 * @ingroup gui
 * Simple list model to provide the list view with data for the Unified Search results.
 */

class UnifiedSearchResultsListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool isSearchInProgress READ isSearchInProgress NOTIFY isSearchInProgressChanged)
    Q_PROPERTY(QString currentFetchMoreInProgressProviderId READ currentFetchMoreInProgressProviderId NOTIFY
            currentFetchMoreInProgressProviderIdChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm NOTIFY searchTermChanged)
    Q_PROPERTY(bool waitingForSearchTermEditEnd READ waitingForSearchTermEditEnd NOTIFY waitingForSearchTermEditEndChanged)
    Q_PROPERTY(bool isFetchMoreInProgress READ isFetchMoreInProgress NOTIFY currentFetchMoreInProgressProviderIdChanged)
    Q_PROPERTY(bool hasSearchTerm READ hasSearchTerm NOTIFY searchTermChanged)
    Q_PROPERTY(bool hasSearchError READ hasSearchError NOTIFY errorStringChanged)
    Q_PROPERTY(bool canEditSearch READ canEditSearch NOTIFY canEditSearchChanged)
    Q_PROPERTY(SearchState searchState READ searchState NOTIFY searchStateChanged)

    struct UnifiedSearchProvider
    {
        QString _id;
        QString _name;
        qint32 _cursor = -1; // current pagination value
        qint32 _pageSize = -1; // how many max items per step of pagination
        bool _isPaginated = false;
        qint32 _order = std::numeric_limits<qint32>::max(); // sorting order (smaller number has bigger priority)
    };

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

    enum DataRole {
        ProviderNameRole = Qt::UserRole + 1,
        ProviderIdRole,
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
    };

    explicit UnifiedSearchResultsListModel(AccountState *accountState, QObject *parent = nullptr);

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] bool isSearchInProgress() const;

    [[nodiscard]] QString currentFetchMoreInProgressProviderId() const;
    [[nodiscard]] QString searchTerm() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] bool waitingForSearchTermEditEnd() const;

    [[nodiscard]] bool isFetchMoreInProgress() const;
    [[nodiscard]] bool hasSearchTerm() const;
    [[nodiscard]] bool hasSearchError() const;
    [[nodiscard]] bool canEditSearch() const;
    [[nodiscard]] SearchState searchState() const;

    Q_INVOKABLE void resultClicked(const QString &providerId, const QUrl &resourceUrl) const;
    Q_INVOKABLE void fetchMoreTriggerClicked(const QString &providerId);

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    void startSearch();
    void startSearchForProvider(const QString &providerId, qint32 cursor = -1);

    void parseResultsForProvider(const QJsonObject &data, const QString &providerId, bool fetchedMore = false);

    // append initial search results to the list
    void appendResults(QVector<UnifiedSearchResult> results, const UnifiedSearchProvider &provider);

    // append pagination results to existing results from the initial search
    void appendResultsToProvider(const QVector<UnifiedSearchResult> &results, const UnifiedSearchProvider &provider);

    void removeFetchMoreTrigger(const QString &providerId);

    void disconnectAndClearSearchJobs();

    void clearCurrentFetchMoreInProgressProviderId();

signals:
    void currentFetchMoreInProgressProviderIdChanged();
    void isSearchInProgressChanged();
    void errorStringChanged();
    void searchTermChanged();
    void waitingForSearchTermEditEndChanged();
    void canEditSearchChanged();
    void searchStateChanged();

public slots:
    void setSearchTerm(const QString &term);

private slots:
    void slotSearchTermEditingFinished();
    void slotFetchProvidersFinished(const QJsonDocument &json, int statusCode);
    void slotSearchForProviderFinished(const QJsonDocument &json, int statusCode);

private:
    static QUrl openableResourceUrl(const QUrl &resourceUrl, const QUrl &accountUrl);

    QMap<QString, UnifiedSearchProvider> _providers;
    QVector<UnifiedSearchResult> _results;

    QString _searchTerm;
    QString _errorString;
    bool _waitingForSearchTermEditEnd = false;

    QString _currentFetchMoreInProgressProviderId;

    QMap<QString, QMetaObject::Connection> _searchJobConnections;

    QTimer _unifiedSearchTextEditingFinishedTimer;

    AccountState *_accountState = nullptr;
};
}
