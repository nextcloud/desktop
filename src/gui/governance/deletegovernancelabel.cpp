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
}

void DeleteGovernanceLabel::start()
{
    setOcsGovernanceJob(QPointer<OcsGovernanceJob>{new OcsGovernanceJob{account()}});

    connect(ocsGovernanceJob().data(), &OcsJob::jobFinished,
            this, &DeleteGovernanceLabel::jobDone);
    connect(ocsGovernanceJob().data(), &OcsJob::ocsError,
            this, &DeleteGovernanceLabel::finishedWithError);

    ocsGovernanceJob()->setPath(buildPath());
    ocsGovernanceJob()->setMethod("DELETE");

    ocsGovernanceJob()->start();
}

void DeleteGovernanceLabel::jobDone(QJsonDocument reply, [[maybe_unused]] int statusCode)
{
    Q_EMIT finished(reply);
}

} // namespace OCC
