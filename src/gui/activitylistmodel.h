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

#ifndef ACTIVITYLISTMODEL_H
#define ACTIVITYLISTMODEL_H

#include <QtCore>

#include "activitydata.h"

class QJsonDocument;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcActivity)

class AccountState;

/**
 * @brief The ActivityListModel
 * @ingroup gui
 *
 * Simple list model to provide the list view with data.
 */

class ActivityListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ActivityListModel(AccountState *accountState, QWidget *parent = Q_NULLPTR);

    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;

    bool canFetchMore(const QModelIndex &) const Q_DECL_OVERRIDE;
    void fetchMore(const QModelIndex &) Q_DECL_OVERRIDE;

    ActivityList activityList() { return _finalList; }
    ActivityList errorsList() { return _notificationErrorsLists; }
    void addNotificationToActivityList(Activity activity);
    void addErrorToActivityList(Activity activity);
    void addSyncFileItemToActivityList(Activity activity);
    void removeActivityFromActivityList(int row);
    void removeActivityFromActivityList(Activity activity);

public slots:
    void slotRefreshActivity();
    void slotRemoveAccount();

private slots:
    void slotActivitiesReceived(const QJsonDocument &json, int statusCode);

signals:
    void activityJobStatusCode(int statusCode);

private:
    void startFetchJob();
    void combineActivityLists();

    ActivityList _activityLists;
    ActivityList _syncFileItemLists;
    ActivityList _notificationLists;
    ActivityList _notificationErrorsLists;
    ActivityList _finalList;
    AccountState *_accountState;
    bool _currentlyFetching = true;
};
}
#endif // ACTIVITYLISTMODEL_H
