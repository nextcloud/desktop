/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "addrecipientjob.h"

#include "share.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
QJsonObject addRecipientBody(const QString &recipientTypeClass,
                             const QString &recipientValue,
                             const std::optional<QString> &instance)
{
    auto body = QJsonObject{{"class"_L1, recipientTypeClass}, {"value"_L1, recipientValue}};
    if (instance) {
        body.insert("instance"_L1, *instance);
    }
    return body;
}
}

AddRecipientJob::AddRecipientJob(AccountPtr account,
                                 Share &share,
                                 const QString &recipientTypeClass,
                                 const QString &recipientValue,
                                 const std::optional<QString> &instance)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/recipient"_L1.arg(share.id()),
                     "POST"_ba,
                     {.parameters = {}, .passStatusCodes = {}, .body = addRecipientBody(recipientTypeClass, recipientValue, instance)}}
{
}

}
