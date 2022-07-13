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
        ActivityRole,
    };
    Q_ENUM(DataRole)

    enum class ActivityEntryType {
        DummyFetchingActivityType,
        ActivityType,
        NotificationType,
        ErrorType,
        IgnoredFileType,
        SyncFileItemType,
        MoreActivitiesAvailableType,
    };
    Q_ENUM(ActivityEntryType);

    explicit ActivityListModel(QObject *parent = nullptr);

    explicit ActivityListModel(AccountState *accountState,
        QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    bool canFetchMore(const QModelIndex &) const override;
    void fetchMore(const QModelIndex &) override;

    ActivityList activityList() { return _finalList; }
    ActivityList errorsList() { return _notificationErrorsLists; }

    AccountState *accountState() const;

    int currentItem() const;

    static constexpr quint32 maxActionButtons()
    {
        return MaxActionButtons;
    }

    QString replyMessageSent(const Activity &activity) const;

public slots:
    void slotRefreshActivity();
    void slotRefreshActivityInitial();
    void slotRemoveAccount();
    void slotTriggerDefaultAction(const int activityIndex);
    void slotTriggerAction(const int activityIndex, const int actionIndex);
    void slotTriggerDismiss(const int activityIndex);

    void addNotificationToActivityList(const Activity &activity);
    void addErrorToActivityList(const Activity &activity);
    void addIgnoredFileToList(const Activity &newActivity);
    void addSyncFileItemToActivityList(const Activity &activity);
    void removeActivityFromActivityList(int row);
    void removeActivityFromActivityList(const Activity &activity);

    void setAccountState(AccountState *state);
    void setReplyMessageSent(const int activityIndex, const QString &message);
    void setCurrentItem(const int currentItem);

signals:
    void activityJobStatusCode(int statusCode);
    void sendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row);

protected:
    QHash<int, QByteArray> roleNames() const override;

    bool currentlyFetching() const;

    const ActivityList &finalList() const; // added for unit tests

protected slots:
    void activitiesReceived(const QJsonDocument &json, int statusCode);
    void setAndRefreshCurrentlyFetching(bool value);
    void setDoneFetching(bool value);
    void setHideOldActivities(bool value);
    void setDisplayActions(bool value);
    void setFinalList(const ActivityList &finalList); // added for unit tests

    virtual void startFetchJob();

private:
    static QVariantList convertLinksToMenuEntries(const Activity &activity);
    static QVariantList convertLinksToActionButtons(const Activity &activity);
    static QVariant convertLinkToActionButton(const ActivityLink &activityLink);

    std::pair<int, int> rowRangeForEntryType(const ActivityEntryType type) const;
    void addEntriesToActivityList(const ActivityList &activityList, const ActivityEntryType type);
    void clearEntriesInActivityList(ActivityEntryType type);
    bool canFetchActivities() const;

    void ingestActivities(const QJsonArray &activities);
    void appendMoreActivitiesAvailableEntry();
    void insertOrRemoveDummyFetchingActivity();

    Activity _notificationIgnoredFiles;
    Activity _dummyFetchingActivities;

    ActivityList _activityLists;
    ActivityList _syncFileItemLists;
    ActivityList _notificationLists;
    ActivityList _listOfIgnoredFiles;
    ActivityList _notificationErrorsLists;
    ActivityList _finalList;

    QSet<qint64> _presentedActivities;

    bool _displayActions = true;

    int _currentItem = 0;
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
