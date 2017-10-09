/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
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

#include "ocsshareejob.h"

namespace OCC {

OcsShareeJob::OcsShareeJob(AccountPtr account)
    : OcsJob(account)
{
    setPath("ocs/v1.php/apps/files_sharing/api/v1/sharees");
    connect(this, &OcsJob::jobFinished, this, &OcsShareeJob::jobDone);
}

void OcsShareeJob::getSharees(const QString &search,
    const QString &itemType,
    int page,
    int perPage)
{
    setVerb("GET");

    addParam(QString::fromLatin1("search"), search);
    addParam(QString::fromLatin1("itemType"), itemType);
    addParam(QString::fromLatin1("page"), QString::number(page));
    addParam(QString::fromLatin1("perPage"), QString::number(perPage));

    start();
}

void OcsShareeJob::jobDone(const QJsonDocument &reply)
{
    emit shareeJobFinished(reply);
}
}
