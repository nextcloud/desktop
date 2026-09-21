/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fixtureactivitylistmodel.h"
#include "accountstate.h"
namespace OCC::DocAssets
{
FixtureActivityListModel::FixtureActivityListModel(const AccountStatePtr &state, QObject *parent)
    : ActivityListModel(state.data(), parent, [state](const QString &name) {
        return name == state->account()->displayName() ? state : AccountStatePtr{};
    })
{
    auto warning = Activity{};
    warning._id = 1;
    warning._type = Activity::SyncFileItemType;
    warning._accName = state->account()->displayName();
    warning._subject = QStringLiteral("Project plan.pdf has a conflict");
    warning._message = QStringLiteral("Both versions were kept. Review the changes before continuing.");
    warning._syncFileItemStatus = SyncFileItem::Conflict;
    warning._dateTime = QDateTime::fromSecsSinceEpoch(1789639200);
    addErrorToActivityList(warning, ErrorType::SyncError);
    auto updated = Activity{};
    updated._id = 2;
    updated._type = Activity::ActivityType;
    updated._accName = warning._accName;
    updated._subject = QStringLiteral("Jamie Rivera updated Project budget.ods");
    updated._message = QStringLiteral("Documents / Project");
    updated._dateTime = warning._dateTime.addSecs(-300);
    addSyncFileItemToActivityList(updated);
    auto shared = updated;
    shared._id = 3;
    shared._subject = QStringLiteral("Alex Morgan shared Project notes.md");
    shared._dateTime = warning._dateTime.addSecs(-600);
    addSyncFileItemToActivityList(shared);
}
}
