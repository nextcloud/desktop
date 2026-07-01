/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "applygovernancelabel.h"

#include "ocsgovernancejob.h"

using namespace Qt::StringLiterals;

namespace OCC
{

ApplyGovernanceLabel::ApplyGovernanceLabel(QObject *parent)
    : OCC::TypedWithLabelIdGovernanceNetworkJob{parent}
{
}

void ApplyGovernanceLabel::classBegin()
{
}

void ApplyGovernanceLabel::componentComplete()
{
    if (checkParameters()) {
        start();
    }
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

void ApplyGovernanceLabel::start(const QString &labelId)
{
    setLabelId(labelId);

    start();
}

QString ApplyGovernanceLabel::buildPath() const
{
    return u"/ocs/v2.php/apps/governance/%1/labels/%2/%3/%4/%5"_s.arg(apiVersionAsString(), entityTypeAsString(), integerEntityIdAsString(), labelTypeAsString(TypedGovernanceNetworkJob::Capitalization::UpCase), labelId());
}

void ApplyGovernanceLabel::jobDone(QJsonDocument reply, [[maybe_unused]] int statusCode)
{
    Q_EMIT finished(reply);
}

} // namespace OCC
