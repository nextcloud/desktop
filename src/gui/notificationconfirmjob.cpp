/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "notificationconfirmjob.h"
#include "networkjobs.h"
#include "account.h"

#include <QBuffer>

namespace OCC {

Q_LOGGING_CATEGORY(lcNotificationsJob, "nextcloud.gui.notifications", QtInfoMsg)

NotificationConfirmJob::NotificationConfirmJob(AccountPtr account)
    : AbstractNetworkJob(account, "")
{
    setIgnoreCredentialFailure(true);
}

void NotificationConfirmJob::setLinkAndVerb(const QUrl &link, const QByteArray &verb)
{
    _link = link;
    _verb = verb;
}

void NotificationConfirmJob::start()
{
    if (!_link.isValid()) {
        qCWarning(lcNotificationsJob) << "Attempt to trigger invalid URL: " << _link.toString();
        return;
    }
    QNetworkRequest req;
    req.setRawHeader("Ocs-APIREQUEST", "true");
    req.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

    sendRequest(_verb, _link, req);

    AbstractNetworkJob::start();
}

bool NotificationConfirmJob::finished()
{
    int replyCode = 0;
    // FIXME: check for the reply code!
    const QString replyStr = reply()->readAll();

    if (replyStr.contains("<?xml version=\"1.0\"?>")) {
        QRegExp rex("<statuscode>(\\d+)</statuscode>");
        if (replyStr.contains(rex)) {
            // this is a error message coming back from ocs.
            replyCode = rex.cap(1).toInt();
        }
    }
    emit jobFinished(replyStr, replyCode);

    return true;
}
}
