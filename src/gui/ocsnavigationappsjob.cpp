/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ocsnavigationappsjob.h"

namespace OCC {

OcsNavigationAppsJob::OcsNavigationAppsJob(AccountPtr account)
    : OcsJob(account)
{
    setPath("ocs/v2.php/core/navigation/apps");
    connect(this, &OcsNavigationAppsJob::jobFinished, this, &OcsNavigationAppsJob::jobDone);
}

void OcsNavigationAppsJob::getNavigationApps()
{
    setVerb("GET");
    addParam("absolute", "true");
    start();
}

void OcsNavigationAppsJob::jobDone(const QJsonDocument &reply, int statusCode)
{
    emit appsJobFinished(reply, statusCode);
}
}
