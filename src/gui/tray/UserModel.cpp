#include "accountmanager.h"
#include "UserModel.h"

namespace OCC {

User::User(const AccountStatePtr &account)
    : _account(account)
{
}

QString User::name() const
{
    return _account->account()->davDisplayName();
}

QString User::server() const
{
    QString serverUrl = _account->account()->url().toString();
    serverUrl.replace(QLatin1String("https://"), QLatin1String(""));
    serverUrl.replace(QLatin1String("http://"), QLatin1String(""));
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

    QString img("data:image/png;base64,");
    img.append(QString::fromLatin1(bArray.toBase64().data()));
    return img;
}

/*-------------------------------------------------------------------------------------*/

UserModel::UserModel(QObject *parent)
    : QAbstractListModel()
    , _currentUser()
{
    for (int i = 0; i < AccountManager::instance()->accounts().size(); i++) {
        addUser(User(AccountManager::instance()->accounts().at(i)));
    }
    _currentUser = &_users.first();
}

UserModel::~UserModel()
{
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