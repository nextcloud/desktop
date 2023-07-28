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
#include "spacesmodel.h"

#include "common/utility.h"
// TODO: move models out from core
#include "gui/models/models.h"
#include "networkjobs.h"

#include "libsync/account.h"

#include "libsync/graphapi/spacesmanager.h"

#include "resources/resources.h"

#include <QIcon>
#include <QPixmap>

namespace {
constexpr QSize ImageSizeC(128, 128);
constexpr QSize ImageMarginC(ImageSizeC * 0.1);
}

using namespace OCC::Spaces;

SpacesModel::SpacesModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

QVariant SpacesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        const auto actionRole = static_cast<Columns>(section);
        switch (role) {
        case Qt::DisplayRole:
            switch (actionRole) {
            case Columns::Sync:
                return tr("Sync");
            case Columns::Name:
                return tr("Name");
            case Columns::Subtitle:
                return tr("Subtitle");
            case Columns::WebUrl:
                return tr("Web URL");
            case Columns::WebDavUrl:
                return tr("Web Dav URL");
            case Columns::Image:
                return tr("Image");
            case Columns::Priority:
                return tr("Priority");
            case Columns::Enabled:
                return tr("Enabled");
            case Columns::SpaceId:
                return tr("SpaceId");
            case Columns::ColumnCount:
                Q_UNREACHABLE();
            }
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

int SpacesModel::rowCount(const QModelIndex &parent) const
{
    Q_ASSERT(checkIndex(parent));
    if (parent.isValid())
        return 0;
    return static_cast<int>(_spacesList.size());
}

int SpacesModel::columnCount(const QModelIndex &parent) const
{
    Q_ASSERT(checkIndex(parent));
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(Columns::ColumnCount);
}

QVariant SpacesModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));

    const auto column = static_cast<Columns>(index.column());
    const auto *space = _spacesList.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case Columns::Sync:
            // TODO: return true if we alreaddy sync the space
            return false;
        case Columns::Name:
            return space->displayName();
        case Columns::Subtitle:
            return space->drive().getDescription();
        case Columns::WebUrl:
            return space->drive().getWebUrl();
        case Columns::WebDavUrl:
            return space->drive().getRoot().getWebDavUrl();
        case Columns::Image:
            return {};
        case Columns::Priority:
            return space->priority();
        case Columns::SpaceId:
            return space->drive().getRoot().getId();
        case Columns::Enabled:
            return !space->disabled();
        case Columns::ColumnCount:
            Q_UNREACHABLE();
        }
        break;
    case Qt::DecorationRole:
        switch (column) {
        case Columns::Image: {
            return space->image();
        }
        default:
            return {};
        }
    case Qt::SizeHintRole: {
        switch (column) {
        case Columns::Image:
            return ImageSizeC + ImageMarginC;
        default:
            return {};
        }
    }
    case Models::UnderlyingDataRole:
        switch (column) {
        case Columns::Image: {
            return space->imageUrl();
        }
        default:
            return data(index, Qt::DisplayRole);
        }
    case Models::FilterRole:
        switch (column) {
        case Columns::Enabled:
            return !space->disabled();
        default:
            Q_UNREACHABLE();
        }
    }
    return {};
}

void SpacesModel::setSpacesManager(GraphApi::SpacesManager *spacesManager)
{
    Q_ASSERT(!_spacesManager);
    _spacesManager = spacesManager;
    beginResetModel();
    _spacesList = _spacesManager->spaces();
    endResetModel();
    connect(_spacesManager, &GraphApi::SpacesManager::updated, this, [this] {
        const auto newSpaces = _spacesManager->spaces();
        if (_spacesList != newSpaces) {
            beginResetModel();
            _spacesList = newSpaces;
            endResetModel();
        }
    });

    connect(_spacesManager, &GraphApi::SpacesManager::spaceChanged, this, [this](GraphApi::Space *space) {
        const auto row = _spacesList.indexOf(space);
        if (row != -1) {
            const auto index = createIndex(row, 0);
            Q_EMIT dataChanged(index, index.siblingAtColumn(static_cast<int>(SpacesModel::Columns::ColumnCount) - 1));
        }
    });
}
