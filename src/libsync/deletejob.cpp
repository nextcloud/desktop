/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "deletejob.h"
#include "account.h"
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcDeleteJob, "nextcloud.sync.networkjob.delete", QtInfoMsg)

DeleteJob::DeleteJob(AccountPtr account, const QString &path, const QMap<QByteArray, QByteArray> &headers, QObject *parent)
    : SimpleFileJob(account, path, parent)
    , _headers(headers)
{
}

DeleteJob::DeleteJob(AccountPtr account, const QUrl &url, const QMap<QByteArray, QByteArray> &headers, QObject *parent)
    : SimpleFileJob(account, QString(), parent)
    , _headers(headers)
    , _url(url)
{
}

void DeleteJob::start()
{
    QNetworkRequest req;
    if (!_folderToken.isEmpty()) {
        req.setRawHeader("e2e-token", _folderToken);
    }

    for (auto oneHeaderIt = _headers.begin(); oneHeaderIt != _headers.end(); ++oneHeaderIt) {
        req.setRawHeader(oneHeaderIt.key(), oneHeaderIt.value());
    }

    if (_skipTrashbin) {
        req.setRawHeader("X-NC-Skip-Trashbin", "true");
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

bool DeleteJob::skipTrashbin() const
{
    return _skipTrashbin;
}

void DeleteJob::setSkipTrashbin(bool skipTrashbin)
{
    _skipTrashbin = skipTrashbin;
}

} // namespace OCC
