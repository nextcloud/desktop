/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setpermissionjob.h"

#include "share.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

SetPermissionJob::SetPermissionJob(AccountPtr account, Share &share, const QString &permissionClass, bool enabled)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/permission"_L1.arg(share.id()),
                     "PUT"_ba,
                     {{"class"_L1, permissionClass}, {"enabled"_L1, enabled ? "true"_L1 : "false"_L1}}}
{
}

}
