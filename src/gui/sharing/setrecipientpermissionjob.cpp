/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setrecipientpermissionjob.h"

#include "unifiedshare.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
QJsonObject recipientPermissionBody(const QString &recipientType,
                                    const QString &recipientValue,
                                    const std::optional<QString> &recipientInstance,
                                    const QString &permissionClass,
                                    bool enabled)
{
    auto body = QJsonObject{{"recipientClass"_L1, recipientType},
                            {"recipientValue"_L1, recipientValue},
                            {"permissionClass"_L1, permissionClass},
                            {"enabled"_L1, enabled}};
    if (recipientInstance) {
        body.insert("recipientInstance"_L1, *recipientInstance);
    }
    return body;
}
}

SetRecipientPermissionJob::SetRecipientPermissionJob(AccountPtr account,
                                                     Share &share,
                                                     const QString &recipientType,
                                                     const QString &recipientValue,
                                                     const std::optional<QString> &recipientInstance,
                                                     const QString &permissionClass,
                                                     bool enabled)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/recipient/permission"_L1.arg(share.id()),
                     "PUT"_ba,
                     {.parameters = {},
                      .passStatusCodes = {},
                      .body = recipientPermissionBody(recipientType, recipientValue, recipientInstance, permissionClass, enabled)}}
{
}

}
