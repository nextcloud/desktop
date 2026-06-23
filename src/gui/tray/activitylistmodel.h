/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ACTIVITYLISTMODEL_H
#define ACTIVITYLISTMODEL_H

#include <QtCore>

#include "activitydata.h"

class QJsonDocument;

namespace ActivityListModelTestUtils {
class TestingALM;
}

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcActivity)

class AccountState;
class ConflictDialog;
class InvalidFilenameDialog;
class CaseClashFilenameDialog;

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
    Q_PROPERTY(AccountState *accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(bool hasSyncConflicts READ hasSyncConflicts NOTIFY hasSyncConflictsChanged)
    Q_PROPERTY(OCC::ActivityList allConflicts READ allConflicts NOTIFY allConflictsChanged)

public:
    enum DataRole {
        IconRole = Qt::UserRole + 1,
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
        OpenablePathRole,
        DisplayLocationRole, // Provides the display path to a file's parent folder, relative to Nextcloud root
        LinkRole,
        PointInTimeRole,
        AccountConnectedRole,
        DisplayActions,
        ShowFileDetailsRole,
        ShareableRole,
        DismissableRole,
        IsCurrentUserFileActivityRole,
        ThumbnailRole,
        TalkNotificationConversationTokenRole,
        TalkNotificationMessageIdRole,
        TalkNotificationMessageSentRole,
        TalkNotificationUserAvatarRole,
        ActivityIndexRole,
        ActivityRole,
        ActivityIntegrationRole
    };
    Q_ENUM(DataRole)

    enum class ErrorType {
        SyncError,
        NetworkError,
    };
    Q_ENUM(ErrorType)

    explicit ActivityListModel(QObject *parent = nullptr);
    explicit ActivityListModel(AccountState *accountState, QObject *parent = nullptr);

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool canFetchMore(const QModelIndex &) const override;

    [[nodiscard]] const ActivityList& activityList() const { return _finalList; }
    [[nodiscard]] const ActivityList& errorsList() const { return _notificationErrorsLists; }

    [[nodiscard]] AccountState *accountState() const;

    [[nodiscard]] int currentItem() const;

    static constexpr quint32 maxActionButtons()
    {
        return MaxActionButtons;
    }

    [[nodiscard]] QString replyMessageSent(const Activity &activity) const;

    [[nodiscard]] bool hasSyncConflicts() const;

    [[nodiscard]] OCC::ActivityList allConflicts() const;

public slots:
    void fetchMore(const QModelIndex &) override;

    void slotRefreshActivity();
    void slotRefreshActivityInitial();
    void slotRemoveAccount();
    void slotTriggerDefaultAction(const int activityIndex);
    void slotTriggerAction(const int activityIndex, const int actionIndex);
    void slotTriggerDismiss(const int activityIndex);

    void addNotificationToActivityList(const OCC::Activity &activity);
    void addErrorToActivityList(const OCC::Activity &activity, const OCC::ActivityListModel::ErrorType type);
    void addSyncFileItemToActivityList(const OCC::Activity &activity);
    void removeActivityFromActivityList(int row);
    void removeActivityFromActivityList(const OCC::Activity &activity);

    void removeOutdatedNotifications(const OCC::ActivityList &receivedNotifications);

    void setAccountState(OCC::AccountState *state);
    void setReplyMessageSent(const int activityIndex, const QString &message);
    void setCurrentItem(const int currentItem);

#ifdef BUILD_FILE_PROVIDER_MODULE
    /// Stable activity `_id` for the per-domain insufficient-quota summary entry. Used by
    /// both `OCC::User` (when constructing the entry) and this model (when removing it on
    /// retry). See https://github.com/nextcloud/desktop/issues/9598.
    [[nodiscard]] static qint64 fileProviderQuotaSummaryActivityId(const QString &domainIdentifier);

    /// Stable activity `_id` for a per-item insufficient-quota entry. Identical hashing on
    /// both producer and consumer keeps `removeFileProviderQuotaActivitiesForDomain(...)`
    /// able to find rows it didn't author.
    [[nodiscard]] static qint64 fileProviderQuotaItemActivityId(const QString &domainIdentifier, const QString &relativePath, const QString &fileName);

    /// Remove the per-folder summary entry plus every per-item quota entry that belongs to
    /// the given domain from the activity list. Called when the user clicks "Retry all
    /// uploads" so the UI gives immediate feedback that the click did something.
    void removeFileProviderQuotaActivitiesForDomain(const QString &domainIdentifier);
#endif

signals:
    void accountStateChanged();
    void hasSyncConflictsChanged();
    void allConflictsChanged();

    void activityJobStatusCode(int statusCode);
    void sendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row);

    void interactiveActivityReceived();

    void showSettingsDialog();

#ifdef BUILD_FILE_PROVIDER_MODULE
    /**
     * @brief Emitted when the user clicks the "Retry all uploads" button on a file provider
     * insufficient-quota summary entry. The argument is the file provider domain identifier;
     * the slot in `OCC::User` calls `signalErrorResolved` + `signalEnumerator` for that domain.
     * See https://github.com/nextcloud/desktop/issues/9598.
     */
    void fileProviderRetryUploadsRequested(const QString &domainIdentifier);
#endif

protected:
    [[nodiscard]] bool currentlyFetching() const;

protected slots:
    void activitiesReceived(const QJsonDocument &json, int statusCode);
    void setAndRefreshCurrentlyFetching(bool value);
    void setDoneFetching(bool value);
    void setHideOldActivities(bool value);
    void setDisplayActions(bool value);

    virtual void startFetchJob();

private slots:
    void addEntriesToActivityList(const OCC::ActivityList &activityList);
    void accountStateHasChanged();
    void ingestActivities(const QJsonArray &activities);
    void appendMoreActivitiesAvailableEntry();
    void insertOrRemoveDummyFetchingActivity();
    void triggerCaseClashAction(OCC::Activity activity);

private:
    static QVariantList convertLinksToMenuEntries(const Activity &activity);
    static QVariantList convertLinksToActionButtons(const Activity &activity);
    static QVariant convertLinkToActionButton(const ActivityLink &activityLink);

    [[nodiscard]] bool canFetchActivities() const;

    void displaySingleConflictDialog(const Activity &activity);
    void setHasSyncConflicts(bool conflictsFound);

    Activity _notificationIgnoredFiles;
    Activity _dummyFetchingActivities;

    ActivityList _activityLists;
    ActivityList _notificationErrorsLists;
    ActivityList _conflictsList;
    ActivityList _finalList;

    QSet<qint64> _presentedActivities;
    QSet<qint64> _activeNotificationIds;

    bool _displayActions = true;

    qint64 _currentItem = 0;
    static constexpr int _maxActivities = 100;
    static constexpr int _maxActivitiesDays = 30;
    bool _showMoreActivitiesAvailableEntry = false;

    QPointer<ConflictDialog> _currentConflictDialog;
    QPointer<InvalidFilenameDialog> _currentInvalidFilenameDialog;
    QPointer<CaseClashFilenameDialog> _currentCaseClashFilenameDialog;

    AccountState *_accountState = nullptr;
    bool _currentlyFetching = false;
    bool _doneFetching = false;
    bool _hideOldActivities = true;

    bool _hasSyncConflicts = false;

    bool _accountStateWasConnected = false;

    QElapsedTimer _durationSinceDisconnection;

    static constexpr quint32 MaxActionButtons = 3;

    friend class ActivityListModelTestUtils::TestingALM;
};
}

#endif // ACTIVITYLISTMODEL_H
