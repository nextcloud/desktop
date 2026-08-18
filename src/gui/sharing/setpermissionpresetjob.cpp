/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setpermissionpresetjob.h"

#include "share.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

SetPermissionPresetJob::SetPermissionPresetJob(AccountPtr account, Share &share, const QString &permissionPreset)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/permission/preset"_L1.arg(share.id()),
                     "PUT"_ba,
                     {.parameters = {}, .passStatusCodes = {}, .body = QJsonObject{{"permissionPresetClass"_L1, permissionPreset}}}}
{
}

}
