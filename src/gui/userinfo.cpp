/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 * Copyright (C) by Michael Schuster <michael@schuster.ms>
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

#include "userinfo.h"
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

namespace {
    static const int defaultIntervalT = 30 * 1000;
    static const int failIntervalT = 5 * 1000;
}

UserInfo::UserInfo(AccountState *accountState, bool allowDisconnectedAccountState, bool fetchAvatarImage, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
    , _allowDisconnectedAccountState(allowDisconnectedAccountState)
    , _fetchAvatarImage(fetchAvatarImage)
    , _lastQuotaTotalBytes(0)
    , _lastQuotaUsedBytes(0)
    , _active(false)
{
    connect(accountState, &AccountState::stateChanged,
        this, &UserInfo::slotAccountStateChanged);
    connect(&_jobRestartTimer, &QTimer::timeout, this, &UserInfo::slotFetchInfo);
    _jobRestartTimer.setSingleShot(true);
}

void UserInfo::setActive(bool active)
{
    _active = active;
    slotAccountStateChanged();
}


void UserInfo::slotAccountStateChanged()
{
    if (canGetInfo()) {
        // Obviously assumes there will never be more than thousand of hours between last info
        // received and now, hence why we static_cast
        auto elapsed = static_cast<int>(_lastInfoReceived.msecsTo(QDateTime::currentDateTime()));
        if (_lastInfoReceived.isNull() || elapsed >= defaultIntervalT) {
            slotFetchInfo();
        } else {
            _jobRestartTimer.start(defaultIntervalT - elapsed);
        }
    } else {
        _jobRestartTimer.stop();
    }
}

void UserInfo::slotRequestFailed()
{
    _lastQuotaTotalBytes = 0;
    _lastQuotaUsedBytes = 0;
    _jobRestartTimer.start(failIntervalT);
}

bool UserInfo::canGetInfo() const
{
    if (!_accountState || !_active || !_accountState->account() || _accountState->account()->isPublicShareLink()) {
        return false;
    }
    AccountPtr account = _accountState->account();
    return (_accountState->isConnected() || _allowDisconnectedAccountState)
        && account->credentials()
        && account->credentials()->ready();
}

void UserInfo::slotFetchInfo()
{
    if (!canGetInfo()) {
        return;
    }

    if (_job) {
        // The previous job was not finished?  Then we cancel it!
        _job->deleteLater();
    }

    AccountPtr account = _accountState->account();
    _job = new JsonApiJob(account, QLatin1String("ocs/v1.php/cloud/user"), this);
    _job->setTimeout(20 * 1000);
    connect(_job.data(), &JsonApiJob::jsonReceived, this, &UserInfo::slotUpdateLastInfo);
    connect(_job.data(), &AbstractNetworkJob::networkError, this, &UserInfo::slotRequestFailed);
    _job->start();
}

void UserInfo::slotUpdateLastInfo(const QJsonDocument &json)
{
    auto objData = json.object().value("ocs").toObject().value("data").toObject();

    AccountPtr account = _accountState->account();

    // User Info
    QString user = objData.value("id").toString();
    if (!user.isEmpty()) {
        account->setDavUser(user);
    }
    QString displayName = objData.value("display-name").toString();
    if (!displayName.isEmpty()) {
        account->setDavDisplayName(displayName);
    }

    // Quota
    auto objQuota = objData.value("quota").toObject();
    qint64 used = objQuota.value("used").toDouble();
    qint64 total = objQuota.value("quota").toDouble();

    if(_lastInfoReceived.isNull() || _lastQuotaUsedBytes != used || _lastQuotaTotalBytes != total) {
        _lastQuotaUsedBytes = used;
        _lastQuotaTotalBytes = total;
        emit quotaUpdated(_lastQuotaTotalBytes, _lastQuotaUsedBytes);
    }

    _jobRestartTimer.start(defaultIntervalT);
    _lastInfoReceived = QDateTime::currentDateTime();

    // Avatar Image
    if(_fetchAvatarImage && !account->isPublicShareLink()) {
        auto *job = new AvatarJob(account, account->davUser(), 128, this);
        job->setTimeout(20 * 1000);
        QObject::connect(job, &AvatarJob::avatarPixmap, this, &UserInfo::slotAvatarImage);
        job->start();
    }
    else
        emit fetchedLastInfo(this);
}

void UserInfo::slotAvatarImage(const QImage &img)
{
    _accountState->account()->setAvatar(img);

    emit fetchedLastInfo(this);
}

} // namespace OCC
