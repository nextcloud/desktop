/*
 * Copyright (C) by Camila Ayres <camila@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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
