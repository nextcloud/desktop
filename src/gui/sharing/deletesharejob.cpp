/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "deletesharejob.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

DeleteShareJob::DeleteShareJob(AccountPtr account, const QString &shareId)
    : UnifiedSharingRequest{std::move(account),
                            "/ocs/v2.php/apps/sharing/api/v1/share/%1"_L1.arg(shareId),
                            "DELETE"_ba,
                            {.parameters = {}, .passStatusCodes = QList<int>{204}, .body = {}}}
{
}

}
