/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "sortedsharemodel.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcSortedShareModel, "com.nextcloud.sortedsharemodel")

SortedShareModel::SortedShareModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void SortedShareModel::sortModel()
{
    sort(0);
}

ShareModel *SortedShareModel::shareModel() const
{
    return qobject_cast<ShareModel*>(sourceModel());
}

void SortedShareModel::setShareModel(ShareModel *shareModel)
{
    const auto currentSetModel = sourceModel();

    if(currentSetModel) {
        disconnect(currentSetModel, &ShareModel::rowsInserted, this, &SortedShareModel::sortModel);
        disconnect(currentSetModel, &ShareModel::rowsMoved, this, &SortedShareModel::sortModel);
        disconnect(currentSetModel, &ShareModel::rowsRemoved, this, &SortedShareModel::sortModel);
        disconnect(currentSetModel, &ShareModel::dataChanged, this, &SortedShareModel::sortModel);
        disconnect(currentSetModel, &ShareModel::modelReset, this, &SortedShareModel::sortModel);
    }

    // Re-sort model when any changes take place
    connect(shareModel, &ShareModel::rowsInserted, this, &SortedShareModel::sortModel);
    connect(shareModel, &ShareModel::rowsMoved, this, &SortedShareModel::sortModel);
    connect(shareModel, &ShareModel::rowsRemoved, this, &SortedShareModel::sortModel);
    connect(shareModel, &ShareModel::dataChanged, this, &SortedShareModel::sortModel);
    connect(shareModel, &ShareModel::modelReset, this, &SortedShareModel::sortModel);

    setSourceModel(shareModel);
    sortModel();
    Q_EMIT shareModelChanged();
}

bool SortedShareModel::lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const
{
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        return false;
    }

    const auto leftShare = sourceLeft.data(ShareModel::ShareRole).value<SharePtr>();
    const auto rightShare = sourceRight.data(ShareModel::ShareRole).value<SharePtr>();

    if (leftShare.isNull() || rightShare.isNull()) {
        return false;
    }

    const auto leftShareType = leftShare->getShareType();

    // Placeholder link shares always go at top
    if (leftShareType == Share::TypePlaceholderLink) {
        return true;
    } else if (leftShareType == Share::TypeInternalLink) {
        // Internal links always at bottom
        return false;
    }

    const auto rightShareType = rightShare->getShareType();

    // Placeholder link shares always go at top
    if (rightShareType == Share::TypePlaceholderLink) {
        return false;
    } else if (rightShareType == Share::TypeInternalLink) {
        // Internal links always at bottom
        return true;
    }

    // We want to place link shares at the top
    if (leftShareType == Share::TypeLink && rightShareType != Share::TypeLink) {
        return true;
    } else if (rightShareType == Share::TypeLink && leftShareType != Share::TypeLink) {
        return false;
    } else if (leftShareType != rightShareType) {
        return leftShareType < rightShareType;
    }

    if (leftShareType == Share::TypeLink) {
        const auto leftLinkShare = leftShare.objectCast<LinkShare>();
        const auto rightLinkShare = rightShare.objectCast<LinkShare>();

        if(leftLinkShare.isNull() || rightLinkShare.isNull()) {
            qCWarning(lcSortedShareModel) << "One of compared shares is a null pointer after conversion despite having same share type. Left link share is null:" << leftLinkShare.isNull()
                                          << "Right link share is null: " << rightLinkShare.isNull();
            return false;
        }

        return leftLinkShare->getLabel() < rightLinkShare->getLabel();

    } else if (leftShare->getShareWith()) {
        if(rightShare->getShareWith().isNull()) {
            return true;
        }

        return leftShare->getShareWith()->format() < rightShare->getShareWith()->format();
    }

    return false;
}

} // namespace OCC
