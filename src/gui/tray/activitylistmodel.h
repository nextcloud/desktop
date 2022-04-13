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

    Q_PROPERTY(quint32 maxActionButtons READ maxActionButtons CONSTANT)

    Q_PROPERTY(AccountState *accountState READ accountState CONSTANT)
public:
    enum DataRole {
        DarkIconRole = Qt::UserRole + 1,
        LightIconRole,
        AccountRole,
        ObjectTypeRole,
        ObjectIdRole,
        ObjectNameRole,
        ActionsLinksRole,
        ActionsLinksContextMenuRole,
        ActionsLinksForActionButtonsRole,
        ActionTextRole,
        ActionTextColorRole,
        ActionRole,
        MessageRole,
        DisplayPathRole,
        PathRole,
        DisplayLocationRole, // Provides the display path to a file's parent folder, relative to Nextcloud root
        LinkRole,
        PointInTimeRole,
        AccountConnectedRole,
        DisplayActions,
        ShareableRole,
        IsCurrentUserFileActivityRole,
        ThumbnailRole,
        TalkNotificationConversationTokenRole,
        TalkNotificationMessageIdRole,
        TalkNotificationMessageSentRole,
        TalkNotificationUserAvatarRole,
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

    AccountState *accountState() const;
    void setAccountState(AccountState *state);

    static constexpr quint32 maxActionButtons()
    {
        return MaxActionButtons;
    }

    void setCurrentItem(const int currentItem);

    void setReplyMessageSent(const int activityIndex, const QString &message);
    QString replyMessageSent(const Activity &activity) const;

public slots:
    void slotRefreshActivity();
    void slotRefreshActivityInitial();
    void slotRemoveAccount();
    void slotTriggerDefaultAction(const int activityIndex);
    void slotTriggerAction(const int activityIndex, const int actionIndex);
    void slotTriggerDismiss(const int activityIndex);

signals:
    void activityJobStatusCode(int statusCode);
    void sendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row);

protected:
    void setup();
    void activitiesReceived(const QJsonDocument &json, int statusCode);
    QHash<int, QByteArray> roleNames() const override;

    void setAndRefreshCurrentlyFetching(bool value);
    bool currentlyFetching() const;
    void setDoneFetching(bool value);
    void setHideOldActivities(bool value);
    void setDisplayActions(bool value);

    virtual void startFetchJob();

    // added these for unit tests
    void setFinalList(const ActivityList &finalList);
    const ActivityList &finalList() const;
    int currentItem() const;
    //

private:
    static QVariantList convertLinksToMenuEntries(const Activity &activity);
    static QVariantList convertLinksToActionButtons(const Activity &activity);
    static QVariant convertLinkToActionButton(const ActivityLink &activityLink);
    void combineActivityLists();
    bool canFetchActivities() const;

    void ingestActivities(const QJsonArray &activities);
    void appendMoreActivitiesAvailableEntry();

    void insertOrRemoveDummyFetchingActivity();

    void clearActivities();

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

    static constexpr quint32 MaxActionButtons = 3;
};
}

#endif // ACTIVITYLISTMODEL_H
