/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setpermissionpresetjob.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

SetPermissionPresetJob::SetPermissionPresetJob(AccountPtr account, const QString &shareId, const QString &permissionPreset)
    : UpdateShareJob{std::move(account),
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/permission/preset"_L1.arg(shareId),
                     "PUT"_ba,
                     {.parameters = {}, .passStatusCodes = {}, .body = QJsonObject{{"permissionPresetClass"_L1, permissionPreset}}}}
{
}

}
