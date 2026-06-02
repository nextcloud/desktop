/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "getavailablegovernancelabels.h"

namespace OCC
{

GetAvailableGovernanceLabels::GetAvailableGovernanceLabels(QObject *parent)
    : OCC::TypedGovernanceNetworkJob{parent}
{
}

} // namespace OCC
