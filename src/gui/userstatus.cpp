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
#include "capabilities.h"

#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

namespace OCC {

Q_LOGGING_CATEGORY(lcUserStatus, "nextcloud.gui.userstatus", QtInfoMsg)

namespace {
    UserStatus::Status stringToEnum(const QString &status) 
    {
        // it needs to match the Status enum
        const QHash<QString, UserStatus::Status> preDefinedStatus{
            {"online", UserStatus::Status::Online},
            {"dnd", UserStatus::Status::DoNotDisturb},
            {"away", UserStatus::Status::Away},
            {"offline", UserStatus::Status::Offline},
            {"invisible", UserStatus::Status::Invisible}
        };
        
        // api should return invisible, dnd,... toLower() it is to make sure 
        // it matches _preDefinedStatus, otherwise the default is online (0)
        return preDefinedStatus.value(status.toLower(), UserStatus::Status::Online);
    }
    
    QString enumToString(UserStatus::Status status) 
    {
        switch (status) {
        case UserStatus::Status::Away:
            return QObject::tr("Away");
        case UserStatus::Status::DoNotDisturb:
            return QObject::tr("Do not disturb");
        case UserStatus::Status::Invisible:
        case UserStatus::Status::Offline:
            return QObject::tr("Offline");
        case UserStatus::Status::Online:
            return QObject::tr("Online");
        }
        
        Q_UNREACHABLE();
    }
}

UserStatus::UserStatus(QObject *parent)
    : QObject(parent)
{
}

void UserStatus::fetchUserStatus(AccountPtr account)
{
    if (!account->capabilities().userStatus()) {
        return;
    }
    
    if (_job) {
        _job->deleteLater();
    }

    _job = new JsonApiJob(account, QStringLiteral("/ocs/v2.php/apps/user_status/api/v1/user_status"), this);
    connect(_job.data(), &JsonApiJob::jsonReceived, this, &UserStatus::slotFetchUserStatusFinished);
    _job->start();
}

void UserStatus::slotFetchUserStatusFinished(const QJsonDocument &json, int statusCode)
{
    const QJsonObject defaultValues {
        {"icon", ""},
        {"message", ""},
        {"status", "online"},
        {"messageIsPredefined", "false"},
        {"statusIsUserDefined", "false"}
    };
    
    if (statusCode != 200) {
        qCInfo(lcUserStatus) << "Slot fetch UserStatus finished with status code" << statusCode;
        qCInfo(lcUserStatus) << "Using then default values as if user has not set any status" << defaultValues;
    }
    
    const auto retrievedData = json.object().value("ocs").toObject().value("data").toObject(defaultValues);

    _emoji = retrievedData.value("icon").toString().trimmed();
    _status = stringToEnum(retrievedData.value("status").toString());
    _message = retrievedData.value("message").toString().trimmed();

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

QString UserStatus::emoji() const
{
    return _emoji;
}

QUrl UserStatus::icon() const
{
    switch (_status) {
    case Status::Away:
        return Theme::instance()->statusAwayImageSource();
    case Status::DoNotDisturb:
        return Theme::instance()->statusDoNotDisturbImageSource();
    case Status::Invisible:
    case Status::Offline:
        return Theme::instance()->statusInvisibleImageSource();
    case Status::Online:
        return Theme::instance()->statusOnlineImageSource();
    }
    
    Q_UNREACHABLE();
}

} // namespace OCC
