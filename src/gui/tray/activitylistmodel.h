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
class ConflictDialog;
class InvalidFilenameDialog;

/**
 * @brief The ActivityListModel
 * @ingroup gui
 *
 * Simple list model to provide the list view with data.
 */

class ActivityListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(AccountState *accountState READ accountState CONSTANT)
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
        SyncFileStatusRole,
        DisplayActions,
        ShareableRole,
    };
    Q_ENUM(DataRole)

    explicit ActivityListModel(QObject *parent = nullptr);

    explicit ActivityListModel(AccountState *accountState,
        QObject *parent = nullptr);

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

    Q_INVOKABLE void triggerDefaultAction(int activityIndex);
    Q_INVOKABLE void triggerAction(int activityIndex, int actionIndex);

    AccountState *accountState() const;

public slots:
    void slotRefreshActivity();
    void slotRemoveAccount();

signals:
    void activityJobStatusCode(int statusCode);
    void sendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row);

protected:
    void activitiesReceived(const QJsonDocument &json, int statusCode);
    QHash<int, QByteArray> roleNames() const override;

    void setAccountState(AccountState *state);
    void setCurrentlyFetching(bool value);
    bool currentlyFetching() const;
    void setDoneFetching(bool value);
    void setHideOldActivities(bool value);
    void setDisplayActions(bool value);

    virtual void startFetchJob();

private:
    void combineActivityLists();
    bool canFetchActivities() const;

    ActivityList _activityLists;
    ActivityList _syncFileItemLists;
    ActivityList _notificationLists;
    ActivityList _listOfIgnoredFiles;
    Activity _notificationIgnoredFiles;
    ActivityList _notificationErrorsLists;
    ActivityList _finalList;
    int _currentItem = 0;

    bool _displayActions = true;

    int _totalActivitiesFetched = 0;
    int _maxActivities = 100;
    int _maxActivitiesDays = 30;
    bool _showMoreActivitiesAvailableEntry = false;

    QPointer<ConflictDialog> _currentConflictDialog;
    QPointer<InvalidFilenameDialog> _currentInvalidFilenameDialog;

    AccountState *_accountState = nullptr;
    bool _currentlyFetching = false;
    bool _doneFetching = false;
    bool _hideOldActivities = true;
};
}

#endif // ACTIVITYLISTMODEL_H
