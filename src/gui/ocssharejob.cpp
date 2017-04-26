/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#include "ocssharejob.h"
#include "networkjobs.h"
#include "account.h"

#include <QBuffer>
#include <QJsonDocument>

namespace OCC {

OcsShareJob::OcsShareJob(AccountPtr account)
: OcsJob(account)
{
    setPath("ocs/v1.php/apps/files_sharing/api/v1/shares");
    connect(this, SIGNAL(jobFinished(QJsonDocument)), this, SLOT(jobDone(QJsonDocument)));
}

void OcsShareJob::getShares(const QString &path)
{
    setVerb("GET");

    addParam(QString::fromLatin1("path"), path);
    addPassStatusCode(404);

    start();
}

void OcsShareJob::deleteShare(const QString &shareId)
{
    appendPath(shareId);
    setVerb("DELETE");

    start();
}

void OcsShareJob::setExpireDate(const QString &shareId, const QDate &date)
{
    appendPath(shareId);
    setVerb("PUT");

    if (date.isValid()) {
        addParam(QString::fromLatin1("expireDate"), date.toString("yyyy-MM-dd"));
    } else {
        addParam(QString::fromLatin1("expireDate"), QString());
    }
    _value = date;

    start();
}

void OcsShareJob::setPassword(const QString &shareId, const QString &password)
{
    appendPath(shareId);
    setVerb("PUT");

    addParam(QString::fromLatin1("password"), password);
    _value = password;

    start();
}

void OcsShareJob::setPublicUpload(const QString &shareId, bool publicUpload)
{
    appendPath(shareId);
    setVerb("PUT");

    const QString value = QString::fromLatin1(publicUpload ? "true" : "false");
    addParam(QString::fromLatin1("publicUpload"), value);
    _value = publicUpload;

    start();
}

void OcsShareJob::setName(const QString &shareId, const QString &name)
{
    appendPath(shareId);
    setVerb("PUT");
    addParam(QString::fromLatin1("name"), name);
    _value = name;

    start();
}

void OcsShareJob::setPermissions(const QString &shareId, 
                                 const Share::Permissions permissions)
{
    appendPath(shareId);
    setVerb("PUT");

    addParam(QString::fromLatin1("permissions"), QString::number(permissions));
    _value = (int)permissions;

    start();
}

void OcsShareJob::createLinkShare(const QString &path,
                                  const QString &name,
                                  const QString &password)
{
    setVerb("POST");

    addParam(QString::fromLatin1("path"), path);
    addParam(QString::fromLatin1("shareType"), QString::number(Share::TypeLink));

    if (!name.isEmpty()) {
        addParam(QString::fromLatin1("name"), name);
    }
    if (!password.isEmpty()) {
        addParam(QString::fromLatin1("password"), password);
    }

    addPassStatusCode(403);

    start();
}

void OcsShareJob::createShare(const QString& path, 
                              const Share::ShareType shareType,
                              const QString& shareWith,
                              const Share::Permissions permissions)
{
    setVerb("POST");

    addParam(QString::fromLatin1("path"), path);
    addParam(QString::fromLatin1("shareType"), QString::number(shareType));
    addParam(QString::fromLatin1("shareWith"), shareWith);
    if (!(permissions & SharePermissionDefault)) {
        addParam(QString::fromLatin1("permissions"), QString::number(permissions));
    }

    start();
}

void OcsShareJob::getSharedWithMe()
{
    setVerb("GET");
    addParam(QLatin1String("shared_with_me"), QLatin1String("true"));
    start();
}

void OcsShareJob::jobDone(QJsonDocument reply)
{
    emit shareJobFinished(reply, _value);
}

}
