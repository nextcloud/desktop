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

#include "theme.h"

#include "servernotificationhandler.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcActivity, "nextcloud.gui.activity", QtInfoMsg)

ActivityListModel::ActivityListModel(AccountState *accountState, QWidget *parent)
    : QAbstractListModel(parent)
    , _accountState(accountState)
{
}

QVariant ActivityListModel::data(const QModelIndex &index, int role) const
{
    Activity a;

    // filter the get action here
    // send only the text of the get action
    // if there is more than one send the icon? the ...

    if (!index.isValid())
        return QVariant();

    a = _finalList.at(index.row());
    AccountStatePtr ast = AccountManager::instance()->account(a._accName);
    if (!ast && _accountState != ast.data())
        return QVariant();
    QStringList list;

    switch (role) {
    case ActivityItemDelegate::PathRole:
        if(!a._file.isEmpty()){
            auto folder = FolderMan::instance()->folder(a._folder);
            list = FolderMan::instance()->findFileInLocalFolders(folder->remotePath() + a._file, ast->account());
            if (list.count() > 0) {
                return QVariant(list.at(0));
            }
            // File does not exist anymore? Let's try to open its path
            list = FolderMan::instance()->findFileInLocalFolders(QFileInfo(folder->remotePath() + a._file).path(), ast->account());
            if (list.count() > 0) {
                return QVariant(list.at(0));
            }
        }
        return QVariant();
        break;
     case ActivityItemDelegate::ActionsLinksRole:{
        QList<QVariant> customList;
        foreach (ActivityLink customItem, a._links) {
            QVariant customVariant;
            customVariant.setValue(customItem);
            customList << customVariant;
        }
        return customList;
        break;
    }
    case ActivityItemDelegate::ActionIconRole:
        if(a._type == Activity::NotificationType){
           QIcon cachedIcon = ServerNotificationHandler::iconCache.value(a._id);
           if(!cachedIcon.isNull())
               return cachedIcon;
           else return QIcon(QLatin1String(":/client/resources/bell.svg"));
        } else if(a._type == Activity::SyncResultType){
            return QIcon(QLatin1String(":/client/resources/state-error.svg"));
        } else if(a._type == Activity::SyncFileItemType){
               if(a._status == SyncFileItem::NormalError
                   || a._status == SyncFileItem::FatalError
                   || a._status == SyncFileItem::DetailError
                   || a._status == SyncFileItem::BlacklistedError) {
                   return QIcon(QLatin1String(":/client/resources/state-error.svg"));
               } else if(a._status == SyncFileItem::SoftError
                         || a._status == SyncFileItem::FileIgnored
                         || a._status == SyncFileItem::Conflict
                         || a._status == SyncFileItem::Restoration){
                   return QIcon(QLatin1String(":/client/resources/state-warning.svg"));
               }
        } else return QIcon(QLatin1String(":/client/resources/activity.png"));
        return QVariant();
        break;
    case ActivityItemDelegate::ObjectTypeRole:
        return a._objectType;
        break;
    case ActivityItemDelegate::ActionRole:{
        QVariant type;
        type.setValue(a._type);
        return type;
        break;
    }
    case Qt::ToolTipRole:
    case ActivityItemDelegate::ActionTextRole:
        return a._subject;
        break;
    case ActivityItemDelegate::MessageRole:
        return a._message;
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

    if (_accountState && _accountState->isConnected()) {
        if (_activityLists.count() == 0 && !_currentlyFetching) {
            return true;
        }
    }

    return false;
}

void ActivityListModel::startFetchJob()
{
    if (!_accountState->isConnected()) {
        return;
    }
    JsonApiJob *job = new JsonApiJob(_accountState->account(), QLatin1String("ocs/v2.php/cloud/activity"), this);
    QObject::connect(job, &JsonApiJob::jsonReceived,
        this, &ActivityListModel::slotActivitiesReceived);

    QUrlQuery params;
    params.addQueryItem(QLatin1String("page"), QLatin1String("0"));
    params.addQueryItem(QLatin1String("pagesize"), QLatin1String("100"));
    job->addQueryParams(params);

    _currentlyFetching = true;
    qCInfo(lcActivity) << "Start fetching activities for " << _accountState->account()->displayName();
    job->start();
}

void ActivityListModel::slotActivitiesReceived(const QJsonDocument &json, int statusCode)
{
    auto activities = json.object().value("ocs").toObject().value("data").toArray();

    ActivityList list;
    auto ast = _accountState;
    if (!ast)
        return;

    _currentlyFetching = 0;

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

    _activityLists = list;

    emit activityJobStatusCode(statusCode);

    combineActivityLists();
}

void ActivityListModel::addErrorToActivityList(Activity activity) {
    qCInfo(lcActivity) << "Error successfully added to the notification list: " << activity._subject;
    _notificationErrorsLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::addNotificationToActivityList(Activity activity) {
    qCInfo(lcActivity) << "Notification successfully added to the notification list: " << activity._subject;
    _notificationLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::removeActivityFromActivityList(int row) {
    Activity activity =  _finalList.at(row);
    removeActivityFromActivityList(activity);
}

void ActivityListModel::removeActivityFromActivityList(Activity activity) {
    qCInfo(lcActivity) << "Activity/Notification/Error successfully dismissed: " << activity._subject;
    qCInfo(lcActivity) << "Trying to remove Activity/Notification/Error from view... ";

    int index = -1;
    if(activity._type == Activity::ActivityType){
        index = _activityLists.indexOf(activity);
        if(index != -1) _activityLists.removeAt(index);
    } else if(activity._type == Activity::NotificationType){
        index = _notificationLists.indexOf(activity);
        if(index != -1) _notificationLists.removeAt(index);
    } else {
        index = _notificationErrorsLists.indexOf(activity);
        if(index != -1) _notificationErrorsLists.removeAt(index);
    }

    if(index != -1){
        qCInfo(lcActivity) << "Activity/Notification/Error successfully removed from the list.";
        qCInfo(lcActivity) << "Updating Activity/Notification/Error view.";
        combineActivityLists();
    }
}


void ActivityListModel::combineActivityLists()
{
    ActivityList resultList;

    std::sort(_notificationErrorsLists.begin(), _notificationErrorsLists.end());
    resultList.append(_notificationErrorsLists);

    std::sort(_notificationLists.begin(), _notificationLists.end());
    resultList.append(_notificationLists);

    std::sort(_activityLists.begin(), _activityLists.end());
    resultList.append(_activityLists);

    beginResetModel();
    _finalList.clear();
    endResetModel();

    beginInsertRows(QModelIndex(), 0, resultList.count());
    _finalList = resultList;
    endInsertRows();
}

void ActivityListModel::fetchMore(const QModelIndex &)
{
    if (_accountState->isConnected()) {
        _activityLists = ActivityList();
        startFetchJob();
    }
}

void ActivityListModel::slotRefreshActivity()
{
    _activityLists.clear();
    startFetchJob();
}

void ActivityListModel::slotRemoveAccount()
{
    _finalList.clear();
    _activityLists.clear();
    _currentlyFetching = false;
}
}
