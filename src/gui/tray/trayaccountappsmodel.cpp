/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "trayaccountappsmodel.h"

#include "accountmanager.h"
#include "guiutility.h"

namespace OCC {

TrayAccountAppsModel *TrayAccountAppsModel::_instance = nullptr;

TrayAccountAppsModel *TrayAccountAppsModel::instance()
{
    if (!_instance) {
        _instance = new TrayAccountAppsModel();
    }
    return _instance;
}

TrayAccountAppsModel::TrayAccountAppsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void TrayAccountAppsModel::setUserId(const int userId)
{
    const auto apps = appsForUserId(userId);
    if (_userId == userId && _apps == apps) {
        return;
    }

    _userId = userId;
    if (_apps == apps) {
        return;
    }

    const auto oldCount = _apps.size();

    if (!_apps.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, _apps.size() - 1);
        _apps.clear();
        endRemoveRows();
    }

    if (!apps.isEmpty()) {
        beginInsertRows(QModelIndex(), 0, apps.size() - 1);
        _apps = apps;
        endInsertRows();
    }

    if (_apps.size() != oldCount) {
        emit countChanged();
    }
}

AccountAppList TrayAccountAppsModel::appsForUserId(const int userId) const
{
    const auto accounts = AccountManager::instance()->accounts();
    if (userId < 0 || userId >= accounts.size()) {
        return {};
    }

    const auto account = accounts.at(userId);
    if (!account) {
        return {};
    }

    return account->appList();
}

void TrayAccountAppsModel::openAppUrl(const QUrl &url)
{
    Utility::openBrowser(url);
}

int TrayAccountAppsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return count();
}

int TrayAccountAppsModel::count() const
{
    return _apps.size();
}

QVariant TrayAccountAppsModel::data(const QModelIndex &index, const int role) const
{
    auto result = QVariant{};

    if (index.row() < 0 || index.row() >= _apps.size()) {
        return result;
    }

    if (role >= static_cast<int>(Roles::NameRole) && role < static_cast<int>(Roles::IconUrlRole)) {
        switch (static_cast<Roles>(role)) {
        case Roles::NameRole:
            result = _apps[index.row()]->name();
            break;
        case Roles::UrlRole:
            result = _apps[index.row()]->url();
            break;
        case Roles::IconUrlRole:
            result = _apps[index.row()]->iconUrl().toString();
            break;
        }
    }

    return result;
}

QHash<int, QByteArray> TrayAccountAppsModel::roleNames() const
{
    return {
        { static_cast<int>(Roles::NameRole), "appName" },
        { static_cast<int>(Roles::UrlRole), "appUrl" },
        { static_cast<int>(Roles::IconUrlRole), "appIconUrl" },
    };
}

} // namespace OCC
