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

#include "ActivityData.h"

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
    enum DataRole {
    ActionIconRole = Qt::UserRole + 1,
    UserIconRole,
    AccountRole,
    ObjectTypeRole,
    ActionsLinksRole,
    ActionTextRole,
    ActionTextColorRole,
    ActionRole,
    MessageRole,
    DisplayPathRole,
    PathRole,
    AbsolutePathRole,
    LinkRole,
    PointInTimeRole,
    AccountConnectedRole,
    SyncFileStatusRole};

    explicit ActivityListModel(AccountState *accountState, QObject* parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    bool canFetchMore(const QModelIndex &) const override;
    void fetchMore(const QModelIndex &) override;

    ActivityList activityList() { return _finalList; }
    ActivityList errorsList() { return _notificationErrorsLists; }
    void addNotificationToActivityList(Activity activity);
    void clearNotifications();
    void addErrorToActivityList(Activity activity);
    void addIgnoredFileToList(Activity newActivity);
    void addSyncFileItemToActivityList(Activity activity);
    void removeActivityFromActivityList(int row);
    void removeActivityFromActivityList(Activity activity);

    Q_INVOKABLE void triggerActionAtIndex(int id) const;

public slots:
    void slotRefreshActivity();
    void slotRemoveAccount();

private slots:
    void slotActivitiesReceived(const QJsonDocument &json, int statusCode);
    void slotIconDownloaded(QByteArray iconData);

signals:
    void activityJobStatusCode(int statusCode);

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    void startFetchJob();
    void combineActivityLists();
    bool canFetchActivities() const;

    ActivityList _activityLists;
    ActivityList _syncFileItemLists;
    ActivityList _notificationLists;
    ActivityList _listOfIgnoredFiles;
    Activity _notificationIgnoredFiles;
    ActivityList _notificationErrorsLists;
    ActivityList _finalList;
    AccountState *_accountState;
    bool _currentlyFetching = false;
    bool _doneFetching = false;
    int _currentItem = 0;

    int _totalActivitiesFetched = 0;
    int _maxActivities = 100;
    int _maxActivitiesDays = 30;
    bool _showMoreActivitiesAvailableEntry = false;
};
}

#endif // ACTIVITYLISTMODEL_H
