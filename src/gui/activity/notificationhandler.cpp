/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "notificationhandler.h"

#include "accountstate.h"
#include "capabilities.h"
#include "networkjobs.h"

#include "iconjob.h"

#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace OCC {

Q_LOGGING_CATEGORY(lcServerNotification, "nextcloud.gui.servernotification", QtInfoMsg)

const QString notificationsPath = QLatin1String("ocs/v2.php/apps/notifications/api/v2/notifications");
const char propertyAccountStateC[] = "oc_account_state";
const int successStatusCode = 200;
const int notModifiedStatusCode = 304;

ServerNotificationHandler::ServerNotificationHandler(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
{
}

bool ServerNotificationHandler::startFetchNotifications()
{
    // check connectivity and credentials
    if (!(_accountState && _accountState->isConnected() && _accountState->account() && _accountState->account()->credentials() && _accountState->account()->credentials()->ready())) {
        deleteLater();
        return false;
    }
    // check if the account has notifications enabled. If the capabilities are
    // not yet valid, its assumed that notifications are available.
    if (_accountState->account()->capabilities().isValid()) {
        if (!_accountState->account()->capabilities().notificationsAvailable()) {
            qCInfo(lcServerNotification) << "Account" << _accountState->account()->displayName() << "does not have notifications enabled.";
            deleteLater();
            return false;
        }
    }

    // if the previous notification job has finished, start next.
    _notificationJob = new JsonApiJob(_accountState->account(), notificationsPath, this);
    QObject::connect(_notificationJob.data(), &JsonApiJob::jsonReceived,
        this, &ServerNotificationHandler::slotNotificationsReceived);
    QObject::connect(_notificationJob.data(), &JsonApiJob::etagResponseHeaderReceived,
        this, &ServerNotificationHandler::slotEtagResponseHeaderReceived);
    _notificationJob->setProperty(propertyAccountStateC, QVariant::fromValue<AccountState *>(_accountState));
    _notificationJob->addRawHeader("If-None-Match", _accountState->notificationsEtagResponseHeader());
    _notificationJob->start();
    return true;
}

void ServerNotificationHandler::slotEtagResponseHeaderReceived(const QByteArray &value, int statusCode)
{
    if (statusCode == successStatusCode) {
        qCInfo(lcServerNotification) << "New Notification ETag Response Header received " << value;
        auto *account = qvariant_cast<AccountState *>(sender()->property(propertyAccountStateC));
        account->setNotificationsEtagResponseHeader(value);
    }
}

void ServerNotificationHandler::slotNotificationsReceived(const QJsonDocument &json, int statusCode)
{
    if (statusCode != successStatusCode && statusCode != notModifiedStatusCode) {
        qCWarning(lcServerNotification) << "Notifications failed with status code " << statusCode;
        deleteLater();
        emit jobFinished();
        return;
    }

    if (statusCode == notModifiedStatusCode) {
        qCInfo(lcServerNotification) << "Status code " << statusCode << " Not Modified - No new notifications.";
        deleteLater();
        emit jobFinished();
        return;
    }

    // In theory the server should five us a 304 Not Modified if there are no new notifications.
    // But in practice, the server doesn't always do that. So we need to compare the ETag headers.
    const auto postFetchEtagHeader = _accountState->notificationsEtagResponseHeader();
    if (!_preFetchEtagHeader.isEmpty() || _preFetchEtagHeader == postFetchEtagHeader) {
        qCInfo(lcServerNotification) << "Notifications ETag header is the same as before, no new notifications.";
        deleteLater();
        emit jobFinished();
        return;
    }
    _preFetchEtagHeader = postFetchEtagHeader;

    const auto notifies = json.object().value("ocs"_L1).toObject().value("data"_L1).toArray();

    auto accountState = qvariant_cast<AccountState *>(sender()->property(propertyAccountStateC));

    ActivityList list;
    ActivityList callList;

    for (const auto &element : notifies) {
        const auto json = element.toObject();
        auto activity = Activity::fromActivityJson(json, accountState->account());

        activity._type = Activity::NotificationType;
        activity._id = json.value("notification_id"_L1).toInteger();

        if (json.contains("subjectRichParameters")) {
            const auto richParams = json.value("subjectRichParameters"_L1).toObject();
            const auto richParamsKeys = richParams.keys();
            for(const auto &key : richParamsKeys) {
                const auto parameterJsonObject = richParams.value(key).toObject();
                activity._subjectRichParameters.insert(key, QVariant::fromValue(Activity::RichSubjectParameter{
                                                    parameterJsonObject.value("type"_L1).toString(),
                                                    parameterJsonObject.value("id"_L1).toString(),
                                                    parameterJsonObject.value("name"_L1).toString(),
                                                    QString(),
                                                    QUrl()
                                                     }));
            }
        }

        if (json.contains("shouldNotify"_L1)) {
            activity._shouldNotify = json.value("shouldNotify"_L1).toBool(true);
        }

        // 2 cases to consider:
        // 1. server == 24 & has Talk: object_type is chat/call/room & object_id contains conversationToken/messageId
        // 2. server < 24 & has Talk: object_type is chat/call/room & object_id contains _only_ conversationToken
        if (activity._objectType == "chat"_L1 || activity._objectType == "call"_L1 || activity._objectType == "room"_L1) {
            const auto objectId = json.value("object_id"_L1).toString();
            const auto objectIdData = objectId.split(u'/');

            ActivityLink link;
            link._label = tr("Reply");
            link._verb = "REPLY";
            link._primary = true;

            activity._talkNotificationData.conversationToken = objectIdData.first();

            if (activity._objectType == "chat" && objectIdData.size() > 1) {
                activity._talkNotificationData.messageId = objectIdData.last();
            } else {
                qCInfo(lcServerNotification) << "Replying directly to Talk conversation" << activity._talkNotificationData.conversationToken
                                             << "will not be possible because the notification doesn't contain the message ID.";
            }

            if (activity._subjectRichParameters.contains("user"_L1)) {

                // callback then it is the primary action
                if (activity._objectType == "call"_L1) {
                    link._primary = false;
                }

                activity._talkNotificationData.userAvatar = accountState->account()->url().toString()
                    + QStringLiteral("/index.php/avatar/")
                    + activity._subjectRichParameters["user"_L1].value<Activity::RichSubjectParameter>().id
                    + QStringLiteral("/128");
            }

            // We want to serve incoming call dialogs to the user for calls that
            if (activity._objectType == "call"_L1 && activity._dateTime.secsTo(QDateTime::currentDateTime()) < 120) {
                callList.append(activity);
            }

            activity._links.insert(link._primary? 0 : activity._links.size(), link);
        }

        // e.g. announcement
        if (activity._objectType != "remote_share"_L1 && activity._links.isEmpty()) {
            ActivityLink link;
            link._label = tr("Dismiss");
            link._verb = "DELETE";
            link._primary = true;
            activity._links.append(link);
        }

        QUrl url(json.value("link"_L1).toString());
        if (!url.isEmpty()) {
            if (url.host().isEmpty()) {
                url.setScheme(accountState->account()->url().scheme());
                url.setHost(accountState->account()->url().host());
            }
            if (url.port() == -1) {
                url.setPort(accountState->account()->url().port());
            }
        }
        activity._link = url;

        list.append(activity);
    }
    emit newNotificationList(list);
    emit newIncomingCallsList(callList);
    emit jobFinished();

    deleteLater();
}
}
