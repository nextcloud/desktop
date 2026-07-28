/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "removerecipientjob.h"

#include "share.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

RemoveRecipientJob::RemoveRecipientJob(AccountPtr account, Share &share, const QString &recipientType, const QString &recipientValue)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/recipient"_L1.arg(share.id()),
                     "DELETE"_ba,
                     {{"class"_L1, recipientType}, {"value"_L1, recipientValue}}}
{
}

}
