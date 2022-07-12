/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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

#include "ocsuserstatusconnector.h"
#include "account.h"
#include "userstatusconnector.h"

#include <networkjobs.h>

#include <QDateTime>
#include <QtGlobal>
#include <QJsonDocument>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <qdatetime.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qloggingcategory.h>

namespace {

Q_LOGGING_CATEGORY(lcOcsUserStatusConnector, "nextcloud.gui.ocsuserstatusconnector", QtInfoMsg)

OCC::UserStatus::OnlineStatus stringToUserOnlineStatus(const QString &status)
{
    // it needs to match the Status enum
    const QHash<QString, OCC::UserStatus::OnlineStatus> preDefinedStatus {
        { "online", OCC::UserStatus::OnlineStatus::Online },
        { "dnd", OCC::UserStatus::OnlineStatus::DoNotDisturb },
        { "away", OCC::UserStatus::OnlineStatus::Away },
        { "offline", OCC::UserStatus::OnlineStatus::Offline },
        { "invisible", OCC::UserStatus::OnlineStatus::Invisible }
    };

    // api should return invisible, dnd,... toLower() it is to make sure
    // it matches _preDefinedStatus, otherwise the default is online (0)
    return preDefinedStatus.value(status.toLower(), OCC::UserStatus::OnlineStatus::Online);
}

QString onlineStatusToString(OCC::UserStatus::OnlineStatus status)
{
    switch (status) {
    case OCC::UserStatus::OnlineStatus::Online:
        return QStringLiteral("online");
    case OCC::UserStatus::OnlineStatus::DoNotDisturb:
        return QStringLiteral("dnd");
    case OCC::UserStatus::OnlineStatus::Away:
        return QStringLiteral("offline");
    case OCC::UserStatus::OnlineStatus::Offline:
        return QStringLiteral("offline");
    case OCC::UserStatus::OnlineStatus::Invisible:
        return QStringLiteral("invisible");
    }
    return QStringLiteral("online");
}

OCC::Optional<OCC::ClearAt> jsonExtractClearAt(QJsonObject jsonObject)
{
    OCC::Optional<OCC::ClearAt> clearAt {};
    if (jsonObject.contains("clearAt") && !jsonObject.value("clearAt").isNull()) {
        OCC::ClearAt clearAtValue;
        clearAtValue._type = OCC::ClearAtType::Timestamp;
        clearAtValue._timestamp = jsonObject.value("clearAt").toInt();
        clearAt = clearAtValue;
    }
    return clearAt;
}

OCC::UserStatus jsonExtractUserStatus(QJsonObject json)
{
    const auto clearAt = jsonExtractClearAt(json);

    const OCC::UserStatus userStatus(json.value("messageId").toString(),
        json.value("message").toString().trimmed(),
        json.value("icon").toString().trimmed(), stringToUserOnlineStatus(json.value("status").toString()),
        json.value("messageIsPredefined").toBool(false), clearAt);

    return userStatus;
}

OCC::UserStatus jsonToUserStatus(const QJsonDocument &json)
{
    const QJsonObject defaultValues {
        { "icon", "" },
        { "message", "" },
        { "status", "online" },
        { "messageIsPredefined", "false" },
        { "statusIsUserDefined", "false" }
    };
    const auto retrievedData = json.object().value("ocs").toObject().value("data").toObject(defaultValues);
    return jsonExtractUserStatus(retrievedData);
}

quint64 clearAtEndOfToTimestamp(const OCC::ClearAt &clearAt)
{
    Q_ASSERT(clearAt._type == OCC::ClearAtType::EndOf);

    if (clearAt._endof == "day") {
        return QDate::currentDate().addDays(1).startOfDay().toSecsSinceEpoch();
    } else if (clearAt._endof == "week") {
        const auto days = Qt::Sunday - QDate::currentDate().dayOfWeek();
        return QDate::currentDate().addDays(days + 1).startOfDay().toSecsSinceEpoch();
    }
    qCWarning(lcOcsUserStatusConnector) << "Can not handle clear at endof day type" << clearAt._endof;
    return QDateTime::currentDateTime().toSecsSinceEpoch();
}

quint64 clearAtPeriodToTimestamp(const OCC::ClearAt &clearAt)
{
    return QDateTime::currentDateTime().addSecs(clearAt._period).toSecsSinceEpoch();
}

quint64 clearAtToTimestamp(const OCC::ClearAt &clearAt)
{
    switch (clearAt._type) {
    case OCC::ClearAtType::Period: {
        return clearAtPeriodToTimestamp(clearAt);
    }

    case OCC::ClearAtType::EndOf: {
        return clearAtEndOfToTimestamp(clearAt);
    }

    case OCC::ClearAtType::Timestamp: {
        return clearAt._timestamp;
    }
    }

    return 0;
}

quint64 clearAtToTimestamp(const OCC::Optional<OCC::ClearAt> &clearAt)
{
    if (clearAt) {
        return clearAtToTimestamp(*clearAt);
    }
    return 0;
}

OCC::Optional<OCC::ClearAt> jsonToClearAt(QJsonObject jsonObject)
{
    OCC::Optional<OCC::ClearAt> clearAt;

    if (jsonObject.value("clearAt").isObject() && !jsonObject.value("clearAt").isNull()) {
        OCC::ClearAt clearAtValue;
        const auto clearAtObject = jsonObject.value("clearAt").toObject();
        const auto typeValue = clearAtObject.value("type").toString("period");
        if (typeValue == "period") {
            const auto timeValue = clearAtObject.value("time").toInt(0);
            clearAtValue._type = OCC::ClearAtType::Period;
            clearAtValue._period = timeValue;
        } else if (typeValue == "end-of") {
            const auto timeValue = clearAtObject.value("time").toString("day");
            clearAtValue._type = OCC::ClearAtType::EndOf;
            clearAtValue._endof = timeValue;
        } else {
            qCWarning(lcOcsUserStatusConnector) << "Can not handle clear type value" << typeValue;
        }
        clearAt = clearAtValue;
    }

    return clearAt;
}

OCC::UserStatus jsonToUserStatus(QJsonObject jsonObject)
{
    const auto clearAt = jsonToClearAt(jsonObject);

    OCC::UserStatus userStatus(
        jsonObject.value("id").toString("no-id"),
        jsonObject.value("message").toString("No message"),
        jsonObject.value("icon").toString("no-icon"),
        OCC::UserStatus::OnlineStatus::Online,
        true,
        clearAt);

    return userStatus;
}

QVector<OCC::UserStatus> jsonToPredefinedStatuses(QJsonArray jsonDataArray)
{
    QVector<OCC::UserStatus> statuses;
    for (const auto &jsonEntry : jsonDataArray) {
        Q_ASSERT(jsonEntry.isObject());
        if (!jsonEntry.isObject()) {
            continue;
        }
        statuses.append(jsonToUserStatus(jsonEntry.toObject()));
    }

    return statuses;
}


const QString baseUrl("/ocs/v2.php/apps/user_status/api/v1");
const QString userStatusBaseUrl = baseUrl + QStringLiteral("/user_status");
}

namespace OCC {

OcsUserStatusConnector::OcsUserStatusConnector(AccountPtr account, QObject *parent)
    : UserStatusConnector(parent)
    , _account(account)
{
    Q_ASSERT(_account);
    _userStatusSupported = _account->capabilities().userStatus();
    _userStatusEmojisSupported = _account->capabilities().userStatusSupportsEmoji();
}

void OcsUserStatusConnector::fetchUserStatus()
{
    qCDebug(lcOcsUserStatusConnector) << "Try to fetch user status";

    if (!_userStatusSupported) {
        qCDebug(lcOcsUserStatusConnector) << "User status not supported";
        emit error(Error::UserStatusNotSupported);
        return;
    }

    startFetchUserStatusJob();
}

void OcsUserStatusConnector::startFetchUserStatusJob()
{
    if (_getUserStatusJob) {
        qCDebug(lcOcsUserStatusConnector) << "Get user status job is already running.";
        return;
    }

    _getUserStatusJob = new JsonApiJob(_account, userStatusBaseUrl, this);
    connect(_getUserStatusJob, &JsonApiJob::jsonReceived, this, &OcsUserStatusConnector::onUserStatusFetched);
    _getUserStatusJob->start();
}

void OcsUserStatusConnector::onUserStatusFetched(const QJsonDocument &json, int statusCode)
{
    logResponse("user status fetched", json, statusCode);

    if (statusCode != 200) {
        qCInfo(lcOcsUserStatusConnector) << "Slot fetch UserStatus finished with status code" << statusCode;
        emit error(Error::CouldNotFetchUserStatus);
        return;
    }

    const auto oldOnlineState = _userStatus.state();
    _userStatus = jsonToUserStatus(json);

    emit userStatusFetched(_userStatus);

    if (oldOnlineState != _userStatus.state()) {
        emit serverUserStatusChanged();
    }
}

void OcsUserStatusConnector::startFetchPredefinedStatuses()
{
    if (_getPredefinedStausesJob) {
        qCDebug(lcOcsUserStatusConnector) << "Get predefined statuses job is already running";
        return;
    }

    _getPredefinedStausesJob = new JsonApiJob(_account,
        baseUrl + QStringLiteral("/predefined_statuses"), this);
    connect(_getPredefinedStausesJob, &JsonApiJob::jsonReceived, this,
        &OcsUserStatusConnector::onPredefinedStatusesFetched);
    _getPredefinedStausesJob->start();
}

void OcsUserStatusConnector::fetchPredefinedStatuses()
{
    if (!_userStatusSupported) {
        emit error(Error::UserStatusNotSupported);
        return;
    }
    startFetchPredefinedStatuses();
}

void OcsUserStatusConnector::onPredefinedStatusesFetched(const QJsonDocument &json, int statusCode)
{
    logResponse("predefined statuses", json, statusCode);

    if (statusCode != 200) {
        qCInfo(lcOcsUserStatusConnector) << "Slot predefined user statuses finished with status code" << statusCode;
        emit error(Error::CouldNotFetchPredefinedUserStatuses);
        return;
    }
    const auto jsonData = json.object().value("ocs").toObject().value("data");
    Q_ASSERT(jsonData.isArray());
    if (!jsonData.isArray()) {
        return;
    }
    const auto statuses = jsonToPredefinedStatuses(jsonData.toArray());
    emit predefinedStatusesFetched(statuses);
}

void OcsUserStatusConnector::logResponse(const QString &message, const QJsonDocument &json, int statusCode)
{
    qCDebug(lcOcsUserStatusConnector) << "Response from:" << message << "Status:" << statusCode << "Json:" << json;
}

void OcsUserStatusConnector::setUserStatusOnlineStatus(UserStatus::OnlineStatus onlineStatus)
{
    _setOnlineStatusJob = new JsonApiJob(_account,
        userStatusBaseUrl + QStringLiteral("/status"), this);
    _setOnlineStatusJob->setVerb(JsonApiJob::Verb::Put);
    // Set body
    QJsonObject dataObject;
    dataObject.insert("statusType", onlineStatusToString(onlineStatus));
    QJsonDocument body;
    body.setObject(dataObject);
    _setOnlineStatusJob->setBody(body);
    connect(_setOnlineStatusJob, &JsonApiJob::jsonReceived, this, &OcsUserStatusConnector::onUserStatusOnlineStatusSet);
    _setOnlineStatusJob->start();
}

void OcsUserStatusConnector::setUserStatusMessagePredefined(const UserStatus &userStatus)
{
    Q_ASSERT(userStatus.messagePredefined());
    if (!userStatus.messagePredefined()) {
        return;
    }

    _setMessageJob = new JsonApiJob(_account, userStatusBaseUrl + QStringLiteral("/message/predefined"), this);
    _setMessageJob->setVerb(JsonApiJob::Verb::Put);
    // Set body
    QJsonObject dataObject;
    dataObject.insert("messageId", userStatus.id());
    if (userStatus.clearAt()) {
        dataObject.insert("clearAt", static_cast<int>(clearAtToTimestamp(userStatus.clearAt())));
    } else {
        dataObject.insert("clearAt", QJsonValue());
    }
    QJsonDocument body;
    body.setObject(dataObject);
    _setMessageJob->setBody(body);
    connect(_setMessageJob, &JsonApiJob::jsonReceived, this, &OcsUserStatusConnector::onUserStatusMessageSet);
    _setMessageJob->start();
}

void OcsUserStatusConnector::setUserStatusMessageCustom(const UserStatus &userStatus)
{
    Q_ASSERT(!userStatus.messagePredefined());
    if (userStatus.messagePredefined()) {
        return;
    }

    if (!_userStatusEmojisSupported) {
        emit error(Error::EmojisNotSupported);
        return;
    }
    _setMessageJob = new JsonApiJob(_account, userStatusBaseUrl + QStringLiteral("/message/custom"), this);
    _setMessageJob->setVerb(JsonApiJob::Verb::Put);
    // Set body
    QJsonObject dataObject;
    dataObject.insert("statusIcon", userStatus.icon());
    dataObject.insert("message", userStatus.message());
    const auto clearAt = userStatus.clearAt();
    if (clearAt) {
        dataObject.insert("clearAt", static_cast<int>(clearAtToTimestamp(*clearAt)));
    } else {
        dataObject.insert("clearAt", QJsonValue());
    }
    QJsonDocument body;
    body.setObject(dataObject);
    _setMessageJob->setBody(body);
    connect(_setMessageJob, &JsonApiJob::jsonReceived, this, &OcsUserStatusConnector::onUserStatusMessageSet);
    _setMessageJob->start();
}

void OcsUserStatusConnector::setUserStatusMessage(const UserStatus &userStatus)
{
    if (userStatus.messagePredefined()) {
        setUserStatusMessagePredefined(userStatus);
        return;
    }
    setUserStatusMessageCustom(userStatus);
}

void OcsUserStatusConnector::setUserStatus(const UserStatus &userStatus)
{
    if (!_userStatusSupported) {
        emit error(Error::UserStatusNotSupported);
        return;
    }

    if (_setOnlineStatusJob || _setMessageJob) {
        qCDebug(lcOcsUserStatusConnector) << "Set online status job or set message job are already running.";
        return;
    }

    if (userStatus.state() != _userStatus.state()) {
        setUserStatusOnlineStatus(userStatus.state());
    }
    setUserStatusMessage(userStatus);
}

void OcsUserStatusConnector::onUserStatusOnlineStatusSet(const QJsonDocument &json, int statusCode)
{
    logResponse("Online status set", json, statusCode);

    if (statusCode != 200) {
        emit error(Error::CouldNotSetUserStatus);
        return;
    }

    const auto oldOnlineState = _userStatus.state();
    _userStatus.setState(jsonToUserStatus(json).state());

    emit userStatusSet();

    if (oldOnlineState != _userStatus.state()) {
        emit serverUserStatusChanged();
    }
}

void OcsUserStatusConnector::onUserStatusMessageSet(const QJsonDocument &json, int statusCode)
{
    logResponse("Message set", json, statusCode);

    if (statusCode != 200) {
        emit error(Error::CouldNotSetUserStatus);
        return;
    }

    // We fetch the user status again because json does not contain
    // the new message when user status was set from a predefined
    // message
    fetchUserStatus();

    emit userStatusSet();
}

void OcsUserStatusConnector::clearMessage()
{
    _clearMessageJob = new JsonApiJob(_account, userStatusBaseUrl + QStringLiteral("/message"));
    _clearMessageJob->setVerb(JsonApiJob::Verb::Delete);
    connect(_clearMessageJob, &JsonApiJob::jsonReceived, this, &OcsUserStatusConnector::onMessageCleared);
    _clearMessageJob->start();
}

UserStatus OcsUserStatusConnector::userStatus() const
{
    return _userStatus;
}

void OcsUserStatusConnector::onMessageCleared(const QJsonDocument &json, int statusCode)
{
    logResponse("Message cleared", json, statusCode);

    if (statusCode != 200) {
        emit error(Error::CouldNotClearMessage);
        return;
    }

    const auto onlineState = _userStatus.state();

    _userStatus = {};
    _userStatus.setState(onlineState);
    emit messageCleared();
}
}
