/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setpermissionjob.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

SetPermissionJob::SetPermissionJob(AccountPtr account, const QString &shareId, const QString &permissionClass, bool enabled)
    : UpdateShareJob{std::move(account),
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/permission"_L1.arg(shareId),
                     "PUT"_ba,
                     {.parameters = {}, .passStatusCodes = {}, .body = QJsonObject{{"class"_L1, permissionClass}, {"enabled"_L1, enabled}}}}
{
}

}
