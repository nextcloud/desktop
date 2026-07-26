/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "typedwithlabelidgovernancenetworkjob.h"

using namespace Qt::StringLiterals;

namespace OCC
{

TypedWithLabelIdGovernanceNetworkJob::TypedWithLabelIdGovernanceNetworkJob(QObject *parent)
    : OCC::TypedGovernanceNetworkJob{parent}
{
}

QString TypedWithLabelIdGovernanceNetworkJob::labelId() const
{
    return _labelId;
}

void TypedWithLabelIdGovernanceNetworkJob::setLabelId(const QString &newLabelId)
{
    if (_labelId == newLabelId) {
        return;
    }

    _labelId = newLabelId;
    Q_EMIT labelIdChanged();
}

bool TypedWithLabelIdGovernanceNetworkJob::checkParameters() const
{
    auto result = TypedGovernanceNetworkJob::checkParameters();

    if (!result) {
        return result;
    }

    if (_labelId.isEmpty()) {
        result = false;
        return result;
    }

    return result;
}

} // namespace OCC
