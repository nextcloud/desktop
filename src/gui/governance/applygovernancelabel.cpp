/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "applygovernancelabel.h"

#include "ocsgovernancejob.h"

namespace OCC
{

ApplyGovernanceLabel::ApplyGovernanceLabel(QObject *parent)
    : OCC::TypedWithLabelIdGovernanceNetworkJob{parent}
{
    connect(this, &ApplyGovernanceLabel::apiVersionChanged, this, &ApplyGovernanceLabel::initialize);
    connect(this, &ApplyGovernanceLabel::entityTypeChanged, this, &ApplyGovernanceLabel::initialize);
    connect(this, &ApplyGovernanceLabel::customEntityTypeChanged, this, &ApplyGovernanceLabel::initialize);
    connect(this, &ApplyGovernanceLabel::entityIdChanged, this, &ApplyGovernanceLabel::initialize);
    connect(this, &ApplyGovernanceLabel::accountChanged, this, &ApplyGovernanceLabel::initialize);
    connect(this, &ApplyGovernanceLabel::labelTypeChanged, this, &ApplyGovernanceLabel::initialize);
    connect(this, &ApplyGovernanceLabel::labelIdChanged, this, &ApplyGovernanceLabel::initialize);
}

void ApplyGovernanceLabel::start()
{
    if (!checkParameters()) {
        Q_EMIT finishedWitherror(500, {});
        return;
    }

    setOcsGovernanceJob(QPointer<OcsGovernanceJob>{new OcsGovernanceJob{account()}});

    connect(ocsGovernanceJob().data(), &OcsJob::jobFinished,
            this, &ApplyGovernanceLabel::jobDone);
    connect(ocsGovernanceJob().data(), &OcsJob::ocsError,
            this, &ApplyGovernanceLabel::finishedWitherror);

    ocsGovernanceJob()->setPath(buildPath());
    ocsGovernanceJob()->setMethod("POST");

    ocsGovernanceJob()->start();
}

void ApplyGovernanceLabel::jobDone(QJsonDocument reply, [[maybe_unused]] int statusCode)
{
    Q_EMIT finished(reply);
}

void ApplyGovernanceLabel::initialize()
{
    if (checkParameters()) {
        start();
    }
}

} // namespace OCC
