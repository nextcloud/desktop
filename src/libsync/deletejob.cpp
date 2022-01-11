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

#include "deletejob.h"
#include "account.h"
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcDeleteJob, "nextcloud.sync.networkjob.delete", QtInfoMsg)

DeleteJob::DeleteJob(AccountPtr account, const QString &path, QObject *parent)
    : SimpleFileJob(account, path, parent)
{
}

DeleteJob::DeleteJob(AccountPtr account, const QUrl &url, QObject *parent)
    : SimpleFileJob(account, QString(), parent)
    , _url(url)
{
}

void DeleteJob::start()
{
    QNetworkRequest req;
    if (!_folderToken.isEmpty()) {
        req.setRawHeader("e2e-token", _folderToken);
    }

    if (_url.isValid()) {
        startRequest("DELETE", _url, req);
    } else {
        startRequest("DELETE", req);
    }
}

QByteArray DeleteJob::folderToken() const
{
    return _folderToken;
}

void DeleteJob::setFolderToken(const QByteArray &folderToken)
{
    _folderToken = folderToken;
}

} // namespace OCC
