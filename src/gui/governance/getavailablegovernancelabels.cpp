/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "getavailablegovernancelabels.h"

#include "ocsgovernancejob.h"

using namespace Qt::StringLiterals;

namespace OCC
{

GetAvailableGovernanceLabels::GetAvailableGovernanceLabels(QObject *parent)
    : OCC::TypedGovernanceNetworkJob{parent}
{
}

void GetAvailableGovernanceLabels::start()
{
    setOcsGovernanceJob(QPointer<OcsGovernanceJob>{new OcsGovernanceJob{account()}});

    connect(ocsGovernanceJob().data(), &OcsJob::jobFinished,
            this, &GetAvailableGovernanceLabels::jobDone);
    connect(ocsGovernanceJob().data(), &OcsJob::ocsError,
            this, &GetAvailableGovernanceLabels::finishedWithError);

    ocsGovernanceJob()->setPath(buildPath());
    ocsGovernanceJob()->setMethod("GET");

    ocsGovernanceJob()->start();
}

void GetAvailableGovernanceLabels::jobDone(QJsonDocument reply, [[maybe_unused]] int statusCode)
{
    Q_EMIT finished(reply);
}

QString GetAvailableGovernanceLabels::buildPath() const
{
    return u"/ocs/v2.php/apps/governance/%1/labels/%2/%3/%4/available"_s.arg(apiVersionAsString(), entityTypeAsString(), entityId(), labelTypeAsString());
}

} // namespace OCC
