/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "destroysharejob.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

DestroyShareJob::DestroyShareJob(AccountPtr account, const QString &shareId)
    : UnifiedSharingRequest{std::move(account),
                            "/ocs/v2.php/apps/sharing/api/v1/share/%1"_L1.arg(shareId),
                            "DELETE"_ba,
                            {.passStatusCodes = QList<int>{204}}}
{
}

}
