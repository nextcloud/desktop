/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typedgovernancenetworkjob.h"

namespace OCC
{

TypedGovernanceNetworkJob::TypedGovernanceNetworkJob(QObject *parent)
    : OCC::GovernanceNetworkJob{parent}
{
}

GovernanceNetworkJob::LabelType TypedGovernanceNetworkJob::labelType() const
{
    return _labelType;
}

void TypedGovernanceNetworkJob::setLabelType(LabelType newLabelType)
{
    if (_labelType == newLabelType) {
        return;
    }

    _labelType = newLabelType;
    Q_EMIT labelTypeChanged();
}

} // namespace OCC
