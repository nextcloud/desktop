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

#include <QJsonDocument>
#include <QJsonObject>

namespace OCC {

ShibbolethUserJob::ShibbolethUserJob(AccountPtr account, QObject* parent)
    : JsonApiJob(account, QLatin1String("ocs/v1.php/cloud/user"), parent)
{
    setIgnoreCredentialFailure(true);
    connect(this, SIGNAL(jsonReceived(QJsonDocument, int)), this, SLOT(slotJsonReceived(QJsonDocument, int)));
}

void ShibbolethUserJob::slotJsonReceived(const QJsonDocument &json, int statusCode)
{
    if( statusCode != 100 ) {
        qWarning() << "JSON Api call resulted in status code != 100";
    }
    QString user =  json.object().value("ocs").toObject().value("data").toObject().value("id").toString();
    //qDebug() << "cloud/user: " << json << "->" << user;
    emit userFetched(user);
}

}
