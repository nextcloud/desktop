#include "accountmanager.h"
#include "menumodel.h"

namespace OCC {

User::User(const QString &name, const QString &server, const QString &avatar)
    : _name(name)
    , _server(server)
    , _avatar(avatar)
{
}

QString User::name() const
{
    return _name;
}

QString User::server() const
{
    return _server;
}

QString User::avatar() const
{
    return _avatar;
}

UserModel::UserModel(QObject *parent)
    : QAbstractListModel()
{
    for (size_t i = 0; i < AccountManager::instance()->accounts().size(); i++) {
        addUser(User("test", "test", "test"));
    }
}

UserModel::~UserModel()
{
}

void UserModel::addUser(const User &user)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    _users << user;
    endInsertRows();
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