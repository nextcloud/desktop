#include "NotificationHandler.h"
#include "UserModel.h"

#include "accountmanager.h"
#include "owncloudgui.h"
#include <pushnotifications.h>
#include "syncengine.h"
#include "ocsjob.h"
#include "configfile.h"
#include "notificationconfirmjob.h"
#include "logger.h"
#include "guiutility.h"

#include <QDesktopServices>
#include <QIcon>
#include <QMessageBox>
#include <QSvgRenderer>
#include <QPainter>
#include <QPushButton>

// time span in milliseconds which has to be between two
// refreshes of the notifications
#define NOTIFICATION_REQUEST_FREE_PERIOD 15000

namespace OCC {

User::User(AccountStatePtr &account, const bool &isCurrent, QObject *parent)
    : QObject(parent)
    , _account(account)
    , _isCurrentUser(isCurrent)
    , _activityModel(new ActivityListModel(_account.data()))
    , _notificationRequestsRunning(0)
{
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::progressInfo,
        this, &User::slotProgressInfo);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::itemCompleted,
        this, &User::slotItemCompleted);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::syncError,
        this, &User::slotAddError);

    connect(&_notificationCheckTimer, &QTimer::timeout,
        this, &User::slotRefresh);

    connect(_account.data(), &AccountState::stateChanged,
            [=]() { if (isConnected()) {slotRefreshImmediately();} });
    connect(_account.data(), &AccountState::stateChanged, this, &User::accountStateChanged);
    connect(_account.data(), &AccountState::hasFetchedNavigationApps,
        this, &User::slotRebuildNavigationAppList);
    connect(_account->account().data(), &Account::accountChangedDisplayName, this, &User::nameChanged);

    connect(FolderMan::instance(), &FolderMan::folderListChanged, this, &User::hasLocalFolderChanged);

    connect(this, &User::guiLog, Logger::instance(), &Logger::guiLog);

    connect(_account->account().data(), &Account::accountChangedAvatar, this, &User::avatarChanged);
    connect(_account.data(), &AccountState::statusChanged, this, &User::statusChanged);

    connect(_activityModel, &ActivityListModel::sendNotificationRequest, this, &User::slotSendNotificationRequest);
}

void User::slotBuildNotificationDisplay(const ActivityList &list)
{
    // Whether a new notification was added to the list
    bool newNotificationShown = false;

    _activityModel->clearNotifications();

    foreach (auto activity, list) {
        if (_blacklistedNotifications.contains(activity)) {
            qCInfo(lcActivity) << "Activity in blacklist, skip";
            continue;
        }

        // handle gui logs. In order to NOT annoy the user with every fetching of the
        // notifications the notification id is stored in a Set. Only if an id
        // is not in the set, it qualifies for guiLog.
        // Important: The _guiLoggedNotifications set must be wiped regularly which
        // will repeat the gui log.

        // after one hour, clear the gui log notification store
        if (_guiLogTimer.elapsed() > 60 * 60 * 1000) {
            _guiLoggedNotifications.clear();
        }

        if (!_guiLoggedNotifications.contains(activity._id)) {
            newNotificationShown = true;
            _guiLoggedNotifications.insert(activity._id);

            // Assemble a tray notification for the NEW notification
            ConfigFile cfg;
            if (cfg.optionalServerNotifications() && isDesktopNotificationsAllowed()) {
                if (AccountManager::instance()->accounts().count() == 1) {
                    emit guiLog(activity._subject, "");
                } else {
                    emit guiLog(activity._subject, activity._accName);
                }
            }
        }

        _activityModel->addNotificationToActivityList(activity);
    }

    // restart the gui log timer now that we show a new notification
    if (newNotificationShown) {
        _guiLogTimer.start();
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

    // connection to WebSocket may have dropped or an error occured, so we need to bring back the polling until we have re-established the connection
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
    if (_account.data() && _account.data()->isConnected()) {
        slotRefreshActivities();
    }
    slotRefreshNotifications();
}

void User::slotRefresh()
{
    slotRefreshUserStatus();
    
    if (checkPushNotificationsAreReady()) {
        // we are relying on WebSocket push notifications - ignore refresh attempts from UI
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
        if (!timer.isValid()) {
            slotRefreshActivities();
        }
        slotRefreshNotifications();
        timer.start();
    }
}

void User::slotRefreshActivities()
{
    _activityModel->slotRefreshActivity();
}

void User::slotRefreshUserStatus() 
{
    // TODO: check for _account->account()->capabilities().userStatus() 
    if (_account.data() && _account.data()->isConnected()) {
        _account.data()->fetchUserStatus();
    }
}

void User::slotRefreshNotifications()
{
    // start a server notification handler if no notification requests
    // are running
    if (_notificationRequestsRunning == 0) {
        auto *snh = new ServerNotificationHandler(_account.data());
        connect(snh, &ServerNotificationHandler::newNotificationList,
            this, &User::slotBuildNotificationDisplay);

        snh->slotFetchNotifications();
    } else {
        qCWarning(lcActivity) << "Notification request counter not zero.";
    }
}

void User::slotRebuildNavigationAppList()
{
    emit serverHasTalkChanged();
    // Rebuild App list
    UserAppsModel::instance()->buildAppList();
}

void User::slotNotificationRequestFinished(int statusCode)
{
    int row = sender()->property("activityRow").toInt();

    // the ocs API returns stat code 100 or 200 inside the xml if it succeeded.
    if (statusCode != OCS_SUCCESS_STATUS_CODE && statusCode != OCS_SUCCESS_STATUS_CODE_V2) {
        qCWarning(lcActivity) << "Notification Request to Server failed, leave notification visible.";
    } else {
        // to do use the model to rebuild the list or remove the item
        qCWarning(lcActivity) << "Notification Request to Server successed, rebuilding list.";
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
        foreach (Activity activity, _activityModel->errorsList()) {
            if (activity._folder != folder) {
                continue;
            }

            if (style == LocalDiscoveryStyle::FilesystemOnly) {
                _activityModel->removeActivityFromActivityList(activity);
                continue;
            }

            if (activity._status == SyncFileItem::Conflict && !QFileInfo(f->path() + activity._file).exists()) {
                _activityModel->removeActivityFromActivityList(activity);
                continue;
            }

            if (activity._status == SyncFileItem::FileLocked && !QFileInfo(f->path() + activity._file).exists()) {
                _activityModel->removeActivityFromActivityList(activity);
                continue;
            }


            if (activity._status == SyncFileItem::FileIgnored && !QFileInfo(f->path() + activity._file).exists()) {
                _activityModel->removeActivityFromActivityList(activity);
                continue;
            }


            if (!QFileInfo(f->path() + activity._file).exists()) {
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
        foreach (Activity activity, _activityModel->errorsList()) {
            if (activity._folder == folder
                && activity._status == SyncFileItem::Conflict) {
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
        activity._status = SyncResult::Error;
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

        // add 'other errors' to activity list
        _activityModel->addErrorToActivityList(activity);
    }
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

void User::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    auto folderInstance = FolderMan::instance()->folder(folder);

    if (!folderInstance)
        return;

    if (!isActivityOfCurrentAccount(folderInstance)) {
        return;
    }

    if (isUnsolvableConflict(item)) {
        return;
    }

    qCWarning(lcActivity) << "Item " << item->_file << " retrieved resulted in " << item->_errorString;

    Activity activity;
    activity._type = Activity::SyncFileItemType; //client activity
    activity._status = item->_status;
    activity._dateTime = QDateTime::currentDateTime();
    activity._message = item->_originalFile;
    activity._link = folderInstance->accountState()->account()->url();
    activity._accName = folderInstance->accountState()->account()->displayName();
    activity._file = item->_file;
    activity._folder = folder;
    activity._fileAction = "";

    if (item->_instruction == CSYNC_INSTRUCTION_REMOVE) {
        activity._fileAction = "file_deleted";
    } else if (item->_instruction == CSYNC_INSTRUCTION_NEW) {
        activity._fileAction = "file_created";
    } else if (item->_instruction == CSYNC_INSTRUCTION_RENAME) {
        activity._fileAction = "file_renamed";
    } else {
        activity._fileAction = "file_changed";
    }

    if (item->_status == SyncFileItem::NoStatus || item->_status == SyncFileItem::Success) {
        qCWarning(lcActivity) << "Item " << item->_file << " retrieved successfully.";

        if (item->_direction != SyncFileItem::Up) {
            activity._message = tr("Synced %1").arg(item->_originalFile);
        } else if (activity._fileAction == "file_renamed") {
            activity._message = tr("You renamed %1").arg(item->_originalFile);
        } else if (activity._fileAction == "file_deleted") {
            activity._message = tr("You deleted %1").arg(item->_originalFile);
        } else if (activity._fileAction == "file_created") {
            activity._message = tr("You created %1").arg(item->_originalFile);
        } else {
            activity._message = tr("You changed %1").arg(item->_originalFile);
        }

        _activityModel->addSyncFileItemToActivityList(activity);
    } else {
        qCWarning(lcActivity) << "Item " << item->_file << " retrieved resulted in error " << item->_errorString;
        activity._subject = item->_errorString;

        if (item->_status == SyncFileItem::Status::FileIgnored) {
            _activityModel->addIgnoredFileToList(activity);
        } else {
            // add 'protocol error' to activity list
            _activityModel->addErrorToActivityList(activity);
        }
    }
}

AccountPtr User::account() const
{
    return _account->account();
}

void User::setCurrentUser(const bool &isCurrent)
{
    _isCurrentUser = isCurrent;
}

Folder *User::getFolder() const
{
    foreach (Folder *folder, FolderMan::instance()->map()) {
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

void User::openLocalFolder()
{
    const auto folder = getFolder();

    if (folder) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder->path()));
    }
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
    // If davDisplayName is empty (can be several reasons, simplest is missing login at startup), fall back to username
    QString name = _account->account()->davDisplayName();
    if (name == "") {
        name = _account->account()->credentials()->user();
    }
    return name;
}

QString User::server(bool shortened) const
{
    QString serverUrl = _account->account()->url().toString();
    if (shortened) {
        serverUrl.replace(QLatin1String("https://"), QLatin1String(""));
        serverUrl.replace(QLatin1String("http://"), QLatin1String(""));
    }
    return serverUrl;
}

UserStatus::Status User::status() const
{
    return _account->status();
}

QString User::statusMessage() const
{
    return _account->statusMessage();
}

QUrl User::statusIcon() const
{
    return _account->statusIcon();
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

AccountApp *User::talkApp() const
{
    return _account->findApp(QStringLiteral("spreed"));
}

bool User::hasActivities() const
{
    return _account->account()->capabilities().hasActivities();
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


bool User::isDesktopNotificationsAllowed() const
{
    return _account.data()->isDesktopNotificationsAllowed();
}

void User::removeAccount() const
{
    AccountManager::instance()->deleteAccount(_account.data());
    AccountManager::instance()->save();
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
    // TODO: Remember selected user from last quit via settings file
    if (AccountManager::instance()->accounts().size() > 0) {
        buildUserList();
    }

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &UserModel::buildUserList);
}

void UserModel::buildUserList()
{
    for (int i = 0; i < AccountManager::instance()->accounts().size(); i++) {
        auto user = AccountManager::instance()->accounts().at(i);
        addUser(user);
    }
    if (_init) {
        _users.first()->setCurrentUser(true);
        connect(_users.first(), &User::accountStateChanged, this, &UserModel::refreshCurrentUserGui);
        _init = false;
    }
}

Q_INVOKABLE int UserModel::numUsers()
{
    return _users.size();
}

Q_INVOKABLE int UserModel::currentUserId() const
{
    return _currentUserId;
}

Q_INVOKABLE bool UserModel::isUserConnected(const int &id)
{
    if (id < 0 || id >= _users.size())
        return false;

    return _users[id]->isConnected();
}

Q_INVOKABLE QUrl UserModel::statusIcon(int id)
{
    if (id < 0 || id >= _users.size()) {
        return {};
    }

    return _users[id]->statusIcon();
}


QImage UserModel::avatarById(const int &id)
{
    if (id < 0 || id >= _users.size())
        return {};

    return _users[id]->avatar();
}

Q_INVOKABLE QString UserModel::currentUserServer()
{
    if (_currentUserId < 0 || _currentUserId >= _users.size())
        return {};

    return _users[_currentUserId]->server();
}

void UserModel::addUser(AccountStatePtr &user, const bool &isCurrent)
{
    bool containsUser = false;
    for (const auto &u : qAsConst(_users)) {
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
            emit dataChanged(index(row, 0), index(row, 0), {UserModel::StatusIconRole, 
                                                            UserModel::StatusMessageRole});
        });

        _users << u;
        if (isCurrent) {
            _currentUserId = _users.indexOf(_users.last());
            connect(u, &User::accountStateChanged, this, &UserModel::refreshCurrentUserGui);
        }

        endInsertRows();
        ConfigFile cfg;
        _users.last()->setNotificationRefreshInterval(cfg.notificationRefreshInterval());
        emit newUserSelected();
    }
}

int UserModel::currentUserIndex()
{
    return _currentUserId;
}

Q_INVOKABLE void UserModel::openCurrentAccountLocalFolder()
{
    if (_currentUserId < 0 || _currentUserId >= _users.size())
        return;

    _users[_currentUserId]->openLocalFolder();
}

Q_INVOKABLE void UserModel::openCurrentAccountTalk()
{
    if (!currentUser())
        return;

    const auto talkApp = currentUser()->talkApp();
    if (talkApp) {
        Utility::openBrowser(talkApp->url());
    } else {
        qCWarning(lcActivity) << "The Talk app is not enabled on" << currentUser()->server();
    }
}

Q_INVOKABLE void UserModel::openCurrentAccountServer()
{
    if (_currentUserId < 0 || _currentUserId >= _users.size())
        return;

    QString url = _users[_currentUserId]->server(false);
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        url = "https://" + _users[_currentUserId]->server(false);
    }

    QDesktopServices::openUrl(url);
}

Q_INVOKABLE void UserModel::switchCurrentUser(const int &id)
{
    if (_currentUserId < 0 || _currentUserId >= _users.size())
        return;

    disconnect(_users[_currentUserId], &User::accountStateChanged, this, &UserModel::refreshCurrentUserGui);
    _users[_currentUserId]->setCurrentUser(false);
    _users[id]->setCurrentUser(true);
    connect(_users[id], &User::accountStateChanged, this, &UserModel::refreshCurrentUserGui);
    _currentUserId = id;
    emit refreshCurrentUserGui();
    emit newUserSelected();
}

Q_INVOKABLE void UserModel::login(const int &id)
{
    if (id < 0 || id >= _users.size())
        return;

    _users[id]->login();
    emit refreshCurrentUserGui();
}

Q_INVOKABLE void UserModel::logout(const int &id)
{
    if (id < 0 || id >= _users.size())
        return;

    _users[id]->logout();
    emit refreshCurrentUserGui();
}

Q_INVOKABLE void UserModel::removeAccount(const int &id)
{
    if (id < 0 || id >= _users.size())
        return;

    QMessageBox messageBox(QMessageBox::Question,
        tr("Confirm Account Removal"),
        tr("<p>Do you really want to remove the connection to the account <i>%1</i>?</p>"
           "<p><b>Note:</b> This will <b>not</b> delete any files.</p>")
            .arg(_users[id]->name()),
        QMessageBox::NoButton);
    QPushButton *yesButton =
        messageBox.addButton(tr("Remove connection"), QMessageBox::YesRole);
    messageBox.addButton(tr("Cancel"), QMessageBox::NoRole);

    messageBox.exec();
    if (messageBox.clickedButton() != yesButton) {
        return;
    }

    if (_users[id]->isCurrentUser()) {
        disconnect(_users[id], &User::accountStateChanged, this, &UserModel::refreshCurrentUserGui);
    }

    if (_users[id]->isCurrentUser() && _users.count() > 1) {
        id == 0 ? switchCurrentUser(1) : switchCurrentUser(0);
    }

    _users[id]->logout();
    _users[id]->removeAccount();

    beginRemoveRows(QModelIndex(), id, id);
    _users.removeAt(id);
    endRemoveRows();

    emit refreshCurrentUserGui();
}

int UserModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return _users.count();
}

QVariant UserModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= _users.count()) {
        return QVariant();
    }

    if (role == NameRole) {
        return _users[index.row()]->name();
    } else if (role == ServerRole) {
        return _users[index.row()]->server();
    } else if (role == StatusIconRole) {
        return _users[index.row()]->statusIcon();
    } else if (role == StatusMessageRole) {
        return _users[index.row()]->statusMessage();
    } else if (role == AvatarRole) {
        return _users[index.row()]->avatarUrl();
    } else if (role == IsCurrentUserRole) {
        return _users[index.row()]->isCurrentUser();
    } else if (role == IsConnectedRole) {
        return _users[index.row()]->isConnected();
    } else if (role == IdRole) {
        return index.row();
    }
    return QVariant();
}

QHash<int, QByteArray> UserModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[ServerRole] = "server";
    roles[StatusIconRole] = "statusIcon";
    roles[StatusMessageRole] = "statusMessage";
    roles[AvatarRole] = "avatar";
    roles[IsCurrentUserRole] = "isCurrentUser";
    roles[IsConnectedRole] = "isConnected";
    roles[IdRole] = "id";
    return roles;
}

ActivityListModel *UserModel::currentActivityModel()
{
    if (currentUserIndex() < 0 || currentUserIndex() >= _users.size())
        return nullptr;

    return _users[currentUserIndex()]->getActivityModel();
}

bool UserModel::currentUserHasActivities()
{
    if (currentUserIndex() < 0 || currentUserIndex() >= _users.size())
        return false;

    return _users[currentUserIndex()]->hasActivities();
}

bool UserModel::currentUserHasLocalFolder()
{
    if (currentUserIndex() < 0 || currentUserIndex() >= _users.size())
        return false;

    return _users[currentUserIndex()]->getFolder() != nullptr;
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

ImageProvider::ImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(size)
    Q_UNUSED(requestedSize)

    const auto makeIcon = [](const QString &path) {
        QImage image(128, 128, QImage::Format_ARGB32);
        image.fill(Qt::GlobalColor::transparent);
        QPainter painter(&image);
        QSvgRenderer renderer(path);
        renderer.render(&painter);
        return image;
    };

    if (id == QLatin1String("fallbackWhite")) {
        return makeIcon(QStringLiteral(":/client/theme/white/user.svg"));
    }

    if (id == QLatin1String("fallbackBlack")) {
        return makeIcon(QStringLiteral(":/client/theme/black/user.svg"));
    }

    const int uid = id.toInt();
    return UserModel::instance()->avatarById(uid);
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
        foreach (AccountApp *app, UserModel::instance()->appList()) {
            // Filter out Talk because we have a dedicated button for it
            if (talkApp && app->id() == talkApp->id())
                continue;

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
