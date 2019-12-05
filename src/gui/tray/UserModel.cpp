#include "accountmanager.h"
#include "owncloudgui.h"
#include "UserModel.h"

#include <QIcon>

namespace OCC {

User::User(AccountStatePtr &account, const bool &isCurrent)
    : _account(account)
    , _isCurrentUser(isCurrent)
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

QString User::server() const
{
    QString serverUrl = _account->account()->url().toString();
    serverUrl.replace(QLatin1String("https://"), QLatin1String(""));
    serverUrl.replace(QLatin1String("http://"), QLatin1String(""));
    if (serverUrl.size() > 21) {
        serverUrl.truncate(19);
        serverUrl.append("...");
    }
    return serverUrl;
}

QImage User::avatar() const
{
    QImage img = AvatarJob::makeCircularAvatar(_account->account()->avatar());
    if (img.isNull()) {
        img = QImage(":/client/resources/account.svg");
    }
    return img;
}

bool User::isCurrentUser() const
{
    return _isCurrentUser;
}

bool User::isConnected() const
{
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
    //  this is the reason why this looks like an unnecessary double check atm
    /*if (AccountManager::instance()->accounts().size() > 0) {
        addUser(AccountManager::instance()->accounts().first(), true);
    } else {
        return;
    }*/
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

QImage UserModel::currentUserAvatar()
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

void UserModel::addUser(AccountStatePtr &user, const bool &isCurrent)
{
    auto newUser = User(user, isCurrent);
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    _users << newUser;
    if (isCurrent) {
        _currentUserId = _users.indexOf(newUser);
    }
    endInsertRows();
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