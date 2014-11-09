/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#include "shibbolethuserjob.h"
#include <account.h>
#include <json.h>

namespace OCC {

ShibbolethUserJob::ShibbolethUserJob(Account* account, QObject* parent)
: AbstractNetworkJob(account, QLatin1String("ocs/v1.php/cloud/user"), parent)
{
    setIgnoreCredentialFailure(true);
}

void ShibbolethUserJob::start()
{
    QNetworkRequest req;
    req.setRawHeader("OCS-APIREQUEST", "true");
    QUrl url = Account::concatUrlPath(account()->url(), path());
    url.setQueryItems(QList<QPair<QString, QString> >() << qMakePair(QString::fromLatin1("format"), QString::fromLatin1("json")));
    setReply(davRequest("GET", url, req));
    setupConnections(reply());
    AbstractNetworkJob::start();
}

bool ShibbolethUserJob::finished()
{
    bool success = false;
    QVariantMap json = QtJson::parse(QString::fromUtf8(reply()->readAll()), success).toMap();
    // empty or invalid response
    if (!success || json.isEmpty()) {
        qDebug() << "cloud/user: invalid JSON!";
        emit userFetched(QString());
        return true;
    }

    QString user =  json.value("ocs").toMap().value("data").toMap().value("id").toString();
    //qDebug() << "cloud/user: " << json << "->" << user;
    emit userFetched(user);
    return true;
}



}
