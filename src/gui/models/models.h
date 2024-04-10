/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
#pragma once

#include <QModelIndexList>
#include <QSortFilterProxyModel>
#include <QString>
#include <QtGlobal>

class QSortFilterProxyModel;
class QMenu;

namespace OCC {

namespace Models {
    Q_NAMESPACE

    enum DataRoles {
        UnderlyingDataRole = Qt::UserRole + 100,
        StringFormatWidthRole, // The width for a cvs formatted column

        // data() should return boolean values for this role to work in conjunction with FilteringProxyModel
        FilterRole,
    };
    Q_ENUM_NS(DataRoles)

    class SignalledQSortFilterProxyModel : public QSortFilterProxyModel
    {
        Q_OBJECT

    public:
        using QSortFilterProxyModel::QSortFilterProxyModel;

        void setFilterFixedStringSignalled(const QString &pattern);

    Q_SIGNALS:
        void filterChanged();
    };


    /**
     * Returns a cvs representation of a table
     */
    QString formatSelection(const QModelIndexList &items, int dataRole = Qt::DisplayRole);

    std::function<void()> addFilterMenuItems(QMenu *menu, const QStringList &candidates, SignalledQSortFilterProxyModel *model, int column, const QString &columnName, int role);

    /**
     * Returns a vector with indices
     * This is handy to iterate over the columns
     */
    template <typename T>
    auto range(T start, T end)
    {
        std::vector<T> out;
        out.reserve(end - start);
        for (auto i = start; i < end; ++i) {
            out.push_back(i);
        }
        return out;
    }

    template <typename T>
    auto range(T end)
    {
        return range<T>(0, end);
    }

    class FilteringProxyModel : public QSortFilterProxyModel
    {
        using QSortFilterProxyModel::QSortFilterProxyModel;

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    };
} // OCC::Models namespace
} // OCC namespace
