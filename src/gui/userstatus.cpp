/*
 * Copyright (C) by Camila <hello@camila.codes>
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

#include "userstatus.h"
#include "account.h"
#include "accountstate.h"
#include "networkjobs.h"
#include "folderman.h"
#include "creds/abstractcredentials.h"
#include "theme.h"

#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

namespace OCC {

UserStatus::UserStatus(QObject *parent)
    : QObject(parent)
    , _message("")
{
}

UserStatus::Status UserStatus::stringToEnum(const QString &status) const 
{
    // api should return invisible, dnd,... toLower() it is to make sure 
    // it matches _preDefinedStatus, otherwise the default is online (0)
    const auto statusEnum = _preDefinedStatus.value(status.isEmpty()? "online" : status.toLower(), 0);
    return static_cast<Status>(statusEnum);
}

void UserStatus::fetchUserStatus(AccountPtr account)
{
    if (_job) {
        _job->deleteLater();
    }

    _job = new JsonApiJob(account, QStringLiteral("/ocs/v2.php/apps/user_status/api/v1/user_status"), this);
    connect(_job.data(), &JsonApiJob::jsonReceived, this, &UserStatus::slotFetchUserStatusFinished);
    _job->start();
}

void UserStatus::slotFetchUserStatusFinished(const QJsonDocument &json)
{
    const auto retrievedData = json.object().value("ocs").toObject().value("data").toObject();
    const auto emoji = retrievedData.value("icon").toString();
    const auto message = retrievedData.value("message").toString();
    auto statusString = retrievedData.value("status").toString(); 
    _status = stringToEnum(statusString);
    
    // to display it to the user like 'Invisible' instead of 'invisible'
    statusString.replace(0, 1, statusString.at(0).toUpper());  

    const auto visibleStatusText = message.isEmpty()
                                ? _status == DoNotDisturb? tr("Do not disturb") 
                                                : tr(qPrintable(statusString))
                                : message;

    _message = QString("%1 %2").arg(emoji, visibleStatusText);
    emit fetchUserStatusFinished();
}

UserStatus::Status UserStatus::status() const
{
    return _status;
}

QString UserStatus::message() const
{
    return _message;
}

QUrl UserStatus::icon() const
{
    switch (_status) {
    case Online:
        return Theme::instance()->statusOnlineImageSource();
    case Away:
        return Theme::instance()->statusAwayImageSource();
    case DoNotDisturb:
        return Theme::instance()->statusDoNotDisturbImageSource();
    case Invisible:
    case Offline:
        return Theme::instance()->statusInvisibleImageSource();
    default:
        return Theme::instance()->statusOnlineImageSource();
    }
}

} // namespace OCC
