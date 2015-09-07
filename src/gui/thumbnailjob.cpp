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

#include "thumbnailjob.h"
#include "networkjobs.h"
#include "account.h"
#include "json.h"

namespace OCC {

ThumbnailJob::ThumbnailJob(const QString &path, AccountPtr account, QObject* parent)
: AbstractNetworkJob(account, "", parent)
{
    _url = Account::concatUrlPath(account->url(), QLatin1String("index.php/apps/files/api/v1/thumbnail/150/150/") + path);
    setIgnoreCredentialFailure(true);
}

void ThumbnailJob::start()
{
    qDebug() << Q_FUNC_INFO;
    setReply(getRequest(_url));
    setupConnections(reply());
    AbstractNetworkJob::start();
}

bool ThumbnailJob::finished()
{
    emit jobFinished(reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), reply()->readAll());
    return true;
}

}
