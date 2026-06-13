/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typedgovernancenetworkjob.h"

using namespace Qt::StringLiterals;

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

QString TypedGovernanceNetworkJob::labelTypeAsString() const
{
    auto result = QString{};

    switch (_labelType)
    {
    case LabelType::Sensitivity:
        result = u"sensitivity"_s;
        break;
    case LabelType::Retention:
        result = u"retention"_s;
        break;
    case LabelType::Hold:
        result = u"hold"_s;
        break;
    }

    return result;
}

} // namespace OCC
