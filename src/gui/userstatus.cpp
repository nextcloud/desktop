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
#include <theme.h>

#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

namespace OCC {

UserStatus::UserStatus(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
{
    connect(this, &UserStatus::fetchedCurrentUserStatus, _accountState, &AccountState::userStatusChanged);
}

void UserStatus::fetchCurrentUserStatus()
{
    if (_job) {
        _job->deleteLater();
    }

    AccountPtr account = _accountState->account();
    _job = new JsonApiJob(account, QStringLiteral("/ocs/v2.php/apps/user_status/api/v1/user_status"), this);
    connect(_job.data(), &JsonApiJob::jsonReceived, this, &UserStatus::slotFetchedCurrentStatus);
    _job->start();
}

void UserStatus::slotFetchedCurrentStatus(const QJsonDocument &json)
{
    const auto retrievedData = json.object().value("ocs").toObject().value("data").toObject();
    const auto icon = retrievedData.value("icon").toString();
    const auto message = retrievedData.value("message").toString();
    auto status = retrievedData.value("status").toString();

    if(message.isEmpty()) {
        if(status == "dnd") {
            status = tr("Do not disturb");
        }
    } else {
        status = message;
    }

    _currentUserStatus = QString("%1 %2").arg(icon, status);
    emit fetchedCurrentUserStatus();
}

QString UserStatus::currentUserStatus() const
{
    return _currentUserStatus;
}

} // namespace OCC
