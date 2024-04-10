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
#include "networkjobs.h"

#include "libsync/account.h"

#include "libsync/graphapi/spacesmanager.h"

#include "resources/resources.h"

#include <QIcon>
#include <QPixmap>
#include <QRandomGenerator>

namespace {
constexpr QSize ImageSizeC(128, 128);
constexpr QSize ImageMarginC(ImageSizeC * 0.1);
}

using namespace OCC::Spaces;

SpacesModel::SpacesModel(QObject *parent)
    : QAbstractTableModel(parent)
{
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
    return 1;
}

QVariant SpacesModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));

    const auto *space = _spacesList.at(index.row());
    switch (static_cast<Roles>(role)) {
    case Roles::Sync:
        // TODO: return true if we alreaddy sync the space
        return false;
    case Roles::Name:
        return space->displayName();
    case Roles::Subtitle:
        return space->drive().getDescription();
    case Roles::WebUrl:
        return space->drive().getWebUrl();
    case Roles::WebDavUrl:
        return space->webdavUrl();
    case Roles::Image:
        return QStringLiteral("image://space/%1/%2").arg(QString::number(QRandomGenerator::global()->generate()), space->drive().getRoot().getId());
    case Roles::Priority:
        // everything will be sorted in descending order, multiply the priority by 100 and prefer A over Z by appling a negative factor
        return space->priority() * 100 - (space->displayName().isEmpty() ? 0 : static_cast<int64_t>(space->displayName().at(0).toLower().unicode()));
    case Roles::Space:
        return QVariant::fromValue(space);
    case Roles::Enabled:
        return !space->disabled();
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
            Q_EMIT dataChanged(index, index);
        }
    });
}
QHash<int, QByteArray> SpacesModel::roleNames() const
{
    return {
        {static_cast<int>(Roles::Name), "name"},
        {static_cast<int>(Roles::Subtitle), "subtitle"},
        {static_cast<int>(Roles::Image), "imageUrl"},
        {static_cast<int>(Roles::Space), "space"},
    };
}
