/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "getgovernancelabels.h"

#include "ocsgovernancejob.h"

using namespace Qt::StringLiterals;

namespace OCC
{

GetGovernanceLabels::GetGovernanceLabels(QObject *parent)
    : OCC::GovernanceNetworkJob{parent}
{
}

void GetGovernanceLabels::start()
{
    if (!checkParameters()) {
        Q_EMIT finishedWithError(500, {});
        return;
    }

    setOcsGovernanceJob(QPointer<OcsGovernanceJob>{new OcsGovernanceJob{account()}});

    connect(ocsGovernanceJob().data(), &OcsJob::jobFinished,
            this, &GetGovernanceLabels::jobDone);
    connect(ocsGovernanceJob().data(), &OcsJob::ocsError,
            this, &GetGovernanceLabels::finishedWithError);

    ocsGovernanceJob()->setPath(buildPath());
    ocsGovernanceJob()->setMethod("GET");

    ocsGovernanceJob()->start();
    Q_EMIT started();
}

void GetGovernanceLabels::start(const QString &entityId)
{
    setEntityId(entityId);
    start();
}

QString GetGovernanceLabels::buildPath() const
{
    return u"/ocs/v2.php/apps/governance/%1/labels/%2/%3"_s.arg(apiVersionAsString(), entityTypeAsString(), integerEntityIdAsString());
}

} // namespace OCC
