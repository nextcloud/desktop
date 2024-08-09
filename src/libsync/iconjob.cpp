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

IconJob::IconJob(AccountPtr account, const QUrl &url, QObject *parent)
    : QObject(parent)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, true);
    const auto reply = account->sendRawRequest(QByteArrayLiteral("GET"), url, request);
    connect(reply, &QNetworkReply::finished, this, &IconJob::finished);
}

void IconJob::finished()
{
    const auto reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) {
        return;
    }
    deleteLater();

    const auto networkError = reply->error();
    if (networkError != QNetworkReply::NoError) {
        emit error(networkError);
        return;
    }

    emit jobFinished(reply->readAll());
}
}
