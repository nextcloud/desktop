/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef USERMODEL_H
#define USERMODEL_H

#include <QAbstractListModel>
#include <QImage>
#include <QDateTime>
#include <QJsonDocument>
#include <QStringList>
#include <QQuickImageProvider>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QVector>

#include "accountfwd.h"
#include "accountmanager.h"
#include "activitydata.h"
#include "activitylistmodel.h"
#include "folderman.h"
#include "userinfo.h"
#include "userstatusconnector.h"
#include "userstatusselectormodel.h"
#include <chrono>

namespace OCC {
class UnifiedSearchResultsListModel;
class OcsAssistantConnector;


class TrayFolderInfo
{
    Q_GADGET

    Q_PROPERTY(QString name MEMBER _name)
    Q_PROPERTY(QString parentPath MEMBER _parentPath)
    Q_PROPERTY(QString fullPath MEMBER _fullPath)
    Q_PROPERTY(bool isGroupFolder READ isGroupFolder CONSTANT)
public:
    enum FolderType { Folder, GroupFolder };

    TrayFolderInfo(const QString &name, const QString &parentPath, const QString &fullPath, FolderType folderType);
    TrayFolderInfo() = default;
    [[nodiscard]] bool isGroupFolder() const;

    QString _name;
    QString _parentPath;
    QString _fullPath;
    FolderType _folderType = Folder;
};

class User : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString server READ server CONSTANT)
    Q_PROPERTY(QColor headerColor READ headerColor NOTIFY headerColorChanged)
    Q_PROPERTY(QColor headerTextColor READ headerTextColor NOTIFY headerTextColorChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(bool serverHasUserStatus READ serverHasUserStatus CONSTANT)
    Q_PROPERTY(UserStatus::OnlineStatus status READ status NOTIFY statusChanged)
    Q_PROPERTY(QUrl statusIcon READ statusIcon NOTIFY statusChanged)
    Q_PROPERTY(QString statusEmoji READ statusEmoji NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool desktopNotificationsAllowed READ isDesktopNotificationsAllowed NOTIFY desktopNotificationsAllowedChanged)
    Q_PROPERTY(bool hasLocalFolder READ hasLocalFolder NOTIFY hasLocalFolderChanged)
    Q_PROPERTY(bool isFeaturedAppEnabled READ isFeaturedAppEnabled NOTIFY featuredAppChanged)
    Q_PROPERTY(QString featuredAppIcon READ featuredAppIcon NOTIFY featuredAppChanged)
    Q_PROPERTY(QString featuredAppAccessibleName READ featuredAppAccessibleName NOTIFY featuredAppChanged)
    Q_PROPERTY(QString avatar READ avatarUrl NOTIFY avatarChanged)
    Q_PROPERTY(QUrl syncStatusIcon READ syncStatusIcon NOTIFY syncStatusChanged)
    Q_PROPERTY(bool syncStatusOk READ syncStatusOk NOTIFY syncStatusChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY accountStateChanged)
    Q_PROPERTY(bool needsToSignTermsOfService READ needsToSignTermsOfService NOTIFY accountStateChanged)
    Q_PROPERTY(UnifiedSearchResultsListModel* unifiedSearchResultsListModel READ getUnifiedSearchResultsListModel CONSTANT)
    Q_PROPERTY(QVariantList groupFolders READ groupFolders NOTIFY groupFoldersChanged)
    Q_PROPERTY(bool canLogout READ canLogout CONSTANT)
    Q_PROPERTY(bool isAssistantEnabled READ isNcAssistantEnabled NOTIFY assistantStateChanged)
    Q_PROPERTY(QString assistantQuestion READ assistantQuestion NOTIFY assistantQuestionChanged)
    Q_PROPERTY(QString assistantResponse READ assistantResponse NOTIFY assistantResponseChanged)
    Q_PROPERTY(QString assistantError READ assistantError NOTIFY assistantErrorChanged)
    Q_PROPERTY(QVariantList assistantMessages READ assistantMessages NOTIFY assistantMessagesChanged)
    Q_PROPERTY(bool assistantRequestInProgress READ assistantRequestInProgress NOTIFY assistantRequestInProgressChanged)

public:
    User(AccountStatePtr &account, const bool &isCurrent = false, QObject *parent = nullptr);

    [[nodiscard]] AccountPtr account() const;
    [[nodiscard]] AccountStatePtr accountState() const;

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] bool needsToSignTermsOfService() const;
    [[nodiscard]] bool isCurrentUser() const;
    void setCurrentUser(const bool &isCurrent);
    [[nodiscard]] Folder *getFolder() const;
    ActivityListModel *getActivityModel();
    [[nodiscard]] UnifiedSearchResultsListModel *getUnifiedSearchResultsListModel() const;
    void openLocalFolder() const;
    void openFolderLocallyOrInBrowser(const QString &fullRemotePath);
    [[nodiscard]] QString name() const;
    [[nodiscard]] QString server(bool shortened = true) const;
    [[nodiscard]] bool hasLocalFolder() const;
    [[nodiscard]] bool isFeaturedAppEnabled() const;
    [[nodiscard]] QString featuredAppIcon() const;
    [[nodiscard]] QString featuredAppAccessibleName() const;
    [[nodiscard]] bool serverHasUserStatus() const;
    [[nodiscard]] AccountApp *talkApp() const;
    [[nodiscard]] bool hasActivities() const;
    [[nodiscard]] bool isNcAssistantEnabled() const;
    [[nodiscard]] QColor accentColor() const;
    [[nodiscard]] QColor headerColor() const;
    [[nodiscard]] QColor headerTextColor() const;
    [[nodiscard]] AccountAppList appList() const;
    [[nodiscard]] QImage avatar() const;
    void login() const;
    void logout() const;
    void removeAccount() const;
    [[nodiscard]] QString avatarUrl() const;
    [[nodiscard]] bool isDesktopNotificationsAllowed() const;
    [[nodiscard]] UserStatus::OnlineStatus status() const;
    [[nodiscard]] QString statusMessage() const;
    [[nodiscard]] QUrl statusIcon() const;
    [[nodiscard]] QString statusEmoji() const;
    [[nodiscard]] QUrl syncStatusIcon() const;
    [[nodiscard]] bool syncStatusOk() const;
    void processCompletedSyncItem(const Folder *folder, const SyncFileItemPtr &item);
    [[nodiscard]] const QVariantList &groupFolders() const;
    [[nodiscard]] bool canLogout() const;
    [[nodiscard]] bool isPublicShareLink() const;
    [[nodiscard]] QString assistantQuestion() const;
    [[nodiscard]] QString assistantResponse() const;
    [[nodiscard]] QString assistantError() const;
    [[nodiscard]] QVariantList assistantMessages() const;
    [[nodiscard]] bool assistantRequestInProgress() const;

    Q_INVOKABLE void submitAssistantQuestion(const QString &question);
    Q_INVOKABLE void clearAssistantResponse();

signals:
    void nameChanged();
    void hasLocalFolderChanged();
    void featuredAppChanged();
    void avatarChanged();
    void accountStateChanged();
    void statusChanged();
    void desktopNotificationsAllowedChanged();
    void headerColorChanged();
    void headerTextColorChanged();
    void accentColorChanged();
    void syncStatusChanged();
    void sendReplyMessage(const int activityIndex, const QString &conversationToken, const QString &message, const QString &replyTo);
    void groupFoldersChanged();
    void assistantStateChanged();
    void assistantQuestionChanged();
    void assistantResponseChanged();
    void assistantErrorChanged();
    void assistantMessagesChanged();
    void assistantRequestInProgressChanged();

public slots:
    void slotItemCompleted(const QString &folder, const OCC::SyncFileItemPtr &item);
    void slotProgressInfo(const QString &folder, const OCC::ProgressInfo &progress);
    void slotAddError(const QString &folderAlias, const QString &message, OCC::ErrorCategory category);
    void slotAddErrorToGui(const QString &folderAlias, const OCC::SyncFileItem::Status status, const QString &errorMessage, const QString &subject, const OCC::ErrorCategory category);
    void slotAddNotification(const OCC::Folder *folder, const OCC::Activity &activity);
    void slotNotificationRequestFinished(int statusCode);
    void slotNotifyNetworkError(QNetworkReply *reply);
    void slotEndNotificationRequest(int replyCode);
    void slotNotifyServerFinished(const QString &reply, int replyCode);
    void slotSendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row);
    void slotBuildNotificationDisplay(const OCC::ActivityList &list);
    void slotNotificationFetchFinished();
    void slotBuildIncomingCallDialogs(const OCC::ActivityList &list);
    void slotRefreshNotifications();
    void slotRefreshActivitiesInitial();
    void slotRefreshActivities();
    void slotRefresh();
    void slotRefreshUserStatus();
    void slotRefreshImmediately();
    void setNotificationRefreshInterval(std::chrono::milliseconds interval);
    void slotRebuildNavigationAppList();
    void slotSendReplyMessage(const int activityIndex, const QString &conversationToken, const QString &message, const QString &replyTo);
    void forceSyncNow() const;
    void slotAccountCapabilitiesChangedRefreshGroupFolders();
    void slotFetchGroupFolders();

private slots:
    void slotPushNotificationsReady();
    void slotDisconnectPushNotifications();
    void slotReceivedPushFilesChanges(Account *account);
    void slotReceivedPushFileIdsChanges(Account *account, const QList<qint64> &fileIds);
    void slotReceivedPushNotification(OCC::Account *account);
    void slotReceivedPushActivity(OCC::Account *account);
    void slotCheckExpiredActivities();
    void slotGroupFoldersFetched(QNetworkReply *reply);
    void slotQuotaChanged(const int64_t &usedBytes, const int64_t &availableBytes);
    void slotAssistantPoll();
    void slotAssistantTaskTypesFetched(const QJsonDocument &json, int statusCode);
    void slotAssistantTasksFetched(const QJsonDocument &json, int statusCode);
    void slotAssistantTaskScheduled(const QJsonDocument &json, int statusCode);
    void slotAssistantTaskDeleted(int statusCode);
    void slotAssistantRequestError(const QString &context, int statusCode);
    void checkNotifiedNotifications();
    void showDesktopNotification(const QString &title, const QString &message, const qint64 notificationId);
    void showDesktopNotification(const OCC::Activity &activity);
    void showDesktopNotification(const OCC::ActivityList &activityList);
    void showDesktopTalkNotification(const OCC::Activity &activity);

private:
    void prePendGroupFoldersWithLocalFolder();
    void parseNewGroupFolderPath(const QString &path);
    void connectPushNotifications() const;
    [[nodiscard]] bool checkPushNotificationsAreReady() const;

    bool isActivityOfCurrentAccount(const Folder *folder) const;
    [[nodiscard]] bool isUnsolvableConflict(const SyncFileItemPtr &item) const;
    void updateSyncStatus();

    bool notificationAlreadyShown(const qint64 notificationId);
    bool canShowNotification(const qint64 notificationId);

    [[nodiscard]] bool serverHasTalk() const;

    AccountStatePtr _account;
    bool _isCurrentUser;
    ActivityListModel *_activityModel;
    UnifiedSearchResultsListModel *_unifiedSearchResultsModel;
    
    QVariantList _trayFolderInfos;

    QTimer _expiredActivitiesCheckTimer;
    QTimer _notificationCheckTimer;
    QHash<AccountState *, QElapsedTimer> _timeSinceLastCheck;

    QElapsedTimer _guiLogTimer;
    QSet<qint64> _notifiedNotifications;
    QSet<qint64> _activeNotifications;
    QMimeDatabase _mimeDb;

    // number of currently running notification requests. If non zero,
    // no query for notifications is started.
    int _notificationRequestsRunning = 0;

    int _lastTalkNotificationsReceivedCount = 0;

    bool _isNotificationFetchRunning = false;

    // used for quota warnings
    int _lastQuotaPercent = 0;
    Activity _lastQuotaActivity;

    QPointer<OcsAssistantConnector> _assistantConnector;
    QTimer _assistantPollTimer;
    int _assistantPollAttempts = 0;
    int _assistantMaxPollAttempts = 60;
    qint64 _assistantTaskId = -1;
    QString _assistantTaskType;
    QString _assistantQuestion;
    QString _assistantResponse;
    QString _assistantError;
    QVariantList _assistantMessages;
    bool _assistantRequestInProgress = false;

    QUrl _syncStatusIcon;
    bool _syncStatusOk = true;
};

class UserModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(User* currentUser READ currentUser NOTIFY currentUserChanged)
    Q_PROPERTY(int currentUserId READ currentUserId WRITE setCurrentUserId NOTIFY currentUserChanged)
    Q_PROPERTY(bool hasSyncErrors READ hasSyncErrors NOTIFY syncErrorUsersChanged)
    Q_PROPERTY(int syncErrorUserCount READ syncErrorUserCount NOTIFY syncErrorUsersChanged)
    Q_PROPERTY(int firstSyncErrorUserId READ firstSyncErrorUserId NOTIFY syncErrorUsersChanged)
    Q_PROPERTY(User* firstSyncErrorUser READ firstSyncErrorUser NOTIFY syncErrorUsersChanged)
public:

    static UserModel *instance();
    ~UserModel() override = default;

    void addUser(AccountStatePtr &user, const bool &isCurrent = false);
    int currentUserIndex();

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    [[nodiscard]]  QImage avatarById(const int id) const;

    [[nodiscard]] User *currentUser() const;
    [[nodiscard]] User *findUserForAccount(AccountState *account) const;
    [[nodiscard]] int findUserIdForAccount(AccountState *account) const;

    Q_INVOKABLE int numUsers();
    Q_INVOKABLE QString currentUserServer();
    [[nodiscard]] int currentUserId() const;

    Q_INVOKABLE bool isUserConnected(const int id);
    [[nodiscard]] bool hasSyncErrors() const;
    [[nodiscard]] int syncErrorUserCount() const;
    [[nodiscard]] int firstSyncErrorUserId() const;
    [[nodiscard]] User *firstSyncErrorUser() const;

    Q_INVOKABLE std::shared_ptr<OCC::UserStatusConnector> userStatusConnector(int id);

    ActivityListModel *currentActivityModel();

    enum UserRoles {
        NameRole = Qt::UserRole + 1,
        ServerRole,
        ServerHasUserStatusRole,
        StatusRole,
        StatusIconRole,
        StatusEmojiRole,
        StatusMessageRole,
        DesktopNotificationsAllowedRole,
        AvatarRole,
        IsCurrentUserRole,
        IsConnectedRole,
        IdRole,
        CanLogoutRole,
        RemoveAccountTextRole,
        SyncStatusIconRole,
        SyncStatusOkRole,
    };

    [[nodiscard]] AccountAppList appList() const;

signals:
    void addAccount();
    void currentUserChanged();
    void syncErrorUsersChanged();

public slots:
    void fetchCurrentActivityModel();
    void openCurrentAccountLocalFolder();
    void openCurrentAccountServer();
    void openCurrentAccountFolderFromTrayInfo(const QString &fullRemotePath);
    void openCurrentAccountFeaturedApp();
    Q_INVOKABLE void refreshSyncErrorUsers();
    void setCurrentUserId(const int id);
    void login(const int id);
    void logout(const int id);
    void removeAccount(const int id);

protected:
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    static UserModel *_instance;
    UserModel(QObject *parent = nullptr);
    QList<User*> _users;
    int _currentUserId = -1;
    bool _init = true;
    QVector<int> _syncErrorUserIds;

    void updateSyncErrorUsers();
    [[nodiscard]] bool userHasSyncErrors(const User *user) const;

    void buildUserList();
    void addAccsToUserList();
    void setInitialUser();
};

class ImageProvider : public QQuickAsyncImageProvider
{
    Q_OBJECT

public:
    ImageProvider() = default;
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    QThreadPool _pool;
};

class UserAppsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    static UserAppsModel *instance();
    ~UserAppsModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    enum UserAppsRoles {
        NameRole = Qt::UserRole + 1,
        UrlRole,
        IconUrlRole
    };

    void buildAppList();

public slots:
    void openAppUrl(const QUrl &url);

protected:
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    static UserAppsModel *_instance;
    UserAppsModel(QObject *parent = nullptr);

    AccountAppList _apps;
};
}
#endif // USERMODEL_H
