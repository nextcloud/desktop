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
#include "models.h"

#include <QActionGroup>
#include <QApplication>
#include <QItemSelectionRange>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTextStream>
#include <QTimer>

#include <functional>

void OCC::Models::SignalledQSortFilterProxyModel::setFilterFixedStringSignalled(const QString &pattern)
{
    setFilterFixedString(pattern);
    emit filterChanged();
}

QString OCC::Models::formatSelection(const QModelIndexList &items, int dataRole)
{
    if (items.isEmpty()) {
        return {};
    }
    const auto columns = items.first().model()->columnCount();
    QString out;
    QTextStream stream(&out);

    QString begin;
    QString end;

    const auto iterate = [columns](const std::function<void(int)> &f) {
        if (qApp->layoutDirection() != Qt::RightToLeft) {
            for (int c = 0; c < columns; ++c) {
                f(c);
            }
        } else {
            for (int c = columns - 1; c >= 0; --c) {
                f(c);
            }
        }
    };
    if (qApp->layoutDirection() == Qt::RightToLeft) {
        stream << Qt::right;
        begin = QLatin1Char(',');
    } else {
        stream << Qt::left;
        end = QLatin1Char(',');
    }

    iterate([&](int c) {
        const auto width = items.first().model()->headerData(c, Qt::Horizontal, StringFormatWidthRole).toInt();
        Q_ASSERT(width);
        stream << begin
               << qSetFieldWidth(width)
               << items.first().model()->headerData(c, Qt::Horizontal).toString()
               << qSetFieldWidth(0)
               << end;
    });
    stream << Qt::endl;
    for (const auto &index : items) {
        iterate([&](int c) {
            const auto &child = index.siblingAtColumn(c);
            stream << begin
                   << qSetFieldWidth(child.model()->headerData(c, Qt::Horizontal, StringFormatWidthRole).toInt())
                   << child.data(dataRole).toString()
                   << qSetFieldWidth(0)
                   << end;
        });
        stream << Qt::endl;
    }
    return out;
}

std::function<void()> OCC::Models::addFilterMenuItems(QMenu *menu, const QStringList &candidates, SignalledQSortFilterProxyModel *model, int column, const QString &columnName, int role)
{
    menu->addAction(QApplication::translate("OCC::Models", "%1 Filter:").arg(columnName))->setEnabled(false);

    auto filterGroup = new QActionGroup(menu);
    filterGroup->setExclusive(true);

    const auto currentFilter = model->filterRegularExpression().pattern();
    auto addAction = [=](const QString &s, const QString &filter) {
        auto action = menu->addAction(s, menu, [=]() {
            model->setFilterRole(role);
            model->setFilterKeyColumn(column);
            model->setFilterFixedStringSignalled(filter);
        });
        action->setCheckable(true);
        action->setChecked(currentFilter == QRegularExpression::escape(filter));
        filterGroup->addAction(action);
        return action;
    };


    auto noFilter = addAction(QApplication::translate("OCC::Models", "All"), QString());

    for (const auto &c : candidates) {
        addAction(c, c);
    }

    auto resetFunction = [noFilter]() {
        noFilter->setChecked(true);
        noFilter->trigger();
    };
    return resetFunction;
}

void OCC::Models::WeightedQSortFilterProxyModel::setWeightedColumn(int i, Qt::SortOrder sortOrder)
{
    _weightedColumn = i;
    _weightedSortOrder = sortOrder;
}

bool OCC::Models::WeightedQSortFilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    Q_ASSERT(_weightedColumn < source_left.model()->columnCount());
    const uint32_t w1 = source_left.siblingAtColumn(_weightedColumn).data().toInt();
    const uint32_t w2 = source_right.siblingAtColumn(_weightedColumn).data().toInt();
    if (w1 != w2) {
        // always disply on top
        return _weightedSortOrder == Qt::SortOrder::DescendingOrder ? w1 < w2 : w1 > w2;
    }
    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

bool OCC::Models::FilteringProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // column doesn't matter, since we do not filter based on a specific column but on a specific item role
    const auto index = sourceModel()->index(sourceRow, filterKeyColumn(), sourceParent);
    return sourceModel()->data(index, static_cast<int>(filterRole())).toBool();
}
