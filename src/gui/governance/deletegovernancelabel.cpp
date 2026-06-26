/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "deletegovernancelabel.h"

#include "ocsgovernancejob.h"

namespace OCC
{

DeleteGovernanceLabel::DeleteGovernanceLabel(QObject *parent)
    : OCC::TypedWithLabelIdGovernanceNetworkJob{parent}
{
    connect(this, &DeleteGovernanceLabel::apiVersionChanged, this, &DeleteGovernanceLabel::initialize);
    connect(this, &DeleteGovernanceLabel::entityTypeChanged, this, &DeleteGovernanceLabel::initialize);
    connect(this, &DeleteGovernanceLabel::customEntityTypeChanged, this, &DeleteGovernanceLabel::initialize);
    connect(this, &DeleteGovernanceLabel::entityIdChanged, this, &DeleteGovernanceLabel::initialize);
    connect(this, &DeleteGovernanceLabel::accountChanged, this, &DeleteGovernanceLabel::initialize);
    connect(this, &DeleteGovernanceLabel::labelTypeChanged, this, &DeleteGovernanceLabel::initialize);
    connect(this, &DeleteGovernanceLabel::labelIdChanged, this, &DeleteGovernanceLabel::initialize);
}

void DeleteGovernanceLabel::start()
{
    if (!checkParameters()) {
        Q_EMIT finishedWitherror(500, {});
        return;
    }

    setOcsGovernanceJob(QPointer<OcsGovernanceJob>{new OcsGovernanceJob{account()}});

    connect(ocsGovernanceJob().data(), &OcsJob::jobFinished,
            this, &DeleteGovernanceLabel::jobDone);
    connect(ocsGovernanceJob().data(), &OcsJob::ocsError,
            this, &DeleteGovernanceLabel::finishedWitherror);

    ocsGovernanceJob()->setPath(buildPath());
    ocsGovernanceJob()->setMethod("DELETE");

    ocsGovernanceJob()->start();
}

void DeleteGovernanceLabel::jobDone(QJsonDocument reply, [[maybe_unused]] int statusCode)
{
    Q_EMIT finished(reply);
}

void DeleteGovernanceLabel::initialize()
{
    if (checkParameters()) {
        start();
    }
}

} // namespace OCC
