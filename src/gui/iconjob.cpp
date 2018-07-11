/*
 * Copyright (C) by Camila Ayres <hello@camila.codes>
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

#include "iconjob.h"

namespace OCC {

IconJob::IconJob(const QUrl &url, QObject *parent) :
    QObject(parent)
{
    connect(&_accessManager, &QNetworkAccessManager::finished,
            this, &IconJob::finished);

    QNetworkRequest request(url);
    _accessManager.get(request);
}

void IconJob::finished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
        return;

    reply->deleteLater();
    emit jobFinished(reply->readAll());
}
}
