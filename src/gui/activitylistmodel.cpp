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
            QString relPath(a._file);
            if(folder) relPath.prepend(folder->remotePath());
            list = FolderMan::instance()->findFileInLocalFolders(relPath, ast->account());
            if (list.count() > 0) {
                return QVariant(list.at(0));
            }
            // File does not exist anymore? Let's try to open its path
            if(QFileInfo(relPath).exists()) {
                list = FolderMan::instance()->findFileInLocalFolders(QFileInfo(relPath).path(), ast->account());
                if (list.count() > 0) {
                    return QVariant(list.at(0));
                }
            }
        }
        return QVariant();
     case ActivityItemDelegate::ActionsLinksRole:{
        QList<QVariant> customList;
        foreach (ActivityLink customItem, a._links) {
            QVariant customVariant;
            customVariant.setValue(customItem);
            customList << customVariant;
        }
        return customList;
    }
    case ActivityItemDelegate::ActionIconRole:{
        ActionIcon actionIcon;
        if(a._type == Activity::NotificationType){
           QIcon cachedIcon = ServerNotificationHandler::iconCache.value(a._id);
            if(!cachedIcon.isNull()) {
               actionIcon.iconType = ActivityIconType::iconUseCached;
               actionIcon.cachedIcon = cachedIcon;
            } else {
                actionIcon.iconType = ActivityIconType::iconBell;
            }
        } else if(a._type == Activity::SyncResultType){
            actionIcon.iconType = ActivityIconType::iconStateError;
        } else if(a._type == Activity::SyncFileItemType){
               if(a._status == SyncFileItem::NormalError
                   || a._status == SyncFileItem::FatalError
                   || a._status == SyncFileItem::DetailError
                   || a._status == SyncFileItem::BlacklistedError) {
                   actionIcon.iconType = ActivityIconType::iconStateError;
               } else if(a._status == SyncFileItem::SoftError
                         || a._status == SyncFileItem::Conflict
                         || a._status == SyncFileItem::Restoration
                         || a._status == SyncFileItem::FileLocked){
                   actionIcon.iconType = ActivityIconType::iconStateWarning;
               } else if(a._status == SyncFileItem::FileIgnored){
                   actionIcon.iconType = ActivityIconType::iconStateInfo;
               } else {
                   actionIcon.iconType = ActivityIconType::iconStateSync;
               }
        } else {
            actionIcon.iconType = ActivityIconType::iconActivity;
        }
        QVariant icn;
        icn.setValue(actionIcon);
        return icn;
    }
    case ActivityItemDelegate::ObjectTypeRole:
        return a._objectType;
    case ActivityItemDelegate::ActionRole:{
        QVariant type;
        type.setValue(a._type);
        return type;
    }
    case ActivityItemDelegate::ActionTextRole:
        return a._subject;
    case ActivityItemDelegate::MessageRole:
        return a._message;
    case ActivityItemDelegate::LinkRole:
        return a._link;
    case ActivityItemDelegate::AccountRole:
        return a._accName;
    case ActivityItemDelegate::PointInTimeRole:
        return Utility::timeAgoInWords(a._dateTime.toLocalTime());
    case Qt::ToolTipRole:
        return a._dateTime.toLocalTime().toString(Qt::DefaultLocaleShortDate);
    case ActivityItemDelegate::AccountConnectedRole:
        return (ast && ast->isConnected());
    default:
        return QVariant();
    }
    return QVariant();
}

int ActivityListModel::rowCount(const QModelIndex &) const
{
    return _finalList.count();
}

bool ActivityListModel::canFetchMore(const QModelIndex &) const
{
    // We need to be connected to be able to fetch more
    if (_accountState && _accountState->isConnected()) {
        // If the fetching is reported to be done or we are currently fetching we can't fetch more
        if (!_doneFetching && !_currentlyFetching) {
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
    params.addQueryItem(QLatin1String("start"), QString::number(_currentItem));
    params.addQueryItem(QLatin1String("count"), QString::number(100));
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
    if (!ast) {
        return;
    }

    if (activities.size() == 0) {
        _doneFetching = true;
    }

    _currentlyFetching = false;
    _currentItem += activities.size();

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

    _activityLists.append(list);

    emit activityJobStatusCode(statusCode);

    combineActivityLists();
}

void ActivityListModel::addErrorToActivityList(Activity activity) {
    qCInfo(lcActivity) << "Error successfully added to the notification list: " << activity._subject;
    _notificationErrorsLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::addIgnoredFileToList(Activity newActivity) {
    qCInfo(lcActivity) << "First checking for duplicates then add file to the notification list of ignored files: " << newActivity._file;

    bool duplicate = false;
    if(_listOfIgnoredFiles.size() == 0){
        _notificationIgnoredFiles = newActivity;
        _notificationIgnoredFiles._subject = tr("Files from the ignore list as well as symbolic links are not synced. This includes:");
        _listOfIgnoredFiles.append(newActivity);
        return;
    }

    foreach(Activity activity, _listOfIgnoredFiles){
        if(activity._file == newActivity._file){
            duplicate = true;
            break;
        }
    }

    if(!duplicate){
        _notificationIgnoredFiles._message.append(", " + newActivity._file);
    }
}

void ActivityListModel::addNotificationToActivityList(Activity activity) {
    qCInfo(lcActivity) << "Notification successfully added to the notification list: " << activity._subject;
    _notificationLists.prepend(activity);
    combineActivityLists();
}

void ActivityListModel::clearNotifications() {
    qCInfo(lcActivity) << "Clear the notifications";
    _notificationLists.clear();
    combineActivityLists();
}

void ActivityListModel::removeActivityFromActivityList(int row) {
    Activity activity = _finalList.at(row);
    removeActivityFromActivityList(activity);
    combineActivityLists();
}

void ActivityListModel::addSyncFileItemToActivityList(Activity activity) {
    qCInfo(lcActivity) << "Successfully added to the activity list: " << activity._subject;
    _syncFileItemLists.prepend(activity);
    combineActivityLists();
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

    if(_notificationErrorsLists.count() > 0) {
        std::sort(_notificationErrorsLists.begin(), _notificationErrorsLists.end());
        resultList.append(_notificationErrorsLists);
    }
    if(_listOfIgnoredFiles.size() > 0)
        resultList.append(_notificationIgnoredFiles);

    if(_notificationLists.count() > 0) {
        std::sort(_notificationLists.begin(), _notificationLists.end());
        resultList.append(_notificationLists);
    }

    if(_syncFileItemLists.count() > 0) {
        std::sort(_syncFileItemLists.begin(), _syncFileItemLists.end());
        resultList.append(_syncFileItemLists);
    }

    if(_activityLists.count() > 0) {
        std::sort(_activityLists.begin(), _activityLists.end());
        resultList.append(_activityLists);
    }

    beginResetModel();
    _finalList.clear();
    endResetModel();

    beginInsertRows(QModelIndex(), 0, resultList.count());
    _finalList = resultList;
    endInsertRows();
}

bool ActivityListModel::canFetchActivities() const {
    return _accountState->isConnected() && _accountState->account()->capabilities().hasActivities();
}

void ActivityListModel::fetchMore(const QModelIndex &)
{
    if (canFetchActivities()) {
        startFetchJob();
    } else {
        _doneFetching = true;
        combineActivityLists();
    }
}

void ActivityListModel::slotRefreshActivity()
{
    _activityLists.clear();
    _doneFetching = false;
    _currentItem = 0;

    if (canFetchActivities()) {
        startFetchJob();
    } else {
        _doneFetching = true;
        combineActivityLists();
    }
}

void ActivityListModel::slotRemoveAccount()
{
    _finalList.clear();
    _activityLists.clear();
    _currentlyFetching = false;
    _doneFetching = false;
    _currentItem = 0;
}
}
