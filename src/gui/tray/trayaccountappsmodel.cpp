/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "trayaccountappsmodel.h"

#include "accountmanager.h"
#include "account.h"
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

    auto apps = AccountAppList{};
    const auto allApps = account->appList();
    const auto talkApp = account->findApp(QStringLiteral("spreed"));
    const auto assistantEnabled = account->account()->capabilities().ncAssistantEnabled();
    for (const auto app : allApps) {
        // Filter out Talk because we have a dedicated button for it.
        if (talkApp && app->id() == talkApp->id() && !assistantEnabled) {
            continue;
        }

        apps << app;
    }

    return apps;
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
    if (index.row() < 0 || index.row() >= _apps.size()) {
        return {};
    }

    switch (role) {
    case NameRole:
        return _apps[index.row()]->name();
    case UrlRole:
        return _apps[index.row()]->url();
    case IconUrlRole:
        return _apps[index.row()]->iconUrl().toString();
    default:
        return {};
    }
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
