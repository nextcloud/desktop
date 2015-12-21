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

#include "avatarjob.h"
#include "networkjobs.h"
#include "account.h"

#include <QLatin1String>

namespace OCC {

AvatarJob2::AvatarJob2(const QString &userId, int size, AccountPtr account, QObject* parent)
: AbstractNetworkJob(account, QLatin1String("remote.php/dav/avatars/"), parent)
{
    setIgnoreCredentialFailure(true);

    //Append <userid>/<size> to the path
    setPath(path() + userId + QString("/%1").arg(size));
}

void AvatarJob2::start()
{
    sendRequest("GET", Utility::concatUrlPath(account()->url(), path()));
    AbstractNetworkJob::start();
}

bool AvatarJob2::finished()
{
    int statusCode = reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (statusCode != 200) {
        emit avatarNotAvailable(statusCode);
    } else {
        QByteArray data = reply()->readAll();
        QString mimeType = reply()->header(QNetworkRequest::ContentTypeHeader).toString();
        emit avatarReady(data, mimeType);
    }

    return true;
}

}
