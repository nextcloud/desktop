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
#include "account.h"
#include "common/utility.h"
#include "networkjobs.h"

#include <QBuffer>
#include <QJsonDocument>

namespace {
OCC::JsonApiJob *createJob(OCC::AccountPtr account, const QString &path, const QByteArray &verb, const OCC::JsonApiJob::UrlQuery &arguments, QObject *parent)
{
    QString p = QStringLiteral("ocs/v1.php/apps/files_sharing/api/v1/shares");
    if (!path.isEmpty()) {
        p += QLatin1Char('/') + path;
    }
    return new OCC::JsonApiJob(account, p, verb, arguments, {}, parent);
}
}
namespace OCC {


JsonApiJob *OcsShareJob::getShares(AccountPtr account, QObject *parent, const QString &path)
{
    return createJob(account, {}, "GET", { { QStringLiteral("path"), path }, { QStringLiteral("reshares"), QStringLiteral("true") } }, parent);
}

JsonApiJob *OcsShareJob::deleteShare(AccountPtr account, QObject *parent, const QString &shareId)
{
    return createJob(account, shareId, "DELETE", {}, parent);
}

JsonApiJob *OcsShareJob::setExpireDate(AccountPtr account, QObject *parent, const QString &shareId, const QDate &date)
{
    return createJob(account, shareId, "PUT", { { QStringLiteral("expireDate"), date.isValid() ? date.toString(QStringLiteral("yyyy-MM-dd")) : QString() } }, parent);
}

JsonApiJob *OcsShareJob::setPassword(AccountPtr account, QObject *parent, const QString &shareId, const QString &password)
{
    return createJob(account, shareId, "PUT", { { QStringLiteral("password"), password } }, parent);
}

JsonApiJob *OcsShareJob::setPublicUpload(AccountPtr account, QObject *parent, const QString &shareId, bool publicUpload)
{
    return createJob(account, shareId, "PUT", { { QStringLiteral("publicUpload"), publicUpload ? QStringLiteral("true") : QStringLiteral("false") } }, parent);
}

JsonApiJob *OcsShareJob::setName(AccountPtr account, QObject *parent, const QString &shareId, const QString &name)
{
    return createJob(account, shareId, "PUT", { { QStringLiteral("name"), name } }, parent);
}

JsonApiJob *OcsShareJob::setPermissions(AccountPtr account, QObject *parent, const QString &shareId,
    const Share::Permissions permissions)
{
    return createJob(account, shareId, "PUT", { { QStringLiteral("permissions"), QString::number(permissions) } }, parent);
}

JsonApiJob *OcsShareJob::createLinkShare(AccountPtr account, QObject *parent, const QString &path,
    const QString &name,
    const QString &password,
    const QDate &expireDate,
    const Share::Permissions permissions)
{
    JsonApiJob::UrlQuery args {
        { QStringLiteral("path"), path },
        { QStringLiteral("shareType"), QString::number(Share::TypeLink) },
    };

    if (!name.isEmpty()) {
        args.append({ QStringLiteral("name"), name });
    }
    if (!password.isEmpty()) {
        args.append({ QStringLiteral("password"), password });
    }
    if (!expireDate.isNull()) {
        args.append({ QStringLiteral("expireDate"), expireDate.toString(QStringLiteral("yyyy-MM-dd")) });
    }
    if (permissions != SharePermissionDefault) {
        args.append({ QStringLiteral("permissions"), QString::number(permissions) });
    }
    return createJob(account, {}, "POST", args, parent);
}

JsonApiJob *OcsShareJob::createShare(AccountPtr account, QObject *parent, const QString &path,
    const Share::ShareType shareType,
    const QString &shareWith,
    const Share::Permissions permissions)
{
    JsonApiJob::UrlQuery args {
        { QStringLiteral("path"), path },
        { QStringLiteral("shareType"), QString::number(shareType) },
        { QStringLiteral("shareWith"), shareWith }
    };

    if (!(permissions & SharePermissionDefault)) {
        args.append({ QStringLiteral("permissions"), QString::number(permissions) });
    }

    return createJob(account, {}, "POST", args, parent);
}

JsonApiJob *OcsShareJob::getSharedWithMe(AccountPtr account, QObject *parent)
{
    return createJob(account, {}, "GET", { { QStringLiteral("shared_with_me"), QStringLiteral("true") } }, parent);
}

}
