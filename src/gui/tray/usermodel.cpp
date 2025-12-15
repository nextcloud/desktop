/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "notificationhandler.h"
#include "usermodel.h"
#include "common/filesystembase.h"

#include "accountmanager.h"
#include "owncloudgui.h"
#include <pushnotifications.h>
#include "userstatusselectormodel.h"
#include "syncengine.h"
#include "ocsjob.h"
#include "configfile.h"
#include "notificationconfirmjob.h"
#include "logger.h"
#include "guiutility.h"
#include "syncfileitem.h"
#include "systray.h"
#include "tray/activitylistmodel.h"
#include "tray/unifiedsearchresultslistmodel.h"
#include "tray/talkreply.h"
#include "userstatusconnector.h"

#include <QtCore>
#include <QDesktopServices>
#include <QIcon>
#include <QMessageBox>
#include <QSvgRenderer>
#include <QPainter>
#include <QPushButton>
#include <QDateTime>

// time span in milliseconds which has to be between two
// refreshes of the notifications
#define NOTIFICATION_REQUEST_FREE_PERIOD 15000

namespace {
constexpr qint64 expiredActivitiesCheckIntervalMsecs = 1000 * 60;
constexpr qint64 activityDefaultExpirationTimeMsecs = 1000 * 60 * 10;
}

namespace OCC {
    
TrayFolderInfo::TrayFolderInfo(const QString &name, const QString &parentPath, const QString &fullPath, FolderType folderType)
    : _name(name)
    , _parentPath(parentPath)
    , _fullPath(fullPath)
    , _folderType(folderType)
{
}

bool TrayFolderInfo::isGroupFolder() const
{
    return _folderType == GroupFolder;
}

User::User(AccountStatePtr &account, const bool &isCurrent, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _isCurrentUser(isCurrent)
    , _activityModel(new ActivityListModel(_account.data(), this))
    , _unifiedSearchResultsModel(new UnifiedSearchResultsListModel(_account.data(), this))
{
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::progressInfo,
        this, &User::slotProgressInfo);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::itemCompleted,
        this, &User::slotItemCompleted);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::syncError,
        this, &User::slotAddError);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::addErrorToGui,
        this, &User::slotAddErrorToGui);

    connect(&_notificationCheckTimer, &QTimer::timeout,
        this, &User::slotRefresh);

    connect(&_expiredActivitiesCheckTimer, &QTimer::timeout,
        this, &User::slotCheckExpiredActivities);

    connect(_account.data(), &AccountState::stateChanged,
            [=, this]() { if (isConnected()) {slotRefreshImmediately();} });
    connect(_account.data(), &AccountState::stateChanged, this, &User::accountStateChanged);
    connect(_account.data(), &AccountState::hasFetchedNavigationApps,
        this, &User::slotRebuildNavigationAppList);
    connect(_account->account().data(), &Account::accountChangedDisplayName, this, &User::nameChanged);
    connect(_account->account().data(), &Account::rootFolderQuotaChanged, this, &User::slotQuotaChanged);

    connect(FolderMan::instance(), &FolderMan::folderListChanged, this, &User::hasLocalFolderChanged);

    connect(_account->account().data(), &Account::accountChangedAvatar, this, &User::avatarChanged);
    connect(_account->account().data(), &Account::userStatusChanged, this, &User::statusChanged);
    connect(_account.data(), &AccountState::desktopNotificationsAllowedChanged, this, &User::desktopNotificationsAllowedChanged);

    connect(_account->account().data(), &Account::capabilitiesChanged, this, &User::headerColorChanged);
    connect(_account->account().data(), &Account::capabilitiesChanged, this, &User::headerTextColorChanged);
    connect(_account->account().data(), &Account::capabilitiesChanged, this, &User::accentColorChanged);

    connect(_account->account().data(), &Account::capabilitiesChanged, this, &User::slotAccountCapabilitiesChangedRefreshGroupFolders);

    connect(_activityModel, &ActivityListModel::sendNotificationRequest, this, &User::slotSendNotificationRequest);
    connect(_activityModel, &ActivityListModel::showSettingsDialog,
            Systray::instance(), &Systray::openSettings);

    connect(this, &User::sendReplyMessage, this, &User::slotSendReplyMessage);

    connect(_account->account().data(), &Account::userCertificateNeedsMigrationChanged, this, [this] () {
        auto certificateNeedMigration = Activity{};
        certificateNeedMigration._type = Activity::OpenSettingsNotificationType;
        certificateNeedMigration._subject = tr("End-to-end certificate needs to be migrated to a new one");
        certificateNeedMigration._dateTime = QDateTime::fromString(QDateTime::currentDateTime().toString(), Qt::ISODate);
        certificateNeedMigration._message = tr("Trigger the migration");
        certificateNeedMigration._accName = _account->account()->displayName();
        certificateNeedMigration._id = qHash("migrate-certificate");

        _activityModel->removeActivityFromActivityList(certificateNeedMigration);

        if (_account->account()->e2e()->userCertificateNeedsMigration()) {
            _activityModel->addNotificationToActivityList(certificateNeedMigration);
            showDesktopNotification(certificateNeedMigration);
        }
    });
}

void User::checkNotifiedNotifications()
{
    // clear the gui log notification store after one hour has passed since the last received notification
    constexpr qint64 clearGuiLogInterval = 60 * 60 * 1000;
    if (_guiLogTimer.elapsed() > clearGuiLogInterval) {
        _notifiedNotifications.clear();
    }
}

bool User::notificationAlreadyShown(const qint64 notificationId)
{
    checkNotifiedNotifications();
    return _notifiedNotifications.contains(notificationId);
}

bool User::canShowNotification(const qint64 notificationId)
{
    ConfigFile cfg;
    return cfg.optionalServerNotifications() &&
            isDesktopNotificationsAllowed() &&
            !notificationAlreadyShown(notificationId);
}

void User::showDesktopNotification(const QString &title, const QString &message, const qint64 notificationId)
{
    if (!canShowNotification(notificationId)) {
        return;
    }

    _notifiedNotifications.insert(notificationId);
    Logger::instance()->postGuiLog(title, message);
    // restart the gui log timer now that we show a new notification
    _guiLogTimer.start();
}

void User::showDesktopNotification(const Activity &activity)
{
    const auto notificationId = activity._id;
    const auto message = AccountManager::instance()->accounts().count() == 1 ? "" : activity._accName;

    // the user needs to interact with this notification
    if (activity._links.size() > 0) {
        _activityModel->addNotificationToActivityList(activity);
    }

    showDesktopNotification(activity._subject, message, notificationId);
}

void User::showDesktopNotification(const ActivityList &activityList)
{
    const auto subject = tr("%n notification(s)", nullptr, activityList.count());
    const auto notificationId = -static_cast<int>(qHash(subject));

    if (!canShowNotification(notificationId)) {
        return;
    }

    const auto multipleAccounts = AccountManager::instance()->accounts().count() > 1;
    const auto message = multipleAccounts ? activityList.constFirst()._accName : QString();

    // Notification ids are uints, which are 4 bytes. Error activities don't have ids, however, so we generate one.
    // To avoid possible collisions between the activity ids which are actually the notification ids received from
    // the server (which are always positive) and our "fake" error activity ids, we assign a negative id to the
    // error notification.
    //
    // To ensure that we can still treat an unsigned int as normal, we use a long, which is 8 bytes.

    Logger::instance()->postGuiLog(subject, message);

    for (const auto &activity : activityList) {
        _notifiedNotifications.insert(activity._id);
        _activityModel->addNotificationToActivityList(activity);
    }
}

void User::showDesktopTalkNotification(const Activity &activity)
{
    const auto notificationId = activity._id;

    if (!canShowNotification(notificationId) || !ConfigFile().showChatNotifications()) {
        return;
    }

    if (activity._talkNotificationData.messageId.isEmpty()) {
        showDesktopNotification(activity._subject, activity._message, notificationId);
        return;
    }

    _notifiedNotifications.insert(notificationId);
    _activityModel->addNotificationToActivityList(activity);

    Systray::instance()->showTalkMessage(activity._subject,
                                         activity._message,
                                         activity._talkNotificationData.conversationToken,
                                         activity._talkNotificationData.messageId,
                                         _account);
    _guiLogTimer.start();
}

void User::slotBuildNotificationDisplay(const ActivityList &list)
{
    ActivityList toNotifyList;

    _activityModel->removeOutdatedNotifications(list);

    std::copy_if(list.constBegin(), list.constEnd(), std::back_inserter(toNotifyList), [&](const Activity &activity) -> bool {
        if (!activity._shouldNotify) {
            qCDebug(lcActivity).nospace() << "No notification should be sent for activity with id=" << activity._id << " objectType=" << activity._objectType;
            return false;
        }

        if (_notifiedNotifications.contains(activity._id)) {
            qCInfo(lcActivity).nospace() << "Ignoring already notified activity with id=" << activity._id << " objectType=" << activity._objectType;
            return false;
        }

        return true;
    });

    if (toNotifyList.isEmpty()) {
        return;
    }

    if (toNotifyList.size() == 1) {
        const auto &activity = toNotifyList.constFirst();
        if (activity._objectType == QStringLiteral("chat")) {
            // Talk's "call" type is handled in slotBuildIncomingCallDialogs
            showDesktopTalkNotification(activity);
            return;
        }

        showDesktopNotification(activity);
        return;
    }

    showDesktopNotification(toNotifyList);
}

void User::slotNotificationFetchFinished()
{
    _isNotificationFetchRunning = false;
}

void User::slotBuildIncomingCallDialogs(const ActivityList &list)
{
    const ConfigFile cfg;
    const auto userStatus = _account->account()->userStatusConnector()->userStatus().state();
    if (userStatus == OCC::UserStatus::OnlineStatus::DoNotDisturb ||
            !cfg.optionalServerNotifications() ||
            !cfg.showCallNotifications() ||
            !isDesktopNotificationsAllowed()) {
        return;
    }

    const auto systray = Systray::instance();
    if (!systray) {
        qCWarning(lcActivity) << "No systray instance available, can not notify about new calls";
        return;
    }

    for (const auto &activity : list) {
        if (!activity._shouldNotify) {
            qCDebug(lcActivity).nospace() << "No notification should be sent for activity with id=" << activity._id << " objectType=" << activity._objectType;
            continue;
        }

        systray->createCallDialog(activity, _account);
    }
}

void User::setNotificationRefreshInterval(std::chrono::milliseconds interval)
{
    if (!checkPushNotificationsAreReady()) {
        qCDebug(lcActivity) << "Starting Notification refresh timer with " << interval.count() / 1000 << " sec interval";
        _notificationCheckTimer.start(interval.count());
    }
}

void User::slotPushNotificationsReady()
{
    qCInfo(lcActivity) << "Push notifications are ready";

    if (_notificationCheckTimer.isActive()) {
        // as we are now able to use push notifications - let's stop the polling timer
        _notificationCheckTimer.stop();
    }

    connectPushNotifications();
}

void User::slotDisconnectPushNotifications()
{
    disconnect(_account->account()->pushNotifications(), &PushNotifications::notificationsChanged, this, &User::slotReceivedPushNotification);
    disconnect(_account->account()->pushNotifications(), &PushNotifications::activitiesChanged, this, &User::slotReceivedPushActivity);

    disconnect(_account->account().data(), &Account::pushNotificationsDisabled, this, &User::slotDisconnectPushNotifications);

    // connection to WebSocket may have dropped or an error occurred, so we need to bring back the polling until we have re-established the connection
    setNotificationRefreshInterval(ConfigFile().notificationRefreshInterval());
}

void User::slotReceivedPushNotification(Account *account)
{
    if (account->id() == _account->account()->id()) {
        slotRefreshNotifications();
    }
}

void User::slotReceivedPushActivity(Account *account)
{
    if (account->id() == _account->account()->id()) {
        slotRefreshActivities();
    }
}

void User::slotCheckExpiredActivities()
{
    const auto errorsList = _activityModel->errorsList();
    for (const auto &activity : errorsList) {
        if (activity._expireAtMsecs > 0 && QDateTime::currentDateTime().toMSecsSinceEpoch() >= activity._expireAtMsecs) {
            _activityModel->removeActivityFromActivityList(activity);
        }
    }

    if (_activityModel->errorsList().size() == 0) {
        _expiredActivitiesCheckTimer.stop();
    }
}

void User::parseNewGroupFolderPath(const QString &mountPoint)
{
    if (mountPoint.isEmpty()) {
        return;
    }

    auto sanitisedMountPoint = mountPoint;
    sanitisedMountPoint.replace("//", "/");
    auto mountPointSplit = sanitisedMountPoint.split('/', Qt::SkipEmptyParts);

    if (mountPointSplit.isEmpty()) {
        return;
    }

    const auto groupFolderName = mountPointSplit.takeLast();
    const auto parentPath = mountPointSplit.join('/');
    const auto folderInfo = TrayFolderInfo(
        groupFolderName, parentPath, sanitisedMountPoint, TrayFolderInfo::GroupFolder
    );
    const auto folderInfoVariant = QVariant::fromValue(folderInfo);
    _trayFolderInfos.push_back(folderInfoVariant);
}

void User::prePendGroupFoldersWithLocalFolder()
{
    if (!_trayFolderInfos.isEmpty() && !_trayFolderInfos.first().value<TrayFolderInfo>().isGroupFolder()) {
        return;
    }
    const auto localFolderName = getFolder()->shortGuiLocalPath();
    auto localFolderPathSplit = getFolder()->path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!localFolderPathSplit.isEmpty()) {
        localFolderPathSplit.removeLast();
    }
    const auto localFolderParentPath = !localFolderPathSplit.isEmpty() ? localFolderPathSplit.join(QLatin1Char('/')) : "/";
    _trayFolderInfos.push_front(QVariant::fromValue(TrayFolderInfo{localFolderName, localFolderParentPath, getFolder()->path(), TrayFolderInfo::Folder}));
}

void User::connectPushNotifications() const
{
    connect(_account->account().data(), &Account::pushNotificationsDisabled, this, &User::slotDisconnectPushNotifications, Qt::UniqueConnection);

    connect(_account->account()->pushNotifications(), &PushNotifications::notificationsChanged, this, &User::slotReceivedPushNotification, Qt::UniqueConnection);
    connect(_account->account()->pushNotifications(), &PushNotifications::activitiesChanged, this, &User::slotReceivedPushActivity, Qt::UniqueConnection);
}

bool User::checkPushNotificationsAreReady() const
{
    const auto pushNotifications = _account->account()->pushNotifications();

    const auto pushActivitiesAvailable = _account->account()->capabilities().availablePushNotifications() & PushNotificationType::Activities;
    const auto pushNotificationsAvailable = _account->account()->capabilities().availablePushNotifications() & PushNotificationType::Notifications;

    const auto pushActivitiesAndNotificationsAvailable = pushActivitiesAvailable && pushNotificationsAvailable;

    if (pushActivitiesAndNotificationsAvailable && pushNotifications && pushNotifications->isReady()) {
        connectPushNotifications();
        return true;
    } else {
        connect(_account->account().data(), &Account::pushNotificationsReady, this, &User::slotPushNotificationsReady, Qt::UniqueConnection);
        return false;
    }
}

void User::slotRefreshImmediately() {
    if (_account.data() && _account.data()->isConnected() && Systray::instance()->isOpen()) {
        slotRefreshActivities();
    }
    slotRefreshNotifications();
}

void User::slotRefresh()
{
    slotRefreshUserStatus();
    
    if (checkPushNotificationsAreReady()) {
        // we are relying on WebSocket push notifications - ignore refresh attempts from UI
        slotRefreshActivities();
        _timeSinceLastCheck[_account.data()].invalidate();
        return;
    }

    // QElapsedTimer isn't actually constructed as invalid.
    if (!_timeSinceLastCheck.contains(_account.data())) {
        _timeSinceLastCheck[_account.data()].invalidate();
    }
    QElapsedTimer &timer = _timeSinceLastCheck[_account.data()];

    // Fetch Activities only if visible and if last check is longer than 15 secs ago
    if (timer.isValid() && timer.elapsed() < NOTIFICATION_REQUEST_FREE_PERIOD) {
        qCDebug(lcActivity) << "Do not check as last check is only secs ago: " << timer.elapsed() / 1000;
        return;
    }
    if (_account.data() && _account.data()->isConnected()) {
        slotRefreshActivities();
        slotRefreshNotifications();
        timer.start();
    }
}

void User::slotRefreshActivitiesInitial()
{
    if (_account.data()->isConnected() && Systray::instance()->isOpen()) {
        _activityModel->slotRefreshActivityInitial();
    }
}

void User::slotRefreshActivities()
{
    if (_account.data()->isConnected() && Systray::instance()->isOpen()) {
        _activityModel->slotRefreshActivity();
    }
}

void User::slotRefreshUserStatus()
{
    if (_account.data() && _account.data()->isConnected()) {
        _account->account()->userStatusConnector()->fetchUserStatus();
    }
}

void User::slotRefreshNotifications()
{
    // start a server notification handler if no notification requests
    // are running
    if (_notificationRequestsRunning == 0) {
        if (_isNotificationFetchRunning) {
            qCDebug(lcActivity) << "Notification fetch is already running.";
            return;
        }
        auto *snh = new ServerNotificationHandler(_account.data());
        connect(snh, &ServerNotificationHandler::newNotificationList,
            this, &User::slotBuildNotificationDisplay);
        connect(snh, &ServerNotificationHandler::newIncomingCallsList,
            this, &User::slotBuildIncomingCallDialogs);
        connect(snh, &ServerNotificationHandler::jobFinished,
            this, &User::slotNotificationFetchFinished);
        _isNotificationFetchRunning = snh->startFetchNotifications();
    } else {
        qCWarning(lcActivity) << "Notification request counter not zero.";
    }
}

void User::slotRebuildNavigationAppList()
{
    emit featuredAppChanged();
    // Rebuild App list
    UserAppsModel::instance()->buildAppList();
}

void User::slotNotificationRequestFinished(int statusCode)
{
    int row = sender()->property("activityRow").toInt();

    // the ocs API returns stat code 100 or 200 or 202 inside the xml if it succeeded.
    if (statusCode != OCS_SUCCESS_STATUS_CODE
        && statusCode != OCS_SUCCESS_STATUS_CODE_V2
        && statusCode != OCS_ACCEPTED_STATUS_CODE) {
        qCWarning(lcActivity) << "Notification Request to Server failed, leave notification visible.";
    } else {
        // to do use the model to rebuild the list or remove the item
        qCWarning(lcActivity) << "Notification Request to Server succeeded, rebuilding list.";
        _activityModel->removeActivityFromActivityList(row);
    }
}

void User::slotEndNotificationRequest(int replyCode)
{
    _notificationRequestsRunning--;
    slotNotificationRequestFinished(replyCode);
}

void User::slotSendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row)
{
    qCInfo(lcActivity) << "Server Notification Request " << verb << link << "on account" << accountName;

    const QStringList validVerbs = QStringList() << "GET"
                                                 << "PUT"
                                                 << "POST"
                                                 << "DELETE";

    if (validVerbs.contains(verb)) {
        AccountStatePtr acc = AccountManager::instance()->account(accountName);
        if (acc) {
            auto *job = new NotificationConfirmJob(acc->account());
            QUrl l(link);
            job->setLinkAndVerb(l, verb);
            job->setProperty("activityRow", QVariant::fromValue(row));
            connect(job, &AbstractNetworkJob::networkError,
                this, &User::slotNotifyNetworkError);
            connect(job, &NotificationConfirmJob::jobFinished,
                this, &User::slotNotifyServerFinished);
            job->start();

            // count the number of running notification requests. If this member var
            // is larger than zero, no new fetching of notifications is started
            _notificationRequestsRunning++;
        }
    } else {
        qCWarning(lcActivity) << "Notification Links: Invalid verb:" << verb;
    }
}

void User::slotNotifyNetworkError(QNetworkReply *reply)
{
    auto *job = qobject_cast<NotificationConfirmJob *>(sender());
    if (!job) {
        return;
    }

    int resultCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    slotEndNotificationRequest(resultCode);
    qCWarning(lcActivity) << "Server notify job failed with code " << resultCode;
}

void User::slotNotifyServerFinished(const QString &reply, int replyCode)
{
    auto *job = qobject_cast<NotificationConfirmJob *>(sender());
    if (!job) {
        return;
    }

    slotEndNotificationRequest(replyCode);
    qCInfo(lcActivity) << "Server Notification reply code" << replyCode << reply;
}

void User::slotProgressInfo(const QString &folder, const ProgressInfo &progress)
{
    if (progress.status() == ProgressInfo::Reconcile) {
        // Wipe all non-persistent entries - as well as the persistent ones
        // in cases where a local discovery was done.
        auto f = FolderMan::instance()->folder(folder);
        if (!f)
            return;
        const auto &engine = f->syncEngine();
        const auto style = engine.lastLocalDiscoveryStyle();
        for (const auto &activity : _activityModel->errorsList()) {
            if (activity._expireAtMsecs != -1) {
                // we process expired activities in a different slot
                continue;
            }
            if (activity._folder != folder) {
                continue;
            }

            if (style == LocalDiscoveryStyle::FilesystemOnly) {
                _activityModel->removeActivityFromActivityList(activity);
                continue;
            }

            if (const auto filePath = f->path() + activity._file; !FileSystem::fileExists(filePath)) {
                _activityModel->removeActivityFromActivityList(activity);
                continue;
            }

            auto path = QFileInfo(activity._file).dir().path().toUtf8();
            if (path == ".")
                path.clear();

            if (engine.shouldDiscoverLocally(path))
                _activityModel->removeActivityFromActivityList(activity);
        }
    }

    if (progress.status() == ProgressInfo::Done) {
        // We keep track very well of pending conflicts.
        // Inform other components about them.
        QStringList conflicts;
        for (const auto &activity : _activityModel->errorsList()) {
            if (activity._folder == folder
                && activity._syncFileItemStatus == SyncFileItem::Conflict) {
                conflicts.append(activity._file);
            }
        }

        emit ProgressDispatcher::instance()->folderConflicts(folder, conflicts);
    }
}

void User::slotAddError(const QString &folderAlias, const QString &message, ErrorCategory category)
{
    auto folderInstance = FolderMan::instance()->folder(folderAlias);
    if (!folderInstance)
        return;

    if (folderInstance->accountState() == _account.data()) {
        qCWarning(lcActivity) << "Item " << folderInstance->shortGuiLocalPath() << " retrieved resulted in " << message;

        Activity activity;
        activity._type = Activity::SyncResultType;
        activity._syncResultStatus = SyncResult::Error;
        activity._dateTime = QDateTime::fromString(QDateTime::currentDateTime().toString(), Qt::ISODate);
        activity._subject = message;
        activity._message = folderInstance->shortGuiLocalPath();
        activity._link = folderInstance->shortGuiLocalPath();
        activity._accName = folderInstance->accountState()->account()->displayName();
        activity._folder = folderAlias;


        if (category == ErrorCategory::InsufficientRemoteStorage) {
            ActivityLink link;
            link._label = tr("Retry all uploads");
            link._link = folderInstance->path();
            link._verb = "";
            link._primary = true;
            activity._links.append(link);
        }

        auto errorType = ActivityListModel::ErrorType::SyncError;
        // add 'other errors' to activity list
        switch (category) {
        case ErrorCategory::GenericError:
            errorType = ActivityListModel::ErrorType::SyncError;
            break;
        case ErrorCategory::InsufficientRemoteStorage:
            errorType = ActivityListModel::ErrorType::SyncError;
            break;
        case ErrorCategory::NetworkError:
            errorType = ActivityListModel::ErrorType::NetworkError;
            break;
        case ErrorCategory::NoError:
            break;
        }

        _activityModel->addErrorToActivityList(activity, errorType);
    }
}

void User::slotAddErrorToGui(const QString &folderAlias, const SyncFileItem::Status status, const QString &errorMessage, const QString &subject, const ErrorCategory category)
{
    const auto folderInstance = FolderMan::instance()->folder(folderAlias);
    if (!folderInstance) {
        return;
    }

    if (folderInstance->accountState() == _account.data()) {
        qCWarning(lcActivity) << "Item " << folderInstance->shortGuiLocalPath() << " retrieved resulted in " << errorMessage;

        Activity activity;
        activity._type = Activity::SyncFileItemType;
        activity._syncFileItemStatus = status;
        const auto currentDateTime = QDateTime::currentDateTime();
        activity._dateTime = QDateTime::fromString(currentDateTime.toString(), Qt::ISODate);
        activity._expireAtMsecs = currentDateTime.addMSecs(activityDefaultExpirationTimeMsecs).toMSecsSinceEpoch();
        activity._subject = !subject.isEmpty() ? subject : folderInstance->shortGuiLocalPath();
        activity._message = errorMessage;
        activity._link = folderInstance->shortGuiLocalPath();
        activity._accName = folderInstance->accountState()->account()->displayName();
        activity._folder = folderAlias;

        if (status == SyncFileItem::Conflict || status == SyncFileItem::FileNameClash) {
            ActivityLink buttonActivityLink;
            buttonActivityLink._label = tr("Resolve conflict");
            buttonActivityLink._link = activity._link.toString();
            buttonActivityLink._verb = "FIX_CONFLICT_LOCALLY";
            buttonActivityLink._primary = true;

            activity._links = {buttonActivityLink};
        }

        // Error notifications don't have ids by themselves so we will create one for it
        activity._id = -static_cast<int>(qHash(activity._subject + activity._message));

        // add 'other errors' to activity list
        auto errorType = ActivityListModel::ErrorType::SyncError;
        switch (category)
        {
        case ErrorCategory::GenericError:
            errorType = ActivityListModel::ErrorType::SyncError;
            break;
        case ErrorCategory::InsufficientRemoteStorage:
            errorType = ActivityListModel::ErrorType::SyncError;
            break;
        case ErrorCategory::NetworkError:
            errorType = ActivityListModel::ErrorType::NetworkError;
            break;
        case ErrorCategory::NoError:
            errorType = {};
            break;
        }

        _activityModel->addErrorToActivityList(activity, errorType);

        showDesktopNotification(activity);

        if (!_expiredActivitiesCheckTimer.isActive()) {
            _expiredActivitiesCheckTimer.start(expiredActivitiesCheckIntervalMsecs);
        }
    }
}

void User::slotAddNotification(const Folder *folder, const Activity &activity)
{
    if (!isActivityOfCurrentAccount(folder) || _notifiedNotifications.contains(activity._id)) {
        return;
    }

    _notifiedNotifications.insert(activity._id);
    _activityModel->addNotificationToActivityList(activity);
}

bool User::isActivityOfCurrentAccount(const Folder *folder) const
{
    return folder->accountState() == _account.data();
}

bool User::isUnsolvableConflict(const SyncFileItemPtr &item) const
{
    // We just care about conflict issues that we are able to resolve
    return item->_status == SyncFileItem::Conflict && !Utility::isConflictFile(item->_file);
}

void User::processCompletedSyncItem(const Folder *folder, const SyncFileItemPtr &item)
{
    if (item->_direction == SyncFileItem::Down && item->_instruction == CSYNC_INSTRUCTION_SYNC) {
        qCDebug(lcActivity) << "Skipping activities about changes coming from server.";
        return;
    }

    const auto fileActionFromInstruction = [](const int instruction) {
        if (instruction == CSYNC_INSTRUCTION_REMOVE) {
            return QStringLiteral("file_deleted");
        } else if (instruction == CSYNC_INSTRUCTION_NEW) {
            return QStringLiteral("file_created");
        } else if (instruction == CSYNC_INSTRUCTION_RENAME) {
            return QStringLiteral("file_renamed");
        } else {
            return QStringLiteral("file_changed");
        }
    };

    const auto messageFromFileAction = [](const QString &fileAction, const QString &fileName) {
        if (fileAction == QStringLiteral("file_renamed")) {
            return QObject::tr("You renamed %1").arg(fileName);
        } else if (fileAction == QStringLiteral("file_deleted")) {
            return QObject:: tr("You deleted %1").arg(fileName);
        } else if (fileAction == QStringLiteral("file_created")) {
            return QObject::tr("You created %1").arg(fileName);
        } else {
            return QObject::tr("You changed %1").arg(fileName);
        }
    };

    Activity activity;
    activity._type = Activity::SyncFileItemType; //client activity
    activity._objectType = QStringLiteral("files");
    activity._syncFileItemStatus = item->_status;
    activity._dateTime = QDateTime::currentDateTime();
    activity._message = item->_originalFile;
    activity._link = account()->url();
    activity._accName = account()->displayName();
    activity._file = item->_file;
    activity._folder = folder->alias();
    activity._fileAction = "";

    const auto fileName = QFileInfo(item->_originalFile).fileName();

    activity._fileAction = fileActionFromInstruction(item->_instruction);

    if (item->_status == SyncFileItem::NoStatus || item->_status == SyncFileItem::Success) {
        qCDebug(lcActivity) << "Item " << item->_file << " retrieved successfully.";

        if (item->_direction != SyncFileItem::Up) {
            activity._message = QObject::tr("Synced %1").arg(fileName);
        } else {
            activity._message = messageFromFileAction(activity._fileAction, fileName);
        }

        if(activity._fileAction != "file_deleted" && !item->isEmpty()) {
            const auto localFiles = FolderMan::instance()->findFileInLocalFolders(folder->remotePathTrailingSlash() + item->_file, account());
            if (!localFiles.isEmpty()) {
                const auto firstFilePath = localFiles.constFirst();
                const auto itemJournalRecord = item->toSyncJournalFileRecordWithInode(firstFilePath);

                if(!itemJournalRecord.isVirtualFile()) {
                    const auto mimeType = _mimeDb.mimeTypeForFile(QFileInfo(localFiles.constFirst()));

                    // Set the preview data, though for now we can skip setting file ID, link, and view
                    PreviewData preview;
                    preview._mimeType = mimeType.name();
                    preview._filename = fileName;
                    preview._isMimeTypeIcon = true;

                    if(item->isDirectory()) {
                        preview._source = account()->url().toString() + QStringLiteral("/index.php/apps/theming/img/core/filetypes/folder.svg");
                    } else {
                        preview._source = account()->url().toString() + Activity::relativeServerFileTypeIconPath(mimeType);
                    }
                    activity._previews.append(preview);
                }
            }
        }

        _activityModel->addSyncFileItemToActivityList(activity);
    } else {
        qCInfo(lcActivity) << "Item " << item->_file << " retrieved resulted in error " << item->_errorString;

        activity._subject = item->_errorString;
        activity._id = -static_cast<int>(qHash(activity._subject + activity._message));

        if (item->_status != SyncFileItem::Status::FileIgnored) {
            // add 'protocol error' to activity list
            if (item->_status == SyncFileItem::Status::FileNameInvalid || item->_status == SyncFileItem::Status::FileNameInvalidOnServer) {
                ActivityLink buttonActivityLink;
                buttonActivityLink._label = tr("Rename file");
                buttonActivityLink._link = activity._link.toString();
                buttonActivityLink._verb = "RENAME_LOCAL_FILE";
                buttonActivityLink._primary = true;

                activity._links = {buttonActivityLink};

                showDesktopNotification(item->_file, activity._subject, activity._id);
            } else if (item->_status == SyncFileItem::Conflict || item->_status == SyncFileItem::FileNameClash) {
                ActivityLink buttonActivityLink;
                buttonActivityLink._label = tr("Resolve conflict");
                buttonActivityLink._link = activity._link.toString();
                buttonActivityLink._verb = "FIX_CONFLICT_LOCALLY";
                buttonActivityLink._primary = true;

                activity._links = {buttonActivityLink};
            }
            _activityModel->addErrorToActivityList(activity, ActivityListModel::ErrorType::SyncError);
        }
    }
}

const QVariantList &User::groupFolders() const
{
    return _trayFolderInfos;
}

bool User::canLogout() const
{
    return !isPublicShareLink();
}

bool User::isPublicShareLink() const
{
    return _account->account()->isPublicShareLink();
}

void User::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    auto folderInstance = FolderMan::instance()->folder(folder);

    if (!folderInstance || !isActivityOfCurrentAccount(folderInstance) || isUnsolvableConflict(item)) {
        return;
    }

    qCWarning(lcActivity) << "Item " << item->_file << " retrieved resulted in " << item->_errorString;
    processCompletedSyncItem(folderInstance, item);
}

AccountPtr User::account() const
{
    return _account->account();
}

AccountStatePtr User::accountState() const
{
    return _account;
}

void User::setCurrentUser(const bool &isCurrent)
{
    _isCurrentUser = isCurrent;
}

Folder *User::getFolder() const
{
    for (const auto &folder : FolderMan::instance()->map()) {
        if (folder->accountState() == _account.data()) {
            return folder;
        }
    }

    return nullptr;
}

ActivityListModel *User::getActivityModel()
{
    return _activityModel;
}

UnifiedSearchResultsListModel *User::getUnifiedSearchResultsListModel() const
{
    return _unifiedSearchResultsModel;
}

void User::openLocalFolder() const
{
    if (const auto folder = getFolder()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder->path()));
    }
}

void User::openFolderLocallyOrInBrowser(const QString &fullRemotePath)
{
    const auto folder = getFolder();

    if (!folder) {
        return;
    }

    // remove remote path prefix and leading slash
    auto fullRemotePathToPathInDb = folder->remotePath() != QStringLiteral("/") ? fullRemotePath.mid(folder->remotePathTrailingSlash().size()) : fullRemotePath;
    if (fullRemotePathToPathInDb.startsWith("/")) {
        fullRemotePathToPathInDb = fullRemotePathToPathInDb.mid(1);
    }

    SyncJournalFileRecord rec;
    if (folder->journalDb()->getFileRecord(fullRemotePathToPathInDb, &rec) && rec.isValid()) {
        // found folder locally, going to open
        qCInfo(lcActivity) << "Opening locally a folder" << fullRemotePath;
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder->path() + rec.path()));
        return;
    }

    // try to open it in browser
    auto folderUrlForBrowser = Utility::concatUrlPath(_account->account()->url(), QStringLiteral("/index.php/apps/files/"));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("dir"), fullRemotePath);
    folderUrlForBrowser.setQuery(urlQuery);
    if (!folderUrlForBrowser.scheme().startsWith(QStringLiteral("http"))) {
        folderUrlForBrowser.setScheme(QStringLiteral("https"));
    }
    // open https://server.com/index.php/apps/files/?dir=/group_folder/path
    qCInfo(lcActivity) << "Opening in browser a folder" << fullRemotePath;
    Utility::openBrowser(folderUrlForBrowser);
    return;
}

void User::login() const
{
    _account->account()->resetRejectedCertificates();
    _account->signIn();
}

void User::logout() const
{
    _account->signOutByUi();
}

QString User::name() const
{
    if (isPublicShareLink()) {
        return tr("Public Share Link");
    }

    return _account->account()->prettyName();
}

QString User::server(bool shortened) const
{
    auto serverUrl = _account->account()->url();

    if (isPublicShareLink()) {
        serverUrl.setUserName({});
    }
    QString stringServerUrl = serverUrl.toString();
    if (shortened) {
        stringServerUrl.replace(QLatin1String("https://"), QLatin1String(""));
        stringServerUrl.replace(QLatin1String("http://"), QLatin1String(""));

    }
    return stringServerUrl;
}

UserStatus::OnlineStatus User::status() const
{
    return _account->account()->userStatusConnector()->userStatus().state();
}

QString User::statusMessage() const
{
    return _account->account()->userStatusConnector()->userStatus().message();
}

QUrl User::statusIcon() const
{
    return _account->account()->userStatusConnector()->userStatus().stateIcon();
}

QString User::statusEmoji() const
{
    return _account->account()->userStatusConnector()->userStatus().icon();
}

bool User::serverHasUserStatus() const
{
    return _account->account()->capabilities().userStatus();
}

QImage User::avatar() const
{
    return AvatarJob::makeCircularAvatar(_account->account()->avatar());
}

QString User::avatarUrl() const
{
    if (avatar().isNull()) {
        return QString();
    }

    return QStringLiteral("image://avatars/") + _account->account()->id();
}

bool User::hasLocalFolder() const
{
    return getFolder() != nullptr;
}

bool User::serverHasTalk() const
{
    return talkApp() != nullptr;
}

bool User::isFeaturedAppEnabled() const
{
    return isNcAssistantEnabled() || serverHasTalk();
}

QString User::featuredAppIcon() const
{
    return isNcAssistantEnabled() ? "image://svgimage-custom-color/nc-assistant-app.svg"
                                  : "image://svgimage-custom-color/talk-app.svg";
}

QString User::featuredAppAccessibleName() const
{
    return isNcAssistantEnabled() ?
        tr("Open %1 Assistant in browser", "The placeholder will be the application name. Please keep it").arg(APPLICATION_NAME) :
        tr("Open %1 Talk in browser", "The placeholder will be the application name. Please keep it").arg(APPLICATION_NAME);
}

AccountApp *User::talkApp() const
{
    return _account->findApp(QStringLiteral("spreed"));
}

bool User::hasActivities() const
{
    return _account->account()->capabilities().hasActivities();
}

bool User::isNcAssistantEnabled() const
{
    return _account->account()->capabilities().ncAssistantEnabled();
}

QColor User::headerColor() const
{
    return _account->account()->headerColor();
}

QColor User::headerTextColor() const
{
    return _account->account()->headerTextColor();
}

QColor User::accentColor() const
{
    return _account->account()->accentColor();
}

AccountAppList User::appList() const
{
    return _account->appList();
}

bool User::isCurrentUser() const
{
    return _isCurrentUser;
}

bool User::isConnected() const
{
    return (_account->connectionStatus() == AccountState::ConnectionStatus::Connected);
}

bool User::needsToSignTermsOfService() const
{
    return _account->connectionStatus() == AccountState::ConnectionStatus::NeedToSignTermsOfService;
}


bool User::isDesktopNotificationsAllowed() const
{
    return _account.data()->isDesktopNotificationsAllowed();
}

void User::removeAccount() const
{
    AccountManager::instance()->deleteAccount(_account.data());
    AccountManager::instance()->save();
}

void User::slotSendReplyMessage(const int activityIndex, const QString &token, const QString &message, const QString &replyTo)
{
    QPointer<TalkReply> talkReply = new TalkReply(_account.data(), this);
    talkReply->sendReplyMessage(token, message, replyTo);
    connect(talkReply, &TalkReply::replyMessageSent, this, [&, activityIndex](const QString &message) {
        _activityModel->setReplyMessageSent(activityIndex, message);
    });
}

void User::forceSyncNow() const
{
    FolderMan::instance()->forceSyncForFolder(getFolder());
}

void User::slotAccountCapabilitiesChangedRefreshGroupFolders()
{
    if (!_account->account()->capabilities().groupFoldersAvailable()) {
        if (!_trayFolderInfos.isEmpty()) {
            _trayFolderInfos.clear();
            emit groupFoldersChanged();
        }
        return;
    }

    slotFetchGroupFolders();
}

void User::slotFetchGroupFolders()
{
    QNetworkRequest req;
    req.setRawHeader(QByteArrayLiteral("OCS-APIREQUEST"), QByteArrayLiteral("true"));
    QUrlQuery query;
    query.addQueryItem(QLatin1String("format"), QLatin1String("json"));
    query.addQueryItem(QLatin1String("applicable"), QLatin1String("1"));
    QUrl groupFolderListUrl = Utility::concatUrlPath(_account->account()->url(), QStringLiteral("/index.php/apps/groupfolders/folders"));
    groupFolderListUrl.setQuery(query);

    const auto groupFolderListJob = _account->account()->sendRequest(QByteArrayLiteral("GET"), groupFolderListUrl, req);
    connect(groupFolderListJob, &SimpleNetworkJob::finishedSignal, this, &User::slotGroupFoldersFetched);
}

void User::slotQuotaChanged(const int64_t &usedBytes, const int64_t &availableBytes)
{
    if (availableBytes < 0) {
        // values less than 0 -> quota is not set or determinable
        // just reset the status
        _lastQuotaPercent = 0;
        _activityModel->removeActivityFromActivityList(_lastQuotaActivity);
        return;
    }

    int64_t total = usedBytes + availableBytes;
    if (total <= 0 || !ConfigFile().showQuotaWarningNotifications()) {
        return;
    }

    const auto percent = (double)usedBytes / (double)total * 100.0;
    const auto percentInt = qMin(qRound(percent), 100);
    qCDebug(lcActivity) << tr("Quota is updated; %1 percent of the total space is used.").arg(QString::number(percentInt));

    int thresholdPassed = 0;
    if (_lastQuotaPercent < 80 && percentInt >= 80) {
        thresholdPassed = 80;
    }

    if (_lastQuotaPercent < 90 && percentInt >= 90) {
        thresholdPassed = 90;
    }

    if (_lastQuotaPercent < 95 && percentInt >= 95) {
        thresholdPassed = 95;
    }

    if (thresholdPassed > 0) {
        _activityModel->removeActivityFromActivityList(_lastQuotaActivity);

        _lastQuotaActivity._type = Activity::OpenSettingsNotificationType;
        _lastQuotaActivity._dateTime = QDateTime::fromString(QDateTime::currentDateTime().toString(), Qt::ISODate);
        _lastQuotaActivity._subject = tr("Quota Warning - %1 percent or more storage in use").arg(QString::number(thresholdPassed));
        _lastQuotaActivity._accName = account()->displayName();
        _lastQuotaActivity._id = qHash(QDateTime::currentMSecsSinceEpoch());
        showDesktopNotification(_lastQuotaActivity);
        _activityModel->addNotificationToActivityList(_lastQuotaActivity);
    }
    _lastQuotaPercent = percentInt;
}

void User::slotGroupFoldersFetched(QNetworkReply *reply)
{
    Q_ASSERT(reply);
    if (!reply) {
        qCWarning(lcActivity) << "Group folders fetch error";
        return;
    }

    const auto oldSize = _trayFolderInfos.size();
    const auto oldTrayFolderInfos = _trayFolderInfos;
    _trayFolderInfos.clear();

    const auto replyData = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (oldSize != _trayFolderInfos.size()) {
            emit groupFoldersChanged();
        }
        qCWarning(lcActivity) << "Group folders fetch error" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() << replyData;
        return;
    }

    QJsonParseError jsonParseError{};
    const auto json = QJsonDocument::fromJson(replyData, &jsonParseError);

    if (jsonParseError.error != QJsonParseError::NoError) {
        qCWarning(lcActivity) << "Group folders JSON parse error" << jsonParseError.error << jsonParseError.errorString();
        if (oldSize != _trayFolderInfos.size()) {
            emit groupFoldersChanged();
        }
        return;
    }

    const auto obj = json.object().toVariantMap();
    const auto groupFolders = obj["ocs"].toMap()["data"].toMap();

    for (const auto &groupFolder : groupFolders) {
        const auto groupFolderInfo = groupFolder.toMap();
        const auto mountPoint = groupFolderInfo.value(QStringLiteral("mount_point"), {}).toString();
        parseNewGroupFolderPath(mountPoint);
    }
    std::sort(std::begin(_trayFolderInfos), std::end(_trayFolderInfos), [](const auto &leftVariant, const auto &rightVariant) {
        const auto folderInfoA = leftVariant.template value<TrayFolderInfo>();
        const auto folderInfoB = rightVariant.template value<TrayFolderInfo>();
        return folderInfoA._fullPath < folderInfoB._fullPath;
    });

    if (!_trayFolderInfos.isEmpty()) {
        if (hasLocalFolder()) {
            prePendGroupFoldersWithLocalFolder();
        }
    }

    if (oldSize != _trayFolderInfos.size()) {
        emit groupFoldersChanged();
    } else {
        for (int i = 0; i < oldTrayFolderInfos.size(); ++i) {
            const auto oldFolderInfo = oldTrayFolderInfos.at(i).template value<TrayFolderInfo>();
            const auto newFolderInfo = _trayFolderInfos.at(i).template value<TrayFolderInfo>();
            if (oldFolderInfo._folderType != newFolderInfo._folderType || oldFolderInfo._fullPath != newFolderInfo._fullPath) {
                break;
                emit groupFoldersChanged();
            }
        }
    }
}

/*-------------------------------------------------------------------------------------*/

UserModel *UserModel::_instance = nullptr;

UserModel *UserModel::instance()
{
    if (!_instance) {
        _instance = new UserModel();
    }
    return _instance;
}

UserModel::UserModel(QObject *parent)
    : QAbstractListModel(parent)
{
    if (AccountManager::instance()->accounts().size() > 0) {
        setInitialUser();
    }

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &UserModel::addAccsToUserList);
    connect(AccountManager::instance(), &AccountManager::accountListInitialized,
        this, &UserModel::setInitialUser);
}

void UserModel::buildUserList()
{
    for (int i = 0; i < AccountManager::instance()->accounts().size(); i++) {
        auto user = AccountManager::instance()->accounts().at(i);
        addUser(user);
    }
}

void UserModel::addAccsToUserList()
{
    if (_init) {
        return;
    }

    buildUserList();
}

void UserModel::setInitialUser()
{
    if (!_init) {
        return;
    }

    buildUserList();

    if(!_users.isEmpty()) {
        ConfigFile cfg;
        const uint lastSelectedAccountId = cfg.lastSelectedAccount();

        for (int i = 0; i <  _users.size(); i++) {
            if (_users.at(i)->account()->id().toUInt() == lastSelectedAccountId) {
                setCurrentUserId(i);
            }
        }

        if (_currentUserId < 0) {
            setCurrentUserId(0);
        }
    }

    _init = false;
}

int UserModel::numUsers()
{
    return _users.size();
}

int UserModel::currentUserId() const
{
    return _currentUserId;
}

bool UserModel::isUserConnected(const int id)
{
    if (id < 0 || id >= _users.size())
        return false;

    return _users[id]->isConnected();
}

QImage UserModel::avatarById(const int id) const
{
    const auto foundUserByIdIter = std::find_if(std::cbegin(_users), std::cend(_users), [&id](const OCC::User* const user) {
        return user->account()->id() == QString::number(id);
    });

    if (foundUserByIdIter == std::cend(_users)) {
        return {};
    }

    return (*foundUserByIdIter)->avatar();
}

QString UserModel::currentUserServer()
{
    if (_currentUserId < 0 || _currentUserId >= _users.size())
        return {};

    return _users[_currentUserId]->server();
}

void UserModel::addUser(AccountStatePtr &user, const bool &isCurrent)
{
    bool containsUser = false;
    for (const auto &u : std::as_const(_users)) {
        if (u->account() == user->account()) {
            containsUser = true;
            continue;
        }
    }

    if (!containsUser) {
        int row = rowCount();
        beginInsertRows(QModelIndex(), row, row);

        User *u = new User(user, isCurrent);

        connect(u, &User::avatarChanged, this, [this, row] {
           emit dataChanged(index(row, 0), index(row, 0), {UserModel::AvatarRole});
        });

        connect(u, &User::statusChanged, this, [this, row] {
            emit dataChanged(index(row, 0), index(row, 0), {UserModel::StatusRole,
                                                            UserModel::StatusIconRole,
                                                            UserModel::StatusEmojiRole,
                                                            UserModel::StatusMessageRole});
        });
        
        connect(u, &User::desktopNotificationsAllowedChanged, this, [this, row] {
            emit dataChanged(index(row, 0), index(row, 0), { UserModel::DesktopNotificationsAllowedRole });
        });
        
        connect(u, &User::accountStateChanged, this, [this, row] {
            emit dataChanged(index(row, 0), index(row, 0), { UserModel::IsConnectedRole });
        });

        _users << u;
        if (isCurrent || (_currentUserId < 0 && !_init)) {
            setCurrentUserId(_users.size() - 1);
        }

        endInsertRows();
        ConfigFile cfg;
        u->setNotificationRefreshInterval(cfg.notificationRefreshInterval());
        emit currentUserChanged();
    }
}

int UserModel::currentUserIndex()
{
    return _currentUserId;
}

void UserModel::openCurrentAccountLocalFolder()
{
    if (_currentUserId < 0 || _currentUserId >= _users.size())
        return;

    _users[_currentUserId]->openLocalFolder();
}

void UserModel::openCurrentAccountServer()
{
    if (_currentUserId < 0 || _currentUserId >= _users.size())
        return;

    QString url = _users[_currentUserId]->server(false);
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        url = "https://" + _users[_currentUserId]->server(false);
    }

    QDesktopServices::openUrl(url);
}

void UserModel::openCurrentAccountFolderFromTrayInfo(const QString &fullRemotePath)
{
    if (_currentUserId < 0 || _currentUserId >= _users.size()) {
        return;
    }

    _users[_currentUserId]->openFolderLocallyOrInBrowser(fullRemotePath);
}

void UserModel::openCurrentAccountFeaturedApp()
{
    if (!currentUser()) {
        return;
    }

    if (!currentUser()->isFeaturedAppEnabled()) {
        qCWarning(lcActivity) << "There is no feature app enabled on" << currentUser()->server();
        return;
    }

    if (currentUser()->isNcAssistantEnabled()) {
        auto serverUrl = currentUser()->server(false);
        const auto assistanceUrl = serverUrl.append("/apps/assistant/");
        QDesktopServices::openUrl(QUrl::fromUserInput(assistanceUrl));
        return;
    }

    if (const auto talkApp = currentUser()->talkApp()) {
        Utility::openBrowser(talkApp->url());
    }
}


void UserModel::setCurrentUserId(const int id)
{
    Q_ASSERT(id < _users.size());

    if (id < 0 || id >= _users.size()) {
        if (id < 0 && _currentUserId != id) {
            _currentUserId = id;
            emit currentUserChanged();
        }
        return;
    }

    const auto isCurrentUserChanged = !_users[id]->isCurrentUser();
    if (isCurrentUserChanged) {
        for (const auto user : std::as_const(_users)) {
            user->setCurrentUser(false);
        }
        _users[id]->setCurrentUser(true);
    }

    if (_currentUserId == id && isCurrentUserChanged) {
        // order has changed, index remained the same
        emit currentUserChanged();
    } else if (_currentUserId != id) {
        ConfigFile cfg;
        cfg.setLastSelectedAccount(_users[id]->account()->id().toUInt());
        _currentUserId = id;
        emit currentUserChanged();
    }
}

void UserModel::login(const int id)
{
    if (id < 0 || id >= _users.size())
        return;

    _users[id]->login();
}

void UserModel::logout(const int id)
{
    if (id < 0 || id >= _users.size())
        return;

    _users[id]->logout();
}

void UserModel::removeAccount(const int id)
{
    if (id < 0 || id >= _users.size()) {
        return;
    }

    QMessageBox messageBox(QMessageBox::Question,
                           tr("Confirm Account Removal"),
                           tr("<p>Do you really want to remove the connection to the account <i>%1</i>?</p>"
                              "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
                               .arg(_users[id]->name()),
                           QMessageBox::NoButton);
    const auto * const yesButton = messageBox.addButton(tr("Remove connection"), QMessageBox::YesRole);
    messageBox.addButton(tr("Cancel"), QMessageBox::NoRole);

    messageBox.exec();
    if (messageBox.clickedButton() != yesButton) {
        return;
    }

    _users[id]->logout();
    _users[id]->removeAccount();

    beginRemoveRows(QModelIndex(), id, id);
    _users.removeAt(id);
    endRemoveRows();

    if (_users.size() <= 1) {
        setCurrentUserId(_users.size() - 1);
    } else if (currentUserId() > id) {
        // an account was removed from the in-between 0 and the current one, the index of the current one needs a decrement
        setCurrentUserId(currentUserId() - 1);
    } else if (currentUserId() == id) {
        setCurrentUserId(id < _users.size() ? id : id - 1);
    }
}

std::shared_ptr<OCC::UserStatusConnector> UserModel::userStatusConnector(int id)
{
    if (id < 0 || id >= _users.size()) {
        return nullptr;
    }

    return _users[id]->account()->userStatusConnector();
}

int UserModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return _users.count();
}

QVariant UserModel::data(const QModelIndex &index, int role) const
{
    auto result = QVariant{};
    switch (static_cast<UserRoles>(role))
    {
    case NameRole:
        result = _users[index.row()]->name();
        break;
    case ServerRole:
        result = _users[index.row()]->server();
        break;
    case ServerHasUserStatusRole:
        result = _users[index.row()]->serverHasUserStatus();
        break;
    case StatusRole:
        result = QVariant::fromValue(_users[index.row()]->status());
        break;
    case StatusIconRole:
        result = _users[index.row()]->statusIcon();
        break;
    case StatusEmojiRole:
        result = _users[index.row()]->statusEmoji();
        break;
    case StatusMessageRole:
        result = _users[index.row()]->statusMessage();
        break;
    case DesktopNotificationsAllowedRole:
        result = _users[index.row()]->isDesktopNotificationsAllowed();
        break;
    case AvatarRole:
        result = _users[index.row()]->avatarUrl();
        break;
    case IsCurrentUserRole:
        result = _users[index.row()]->isCurrentUser();
        break;
    case IsConnectedRole:
        result = _users[index.row()]->isConnected();
        break;
    case IdRole:
        result = index.row();
        break;
    case CanLogoutRole:
        result = _users[index.row()]->canLogout();
        break;
    case RemoveAccountTextRole:
        result = _users[index.row()]->isPublicShareLink() ? tr("Leave share") : tr("Remove account");
        break;
    }

    return result;
}

QHash<int, QByteArray> UserModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[ServerRole] = "server";
    roles[ServerHasUserStatusRole] = "serverHasUserStatus";
    roles[StatusRole] = "status";
    roles[StatusIconRole] = "statusIcon";
    roles[StatusEmojiRole] = "statusEmoji";
    roles[StatusMessageRole] = "statusMessage";
    roles[DesktopNotificationsAllowedRole] = "desktopNotificationsAllowed";
    roles[AvatarRole] = "avatar";
    roles[IsCurrentUserRole] = "isCurrentUser";
    roles[IsConnectedRole] = "isConnected";
    roles[IdRole] = "id";
    roles[CanLogoutRole] = "canLogout";
    roles[RemoveAccountTextRole] = "removeAccountText";
    return roles;
}

ActivityListModel *UserModel::currentActivityModel()
{
    if (currentUserIndex() < 0 || currentUserIndex() >= _users.size())
        return nullptr;

    return _users[currentUserIndex()]->getActivityModel();
}

void UserModel::fetchCurrentActivityModel()
{
    if (currentUserId() < 0 || currentUserId() >= _users.size())
        return;

    _users[currentUserId()]->slotRefresh();
}

AccountAppList UserModel::appList() const
{
    if (_currentUserId < 0 || _currentUserId >= _users.size())
        return {};

    return _users[_currentUserId]->appList();
}

User *UserModel::currentUser() const
{
    if (currentUserId() < 0 || currentUserId() >= _users.size())
        return nullptr;

    return _users[currentUserId()];
}

User *UserModel::findUserForAccount(AccountState *account) const
{
    Q_ASSERT(account);

    const auto it = std::find_if(_users.cbegin(), _users.cend(), [account](const User *user) {
        return user->account()->id() == account->account()->id();
    });

    if (it == _users.cend()) {
        return nullptr;
    }

    return *it;
}

int UserModel::findUserIdForAccount(AccountState *account) const
{
    const auto it = std::find_if(std::cbegin(_users), std::cend(_users), [=](const User *user) {
        return user->account()->id() == account->account()->id();
    });

    if (it == std::cend(_users)) {
        return -1;
    }

    const auto id = std::distance(std::cbegin(_users), it);
    return id;
}
/*-------------------------------------------------------------------------------------*/

class ImageResponse : public QQuickImageResponse
{
public:
    ImageResponse(const QString &id, const QSize &requestedSize, QThreadPool *pool)
    {
        Q_UNUSED(pool)

        const auto makeIcon = [](const QString &path) {
            QImage image(128, 128, QImage::Format_ARGB32);
            image.fill(Qt::GlobalColor::transparent);
            QPainter painter(&image);
            QSvgRenderer renderer(path);
            renderer.render(&painter);
            return image;
        };

        if (id == QLatin1String("fallbackWhite")) {
            handleDone(makeIcon(QStringLiteral(":/client/theme/white/user.svg")));
            return;
        } else if (id == QLatin1String("fallbackBlack")) {
            handleDone(makeIcon(QStringLiteral(":/client/theme/black/user.svg")));
            return;
        }

        if (id.startsWith("user-id=")) {
            // Format is "image://avatars/user-id=avatar-requested-user/local-user-id:0"
            const auto userIdsString = id.split('=');
            const auto userIds = userIdsString.last().split("/local-account:");
            const auto avatarUserId = userIds.first();
            const auto accountString = userIds.last();
            const auto accountState = AccountManager::instance()->account(accountString);
            Q_ASSERT(accountState);
            Q_ASSERT(accountState->account());
            if (!accountState || !accountState->account()) {
                qCWarning(lcActivity) << "Invalid account:" << accountString;
                return;
            }

            const auto account = accountState->account();
            const auto qnam = account->networkAccessManager();

            QMetaObject::invokeMethod(qnam, [this, requestedSize, avatarUserId, account]() {
                const auto avatarSize = requestedSize.width() > 0 ? requestedSize.width() : 64;
                const auto avatarJob = new AvatarJob(account, avatarUserId, avatarSize);
                connect(avatarJob, &AvatarJob::avatarPixmap, this, [&](const QImage &avatarImg) {
                    QMetaObject::invokeMethod(this, [this, avatarImg] {
                        handleDone(AvatarJob::makeCircularAvatar(avatarImg));
                    });
                });
                avatarJob->start();
            });
            return;
        }

        handleDone(UserModel::instance()->avatarById(id.toInt()));
    }

    void handleDone(const QImage &image)
    {
        _image = image;
        emit finished();
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(_image);
    }

private:
    QImage _image;
};

QQuickImageResponse *ImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    const auto response = new class ImageResponse(id, requestedSize, &_pool);
    return response;
}

/*-------------------------------------------------------------------------------------*/

UserAppsModel *UserAppsModel::_instance = nullptr;

UserAppsModel *UserAppsModel::instance()
{
    if (!_instance) {
        _instance = new UserAppsModel();
    }
    return _instance;
}

UserAppsModel::UserAppsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void UserAppsModel::buildAppList()
{
    if (rowCount() > 0) {
        beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
        _apps.clear();
        endRemoveRows();
    }

    if (UserModel::instance()->appList().count() > 0) {
        const auto talkApp = UserModel::instance()->currentUser()->talkApp();
        const auto &allApps = UserModel::instance()->appList();
        for (const auto &app : allApps) {
            // Filter out Talk because we have a dedicated button for it
            if (talkApp && app->id() == talkApp->id() && !UserModel::instance()->currentUser()->isNcAssistantEnabled()) {
                continue;
            }

            beginInsertRows(QModelIndex(), rowCount(), rowCount());
            _apps << app;
            endInsertRows();
        }
    }
}

void UserAppsModel::openAppUrl(const QUrl &url)
{
    Utility::openBrowser(url);
}

int UserAppsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return _apps.count();
}

QVariant UserAppsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= _apps.count()) {
        return QVariant();
    }

    if (role == NameRole) {
        return _apps[index.row()]->name();
    } else if (role == UrlRole) {
        return _apps[index.row()]->url();
    } else if (role == IconUrlRole) {
        return _apps[index.row()]->iconUrl().toString();
    }
    return QVariant();
}

QHash<int, QByteArray> UserAppsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "appName";
    roles[UrlRole] = "appUrl";
    roles[IconUrlRole] = "appIconUrl";
    return roles;
}
}

