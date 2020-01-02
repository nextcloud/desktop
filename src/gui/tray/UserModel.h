#ifndef USERMODEL_H
#define USERMODEL_H

#include <QAbstractListModel>
#include <QImage>
#include <QStringList>
#include <QQuickImageProvider>

#include "accountmanager.h"
#include "folderman.h"

namespace OCC {

class User
{
public:
    User(AccountStatePtr &account, const bool &isCurrent = false);

    bool operator==(const User &) const;

    bool isConnected() const;
    bool isCurrentUser() const;
    void setCurrentUser(const bool &isCurrent);
    Folder* getFolder();
    void openLocalFolder();
    QString name() const;
    QString server() const;
    QImage avatar() const;
    QString id() const;

private:
    AccountStatePtr _account;
    bool _isCurrentUser;
};

class UserModel : public QAbstractListModel
{
    Q_OBJECT
public:
    static UserModel *instance();
    virtual ~UserModel() {};

    void addUser(AccountStatePtr &user, const bool &isCurrent = false);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    QImage avatarById(const int &id);

    Q_INVOKABLE void openCurrentAccountLocalFolder();
    Q_INVOKABLE QImage currentUserAvatar();
    Q_INVOKABLE int numUsers();
    Q_INVOKABLE bool isCurrentUserConnected();
    Q_INVOKABLE QString currentUserName();
    Q_INVOKABLE QString currentUserServer();
    Q_INVOKABLE void switchCurrentUser(const int &id);

    enum UserRoles {
        NameRole = Qt::UserRole + 1,
        ServerRole,
        AvatarRole,
        IsCurrentUserRole
    };

signals:
    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();
    Q_INVOKABLE void addAccount();
    Q_INVOKABLE void removeAccount();

    Q_INVOKABLE void refreshCurrentUserGui();
    Q_INVOKABLE void newUserSelected();
    Q_INVOKABLE void refreshUserMenu();

protected:
    QHash<int, QByteArray> roleNames() const;

private:
    static UserModel *_instance;
    UserModel(QObject *parent = 0);
    QList<User> _users;
    int _currentUserId;

    void initUserList();
};

class ImageProvider : public QQuickImageProvider
{
public:
    ImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

}
#endif // USERMODEL_H