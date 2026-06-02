/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "getgovernancelabels.h"

namespace OCC
{

GetGovernanceLabels::GetGovernanceLabels(QObject *parent)
    : OCC::GovernanceNetworkJob{parent}
{
}

} // namespace OCC
