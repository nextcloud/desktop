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
    connect(this, &GetGovernanceLabels::apiVersionChanged, this, &GetGovernanceLabels::initialize);
    connect(this, &GetGovernanceLabels::entityTypeChanged, this, &GetGovernanceLabels::initialize);
    connect(this, &GetGovernanceLabels::customEntityTypeChanged, this, &GetGovernanceLabels::initialize);
    connect(this, &GetGovernanceLabels::entityIdChanged, this, &GetGovernanceLabels::initialize);
    connect(this, &GetGovernanceLabels::accountChanged, this, &GetGovernanceLabels::initialize);
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
}

void GetGovernanceLabels::jobDone(QJsonDocument reply, [[maybe_unused]] int statusCode)
{
    Q_EMIT finished(reply);
}

void GetGovernanceLabels::initialize()
{
    if (checkParameters()) {
        start();
    }
}

} // namespace OCC
