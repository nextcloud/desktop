#include "accountmanager.h"
#include "owncloudgui.h"
#include "UserModel.h"

#include <QDesktopServices>
#include <QIcon>

namespace OCC {

User::User(AccountStatePtr &account, const bool &isCurrent)
    : _account(account)
    , _isCurrentUser(isCurrent)
    , _activityModel(new ActivityListModel(_account.data()))
{
}

bool User::operator==(const User &rhs) const
{
    return (this->_account->account() == rhs._account->account());
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
    QDesktopServices::openUrl(this->getFolder()->path());
}

QString User::name() const
{
    // If davDisplayName is empty (can be several reasons, simplest is missing login at startup), fall back to username
    QString name = _account->account()->davDisplayName();
    if (name == "") {
        name = _account->account()->credentials()->user();
    }
    if (name.size() > 19) {
        name.truncate(17);
        name.append("...");
    }
    return name;
}

QString User::server(bool shortened) const
{
    QString serverUrl = _account->account()->url().toString();
    if (shortened) {
        serverUrl.replace(QLatin1String("https://"), QLatin1String(""));
        serverUrl.replace(QLatin1String("http://"), QLatin1String(""));
        if (serverUrl.size() > 21) {
            serverUrl.truncate(19);
            serverUrl.append("...");
        }
    }
    return serverUrl;
}

QImage User::avatar() const
{
    QImage img = AvatarJob::makeCircularAvatar(_account->account()->avatar());
    if (img.isNull()) {
        img = AvatarJob::makeCircularAvatar(QImage(":/client/resources/account.png"));
    }
    return img;
}

bool User::serverHasTalk() const
{
    auto test = _account->hasTalk();
    return _account->hasTalk();
}

bool User::isCurrentUser() const
{
    return _isCurrentUser;
}

bool User::isConnected() const
{
    bool test = (_account->connectionStatus() == AccountState::ConnectionStatus::Connected);
    return (_account->connectionStatus() == AccountState::ConnectionStatus::Connected);
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
    : QAbstractListModel()
    , _currentUserId()
{
    // TODO: Remember selected user from last quit via settings file
    if (AccountManager::instance()->accounts().size() > 0) {
        initUserList();
    }
}

void UserModel::initUserList()
{
    for (int i = 0; i < AccountManager::instance()->accounts().size(); i++) {
        auto user = AccountManager::instance()->accounts().at(i);
        addUser(user);
    }
    _users.first().setCurrentUser(true);
}

Q_INVOKABLE int UserModel::numUsers()
{
    return _users.size();
}

Q_INVOKABLE bool UserModel::isCurrentUserConnected()
{
    return _users[_currentUserId].isConnected();
}

Q_INVOKABLE QImage UserModel::currentUserAvatar()
{
    return _users[_currentUserId].avatar();
}

QImage UserModel::avatarById(const int &id)
{
    return _users[id].avatar();
}

Q_INVOKABLE QString UserModel::currentUserName()
{
    return _users[_currentUserId].name();
}

Q_INVOKABLE QString UserModel::currentUserServer()
{
    return _users[_currentUserId].server();
}

Q_INVOKABLE bool UserModel::currentServerHasTalk()
{
    return _users[_currentUserId].serverHasTalk();
}

void UserModel::addUser(AccountStatePtr &user, const bool &isCurrent)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    _users << User(user, isCurrent);
    if (isCurrent) {
        _currentUserId = _users.indexOf(_users.last());
    }
    endInsertRows();
}

int UserModel::currentUserIndex()
{
    return _currentUserId;
}

Q_INVOKABLE void UserModel::openCurrentAccountLocalFolder()
{
    _users[_currentUserId].openLocalFolder();
}

Q_INVOKABLE void UserModel::openCurrentAccountTalk()
{
    QString url = _users[_currentUserId].server(false) + "/apps/spreed";
    if (!(url.contains("http://") || url.contains("https://"))) {
        url = "https://" + _users[_currentUserId].server(false) + "/apps/spreed";
    }
    QDesktopServices::openUrl(QUrl(url));
}

Q_INVOKABLE void UserModel::openCurrentAccountServer()
{
    QString url = _users[_currentUserId].server(false);
    if (! (url.contains("http://") || url.contains("https://")) ) {
        url = "https://" + _users[_currentUserId].server(false);
    }
    QDesktopServices::openUrl(QUrl(url));
}

Q_INVOKABLE void UserModel::switchCurrentUser(const int &id)
{
    _users[_currentUserId].setCurrentUser(false);
    _users[id].setCurrentUser(true);
    _currentUserId = id;
    emit newUserSelected();
    emit refreshUserMenu();
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

    const User &user = _users[index.row()];
    if (role == NameRole) {
        return user.name();
    } else if (role == ServerRole) {
        return user.server();
    } else if (role == AvatarRole) {
        return user.avatar();
    } else if (role == IsCurrentUserRole) {
        return user.isCurrentUser();
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
    return roles;
}

ActivityListModel *UserModel::currentActivityModel()
{
    return _users[currentUserIndex()].getActivityModel();
}

/*-------------------------------------------------------------------------------------*/

ImageProvider::ImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (id == "currentUser") {
        return UserModel::instance()->currentUserAvatar();
    } else {
        int uid = id.toInt();
        return UserModel::instance()->avatarById(uid);
    }
}

}