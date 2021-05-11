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
#include <QString>
#include <QtGlobal>

class QSortFilterProxyModel;

namespace OCC {

namespace Models {
    enum DataRoles {
        UnderlyingDataRole = Qt::UserRole + 100,
        StringFormatWidthRole // The width for a cvs formated column
    };

    /**
     * Returns a cvs representation of a table
     */
    QString formatSelection(const QModelIndexList &items, int dataRole = Qt::DisplayRole);


    void displayFilterDialog(const QStringList &candidates, QSortFilterProxyModel *model, int column, int role, QWidget *parent = nullptr);


};
}
