#include "NotificationHandler.h"
#include "UserModel.h"

#include "accountmanager.h"
#include "owncloudgui.h"
#include "syncengine.h"
#include "ocsjob.h"
#include "configfile.h"
#include "notificationconfirmjob.h"

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
            [=]() { if (isConnected()) {slotRefresh();} });
    connect(_account.data(), &AccountState::hasFetchedNavigationApps,
        this, &User::slotRebuildNavigationAppList);
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
            if (cfg.optionalServerNotifications()) {
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
    qCDebug(lcActivity) << "Starting Notification refresh timer with " << interval.count() / 1000 << " sec interval";
    _notificationCheckTimer.start(interval.count());
}

void User::slotRefreshImmediately() {
    if (_account.data() && _account.data()->isConnected()) {
        this->slotRefreshActivities();
    }
    this->slotRefreshNotifications();
}

void User::slotRefresh()
{
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
            this->slotRefreshActivities();
        }
        this->slotRefreshNotifications();
        timer.start();
    }
}

void User::slotRefreshActivities()
{
    _activityModel->slotRefreshActivity();
}

void User::slotRefreshNotifications()
{
    // start a server notification handler if no notification requests
    // are running
    if (_notificationRequestsRunning == 0) {
        ServerNotificationHandler *snh = new ServerNotificationHandler(_account.data());
        connect(snh, &ServerNotificationHandler::newNotificationList,
            this, &User::slotBuildNotificationDisplay);

        snh->slotFetchNotifications();
    } else {
        qCWarning(lcActivity) << "Notification request counter not zero.";
    }
}

void User::slotRebuildNavigationAppList()
{
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
            NotificationConfirmJob *job = new NotificationConfirmJob(acc->account());
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
    NotificationConfirmJob *job = qobject_cast<NotificationConfirmJob *>(sender());
    if (!job) {
        return;
    }

    int resultCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    slotEndNotificationRequest(resultCode);
    qCWarning(lcActivity) << "Server notify job failed with code " << resultCode;
}

void User::slotNotifyServerFinished(const QString &reply, int replyCode)
{
    NotificationConfirmJob *job = qobject_cast<NotificationConfirmJob *>(sender());
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
            link._isPrimary = true;
            activity._links.append(link);
        }

        // add 'other errors' to activity list
        _activityModel->addErrorToActivityList(activity);
    }
}

void User::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    auto folderInstance = FolderMan::instance()->folder(folder);

    if (!folderInstance)
        return;

    // check if we are adding it to the right account and if it is useful information (protocol errors)
    if (folderInstance->accountState() == _account.data()) {
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
            
            if (activity._fileAction == "file_renamed") {
                activity._message.prepend(tr("You renamed") + " ");
            } else if (activity._fileAction == "file_deleted") {
                activity._message.prepend(tr("You deleted") + " ");
            } else if (activity._fileAction == "file_created") {
                activity._message.prepend(tr("You created") + " ");
            } else {
                activity._message.prepend(tr("You changed") + " ");
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
}

AccountPtr User::account() const
{
    return _account->account();
}

void User::setCurrentUser(const bool &isCurrent)
{
    _isCurrentUser = isCurrent;
}

Folder *User::getFolder()
{
    foreach (Folder *folder, FolderMan::instance()->map()) {
        if (folder->accountState() == _account.data()) {
            return folder;
        }
    }
}

ActivityListModel *User::getActivityModel()
{
    return _activityModel;
}

void User::openLocalFolder()
{
#ifdef Q_OS_WIN
    QString path = "file:///" + this->getFolder()->path();
#else
    QString path = "file://" + this->getFolder()->path();
#endif
    QDesktopServices::openUrl(path);
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

QImage User::avatar(bool whiteBg) const
{
    QImage img = AvatarJob::makeCircularAvatar(_account->account()->avatar());
    if (img.isNull()) {
        QImage image(128, 128, QImage::Format_ARGB32);
        image.fill(Qt::GlobalColor::transparent);
        QPainter painter(&image);

        QSvgRenderer renderer(QString(whiteBg ? ":/client/theme/black/user.svg" : ":/client/theme/white/user.svg"));
        renderer.render(&painter);

        return image;
    } else {
        return img;
    }
}

bool User::serverHasTalk() const
{
    return _account->hasTalk();
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

void User::removeAccount() const
{
    AccountManager::instance()->deleteAccount(_account.data());
    AccountManager::instance()->save();
}

/*-------------------------------------------------------------------------------------*/

UserModel *UserModel::_instance = nullptr;

UserModel *UserModel::instance()
{
    if (_instance == nullptr) {
        _instance = new UserModel();
    }
    return _instance;
}

UserModel::UserModel(QObject *parent)
    : QAbstractListModel(parent)
    , _currentUserId()
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
        _init = false;
    }
}

Q_INVOKABLE int UserModel::numUsers()
{
    return _users.size();
}

Q_INVOKABLE int UserModel::currentUserId()
{
    return _currentUserId;
}

Q_INVOKABLE bool UserModel::isUserConnected(const int &id)
{
    if (!_users.isEmpty()) {
        return _users[id]->isConnected();
    } else {
        return false;
    }

}

Q_INVOKABLE QImage UserModel::currentUserAvatar()
{
    if (_users.count() >= 1) {
        return _users[_currentUserId]->avatar();
    } else {
        QImage image(128, 128, QImage::Format_ARGB32);
        image.fill(Qt::GlobalColor::transparent);
        QPainter painter(&image);
        QSvgRenderer renderer(QString(":/client/theme/white/user.svg"));
        renderer.render(&painter);

        return image;
    }
}

QImage UserModel::avatarById(const int &id)
{
    return _users[id]->avatar(true);
}

Q_INVOKABLE QString UserModel::currentUserName()
{
    if (_users.count() >= 1) {
        return _users[_currentUserId]->name();
    } else {
        return QString("No users");
    }
}

Q_INVOKABLE QString UserModel::currentUserServer()
{
    if (_users.count() >= 1) {
        return _users[_currentUserId]->server();
    } else {
        return QString("");
    }
}

Q_INVOKABLE bool UserModel::currentServerHasTalk()
{
    if (_users.count() >= 1) {
        return _users[_currentUserId]->serverHasTalk();
    } else {
        return false;
    }
}

void UserModel::addUser(AccountStatePtr &user, const bool &isCurrent)
{
    bool containsUser = false;
    for (int i = 0; i < _users.size(); i++) {
        if (_users[i]->account() == user->account()) {
            containsUser = true;
            continue;
        }
    }

    if (!containsUser) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        _users << new User(user, isCurrent);
        if (isCurrent) {
            _currentUserId = _users.indexOf(_users.last());
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
    _users[_currentUserId]->openLocalFolder();
}

Q_INVOKABLE void UserModel::openCurrentAccountTalk()
{
    QString url = _users[_currentUserId]->server(false) + "/apps/spreed";
    if (!(url.contains("http://") || url.contains("https://"))) {
        url = "https://" + _users[_currentUserId]->server(false) + "/apps/spreed";
    }
    QDesktopServices::openUrl(QUrl(url));
}

Q_INVOKABLE void UserModel::openCurrentAccountServer()
{
    // Don't open this URL when the QML appMenu pops up on click (see Window.qml)
    if(appList().count() > 0)
        return;

    QString url = _users[_currentUserId]->server(false);
    if (!(url.contains("http://") || url.contains("https://"))) {
        url = "https://" + _users[_currentUserId]->server(false);
    }
    QDesktopServices::openUrl(QUrl(url));
}

Q_INVOKABLE void UserModel::switchCurrentUser(const int &id)
{
    _users[_currentUserId]->setCurrentUser(false);
    _users[id]->setCurrentUser(true);
    _currentUserId = id;
    emit refreshCurrentUserGui();
    emit newUserSelected();
}

Q_INVOKABLE void UserModel::login(const int &id)
{
    _users[id]->login();
    emit refreshCurrentUserGui();
}

Q_INVOKABLE void UserModel::logout(const int &id)
{
    _users[id]->logout();
    emit refreshCurrentUserGui();
}

Q_INVOKABLE void UserModel::removeAccount(const int &id)
{
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
    } else if (role == AvatarRole) {
        return _users[index.row()]->avatar();
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
    roles[AvatarRole] = "avatar";
    roles[IsCurrentUserRole] = "isCurrentUser";
    roles[IsConnectedRole] = "isConnected";
    roles[IdRole] = "id";
    return roles;
}

ActivityListModel *UserModel::currentActivityModel()
{
    return _users[currentUserIndex()]->getActivityModel();
}

bool UserModel::currentUserHasActivities()
{
    return _users[currentUserIndex()]->hasActivities();
}

void UserModel::fetchCurrentActivityModel()
{
    _users[currentUserId()]->slotRefresh();
}

AccountAppList UserModel::appList() const
{
    if (_users.count() >= 1) {
        return _users[_currentUserId]->appList();
    } else {
        return AccountAppList();
    }
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

    if (id == "currentUser") {
        return UserModel::instance()->currentUserAvatar();
    } else {
        int uid = id.toInt();
        return UserModel::instance()->avatarById(uid);
    }
}

/*-------------------------------------------------------------------------------------*/

UserAppsModel *UserAppsModel::_instance = nullptr;

UserAppsModel *UserAppsModel::instance()
{
    if (_instance == nullptr) {
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

    if(UserModel::instance()->appList().count() > 0) {
        foreach(AccountApp *app, UserModel::instance()->appList()) {
            // Filter out Talk because we have a dedicated button for it
            if(app->id() == QLatin1String("spreed"))
                continue;

            beginInsertRows(QModelIndex(), rowCount(), rowCount());
            _apps << app;
            endInsertRows();
        }
    }
}

void UserAppsModel::openAppUrl(const QUrl &url)
{
    QDesktopServices::openUrl(url);
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
