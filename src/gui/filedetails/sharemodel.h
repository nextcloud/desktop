/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QAbstractListModel>

#include "accountstate.h"
#include "folder.h"
#include "sharemanager.h"
#include "sharepermissions.h"

namespace OCC {

class ShareModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(AccountState* accountState READ accountState WRITE setAccountState NOTIFY accountStateChanged)
    Q_PROPERTY(QString localPath READ localPath WRITE setLocalPath NOTIFY localPathChanged)
    Q_PROPERTY(bool accountConnected READ accountConnected NOTIFY accountConnectedChanged)
    Q_PROPERTY(bool sharingEnabled READ sharingEnabled NOTIFY sharingEnabledChanged)
    Q_PROPERTY(bool publicLinkSharesEnabled READ publicLinkSharesEnabled NOTIFY publicLinkSharesEnabledChanged)
    Q_PROPERTY(bool userGroupSharingEnabled READ userGroupSharingEnabled NOTIFY userGroupSharingEnabledChanged)
    Q_PROPERTY(bool canShare READ canShare NOTIFY sharePermissionsChanged)
    Q_PROPERTY(bool isShareDisabledEncryptedFolder READ isShareDisabledEncryptedFolder NOTIFY isShareDisabledEncryptedFolderChanged)
    Q_PROPERTY(bool fetchOngoing READ fetchOngoing NOTIFY fetchOngoingChanged)
    Q_PROPERTY(bool hasInitialShareFetchCompleted READ hasInitialShareFetchCompleted NOTIFY hasInitialShareFetchCompletedChanged)
    Q_PROPERTY(bool serverAllowsResharing READ serverAllowsResharing NOTIFY serverAllowsResharingChanged)
    Q_PROPERTY(QVariantList sharees READ sharees NOTIFY shareesChanged)
    Q_PROPERTY(bool displayShareOwner READ displayShareOwner NOTIFY displayShareOwnerChanged)
    Q_PROPERTY(QString shareOwnerDisplayName READ shareOwnerDisplayName NOTIFY shareOwnerDisplayNameChanged)
    Q_PROPERTY(QString shareOwnerAvatar READ shareOwnerAvatar NOTIFY shareOwnerAvatarChanged)
    Q_PROPERTY(bool sharedWithMeExpires READ sharedWithMeExpires NOTIFY sharedWithMeExpiresChanged)
    Q_PROPERTY(QString sharedWithMeRemainingTimeString READ sharedWithMeRemainingTimeString NOTIFY sharedWithMeRemainingTimeStringChanged)

public:
    enum Roles {
        ShareRole = Qt::UserRole + 1,
        ShareTypeRole,
        ShareIdRole,
        IconUrlRole,
        AvatarUrlRole,
        LinkRole,
        LinkShareNameRole,
        LinkShareLabelRole,
        NoteEnabledRole,
        NoteRole,
        ExpireDateEnabledRole,
        ExpireDateEnforcedRole,
        ExpireDateRole,
        EnforcedMaximumExpireDateRole,
        PasswordProtectEnabledRole,
        PasswordRole,
        PasswordEnforcedRole,
        EditingAllowedRole,
        CurrentPermissionModeRole,
        SharedItemTypeRole,
        IsSharePermissionsChangeInProgress,
        HideDownloadEnabledRole,
        IsHideDownloadEnabledChangeInProgress,
        ResharingAllowedRole,
    };
    Q_ENUM(Roles)

    /**
     * Possible share types
     * Need to be in sync with Share::ShareType.
     * We use this in QML.
     */
    enum ShareType {
        ShareTypeUser = Share::TypeUser,
        ShareTypeGroup = Share::TypeGroup,
        ShareTypeLink = Share::TypeLink,
        ShareTypeEmail = Share::TypeEmail,
        ShareTypeRemote = Share::TypeRemote,
        ShareTypeTeam = Share::TypeTeam,
        ShareTypeRoom = Share::TypeRoom,
        ShareTypePlaceholderLink = Share::TypePlaceholderLink,
        ShareTypeInternalLink = Share::TypeInternalLink,
        ShareTypeSecureFileDropPlaceholderLink = Share::TypeSecureFileDropPlaceholderLink,
    };
    Q_ENUM(ShareType);
    
    enum class SharedItemType {
        SharedItemTypeUndefined = -1,
        SharedItemTypeFile,
        SharedItemTypeFolder,
        SharedItemTypeEncryptedFile,
        SharedItemTypeEncryptedFolder,
        SharedItemTypeEncryptedTopLevelFolder,
    };
    Q_ENUM(SharedItemType);
    
    enum class SharePermissionsMode {
        ModeViewOnly,
        ModeUploadAndEditing,
        ModeFileDropOnly,
    };
    Q_ENUM(SharePermissionsMode);

    explicit ShareModel(QObject *parent = nullptr);

    [[nodiscard]] QVariant data(const QModelIndex &index, const int role) const override;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] AccountState *accountState() const;
    [[nodiscard]] QString localPath() const;

    [[nodiscard]] bool accountConnected() const;
    [[nodiscard]] bool sharingEnabled() const;
    [[nodiscard]] bool publicLinkSharesEnabled() const;
    [[nodiscard]] bool userGroupSharingEnabled() const;
    [[nodiscard]] bool canShare() const;
    [[nodiscard]] bool serverAllowsResharing() const;
    [[nodiscard]] bool isShareDisabledEncryptedFolder() const;

    [[nodiscard]] bool fetchOngoing() const;
    [[nodiscard]] bool hasInitialShareFetchCompleted() const;

    [[nodiscard]] QVariantList sharees() const;

    [[nodiscard]] bool displayShareOwner() const;
    [[nodiscard]] QString shareOwnerDisplayName() const;
    [[nodiscard]] QString shareOwnerAvatar() const;
    [[nodiscard]] bool sharedWithMeExpires() const;
    [[nodiscard]] QString sharedWithMeRemainingTimeString() const;

    [[nodiscard]] Q_INVOKABLE static QString generatePassword();

signals:
    void localPathChanged();
    void accountStateChanged();
    void accountConnectedChanged();
    void sharingEnabledChanged();
    void publicLinkSharesEnabledChanged();
    void userGroupSharingEnabledChanged();
    void sharePermissionsChanged();
    void isShareDisabledEncryptedFolderChanged();
    void lockExpireStringChanged();
    void fetchOngoingChanged();
    void hasInitialShareFetchCompletedChanged();
    void shareesChanged();
    void internalLinkReady();
    void serverAllowsResharingChanged();
    void displayShareOwnerChanged();
    void shareOwnerDisplayNameChanged();
    void shareOwnerAvatarChanged();
    void sharedWithMeExpiresChanged();
    void sharedWithMeRemainingTimeStringChanged();

    void serverError(const int code, const QString &message) const;
    void passwordSetError(const QString &shareId, const int code, const QString &message);
    void requestPasswordForLinkShare();
    void requestPasswordForEmailSharee(const OCC::ShareePtr &sharee);

    void sharesChanged();

public slots:
    void setAccountState(OCC::AccountState *accountState);
    void setLocalPath(const QString &localPath);

    void createNewLinkShare() const;
    void createNewLinkShareWithPassword(const QString &password) const;
    void createNewUserGroupShare(const OCC::ShareePtr &sharee);
    void createNewUserGroupShareFromQml(const QVariant &sharee);
    void createNewUserGroupShareWithPassword(const OCC::ShareePtr &sharee, const QString &password) const;
    void createNewUserGroupShareWithPasswordFromQml(const QVariant &sharee, const QString &password) const;

    void deleteShare(const OCC::SharePtr &share) const;
    void deleteShareFromQml(const QVariant &share) const;

    void toggleHideDownloadFromQml(const QVariant &share, const bool enable);
    void toggleShareAllowEditing(const OCC::SharePtr &share, const bool enable);
    void toggleShareAllowEditingFromQml(const QVariant &share, const bool enable);
    void toggleShareAllowResharing(const OCC::SharePtr &share, const bool enable);
    void toggleShareAllowResharingFromQml(const QVariant &share, const bool enable);
    void toggleSharePasswordProtect(const OCC::SharePtr &share, const bool enable);
    void toggleSharePasswordProtectFromQml(const QVariant &share, const bool enable);
    void toggleShareExpirationDate(const OCC::SharePtr &share, const bool enable) const;
    void toggleShareExpirationDateFromQml(const QVariant &share, const bool enable) const;
    void changePermissionModeFromQml(const QVariant &share, const OCC::ShareModel::SharePermissionsMode permissionMode);

    void setLinkShareLabel(const QSharedPointer<OCC::LinkShare> &linkShare, const QString &label) const;
    void setLinkShareLabelFromQml(const QVariant &linkShare, const QString &label) const;
    void setShareExpireDate(const OCC::SharePtr &share, const qint64 milliseconds) const;
    // Needed as ints in QML are 32 bits so we need to use a QVariant
    void setShareExpireDateFromQml(const QVariant &share, const QVariant milliseconds) const;
    void setSharePassword(const OCC::SharePtr &share, const QString &password);
    void setSharePasswordFromQml(const QVariant &share, const QString &password);
    void setShareNote(const OCC::SharePtr &share, const QString &note) const;
    void setShareNoteFromQml(const QVariant &share, const QString &note) const;

private slots:
    void resetData();
    void updateData();
    void initShareManager();
    void handlePlaceholderLinkShare();
    void handleSecureFileDropLinkShare();
    void handleLinkShare();
    void setupInternalLinkShare();
    void setSharePermissionChangeInProgress(const QString &shareId, const bool isInProgress);
    void setHideDownloadEnabledChangeInProgress(const QString &shareId, const bool isInProgress);

    void slotPropfindReceived(const QVariantMap &result);
    void slotAddShare(const OCC::SharePtr &share);
    void slotRemoveShareWithId(const QString &shareId);
    void slotSharesFetched(const QList<OCC::SharePtr> &shares);
    void slotSharedWithMeFetched(const QList<OCC::SharePtr> &shares);
    void slotAddSharee(const OCC::ShareePtr &sharee);
    void slotRemoveSharee(const OCC::ShareePtr &sharee);

    void slotSharePermissionsSet(const QString &shareId);
    void slotSharePasswordSet(const QString &shareId);
    void slotShareNoteSet(const QString &shareId);
    void slotHideDownloadSet(const QString &shareId);
    void slotShareNameSet(const QString &shareId);
    void slotShareLabelSet(const QString &shareId);
    void slotShareExpireDateSet(const QString &shareId);
    void slotDeleteE2EeShare(const OCC::SharePtr &share) const;

private:
    [[nodiscard]] QString displayStringForShare(const SharePtr &share, bool verbose = false) const;
    [[nodiscard]] QString iconUrlForShare(const SharePtr &share) const;
    [[nodiscard]] QString avatarUrlForShare(const SharePtr &share) const;
    [[nodiscard]] long long enforcedMaxExpireDateForShare(const SharePtr &share) const;
    [[nodiscard]] bool expireDateEnforcedForShare(const SharePtr &share) const;
    [[nodiscard]] bool validCapabilities() const;
    [[nodiscard]] bool isSecureFileDropSupportedFolder() const;
    [[nodiscard]] bool isEncryptedItem() const;

    bool _fetchOngoing = false;
    bool _hasInitialShareFetchCompleted = false;
    bool _sharePermissionsChangeInProgress = false;
    bool _hideDownloadEnabledChangeInProgress = false;
    bool _isShareDisabledEncryptedFolder = false;
    SharePtr _placeholderLinkShare;
    SharePtr _internalLinkShare;
    SharePtr _secureFileDropPlaceholderLinkShare;

    QPointer<AccountState> _accountState;
    QPointer<Folder> _synchronizationFolder;

    QString _localPath;
    QString _sharePath;
    SharePermissions _maxSharingPermissions;
    QByteArray _numericFileId;
    SharedItemType _sharedItemType = SharedItemType::SharedItemTypeUndefined;
    SyncJournalFileLockInfo _filelockState;
    QString _privateLinkUrl;
    QByteArray _fileRemoteId;
    bool _displayShareOwner = false;
    QString _shareOwnerDisplayName;
    QString _shareOwnerAvatar;
    bool _sharedWithMeExpires = false;
    QString _sharedWithMeRemainingTimeString;

    QSharedPointer<ShareManager> _manager;

    QVector<SharePtr> _shares;
    QHash<QString, QPersistentModelIndex> _shareIdIndexHash;
    QHash<QString, QString> _shareIdRecentlySetPasswords;
    QVector<ShareePtr> _sharees;
    // Buckets of sharees with the same display name
    QHash<unsigned int, QSharedPointer<QSet<unsigned int>>> _duplicateDisplayNameShareIndices;
};

} // namespace OCC
