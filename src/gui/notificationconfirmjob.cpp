/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "notificationconfirmjob.h"
#include "networkjobs.h"
#include "account.h"
#include "json.h"

#include <QBuffer>

namespace OCC {

NotificationConfirmJob::NotificationConfirmJob(AccountPtr account)
: AbstractNetworkJob(account, "")
{
    setIgnoreCredentialFailure(true);
}

void NotificationConfirmJob::setLinkAndVerb(const QUrl& link, const QString &verb)
{
    _link = link;
    _verb = verb;
}

void NotificationConfirmJob::start()
{
    if( !_link.isValid() ) {
        qDebug() << "Attempt to trigger invalid URL: " << _link.toString();
        return;
    }
    QNetworkRequest req;
    req.setRawHeader("Content-Type", "application/x-www-form-urlencoded");

    QIODevice *iodevice = 0;
    setReply(davRequest(_verb.toAscii(), _link, req, iodevice));
    setupConnections(reply());

    AbstractNetworkJob::start();
}

bool NotificationConfirmJob::finished()
{
    int replyCode = 0;
    // FIXME: check for the reply code!
    const QString replyData = reply()->readAll();

    emit jobFinished(replyData, replyCode);

    return true;

}

}
