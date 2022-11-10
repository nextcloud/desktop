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

#include "thumbnailjob.h"
#include "networkjobs.h"
#include "account.h"

namespace OCC {

ThumbnailJob::ThumbnailJob(const QString &path, AccountPtr account, QObject *parent)
    : AbstractNetworkJob(account, account->url(), QStringLiteral("index.php/apps/files/api/v1/thumbnail/150/150") + path, parent)
{
    Q_ASSERT(path.startsWith(QLatin1Char('/')));
    setIgnoreCredentialFailure(true);
}

void ThumbnailJob::start()
{
    sendRequest("GET");
    AbstractNetworkJob::start();
}

void ThumbnailJob::finished()
{
    const auto result = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QPixmap p;
    if (result == 200) {
        p.loadFromData(reply()->readAll());
        if (p.isNull()) {
            qWarning() << Q_FUNC_INFO << "Invalid thumbnail";
        }
    }
    emit jobFinished(result, p);
}
}
