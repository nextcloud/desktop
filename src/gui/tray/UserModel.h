#ifndef USERMODEL_H
#define USERMODEL_H

#include <QAbstractListModel>
#include <QStringList>

#include "accountmanager.h"

namespace OCC {

class User
{
public:
    User(const AccountStatePtr &account);

    bool isConnected() const;
    void login();
    void logout();
    QString name() const;
    QString server() const;
    QString avatar() const;
    QString id() const;

private:
    AccountStatePtr _account;
};

class UserModel : public QAbstractListModel
{
    Q_OBJECT

public:
    UserModel(QObject *parent = 0);
    virtual ~UserModel();

    void addUser(const User &user);
    void addCurrentUser(const User &user);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();

    Q_INVOKABLE int numUsers();
    Q_INVOKABLE bool isCurrentUserConnected();
    Q_INVOKABLE QString currentUserAvatar();
    Q_INVOKABLE QString currentUserName();
    Q_INVOKABLE QString currentUserServer();

    enum UserRoles {
        NameRole = Qt::UserRole + 1,
        ServerRole,
        AvatarRole
    };

protected:
    QHash<int, QByteArray> roleNames() const;

private:
    QList<User> _users;
    User *_currentUser;
};

}
#endif // USERMODEL_H