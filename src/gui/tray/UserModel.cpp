#include "accountmanager.h"
#include "owncloudgui.h"
#include "UserModel.h"

#include <QIcon>

namespace OCC {

User::User(const AccountStatePtr &account)
    : _account(account)
{
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
    if (serverUrl.size() > 24) {
        serverUrl.truncate(22);
        serverUrl.append("...");
    }
    return serverUrl;
}

// TODO: Lots of memory shifting here
// Probably OK because the avatar is not changing a trillion times per second
// But should consider moving to a generic ImageProvider helper class for img/QML-provision
QString User::avatar() const
{
    QByteArray bArray;
    QBuffer buffer(&bArray);
    buffer.open(QIODevice::WriteOnly);
    AvatarJob::makeCircularAvatar(_account->account()->avatar()).save(&buffer, "PNG");

    // If AvatarJob doesn't deliver anything, fall back to placeholder image (may be due to missing login session)
    if (buffer.size() == 0) {
        QIcon(":/client/resources/account.svg").pixmap(QSize(250, 250)).save(&buffer, "PNG");
    }

    QString img("data:image/png;base64,");
    img.append(QString::fromLatin1(bArray.toBase64().data()));
    return img;
}

QString User::id() const
{
    return _account->account()->id();
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
    , _currentUser(nullptr)
{
    // TODO: Remember selected user from last quit via settings file
    //  this is the reason why this looks like an unnecessary double check atm
    if (AccountManager::instance()->accounts().size() > 0) {
        addCurrentUser(AccountManager::instance()->accounts().first());
    } else {
        return;
    }

    refreshUserList();
}

void UserModel::refreshUserList()
{
    for (int i = 0; i < AccountManager::instance()->accounts().size(); i++) {
        auto user = AccountManager::instance()->accounts().at(i);
        if ((user->account()->id() != _currentUser->id())) {
            addUser(user);
        }
    }
}

Q_INVOKABLE int UserModel::numUsers()
{
    auto test = _users.size();
    return _users.size();
}

Q_INVOKABLE bool UserModel::isCurrentUserConnected()
{
    return _currentUser->isConnected();
}

Q_INVOKABLE QString UserModel::currentUserAvatar()
{
    return _currentUser->avatar();
}

Q_INVOKABLE QString UserModel::currentUserName()
{
    return _currentUser->name();
}

Q_INVOKABLE QString UserModel::currentUserServer()
{
    return _currentUser->server();
}

Q_INVOKABLE void UserModel::switchUser(const int id)
{
    addCurrentUser(_users.at(id));
    refreshUserList();
    emit refreshCurrentUserGui();
}

void UserModel::addUser(const User &user)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    _users << user;
    endInsertRows();
}

void UserModel::addCurrentUser(const User &user)
{
    _currentUser = new User(user);
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
    }
    return QVariant();
}

QHash<int, QByteArray> UserModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[ServerRole] = "server";
    roles[AvatarRole] = "avatar";
    return roles;
}
}