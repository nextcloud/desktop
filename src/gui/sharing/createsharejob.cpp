/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "createsharejob.h"

#include "share.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

CreateShareJob::CreateShareJob(AccountPtr account)
    : UnifiedSharingRequest{account, "/ocs/v2.php/apps/sharing/api/v1/share"_L1, "POST"_ba, {}, {201}}
{
    connect(this, &OcsJob::jobFinished, this, [this, account = std::move(account)](const QJsonDocument &json, int) {
        Q_EMIT shareCreated(Share::fromJson(json, account));
    });
}

}
