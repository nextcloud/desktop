/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setsharestatejob.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
QString stateName(Share::ShareState state)
{
    switch (state) {
    case Share::ShareState::Active:
        return "active"_L1;
    case Share::ShareState::Deleted:
        return "deleted"_L1;
    case Share::ShareState::Draft:
        return "draft"_L1;
    case Share::ShareState::Unknown:
        break;
    }
    Q_UNREACHABLE_RETURN({});
}
}

SetShareStateJob::SetShareStateJob(AccountPtr account, Share &share, Share::ShareState state)
    : UpdateShareJob{std::move(account),
                     share,
                     "/ocs/v2.php/apps/sharing/api/v1/share/%1/state"_L1.arg(share.id()),
                     "PUT"_ba,
                     {.parameters = {}, .passStatusCodes = {}, .body = QJsonObject{{"state"_L1, stateName(state)}}}}
{
}

}
