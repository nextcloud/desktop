/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
