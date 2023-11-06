/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "activitylistmodel.h"
#include <QVector>

#include "sortedactivitylistmodel.h"

namespace
{
    struct ActivityLinksSearchResult {
        bool hasPOST = false;
        bool hasREPLY = false;
        bool hasWEB = false;
        bool hasDELETE = false;
    };

    ActivityLinksSearchResult searchForVerbsInLinks(const QVector<OCC::ActivityLink> &links)
    {
        ActivityLinksSearchResult result;
        for (const auto &link : links) {
            if (link._verb == QByteArrayLiteral("POST")) {
                result.hasPOST = true;
            } else if (link._verb == QByteArrayLiteral("REPLY")) {
                result.hasREPLY = true;
            } else if (link._verb == QByteArrayLiteral("WEB")) {
                result.hasWEB = true;
            } else if (link._verb == QByteArrayLiteral("DELETE")) {
                result.hasDELETE = true;
            }
        }
        return result;
    }
}

namespace OCC {

SortedActivityListModel::SortedActivityListModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    sort(0, Qt::AscendingOrder);
}

bool SortedActivityListModel::lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const
{
    if (!sourceLeft.isValid() || !sourceRight.isValid()) {
        return false;
    }

    const auto leftActivity = sourceLeft.data(ActivityListModel::ActivityRole).value<Activity>();
    const auto rightActivity = sourceRight.data(ActivityListModel::ActivityRole).value<Activity>();

    // First compare by general activity type
    const auto leftType = leftActivity._type;

    if (leftType == Activity::DummyFetchingActivityType) {
        // The fetching activities dummy activity always goes at the top
        return true;
    } else if (leftType == Activity::DummyMoreActivitiesAvailableType) {
        // Likewise the dummy "more activities available" activity always goes at the bottom
        return false;
    }

    const auto leftActivityVerbsSearchResult = searchForVerbsInLinks(leftActivity._links);
    const auto rightActivityVerbsSearchResult = searchForVerbsInLinks(rightActivity._links);

    if (leftActivityVerbsSearchResult.hasPOST != rightActivityVerbsSearchResult.hasPOST) {
        return leftActivityVerbsSearchResult.hasPOST;
    }

    if (leftActivityVerbsSearchResult.hasREPLY != rightActivityVerbsSearchResult.hasREPLY) {
        return leftActivityVerbsSearchResult.hasREPLY;
    }

    if (leftActivityVerbsSearchResult.hasWEB != rightActivityVerbsSearchResult.hasWEB) {
        return leftActivityVerbsSearchResult.hasWEB;
    }

    if (leftActivityVerbsSearchResult.hasDELETE != rightActivityVerbsSearchResult.hasDELETE) {
        return leftActivityVerbsSearchResult.hasDELETE;
    }

    const auto leftActivityIsSecurityAction = leftActivity._fileAction == QStringLiteral("security");
    const auto rightActivityIsSecurityAction = rightActivity._fileAction == QStringLiteral("security");
    if (leftActivityIsSecurityAction != rightActivityIsSecurityAction) {
        return leftActivityIsSecurityAction;
    }

    // Let's now check for errors as we want those near the top too
    // Sync result errors go first
    const auto leftSyncResultStatus = leftActivity._syncResultStatus;
    const auto rightSyncResultStatus = rightActivity._syncResultStatus;

    const auto leftIsSyncResultError = leftSyncResultStatus == SyncResult::Error ||
                                       leftSyncResultStatus == SyncResult::SetupError ||
                                       leftSyncResultStatus == SyncResult::Problem;

    const auto rightIsSyncResultError = rightSyncResultStatus == SyncResult::Error ||
                                        rightSyncResultStatus == SyncResult::SetupError ||
                                        rightSyncResultStatus == SyncResult::Problem;

    if (leftIsSyncResultError != rightIsSyncResultError) {
        return leftIsSyncResultError;
    } // If they are both errors then we will order the errors according to enum order later

    // Then sync file item status errors
    const auto leftSyncFileItemStatus = leftActivity._syncFileItemStatus;
    const auto rightSyncFileItemStatus = rightActivity._syncFileItemStatus;
    const bool leftIsErrorFileItemStatus = leftSyncFileItemStatus != SyncFileItem::NoStatus &&
                                           leftSyncFileItemStatus != SyncFileItem::Success;

    const bool rightIsErrorFileItemStatus = rightSyncFileItemStatus != SyncFileItem::NoStatus &&
                                            rightSyncFileItemStatus != SyncFileItem::Success;

    if (leftIsErrorFileItemStatus != rightIsErrorFileItemStatus) {
        return leftIsErrorFileItemStatus;
    }

    // Let's go back to more broadly comparing by type
    if (const auto rightType = rightActivity._type; leftType != rightType) {
        return leftType < rightType;
    }

    if (leftSyncResultStatus != rightSyncResultStatus) {
        return leftSyncResultStatus < rightSyncResultStatus;
    }

    if (leftSyncFileItemStatus != rightSyncFileItemStatus) {
        return leftSyncFileItemStatus < rightSyncFileItemStatus;
    }

    // Finally sort by time, latest first
    const auto leftDateTime = leftActivity._dateTime;
    const auto rightDateTime = rightActivity._dateTime;

    return leftDateTime > rightDateTime;
}

}
