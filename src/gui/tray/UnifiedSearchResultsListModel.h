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

#include "UnifiedSearchResultCategory.h"

#include <QtCore>

namespace OCC {

class AccountState;

/**
 * @brief The UnifiedSearchResultsListModel
 * @ingroup gui
 *
 * Simple list model to provide the list view with data for the Unified Search results.
 */

class UnifiedSearchResultsListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool isSearchInProgress READ isSearchInProgress NOTIFY isSearchInProgressChanged)
    Q_PROPERTY(QString currentFetchMoreInProgressCategoryId MEMBER _currentFetchMoreInProgressCategoryId NOTIFY currentFetchMoreInProgressCategoryIdChanged)
    Q_PROPERTY(QString errorString MEMBER _errorString)

    class UnifiedSearchProvider
    {
    public:
        QString _id;
        QString _name;
        qint32 _order = INT32_MAX;
    };

public:
    enum DataRole {
        CategoryNameRole = Qt::UserRole + 1,
        CategoryIdRole,
        ImagePlaceholderRole,
        ImagesRole,
        IconRole,
        TitleRole,
        SublineRole,
        ResourceUrlRole,
        RoundedRole,
        ThumbnailUrlRole,
        TypeRole,
        TypeAsStringRole,
    };

    explicit UnifiedSearchResultsListModel(AccountState *accountState, QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    void setSearchTerm(const QString &term);
    QString searchTerm() const;

    bool isSearchInProgress() const;

    Q_INVOKABLE void resultClicked(int resultIndex);

public: signals:
    void currentFetchMoreInProgressCategoryIdChanged();
    void isSearchInProgressChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

private slots:
    void slotSearchTermEditingFinished();

    void slotSearchForProviderFinished(const QJsonDocument &json, int statusCode);

private:
    void startSearch();
    void startSearchForProvider(const UnifiedSearchProvider &provider, qint32 cursor = -1);

    void combineResults();

    void appendResultsToProvider(const UnifiedSearchProvider &provider, QList<UnifiedSearchResult> results);

private:
    QTimer _unifiedSearchTextEditingFinishedTimer;
    QString _searchTerm;
    QMap<QString, UnifiedSearchProvider> _providers;
    AccountState *_accountState;
    QMap<QString, UnifiedSearchResultCategory> _resultsByCategory;
    QList<UnifiedSearchResult> _resultsCombined;

    QString _errorString;

    QString _currentFetchMoreInProgressCategoryId;

    QMap<QString, QMetaObject::Connection> _searchJobConnections;
};
}

#endif // UNIFIEDSEARCHRESULTSLISTMODEL_H
