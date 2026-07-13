/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "deletegovernancelabel.h"

#include "ocsgovernancejob.h"

using namespace Qt::StringLiterals;

namespace OCC
{

DeleteGovernanceLabel::DeleteGovernanceLabel(QObject *parent)
    : OCC::TypedWithLabelIdGovernanceNetworkJob{parent}
{
}

void DeleteGovernanceLabel::start()
{
    if (!checkParameters()) {
        Q_EMIT finishedWithError(500, {});
        return;
    }

    setOcsGovernanceJob(QPointer<OcsGovernanceJob>{new OcsGovernanceJob{account()}});

    connect(ocsGovernanceJob().data(), &OcsJob::jobFinished,
            this, &DeleteGovernanceLabel::jobDone);
    connect(ocsGovernanceJob().data(), &OcsJob::ocsError,
            this, &DeleteGovernanceLabel::finishedWithError);

    ocsGovernanceJob()->setPath(buildPath());
    ocsGovernanceJob()->setMethod("DELETE");

    ocsGovernanceJob()->start();
    Q_EMIT started();
}

void DeleteGovernanceLabel::start(const QString &labelId)
{
    setLabelId(labelId);

    start();
}

QString DeleteGovernanceLabel::buildPath() const
{
    return u"/ocs/v2.php/apps/governance/%1/labels/%2/%3/%4/%5"_s.arg(apiVersionAsString(), entityTypeAsString(), integerEntityIdAsString(), labelTypeAsString(TypedGovernanceNetworkJob::Capitalization::UpCase), labelId());
}

} // namespace OCC
