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

#ifndef UNIFIEDSEARCHRESULTSLISTMODEL_H
#define UNIFIEDSEARCHRESULTSLISTMODEL_H

#include "UnifiedSearchResult.h"

#include <limits>

#include <QtCore>

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcUnifiedSearch)

/**
 * @brief The UnifiedSearchResultsListModel
 * @ingroup gui
 * Simple list model to provide the list view with data for the Unified Search results.
 */

class UnifiedSearchResultsListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool isSearchInProgress READ isSearchInProgress NOTIFY isSearchInProgressChanged)
    Q_PROPERTY(QString currentFetchMoreInProgressCategoryId MEMBER _currentFetchMoreInProgressCategoryId NOTIFY
            currentFetchMoreInProgressCategoryIdChanged)
    Q_PROPERTY(QString errorString MEMBER _errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm NOTIFY searchTermChanged)

    class UnifiedSearchProvider
    {
    public:
        QString _id;
        QString _name;
        qint32 _cursor = -1; // current pagination value
        qint32 _pageSize = -1; // how many max items per step of pagination
        bool _isPaginated = false;
        qint32 _order = std::numeric_limits<quint32>::max(); // sorting order (smaller number has bigger priority)
    };

public:
    enum DataRole {
        ProviderNameRole = Qt::UserRole + 1,
        ProviderIdRole,
        ImagePlaceholderRole,
        IconsRole,
        TitleRole,
        SublineRole,
        ResourceUrlRole,
        RoundedRole,
        TypeRole,
        TypeAsStringRole,
    };

    explicit UnifiedSearchResultsListModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    void setSearchTerm(const QString &term);
    QString searchTerm() const;

    bool isSearchInProgress() const;

    Q_INVOKABLE void resultClicked(const QString &providerId, const QString &resourceUrl);
    Q_INVOKABLE void fetchMoreTriggerClicked(const QString &providerId);

public:
signals:
    void currentFetchMoreInProgressCategoryIdChanged();
    void isSearchInProgressChanged();
    void errorStringChanged();
    void searchTermChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

private slots:
    void slotSearchTermEditingFinished();
    void slotSearchForProviderFinished(const QJsonDocument &json, int statusCode);

private:
    void startSearch();
    void startSearchForProvider(const QString &providerId, qint32 cursor = -1);

    // append initial search results to the list
    void appendResults(QList<UnifiedSearchResult> results, const UnifiedSearchProvider &provider);

    // append pagination results to existing results from the initial search
    void appendResultsToProvider(const QList<UnifiedSearchResult> &results, const UnifiedSearchProvider &provider);

private:
    QMap<QString, UnifiedSearchProvider> _providers;
    QList<UnifiedSearchResult> _results;

    QString _searchTerm;
    QString _errorString;

    QString _currentFetchMoreInProgressCategoryId;

    QMap<QString, QMetaObject::Connection> _searchJobConnections;

    QTimer _unifiedSearchTextEditingFinishedTimer;
};
}

#endif // UNIFIEDSEARCHRESULTSLISTMODEL_H
