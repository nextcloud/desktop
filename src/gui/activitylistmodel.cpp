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
    AccountStatePtr ast = AccountManager::instance()->account(a._accName);
    if (!ast)
        return QVariant();
    QStringList list;

    switch (role) {
    case ActivityItemDelegate::PathRole:
        if(!a._file.isEmpty()){
            list = FolderMan::instance()->findFileInLocalFolders(a._file, ast->account());
            if (list.count() > 0) {
                return QVariant(list.at(0));
            }
            // File does not exist anymore? Let's try to open its path
            list = FolderMan::instance()->findFileInLocalFolders(QFileInfo(a._file).path(), ast->account());
            if (list.count() > 0) {
                return QVariant(list.at(0));
            }
        }
        return QVariant();
        break;
    case ActivityItemDelegate::ActionIconRole:
        return QVariant(); // FIXME once the action can be quantified, display on Icon
        break;
    case ActivityItemDelegate::UserIconRole:
        return QIcon(QLatin1String(":/client/resources/account.png"));
        break;
    case Qt::ToolTipRole:
    case ActivityItemDelegate::ActionTextRole:
        return a._subject;
        break;
    case ActivityItemDelegate::LinkRole:
        return a._link;
        break;
    case ActivityItemDelegate::AccountRole:
        return a._accName;
        break;
    case ActivityItemDelegate::PointInTimeRole:
        return Utility::timeAgoInWords(a._dateTime);
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

    foreach (auto activ, activities) {
        auto json = activ.toObject();

        Activity a;
        a._type = Activity::ActivityType;
        a._accName = ast->account()->displayName();
        a._id = json.value("id").toInt();
        a._subject = json.value("subject").toString();
        a._message = json.value("message").toString();
        a._file = json.value("file").toString();
        a._link = QUrl(json.value("link").toString());
        a._dateTime = QDateTime::fromString(json.value("date").toString(), Qt::ISODate);
        list.append(a);
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
    QList<AccountStatePtr> accounts = AccountManager::instance()->accounts();

    foreach (const AccountStatePtr &asp, accounts) {
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
        int i = 0;
        const QString accountToRemove = ast->account()->displayName();

        QMutableListIterator<Activity> it(_finalList);

        while (it.hasNext()) {
            Activity activity = it.next();
            if (activity._accName == accountToRemove) {
                beginRemoveRows(QModelIndex(), i, i + 1);
                it.remove();
                endRemoveRows();
            }
        }
        _activityLists.remove(ast);
        _currentlyFetching.remove(ast);
    }
}
}
