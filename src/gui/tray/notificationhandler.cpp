#include "notificationhandler.h"

#include "accountstate.h"
#include "capabilities.h"
#include "networkjobs.h"

#include "iconjob.h"

#include <QJsonDocument>
#include <QJsonObject>

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

    auto notifies = json.object().value("ocs").toObject().value("data").toArray();

    auto *ai = qvariant_cast<AccountState *>(sender()->property(propertyAccountStateC));

    ActivityList list;
    ActivityList callList;

    foreach (auto element, notifies) {
        auto json = element.toObject();
        auto a = Activity::fromActivityJson(json, ai->account());

        a._type = Activity::NotificationType;
        a._id = json.value("notification_id").toInt();

        if(json.contains("subjectRichParameters")) {
            const auto richParams = json.value("subjectRichParameters").toObject();
            const auto richParamsKeys = richParams.keys();
            for(const auto &key : richParamsKeys) {
                const auto parameterJsonObject = richParams.value(key).toObject();
                a._subjectRichParameters.insert(key, QVariant::fromValue(Activity::RichSubjectParameter{
                                                    parameterJsonObject.value(QStringLiteral("type")).toString(),
                                                    parameterJsonObject.value(QStringLiteral("id")).toString(),
                                                    parameterJsonObject.value(QStringLiteral("name")).toString(),
                                                    QString(),
                                                    QUrl()
                                                     }));
            }
        }

        if (json.contains("shouldNotify")) {
            a._shouldNotify = json.value("shouldNotify").toBool(true);
        }

        // 2 cases to consider:
        // 1. server == 24 & has Talk: object_type is chat/call/room & object_id contains conversationToken/messageId
        // 2. server < 24 & has Talk: object_type is chat/call/room & object_id contains _only_ conversationToken
        if (a._objectType == "chat" || a._objectType == "call" || a._objectType == "room") {
            const auto objectId = json.value("object_id").toString();
            const auto objectIdData = objectId.split("/");

            ActivityLink al;
            al._label = tr("Reply");
            al._verb = "REPLY";
            al._primary = true;

            a._talkNotificationData.conversationToken = objectIdData.first();

            if (a._objectType == "chat" && objectIdData.size() > 1) {
                a._talkNotificationData.messageId = objectIdData.last();
            } else {
                qCInfo(lcServerNotification) << "Replying directly to Talk conversation" << a._talkNotificationData.conversationToken << "will not be possible because the notification doesn't contain the message ID.";
            }

            if (a._subjectRichParameters.contains("user")) {

                // callback then it is the primary action
                if (a._objectType == "call") {
                    al._primary = false;
                }

                a._talkNotificationData.userAvatar = ai->account()->url().toString() + QStringLiteral("/index.php/avatar/") + a._subjectRichParameters["user"].value<Activity::RichSubjectParameter>().id + QStringLiteral("/128");
            }

            // We want to serve incoming call dialogs to the user for calls that
            if (a._objectType == "call" && a._dateTime.secsTo(QDateTime::currentDateTime()) < 120) {
                callList.append(a);
            }

            a._links.insert(al._primary? 0 : a._links.size(), al);
        }

        QUrl link(json.value("link").toString());
        if (!link.isEmpty()) {
            if (link.host().isEmpty()) {
                link.setScheme(ai->account()->url().scheme());
                link.setHost(ai->account()->url().host());
            }
            if (link.port() == -1) {
                link.setPort(ai->account()->url().port());
            }
        }
        a._link = link;

        list.append(a);
    }
    emit newNotificationList(list);
    emit newIncomingCallsList(callList);
    emit jobFinished();

    deleteLater();
}
}
