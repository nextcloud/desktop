/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QtCore>
#include <QAbstractListModel>
#include <QWidget>
#include <QIcon>
#include <QJsonObject>
#include <QJsonDocument>

#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "folderman.h"
#include "accessmanager.h"
#include "activityitemdelegate.h"
#include "guiutility.h"

#include "activitydata.h"
#include "activitylistmodel.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcActivity, "gui.activity", QtInfoMsg)

ActivityListModel::ActivityListModel(QWidget *parent)
    : QAbstractListModel(parent)
{
}

QVariant ActivityListModel::data(const QModelIndex &index, int role) const
{
    Activity a;

    if (!index.isValid())
        return QVariant();

    a = _finalList.at(index.row());
    AccountStatePtr ast = AccountManager::instance()->account(a.uuid());
    if (!ast)
        return QVariant();
    QStringList list;

    switch (role) {
    case ActivityItemDelegate::PathRole:
        list = FolderMan::instance()->findFileInLocalFolders(a.file(), ast->account());
        if (list.count() > 0) {
            return QVariant(list.at(0));
        }
        // File does not exist anymore? Let's try to open its path
        list = FolderMan::instance()->findFileInLocalFolders(QFileInfo(a.file()).path(), ast->account());
        if (list.count() > 0) {
            return QVariant(list.at(0));
        }
        return QVariant();
        break;
    case ActivityItemDelegate::ActionIconRole:
        return QVariant(); // FIXME once the action can be quantified, display on Icon
        break;
    case ActivityItemDelegate::UserIconRole:
        return !ast->account()->avatar().isNull() ? QIcon(ast->account()->avatar()) : Utility::getCoreIcon(QStringLiteral("account"));
        break;
    case Qt::ToolTipRole:
        return tr("%1 %2 on %3").arg(a.subject(), Utility::timeAgoInWords(a.dateTime()), a.accName());
        break;
    case ActivityItemDelegate::ActionTextRole:
        return a.subject();
        break;
    case ActivityItemDelegate::LinkRole:
        return a.subject();
        break;
    case ActivityItemDelegate::AccountRole:
        return a.accName();
        break;
    case ActivityItemDelegate::PointInTimeRole:
        return Utility::timeAgoInWords(a.dateTime());
        break;
    case ActivityItemDelegate::AccountConnectedRole:
        return (ast && ast->isConnected());
        break;
    default:
        return QVariant();
    }
    return QVariant();
}

int ActivityListModel::rowCount(const QModelIndex &) const
{
    return _finalList.count();
}

// current strategy: Fetch 100 items per Account
// ATTENTION: This method is const and thus it is not possible to modify
// the _activityLists hash or so. Doesn't make it easier...
bool ActivityListModel::canFetchMore(const QModelIndex &) const
{
    if (_activityLists.count() == 0)
        return true;

    for (auto i = _activityLists.begin(); i != _activityLists.end(); ++i) {
        AccountState *ast = i.key();
        if (ast && ast->isConnected()) {
            ActivityList activities = i.value();
            if (activities.count() == 0 && !_currentlyFetching.contains(ast)) {
                return true;
            }
        }
    }

    return false;
}

void ActivityListModel::startFetchJob(AccountState *s)
{
    if (!s->isConnected()) {
        return;
    }
    JsonApiJob *job = new JsonApiJob(s->account(), QLatin1String("ocs/v1.php/cloud/activity"), this);
    QObject::connect(job, &JsonApiJob::jsonReceived,
        this, &ActivityListModel::slotActivitiesReceived);
    job->setProperty("AccountStatePtr", QVariant::fromValue<QPointer<AccountState>>(s));

    QUrlQuery params;
    params.addQueryItem(QLatin1String("page"), QLatin1String("0"));
    params.addQueryItem(QLatin1String("pagesize"), QLatin1String("100"));
    job->addQueryParams(params);

    _currentlyFetching.insert(s);
    qCInfo(lcActivity) << "Start fetching activities for " << s->account()->displayName();
    job->start();
}

void ActivityListModel::slotActivitiesReceived(const QJsonDocument &json, int statusCode)
{
    auto activities = json.object().value("ocs").toObject().value("data").toArray();

    ActivityList list;
    auto ast = qvariant_cast<QPointer<AccountState>>(sender()->property("AccountStatePtr"));
    if (!ast)
        return;

    _currentlyFetching.remove(ast);

    for (const auto &activ : activities) {
        const auto json = activ.toObject();
        list.append(Activity { Activity::ActivityType,
            json.value(QStringLiteral("id")).toVariant().value<Activity::Identifier>(),
            ast->account(),
            json.value(QStringLiteral("subject")).toString(),
            json.value(QStringLiteral("message")).toString(),
            json.value(QStringLiteral("file")).toString(),
            QUrl(json.value(QStringLiteral("link")).toString()),
            QDateTime::fromString(json.value(QStringLiteral("date")).toString(), Qt::ISODate) });
    }

    _activityLists[ast] = list;

    emit activityJobStatusCode(ast, statusCode);

    combineActivityLists();
}


void ActivityListModel::combineActivityLists()
{
    ActivityList resultList;

    foreach (ActivityList list, _activityLists.values()) {
        resultList.append(list);
    }

    std::sort(resultList.begin(), resultList.end());

    beginResetModel();
    _finalList.clear();
    endResetModel();

    beginInsertRows(QModelIndex(), 0, resultList.count());
    _finalList = resultList;
    endInsertRows();
}

void ActivityListModel::fetchMore(const QModelIndex &)
{
    for (const AccountStatePtr &asp : AccountManager::instance()->accounts()) {
        if (!_activityLists.contains(asp.data()) && asp->isConnected()) {
            _activityLists[asp.data()] = ActivityList();
            startFetchJob(asp.data());
        }
    }
}

void ActivityListModel::slotRefreshActivity(AccountState *ast)
{
    if (ast && _activityLists.contains(ast)) {
        _activityLists.remove(ast);
    }
    startFetchJob(ast);
}

void ActivityListModel::slotRemoveAccount(AccountState *ast)
{
    if (_activityLists.contains(ast)) {
        const auto accountToRemove = ast->account()->uuid();

        QMutableListIterator<Activity> it(_finalList);

        int i = 0;
        while (it.hasNext()) {
            Activity activity = it.next();
            if (activity.uuid() == accountToRemove) {
                beginRemoveRows(QModelIndex(), i, i);
                it.remove();
                endRemoveRows();
            } else {
                ++i;
            }
        }
        _activityLists.remove(ast);
        _currentlyFetching.remove(ast);
    }
}
}
