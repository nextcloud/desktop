/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setrecipientsecretjob.h"

#include "share.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
QJsonObject setRecipientSecretBody(const QString &recipientTypeClass,
                                   const QString &recipientValue,
                                   const QString &secret,
                                   const std::optional<QString> &instance)
{
    auto body = QJsonObject{
        {"class"_L1, recipientTypeClass},
        {"value"_L1, recipientValue},
        {"secret"_L1, secret},
    };
    if (instance) {
        body.insert("instance"_L1, *instance);
    }
    return body;
}
}

SetRecipientSecretJob::SetRecipientSecretJob(AccountPtr account,
                                             Share &share,
                                             const QString &recipientTypeClass,
                                             const QString &recipientValue,
                                             const QString &secret,
                                             const std::optional<QString> &instance)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/recipient/secret"_L1.arg(share.id()),
                     "PUT"_ba,
                     {.parameters = {}, .passStatusCodes = {}, .body = setRecipientSecretBody(recipientTypeClass, recipientValue, secret, instance)}}
{
}

}
