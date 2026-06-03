/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "applygovernancelabel.h"

#include "ocsgovernancejob.h"

namespace OCC
{

ApplyGovernanceLabel::ApplyGovernanceLabel(AccountPtr account, QObject *parent)
    : OCC::TypedWithLabelIdGovernanceNetworkJob{account, parent}
{
}

void ApplyGovernanceLabel::start()
{
    setOcsGovernanceJob(QPointer<OcsGovernanceJob>{new OcsGovernanceJob{account()}});

    connect(ocsGovernanceJob().data(), &OcsJob::jobFinished,
            this, &ApplyGovernanceLabel::jobDone);

    ocsGovernanceJob()->setPath(buildPath());
    ocsGovernanceJob()->setMethod("POST");

    ocsGovernanceJob()->start();
}

void ApplyGovernanceLabel::jobDone(QJsonDocument reply, int statusCode)
{
    Q_UNUSED(reply)
    Q_UNUSED(statusCode)

    Q_EMIT finished();
}

} // namespace OCC
