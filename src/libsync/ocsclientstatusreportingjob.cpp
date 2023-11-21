/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
#include "ocsclientstatusreportingjob.h"
#include "networkjobs.h"
#include "account.h"

#include <QBuffer>
#include <QJsonDocument>

namespace OCC {

OcsClientStatusReportingJob::OcsClientStatusReportingJob(AccountPtr account)
    : OcsJob(account)
{
    setPath(QStringLiteral("ocs/v2.php/apps/security_guard/diagnostics"));
    connect(this, &OcsJob::jobFinished, this, &OcsClientStatusReportingJob::jobDone);
}

void OcsClientStatusReportingJob::sendStatusReport(const QVariant &jsonData)
{
    setVerb("PUT");

    addRawHeader("Ocs-APIREQUEST", "true");
    addRawHeader("Content-Type", "application/json");

    const auto url = Utility::concatUrlPath(account()->url(), path());
    sendRequest(_verb, url, _request, QJsonDocument::fromVariant(jsonData.toMap()).toJson());
    AbstractNetworkJob::start();
}

void OcsClientStatusReportingJob::jobDone(QJsonDocument reply)
{
    emit jobFinished(reply, {});
}
}
