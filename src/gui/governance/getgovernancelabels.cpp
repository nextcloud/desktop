/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "getgovernancelabels.h"

#include "ocsgovernancejob.h"

namespace OCC
{

GetGovernanceLabels::GetGovernanceLabels(QObject *parent)
    : OCC::GovernanceNetworkJob{parent}
{
}

void GetGovernanceLabels::start()
{
    setOcsGovernanceJob(QPointer<OcsGovernanceJob>{new OcsGovernanceJob{account()}});

    connect(ocsGovernanceJob().data(), &OcsJob::jobFinished,
            this, &GetGovernanceLabels::jobDone);
    connect(ocsGovernanceJob().data(), &OcsJob::ocsError,
            this, &GetGovernanceLabels::finishedWitherror);

    ocsGovernanceJob()->setPath(buildPath());
    ocsGovernanceJob()->setMethod("GET");

    ocsGovernanceJob()->start();
}

void GetGovernanceLabels::jobDone(QJsonDocument reply, int statusCode)
{
    Q_UNUSED(reply)
    Q_UNUSED(statusCode)

    qCInfo(lcGovernance) << reply;

    Q_EMIT finished();
}

} // namespace OCC
