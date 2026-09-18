/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "removerecipientjob.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
QList<QPair<QString, QString>> removeRecipientParameters(const QString &recipientTypeClass,
                                                         const QString &recipientValue,
                                                         const std::optional<QString> &instance)
{
    auto parameters = QList<QPair<QString, QString>>{{"class"_L1, recipientTypeClass}, {"value"_L1, recipientValue}};
    if (instance) {
        parameters.emplaceBack("instance"_L1, *instance);
    }
    return parameters;
}
}

RemoveRecipientJob::RemoveRecipientJob(AccountPtr account,
                                       const QString &shareId,
                                       const QString &recipientTypeClass,
                                       const QString &recipientValue,
                                       const std::optional<QString> &instance)
    : UpdateShareJob{std::move(account),
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/recipient"_L1.arg(shareId),
                     "DELETE"_ba,
                     {.parameters = removeRecipientParameters(recipientTypeClass, recipientValue, instance), .passStatusCodes = {}, .body = {}}}
{
}

}
