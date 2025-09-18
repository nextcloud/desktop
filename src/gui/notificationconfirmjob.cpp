/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
        static const QRegularExpression rex("<statuscode>(\\d+)</statuscode>");
        const auto rexMatch = rex.match(replyStr);
        if (rexMatch.hasMatch()) {
            // this is a error message coming back from ocs.
            replyCode = rexMatch.captured(1).toInt();
        }
    }
    emit jobFinished(replyStr, replyCode);

    return true;
}
}
