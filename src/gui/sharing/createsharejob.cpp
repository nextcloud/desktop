/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "createsharejob.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

CreateShareJob::CreateShareJob(AccountPtr account)
    : UnifiedSharingRequest{account,
                            "/ocs/v2.php/apps/sharing/api/v1/share"_L1,
                            "POST"_ba,
                            {.parameters = {}, .passStatusCodes = QList<int>{201}, .body = {}}}
{
    connect(this, &OcsJob::jobFinished, this, [this](const QJsonDocument &json, int) {
        Q_EMIT shareCreated(json);
    });
}

}
