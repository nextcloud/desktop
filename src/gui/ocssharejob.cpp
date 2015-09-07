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
: OCSJob(account, parent)
{
    setUrl(Account::concatUrlPath(account->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares")));
}

OcsShareJob::OcsShareJob(int shareId, AccountPtr account, QObject* parent)
: OCSJob(account, parent)
{
    setUrl(Account::concatUrlPath(account->url(), QString("ocs/v1.php/apps/files_sharing/api/v1/shares/%1").arg(shareId)));
}

void OcsShareJob::getShares(const QString &path)
{
    setVerb("GET");
    
    QList<QPair<QString, QString> > getParams;
    getParams.append(qMakePair(QString::fromLatin1("path"), path));
    setGetParams(getParams);

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

    QList<QPair<QString, QString> > postParams;

    if (date.isValid()) {
        postParams.append(qMakePair(QString::fromLatin1("expireDate"), date.toString("yyyy-MM-dd")));
    } else {
        postParams.append(qMakePair(QString::fromLatin1("expireDate"), QString()));
    }

    setPostParams(postParams);
    start();
}

void OcsShareJob::setPassword(const QString &password)
{
    setVerb("PUT");

    QList<QPair<QString, QString> > postParams;
    postParams.append(qMakePair(QString::fromLatin1("password"), password));

    setPostParams(postParams);
    start();
}

void OcsShareJob::createShare(const QString &path, SHARETYPE shareType, const QString &password, const QDate &date)
{
    setVerb("POST");

    QList<QPair<QString, QString> > postParams;
    postParams.append(qMakePair(QString::fromLatin1("path"), path));
    postParams.append(qMakePair(QString::fromLatin1("shareType"), QString::number(static_cast<int>(shareType))));

    if (!password.isEmpty()) {
        postParams.append(qMakePair(QString::fromLatin1("shareType"), password));
    }

    if (date.isValid()) {
        postParams.append(qMakePair(QString::fromLatin1("expireDate"), date.toString("yyyy-MM-dd")));
    }

    setPostParams(postParams);

    addPassStatusCode(403);

    start();
}

}
