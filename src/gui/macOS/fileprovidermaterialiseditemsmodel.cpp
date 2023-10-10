/*
 * Copyright 2023 (c) Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "fileprovidermaterialiseditemsmodel.h"

namespace OCC {

namespace Mac {

FileProviderMaterialisedItemsModel::FileProviderMaterialisedItemsModel(QObject * const parent)
    : QAbstractListModel(parent)
{
}

int FileProviderMaterialisedItemsModel::rowCount(const QModelIndex &parent) const
{
    return 0;
}

QVariant FileProviderMaterialisedItemsModel::data(const QModelIndex &index, int role) const
{
    return {};
}

} // namespace Mac

} // namespace OCC
