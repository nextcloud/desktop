/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenshotusermodel.h"

namespace OCC {

namespace {
enum UserRole : int {
    NameRole = Qt::UserRole + 1,
    ServerRole,
    AvatarRole,
    IsConnectedRole,
    IdRole,
};
}

ScreenshotUserModel::ScreenshotUserModel(QObject *parent)
    : QAbstractListModel(parent)
    , _user(new ScreenshotUser(this))
{
}

int ScreenshotUserModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant ScreenshotUserModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() != 0) {
        return {};
    }
    switch (role) {
    case NameRole:
        return _user->name();
    case ServerRole:
        return _user->server();
    case AvatarRole:
        return _user->avatar();
    case IsConnectedRole:
        return _user->isConnected();
    case IdRole:
        return 0;
    }
    return {};
}

ScreenshotUser *ScreenshotUserModel::currentUser() const
{
    return _user;
}

int ScreenshotUserModel::currentUserId() const
{
    return 0;
}

int ScreenshotUserModel::count() const
{
    return 1;
}

int ScreenshotUserModel::numUsers() const
{
    return count();
}

bool ScreenshotUserModel::isUserConnected(const int id) const
{
    return id == 0;
}

QHash<int, QByteArray> ScreenshotUserModel::roleNames() const
{
    static const auto roles = QHash<int, QByteArray>{
        {NameRole, QByteArrayLiteral("name")},
        {ServerRole, QByteArrayLiteral("server")},
        {AvatarRole, QByteArrayLiteral("avatar")},
        {IsConnectedRole, QByteArrayLiteral("isConnected")},
        {IdRole, QByteArrayLiteral("id")},
    };
    return roles;
}

}
