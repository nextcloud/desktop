/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "activity/activitylistmodel.h"
namespace OCC::DocAssets
{
class FixtureActivityListModel : public ActivityListModel
{
public:
    explicit FixtureActivityListModel(const AccountStatePtr &state, QObject *parent);
    bool canFetchMore(const QModelIndex &) const override
    {
        return false;
    }
};
}
