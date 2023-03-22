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

#include "accessmanager.h"
#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "folderman.h"
#include "guiutility.h"
#include "models.h"
#include "networkjobs/jsonjob.h"

#include "resources/resources.h"

#include "activitydata.h"
#include "activitylistmodel.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcActivity, "gui.activity", QtInfoMsg)

ActivityListModel::ActivityListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

QVariant ActivityListModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid));
    if (!index.isValid()) {
        return {};
    }

    const auto &a = _finalList.at(index.row());
    const AccountStatePtr accountState = AccountManager::instance()->account(a.accountUuid());
    if (!accountState) {
        return {};
    }
    const auto column = static_cast<ActivityRole>(index.column());
    switch (role) {
    case Models::UnderlyingDataRole:
        Q_FALLTHROUGH();
    case Qt::DisplayRole:
        switch (column) {
        case ActivityRole::Account:
            return a.accName();
        case ActivityRole::Text:
            return a.subject();
        case ActivityRole::PointInTime:
            if (role == Models::UnderlyingDataRole) {
                return a.dateTime();
            } else {
                return Utility::timeAgoInWords(a.dateTime());
            }
        case ActivityRole::Path: {
            QStringList list = FolderMan::instance()->findFileInLocalFolders(a.file(), accountState->account());
            if (!list.isEmpty()) {
                return list.at(0);
            }
            // File does not exist anymore? Let's try to open its path
            list = FolderMan::instance()->findFileInLocalFolders(QFileInfo(a.file()).path(), accountState->account());
            if (!list.isEmpty()) {
                return list.at(0);
            }
            return {};
        }
        case ActivityRole::ColumnCount:
            Q_UNREACHABLE();
            break;
        }
        break;
    case Qt::ToolTipRole:
        return tr("%1 %2 on %3").arg(a.subject(), Utility::timeAgoInWords(a.dateTime()), a.accName());
    case Qt::DecorationRole:
        switch (column) {
        case ActivityRole::Text:
            if (!accountState->account()->avatar().isNull()) {
                return QIcon(accountState->account()->avatar());
            } else {
                return Resources::getCoreIcon(QStringLiteral("account"));
            }
        default:
            return {};
        }
    default:
        return {};
    }
    Q_UNREACHABLE();
}

QVariant ActivityListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        const auto actionRole = static_cast<ActivityRole>(section);
        switch (role) {
        case Qt::DisplayRole:
            switch (actionRole) {
            case ActivityRole::Text:
                return tr("Activity");
            case ActivityRole::Account:
                return tr("Account");
            case ActivityRole::PointInTime:
                return tr("Time");
            case ActivityRole::Path:
                return tr("Local path");
            case ActivityRole::ColumnCount:
                Q_UNREACHABLE();
                break;
            }
            break;
        case Models::StringFormatWidthRole:
            switch (actionRole) {
            case ActivityRole::Text:
                return 120;
            case ActivityRole::Account:
                return 20;
            case ActivityRole::PointInTime:
                return 20;
            case ActivityRole::Path:
                return 30;
            case ActivityRole::ColumnCount:
                Q_UNREACHABLE();
                break;
            }
            break;
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

int ActivityListModel::rowCount(const QModelIndex &parent) const
{
    Q_ASSERT(checkIndex(parent));
    if (parent.isValid()) {
        return 0;
    }
    return _finalList.count();
}

int ActivityListModel::columnCount(const QModelIndex &parent) const
{
    Q_ASSERT(checkIndex(parent));
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(ActivityRole::ColumnCount);
}

// current strategy: Fetch 100 items per Account
// ATTENTION: This method is const and thus it is not possible to modify
// the _activityLists hash or so. Doesn't make it easier...
bool ActivityListModel::canFetchMore(const QModelIndex &) const
{
    if (_activityLists.isEmpty())
        return true;

    for (auto i = _activityLists.begin(); i != _activityLists.end(); ++i) {
        AccountStatePtr accountState = i.key();
        if (accountState && accountState->isConnected()) {
            ActivityList activities = i.value();
            if (activities.count() == 0 && !_currentlyFetching.contains(accountState)) {
                return true;
            }
        }
    }

    return false;
}

void ActivityListModel::startFetchJob(AccountStatePtr ast)
{
    if (!ast || !ast->isConnected()) {
        return;
    }
    auto *job = new JsonApiJob(ast->account(), QStringLiteral("ocs/v2.php/cloud/activity"), { { QStringLiteral("page"), QStringLiteral("0") }, { QStringLiteral("pagesize"), QStringLiteral("100") } }, {}, this);

    QObject::connect(job, &JsonApiJob::finishedSignal,
        this, [job, ast, this] {
            _currentlyFetching.remove(ast);
            const auto activities = job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toArray();

            /*
             * in case the activity app is disabled or not installed, the server returns an empty 500 response instead of a response
             * with the expected status code 999
             * we are not entirely sure when this has changed, but it is likely that there is a regression in the activity addon
             * to support this new behavior, we have to fake the expected status code
             */
            if (job->reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
                emit activityJobStatusCode(ast, 999);
                return;
            }

            ActivityList list;
            list.reserve(activities.size());
            for (const auto &activ : activities) {
                const auto json = activ.toObject();
                list.append(Activity{Activity::ActivityType, json.value(QStringLiteral("id")).toString(), ast->account(),
                    json.value(QStringLiteral("subject")).toString(), json.value(QStringLiteral("message")).toString(),
                    json.value(QStringLiteral("file")).toString(), QUrl(json.value(QStringLiteral("link")).toString()),
                    QDateTime::fromString(json.value(QStringLiteral("date")).toString(), Qt::ISODate)});
            }

            _activityLists[ast] = std::move(list);

            emit activityJobStatusCode(ast, job->ocsStatus());

            combineActivityLists();
        });

    _currentlyFetching.insert(ast);
    qCInfo(lcActivity) << "Start fetching activities for " << ast->account()->displayName();
    job->start();
}


void ActivityListModel::combineActivityLists()
{
    ActivityList resultList;
    for (const ActivityList &list : qAsConst(_activityLists)) {
        resultList.append(list);
    }
    setActivityList(std::move(resultList));
}

void ActivityListModel::setActivityList(const ActivityList &&resultList)
{
    beginResetModel();
    _finalList = resultList;
    endResetModel();
}

void ActivityListModel::fetchMore(const QModelIndex &)
{
    for (const AccountStatePtr &asp : AccountManager::instance()->accounts()) {
        if (!_activityLists.contains(asp) && asp->isConnected()) {
            _activityLists[asp] = ActivityList();
            startFetchJob(asp);
        }
    }
}

void ActivityListModel::slotRefreshActivity(const AccountStatePtr &ast)
{
    if (ast && _activityLists.contains(ast)) {
        _activityLists.remove(ast);
    }
    startFetchJob(ast);
}

void ActivityListModel::slotRemoveAccount(AccountStatePtr ast)
{
    if (_activityLists.contains(ast)) {
        const auto accountToRemove = ast->account()->uuid();

        QMutableListIterator<Activity> it(_finalList);

        int i = 0;
        while (it.hasNext()) {
            Activity activity = it.next();
            if (activity.accountUuid() == accountToRemove) {
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
