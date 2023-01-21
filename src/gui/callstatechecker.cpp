/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "callstatechecker.h"
#include "account.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcCallStateChecker, "nextcloud.gui.callstatechecker", QtInfoMsg)

constexpr int successStatusCode = 200;

CallStateChecker::CallStateChecker(QObject *parent)
    : QObject(parent)
{
    setup();
}

void CallStateChecker::setup()
{
    _notificationTimer.setSingleShot(true);
    _notificationTimer.setInterval(60 * 1000);
    connect(&_notificationTimer, &QTimer::timeout, this, &CallStateChecker::slotNotificationTimerElapsed);

    _statusCheckTimer.setInterval(5 * 1000);
    connect(&_statusCheckTimer, &QTimer::timeout, this, &CallStateChecker::slotStatusCheckTimerElapsed);
}

QString CallStateChecker::token() const
{
    return _token;
}

void CallStateChecker::setToken(const QString &token)
{
    _token = token;
    Q_EMIT tokenChanged();
    reset();
}

AccountState* CallStateChecker::accountState() const
{
    return _accountState;
}

void CallStateChecker::setAccountState(AccountState *state)
{
    _accountState = state;
    Q_EMIT accountStateChanged();
    reset();
}

bool CallStateChecker::checking() const
{
    return _checking;
}

void CallStateChecker::setChecking(const bool checking)
{
    if(checking) {
        qCInfo(lcCallStateChecker) << "Starting to check state of call with token:" << _token;
        _notificationTimer.start();
        _statusCheckTimer.start();
    } else {
        qCInfo(lcCallStateChecker) << "Stopping checking of call state for call with token:" << _token;
        _notificationTimer.stop();
        _statusCheckTimer.stop();
        _stateCheckJob.clear();
    }

    _checking = checking;
    Q_EMIT checkingChanged();
}

void CallStateChecker::reset()
{
    qCInfo(lcCallStateChecker, "Resetting call check");
    setChecking(false);
    setChecking(true);
}

void CallStateChecker::slotNotificationTimerElapsed()
{
    qCInfo(lcCallStateChecker) << "Notification timer elapsed, stopping call checking of call with token:" << _token;
    setChecking(false);
    Q_EMIT stopNotifying();
}

void CallStateChecker::slotStatusCheckTimerElapsed()
{
    // Don't run check if another check is still ongoing
    if (_stateCheckJob) {
        return;
    }

    startCallStateCheck();
}

bool CallStateChecker::isAccountServerVersion22OrLater() const
{
    if(!_accountState || !_accountState->account()) {
        return false;
    }

    const auto accountNcVersion = _accountState->account()->serverVersionInt();
    constexpr auto ncVersion22 = OCC::Account::makeServerVersion(22, 0, 0);

    return accountNcVersion >= ncVersion22;
}

void CallStateChecker::startCallStateCheck()
{
    // check connectivity and credentials
    if (!(_accountState && _accountState->isConnected() &&
          _accountState->account() && _accountState->account()->credentials() &&
          _accountState->account()->credentials()->ready())) {
        qCInfo(lcCallStateChecker, "Could not connect, can't check call state.");
        return;
    }

    // Check for token
    if(_token.isEmpty()) {
        qCInfo(lcCallStateChecker, "No call token set, can't check without it.");
        return;
    }

    qCInfo(lcCallStateChecker) << "Checking state of call with token: " << _token;

    const auto spreedPath = QStringLiteral("ocs/v2.php/apps/spreed/");
    const auto callApiPath = isAccountServerVersion22OrLater() ? QStringLiteral("api/v4/call/") : QStringLiteral("api/v1/call/");
    const QString callPath = spreedPath + callApiPath + _token; // Make sure it's a QString and not a QStringBuilder

    _stateCheckJob = new JsonApiJob(_accountState->account(), callPath, this);
    connect(_stateCheckJob.data(), &JsonApiJob::jsonReceived, this, &CallStateChecker::slotCallStateReceived);

    _stateCheckJob->setVerb(JsonApiJob::Verb::Get);
    _stateCheckJob->start();
}

void CallStateChecker::slotCallStateReceived(const QJsonDocument &json, const int statusCode)
{
    if (statusCode != successStatusCode) {
        qCInfo(lcCallStateChecker) << "Failed to retrieve call state data. Server returned status code: " << statusCode;
        return;
    }

    const auto participantsJsonArray = json.object().value("ocs").toObject().value("data").toArray();

    if (participantsJsonArray.empty()) {
        qCInfo(lcCallStateChecker, "Call has no participants and has therefore been abandoned.");
        Q_EMIT stopNotifying();
        setChecking(false);
        return;
    }

    for (const auto &participant : participantsJsonArray) {
        const auto participantDataObject = participant.toObject();
        const auto participantId = isAccountServerVersion22OrLater() ? participantDataObject.value("actorId").toString() : participantDataObject.value("userId").toString();

        if (participantId == _accountState->account()->davUser()) {
            qCInfo(lcCallStateChecker, "Found own account ID in participants list, meaning call has been joined.");
            Q_EMIT stopNotifying();
            setChecking(false);
            return;
        }
    }
}

}
