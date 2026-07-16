/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "notificationmanager.h"

#include "notifications/notificationconfirmjob.h"
#include "notifications/servernotificationhandler.h"
#include "notifications/talkreply.h"

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "configfile.h"
#include "common/utility.h"
#include "folder.h"
#include "guiutility.h"
#include "logger.h"
#include "ocsjob.h"
#include "systray.h"
#include "tray/activitylistmodel.h"
#include "userstatusconnector.h"

#if defined(Q_OS_MACOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14 && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
#include "notifications/macnotificationcenter.h"
#endif

#include <QDateTime>
#include <QHash>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>

#include <algorithm>
#include <iterator>
#include <utility>

namespace {

constexpr auto debugCallNotificationEnvVar = "NEXTCLOUD_DEBUG_CALL_NOTIFICATION";
constexpr auto debugCallNotificationAvatarEnvVar = "NEXTCLOUD_DEBUG_CALL_NOTIFICATION_AVATAR_URL";
constexpr qint64 clearNotificationHistoryIntervalMsecs = 60 * 60 * 1000;

QHash<QString, QPointer<OCC::NotificationManager>> &notificationManagers()
{
    static auto managers = QHash<QString, QPointer<OCC::NotificationManager>>{};
    return managers;
}

bool showDebugCallNotification(const OCC::AccountStatePtr &accountState)
{
    if (!qEnvironmentVariableIsSet(debugCallNotificationEnvVar)) {
        return false;
    }

    const auto systray = OCC::Systray::instance();
    if (!systray || !accountState || !accountState->account()) {
        return true;
    }

    auto activity = OCC::Activity{};
    activity._id = -QDateTime::currentMSecsSinceEpoch();
    activity._objectType = QStringLiteral("call");
    activity._subject = QStringLiteral("Iva Horn would like to talk with you");
    activity._shouldNotify = true;
    activity._dateTime = QDateTime::currentDateTime();
    activity._accName = accountState->account()->displayName();
    activity._talkNotificationData.conversationToken = QStringLiteral("debug-call");

    const auto avatarUrl = qEnvironmentVariable(debugCallNotificationAvatarEnvVar);
    if (!avatarUrl.isEmpty()) {
        activity._talkNotificationData.userAvatar = avatarUrl;
    } else if (!accountState->account()->url().isEmpty() && !accountState->account()->davUser().isEmpty()) {
        activity._talkNotificationData.userAvatar = accountState->account()->url().toString()
            + QStringLiteral("/index.php/avatar/")
            + accountState->account()->davUser()
            + QStringLiteral("/128");
    }

    auto answer = OCC::ActivityLink{};
    answer._label = QObject::tr("Answer");
    answer._verb = "WEB";
    answer._link = accountState->account()->url().toString();
    answer._primary = true;
    activity._links.append(answer);

    systray->createCallDialog(activity, accountState);
    return true;
}

}

namespace OCC {

NotificationManager::NotificationManager(AccountStatePtr accountState, ActivityListModel *activityModel, QObject *parent)
    : QObject(parent)
    , _accountState(std::move(accountState))
    , _activityModel(activityModel)
{
    Q_ASSERT(_accountState && _accountState->account());
    Q_ASSERT(_activityModel);

    if (_accountState && _accountState->account()) {
        notificationManagers().insert(_accountState->account()->id(), this);
    }

    if (_activityModel) {
        connect(_activityModel, &ActivityListModel::sendNotificationRequest,
            this, &NotificationManager::sendNotificationRequest);
#if defined(Q_OS_MACOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14 && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
        connect(_activityModel, &ActivityListModel::notificationRemoved, this, [this](const qint64 notificationId) {
            MacNotificationCenter::removeServerNotification(_accountState->account()->id(), notificationId);
        });
#endif
    }
}

NotificationManager::~NotificationManager()
{
    if (!_accountState || !_accountState->account()) {
        return;
    }

    const auto accountId = _accountState->account()->id();
    if (notificationManagers().value(accountId) == this) {
        notificationManagers().remove(accountId);
    }
}

NotificationManager *NotificationManager::forAccountId(const QString &accountId)
{
    return notificationManagers().value(accountId);
}

void NotificationManager::checkNotifiedNotifications()
{
    if (_notificationHistoryTimer.elapsed() > clearNotificationHistoryIntervalMsecs) {
        _notifiedNotifications.clear();
    }
}

bool NotificationManager::notificationAlreadyShown(const qint64 notificationId)
{
    checkNotifiedNotifications();
    return _notifiedNotifications.contains(notificationId);
}

bool NotificationManager::canShowNotification(const qint64 notificationId)
{
    const auto cfg = ConfigFile{};
    return _accountState
        && cfg.optionalServerNotifications()
        && _accountState->isDesktopNotificationsAllowed()
        && !notificationAlreadyShown(notificationId);
}

void NotificationManager::showNotification(const QString &title, const QString &message, const qint64 notificationId)
{
    if (!canShowNotification(notificationId)) {
        return;
    }

    _notifiedNotifications.insert(notificationId);
    Logger::instance()->postGuiLog(title, message);
    _notificationHistoryTimer.start();
}

void NotificationManager::showNotification(const Activity &activity)
{
    const auto notificationId = activity._id;
    const auto message = AccountManager::instance()->accounts().count() == 1 ? QString{} : activity._accName;

    if (!activity._links.isEmpty() && _activityModel) {
        _activityModel->addNotificationToActivityList(activity);
    }

    showNotification(activity._subject, message, notificationId);
}

void NotificationManager::showServerNotifications(const ActivityList &activities)
{
#if defined(Q_OS_MACOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14 && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
    auto playSound = true;
    for (const auto &activity : activities) {
        MacNotificationCenter::sendServerNotification(activity, _accountState, playSound);
        playSound = false;
    }
#else
    if (activities.size() == 1) {
        const auto &activity = activities.constFirst();
        if (activity._objectType == QStringLiteral("chat")) {
            Logger::instance()->postGuiLog(activity._subject, activity._message);
        } else {
            const auto multipleAccounts = AccountManager::instance()->accounts().size() > 1;
            const auto message = multipleAccounts ? activity._accName : QString{};
            Logger::instance()->postGuiLog(activity._subject, message);
        }
        return;
    }

    const auto subject = tr("%n notification(s)", nullptr, activities.size());
    const auto multipleAccounts = AccountManager::instance()->accounts().size() > 1;
    const auto message = multipleAccounts ? activities.constFirst()._accName : QString{};
    Logger::instance()->postGuiLog(subject, message);
#endif
}

void NotificationManager::buildNotificationDisplay(const ActivityList &list)
{
    if (!_activityModel) {
        return;
    }

    _activityModel->removeOutdatedNotifications(list);
#if defined(Q_OS_MACOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14 && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
    auto activeNotificationIds = QSet<qint64>{};
    activeNotificationIds.reserve(list.size());
    for (const auto &activity : list) {
        activeNotificationIds.insert(activity._id);
    }
    MacNotificationCenter::reconcileServerNotifications(_accountState->account()->id(), activeNotificationIds);
#endif

    const auto cfg = ConfigFile{};
    if (!_accountState
        || !cfg.optionalServerNotifications()
        || !_accountState->isDesktopNotificationsAllowed()) {
        return;
    }

    checkNotifiedNotifications();
    const auto showChatNotifications = cfg.showChatNotifications();

    auto toNotifyList = ActivityList{};
    std::copy_if(list.cbegin(), list.cend(), std::back_inserter(toNotifyList), [this, showChatNotifications](const Activity &activity) {
        if (!activity._shouldNotify) {
            qCDebug(lcActivity).nospace() << "No notification should be sent for activity with id=" << activity._id
                                          << " objectType=" << activity._objectType;
            return false;
        }
        if (activity._objectType == QStringLiteral("chat") && !showChatNotifications) {
            qCDebug(lcActivity).nospace() << "Chat notification disabled for activity with id=" << activity._id;
            return false;
        }
        if (_notifiedNotifications.contains(activity._id)) {
            qCInfo(lcActivity).nospace() << "Ignoring already notified activity with id=" << activity._id
                                         << " objectType=" << activity._objectType;
            return false;
        }
        return true;
    });

    if (toNotifyList.isEmpty()) {
        return;
    }

    for (const auto &activity : std::as_const(toNotifyList)) {
        _notifiedNotifications.insert(activity._id);
        _activityModel->addNotificationToActivityList(activity);
    }
    _notificationHistoryTimer.start();

    showServerNotifications(toNotifyList);
}

void NotificationManager::buildIncomingCallDialogs(const ActivityList &list)
{
    const auto cfg = ConfigFile{};
    const auto userStatus = _accountState->account()->userStatusConnector()->userStatus().state();
    if (userStatus == UserStatus::OnlineStatus::DoNotDisturb
        || !cfg.optionalServerNotifications()
        || !cfg.showCallNotifications()
        || !_accountState->isDesktopNotificationsAllowed()) {
        return;
    }

    const auto systray = Systray::instance();
    if (!systray) {
        qCWarning(lcActivity) << "No systray instance available, can not notify about new calls";
        return;
    }

    for (const auto &activity : list) {
        if (!activity._shouldNotify) {
            qCDebug(lcActivity).nospace() << "No notification should be sent for activity with id=" << activity._id
                                          << " objectType=" << activity._objectType;
            continue;
        }
        systray->createCallDialog(activity, _accountState);
    }
}

void NotificationManager::refresh()
{
    static auto debugCallNotificationShown = false;
    if (!debugCallNotificationShown) {
        debugCallNotificationShown = showDebugCallNotification(_accountState);
    }

    if (_notificationRequestsRunning != 0) {
        qCWarning(lcActivity) << "Notification request counter not zero.";
        return;
    }
    if (_isNotificationFetchRunning) {
        qCDebug(lcActivity) << "Notification fetch is already running.";
        return;
    }

    auto * const handler = new ServerNotificationHandler(_accountState.data());
    connect(handler, &ServerNotificationHandler::newNotificationList,
        this, &NotificationManager::buildNotificationDisplay);
    connect(handler, &ServerNotificationHandler::newIncomingCallsList,
        this, &NotificationManager::buildIncomingCallDialogs);
    connect(handler, &ServerNotificationHandler::jobFinished,
        this, &NotificationManager::notificationFetchFinished);
    _isNotificationFetchRunning = handler->startFetchNotifications();
}

void NotificationManager::handlePushNotification(Account *account)
{
    if (account && _accountState && account->id() == _accountState->account()->id()) {
        refresh();
    }
}

void NotificationManager::notificationFetchFinished()
{
    _isNotificationFetchRunning = false;
}

void NotificationManager::sendNotificationRequest(
    const QString &accountName, const QString &link, const QByteArray &verb, const int row)
{
    qCInfo(lcActivity) << "Server Notification Request" << verb << link << "on account" << accountName;
    if (!_accountState || !_accountState->account() || accountName != _accountState->account()->displayName()) {
        qCWarning(lcActivity) << "Notification action account does not match its notification manager.";
        return;
    }

    auto notificationId = qint64{-1};
    const auto &activities = _activityModel->activityList();
    if (row >= 0 && row < activities.size() && activities.at(row)._type == Activity::NotificationType) {
        notificationId = activities.at(row)._id;
    }
    sendServerNotificationRequest(link, verb, row, notificationId);
}

void NotificationManager::sendServerNotificationRequest(
    const QString &link, const QByteArray &verb, const int row, const qint64 notificationId)
{
    static const auto validVerbs = QList<QByteArray>{
        QByteArrayLiteral("GET"),
        QByteArrayLiteral("PUT"),
        QByteArrayLiteral("POST"),
        QByteArrayLiteral("DELETE"),
    };
    if (!validVerbs.contains(verb)) {
        qCWarning(lcActivity) << "Notification Links: Invalid verb:" << verb;
        return;
    }
    if (!_accountState || !_accountState->account()) {
        return;
    }

    const auto actionUrl = QUrl(link);
    if (!actionUrl.isValid() || actionUrl.isEmpty()) {
        qCWarning(lcActivity) << "Notification action has an invalid URL:" << link;
        return;
    }

    auto * const job = new NotificationConfirmJob(_accountState->account());
    job->setLinkAndVerb(actionUrl, verb);
    job->setProperty("activityRow", row);
    job->setProperty("notificationId", notificationId);
    connect(job, &AbstractNetworkJob::networkError, this, &NotificationManager::notifyNetworkError);
    connect(job, &NotificationConfirmJob::jobFinished, this, &NotificationManager::notifyServerFinished);
    ++_notificationRequestsRunning;
    job->start();
}

void NotificationManager::dismissServerNotification(const qint64 notificationId)
{
    if (notificationId < 0 || !_accountState || !_accountState->account()) {
        return;
    }

    const auto link = Utility::concatUrlPath(
        _accountState->account()->url(),
        QStringLiteral("ocs/v2.php/apps/notifications/api/v2/notifications/%1").arg(notificationId));
    sendServerNotificationRequest(link.toString(), QByteArrayLiteral("DELETE"), -1, notificationId);
}

void NotificationManager::dismissNotification(const int activityIndex)
{
    if (_activityModel) {
        _activityModel->slotTriggerDismiss(activityIndex);
    }
}

void NotificationManager::triggerNotificationAction(const int activityIndex, const int actionIndex)
{
    if (_activityModel) {
        _activityModel->slotTriggerAction(activityIndex, actionIndex);
    }
}

void NotificationManager::triggerServerNotificationAction(
    const qint64 notificationId, const QString &link, const QByteArray &verb)
{
    if (verb == QByteArrayLiteral("WEB")) {
        Utility::openBrowser(QUrl(link));
        return;
    }
    sendServerNotificationRequest(link, verb, -1, notificationId);
}

void NotificationManager::sendTalkReply(
    const QString &reply, const QString &conversationToken, const QString &replyTo)
{
    sendTalkReplyMessage(conversationToken, reply, replyTo, std::nullopt, true);
}

void NotificationManager::sendTalkReply(
    const int activityIndex, const QString &conversationToken, const QString &message, const QString &replyTo)
{
    sendTalkReplyMessage(conversationToken, message, replyTo, activityIndex);
}

void NotificationManager::sendTalkReplyMessage(
    const QString &conversationToken, const QString &message, const QString &replyTo,
    const std::optional<int> activityIndex, const bool logNativeReply)
{
    if (!_accountState) {
        return;
    }

    if (logNativeReply) {
        qCDebug(lcActivity) << "Sending Talk reply from native notification."
                            << "Replying to:" << replyTo
                            << "Token:" << conversationToken
                            << "Account:" << _accountState->account()->id();
    }

    const auto talkReply = new TalkReply(_accountState.data(), this);
    if (activityIndex) {
        connect(talkReply, &TalkReply::replyMessageSent, this, [this, activityIndex = *activityIndex](const QString &sentMessage) {
            if (_activityModel) {
                _activityModel->setReplyMessageSent(activityIndex, sentMessage);
            }
        });
    }
    talkReply->sendReplyMessage(conversationToken, message, replyTo);
}

void NotificationManager::showActivities() const
{
    if (_accountState) {
        Systray::instance()->showActivitiesWindow(_accountState.data());
    }
}

void NotificationManager::notificationRequestFinished(const int statusCode)
{
    const auto row = sender()->property("activityRow").toInt();
    const auto notificationId = sender()->property("notificationId").toLongLong();
    if (statusCode != OCS_SUCCESS_STATUS_CODE
        && statusCode != OCS_SUCCESS_STATUS_CODE_V2
        && statusCode != OCS_ACCEPTED_STATUS_CODE) {
        qCWarning(lcActivity) << "Notification Request to Server failed, leave notification visible.";
        return;
    }

    qCInfo(lcActivity) << "Notification Request to Server succeeded, removing notification.";
    if (notificationId >= 0) {
        _activityModel->removeNotificationFromActivityList(notificationId);
    } else if (row >= 0 && row < _activityModel->rowCount()) {
        _activityModel->removeActivityFromActivityList(row);
    }
}

void NotificationManager::endNotificationRequest(const int replyCode)
{
    --_notificationRequestsRunning;
    notificationRequestFinished(replyCode);
}

void NotificationManager::notifyNetworkError(QNetworkReply *reply)
{
    const auto job = qobject_cast<NotificationConfirmJob *>(sender());
    if (!job) {
        return;
    }

    const auto resultCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    job->setProperty("networkErrorStatusCode", resultCode);
    qCWarning(lcActivity) << "Server notify job failed with code" << resultCode;
}

void NotificationManager::notifyServerFinished(const QString &reply, const int replyCode)
{
    const auto job = qobject_cast<NotificationConfirmJob *>(sender());
    if (!job) {
        return;
    }

    const auto networkErrorStatusCode = job->property("networkErrorStatusCode");
    const auto effectiveReplyCode = networkErrorStatusCode.isValid() ? networkErrorStatusCode.toInt() : replyCode;
    endNotificationRequest(effectiveReplyCode);
    qCInfo(lcActivity) << "Server Notification reply code" << effectiveReplyCode << reply;
}

void NotificationManager::addNotification(const Folder *folder, const Activity &activity)
{
    if (!folder || folder->accountState() != _accountState.data() || _notifiedNotifications.contains(activity._id)) {
        return;
    }

    _notifiedNotifications.insert(activity._id);
    if (_activityModel) {
        _activityModel->addNotificationToActivityList(activity);
    }
    _notificationHistoryTimer.start();
}

}
