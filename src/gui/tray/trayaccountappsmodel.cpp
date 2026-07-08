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
    if (parent.isValid()) {
        return 0;
    }

    return _apps.size();
}

QVariant TrayAccountAppsModel::data(const QModelIndex &index, const int role) const
{
    auto result = QVariant{};

    if (index.row() < 0 || index.row() >= _apps.size()) {
        return result;
    }

    if (role >= NameRole && role <= IconUrlRole) {
        switch (role) {
        case NameRole:
            result = _apps[index.row()]->name();
            break;
        case UrlRole:
            result = _apps[index.row()]->url();
            break;
        case IconUrlRole:
            result = _apps[index.row()]->iconUrl().toString();
            break;
        }
    }

    return result;
}

QHash<int, QByteArray> TrayAccountAppsModel::roleNames() const
{
    return {
        { NameRole, "appName" },
        { UrlRole, "appUrl" },
        { IconUrlRole, "appIconUrl" },
    };
}

} // namespace OCC
