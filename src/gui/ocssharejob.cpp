/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "ocssharejob.h"
#include "networkjobs.h"
#include "account.h"
#include "json.h"

#include <QBuffer>

namespace OCC {

OcsShareJob::OcsShareJob(AccountPtr account, QObject* parent)
: OcsJob(account, parent)
{
    setPath("ocs/v1.php/apps/files_sharing/api/v1/shares");
}

OcsShareJob::OcsShareJob(int shareId, AccountPtr account, QObject* parent)
: OcsJob(account, parent)
{
    setPath(QString("ocs/v1.php/apps/files_sharing/api/v1/shares/%1").arg(shareId));
}

void OcsShareJob::getShares(const QString &path)
{
    setVerb("GET");

    addParam(QString::fromLatin1("path"), path);
    addPassStatusCode(404);

    start();
}

void OcsShareJob::deleteShare()
{
    setVerb("DELETE");

    start();
}

void OcsShareJob::setExpireDate(const QDate &date)
{
    setVerb("PUT");

    if (date.isValid()) {
        addParam(QString::fromLatin1("expireDate"), date.toString("yyyy-MM-dd"));
    } else {
        addParam(QString::fromLatin1("expireDate"), QString());
    }

    start();
}

void OcsShareJob::setPassword(const QString &password)
{
    setVerb("PUT");

    addParam(QString::fromLatin1("password"), password);

    start();
}

void OcsShareJob::setPublicUpload(bool publicUpload)
{
    setVerb("PUT");

    const QString value = QString::fromLatin1(publicUpload ? "true" : "false");
    addParam(QString::fromLatin1("publicUpload"), value);

    start();
}

void OcsShareJob::createShare(const QString &path, SHARETYPE shareType, const QString &password, const QDate &date)
{
    setVerb("POST");

    addParam(QString::fromLatin1("path"), path);
    addParam(QString::fromLatin1("shareType"), QString::number(static_cast<int>(shareType)));

    if (!password.isEmpty()) {
        addParam(QString::fromLatin1("shareType"), password);
    }

    if (date.isValid()) {
        addParam(QString::fromLatin1("expireDate"), date.toString("yyyy-MM-dd"));
    }

    addPassStatusCode(403);

    start();
}

}
