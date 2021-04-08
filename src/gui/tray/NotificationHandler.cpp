#include "NotificationHandler.h"

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
QMap<int, QByteArray> ServerNotificationHandler::iconCache;

ServerNotificationHandler::ServerNotificationHandler(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
{
}

void ServerNotificationHandler::slotFetchNotifications()
{
    // check connectivity and credentials
    if (!(_accountState && _accountState->isConnected() && _accountState->account() && _accountState->account()->credentials() && _accountState->account()->credentials()->ready())) {
        deleteLater();
        return;
    }
    // check if the account has notifications enabled. If the capabilities are
    // not yet valid, its assumed that notifications are available.
    if (_accountState->account()->capabilities().isValid()) {
        if (!_accountState->account()->capabilities().notificationsAvailable()) {
            qCInfo(lcServerNotification) << "Account" << _accountState->account()->displayName() << "does not have notifications enabled.";
            deleteLater();
            return;
        }
    }

    // if the previous notification job has finished, start next.
    _notificationJob = new JsonApiJob(_accountState->account(), notificationsPath, this);
    QObject::connect(_notificationJob.data(), &JsonApiJob::jsonReceived,
        this, &ServerNotificationHandler::slotNotificationsReceived);
    QObject::connect(_notificationJob.data(), &JsonApiJob::etagResponseHeaderReceived,
        this, &ServerNotificationHandler::slotEtagResponseHeaderReceived);
    QObject::connect(_notificationJob.data(), &JsonApiJob::allowDesktopNotificationsChanged,
            this, &ServerNotificationHandler::slotAllowDesktopNotificationsChanged);
    _notificationJob->setProperty(propertyAccountStateC, QVariant::fromValue<AccountState *>(_accountState));
    _notificationJob->addRawHeader("If-None-Match", _accountState->notificationsEtagResponseHeader());
    _notificationJob->start();
}

void ServerNotificationHandler::slotEtagResponseHeaderReceived(const QByteArray &value, int statusCode)
{
    if (statusCode == successStatusCode) {
        qCWarning(lcServerNotification) << "New Notification ETag Response Header received " << value;
        auto *account = qvariant_cast<AccountState *>(sender()->property(propertyAccountStateC));
        account->setNotificationsEtagResponseHeader(value);
    }
}

void ServerNotificationHandler::slotAllowDesktopNotificationsChanged(bool isAllowed)
{
    auto *account = qvariant_cast<AccountState *>(sender()->property(propertyAccountStateC));
    if (account != nullptr) {
       account->setDesktopNotificationsAllowed(isAllowed);
    }
}

void ServerNotificationHandler::slotIconDownloaded(QByteArray iconData)
{
    iconCache.insert(sender()->property("activityId").toInt(),iconData);
}

void ServerNotificationHandler::slotNotificationsReceived(const QJsonDocument &json, int statusCode)
{
    if (statusCode != successStatusCode && statusCode != notModifiedStatusCode) {
        qCWarning(lcServerNotification) << "Notifications failed with status code " << statusCode;
        deleteLater();
        return;
    }

    if (statusCode == notModifiedStatusCode) {
        qCWarning(lcServerNotification) << "Status code " << statusCode << " Not Modified - No new notifications.";
        deleteLater();
        return;
    }

    auto notifies = json.object().value("ocs").toObject().value("data").toArray();

    auto *ai = qvariant_cast<AccountState *>(sender()->property(propertyAccountStateC));

    ActivityList list;

    foreach (auto element, notifies) {
        Activity a;
        auto json = element.toObject();
        a._type = Activity::NotificationType;
        a._accName = ai->account()->displayName();
        a._id = json.value("notification_id").toInt();

        //need to know, specially for remote_share
        a._objectType = json.value("object_type").toString();
        a._status = 0;

        a._subject = json.value("subject").toString();
        a._message = json.value("message").toString();
        a._icon = json.value("icon").toString();

        if (!a._icon.isEmpty()) {
            auto *iconJob = new IconJob(QUrl(a._icon));
            iconJob->setProperty("activityId", a._id);
            connect(iconJob, &IconJob::jobFinished, this, &ServerNotificationHandler::slotIconDownloaded);
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
        a._dateTime = QDateTime::fromString(json.value("datetime").toString(), Qt::ISODate);

        auto actions = json.value("actions").toArray();
        foreach (auto action, actions) {
            auto actionJson = action.toObject();
            ActivityLink al;
            al._label = QUrl::fromPercentEncoding(actionJson.value("label").toString().toUtf8());
            al._link = actionJson.value("link").toString();
            al._verb = actionJson.value("type").toString().toUtf8();
            al._primary = actionJson.value("primary").toBool();

            a._links.append(al);
        }

        // Add another action to dismiss notification on server
        // https://github.com/owncloud/notifications/blob/master/docs/ocs-endpoint-v1.md#deleting-a-notification-for-a-user
        ActivityLink al;
        al._label = tr("Dismiss");
        al._link = Utility::concatUrlPath(ai->account()->url(), notificationsPath + "/" + QString::number(a._id)).toString();
        al._verb = "DELETE";
        al._primary = false;
        a._links.append(al);

        list.append(a);
    }
    emit newNotificationList(list);

    deleteLater();
}
}
