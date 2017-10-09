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
#include <QLoggingCategory>

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcShibboleth)

ShibbolethUserJob::ShibbolethUserJob(AccountPtr account, QObject *parent)
    : JsonApiJob(account, QLatin1String("ocs/v1.php/cloud/user"), parent)
{
    setIgnoreCredentialFailure(true);
    connect(this, &JsonApiJob::jsonReceived, this, &ShibbolethUserJob::slotJsonReceived);
}

void ShibbolethUserJob::slotJsonReceived(const QJsonDocument &json, int statusCode)
{
    if (statusCode != 100) {
        qCWarning(lcShibboleth) << "JSON Api call resulted in status code != 100";
    }
    QString user = json.object().value("ocs").toObject().value("data").toObject().value("id").toString();
    emit userFetched(user);
}
}
