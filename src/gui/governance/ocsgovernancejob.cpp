/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ocsgovernancejob.h"

namespace OCC
{

OcsGovernanceJob::OcsGovernanceJob(AccountPtr account)
    : OCC::OcsJob{account}
{
}

void OcsGovernanceJob::setMethod(const QByteArray &method)
{
    setVerb(method);
}

void OcsGovernanceJob::start()
{
    OcsJob::start();
}

} // namespace OCC
