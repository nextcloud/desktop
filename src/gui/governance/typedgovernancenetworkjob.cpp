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

Governance::LabelType TypedGovernanceNetworkJob::labelType() const
{
    return _labelType;
}

void TypedGovernanceNetworkJob::setLabelType(Governance::LabelType newLabelType)
{
    if (_labelType == newLabelType) {
        return;
    }

    _labelType = newLabelType;
    Q_EMIT labelTypeChanged();
}

QString TypedGovernanceNetworkJob::labelTypeAsString(Capitalization capitalization) const
{
    auto result = QString{};

    switch (capitalization)
    {
    case Capitalization::LowCase:
        switch (_labelType)
        {
        case Governance::LabelType::Sensitivity:
            result = u"sensitivity"_s;
            break;
        case Governance::LabelType::Retention:
            result = u"retention"_s;
            break;
        case Governance::LabelType::LegalHold:
            result = u"hold"_s;
            break;
        case Governance::LabelType::InvalidLabelType:
            result = u"invalid"_s;
            break;
        }
        break;
    case Capitalization::UpCase:
        switch (_labelType)
        {
        case Governance::LabelType::Sensitivity:
            result = u"SENSITIVITY"_s;
            break;
        case Governance::LabelType::Retention:
            result = u"RETENTION"_s;
            break;
        case Governance::LabelType::LegalHold:
            result = u"HOLD"_s;
            break;
        case Governance::LabelType::InvalidLabelType:
            result = u"invalid"_s;
            break;
        }
        break;
    }

    return result;
}

bool TypedGovernanceNetworkJob::checkParameters() const
{
    auto result = GovernanceNetworkJob::checkParameters();

    if (!result) {
        return result;
    }

    if (_labelType == Governance::LabelType::InvalidLabelType) {
        result = false;
        return result;
    }

    return result;
}

} // namespace OCC
