#ifndef USERMODEL_H
#define USERMODEL_H

#include <QAbstractListModel>
#include <QStringList>

namespace OCC {

class User
{
public:
    User(const QString &name, const QString &server, const QString &avatar);

    QString name() const;
    QString server() const;
    QString avatar() const;

private:
    QString _name;
    QString _server;
    QString _avatar;
};

class UserModel : public QAbstractListModel
{
    Q_OBJECT

public:
    UserModel(QObject *parent = 0);
    virtual ~UserModel();

    void addUser(const User &user);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    enum UserRoles {
        NameRole = Qt::UserRole + 1,
        ServerRole,
        AvatarRole
    };

protected:
    QHash<int, QByteArray> roleNames() const;

private:
    QList<User> _users;
};

}
#endif // USERMODEL_H