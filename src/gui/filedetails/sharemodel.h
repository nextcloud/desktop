/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
    Q_PROPERTY(bool fetchOngoing READ fetchOngoing NOTIFY fetchOngoingChanged)
    Q_PROPERTY(bool hasInitialShareFetchCompleted READ hasInitialShareFetchCompleted NOTIFY hasInitialShareFetchCompletedChanged)

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
        ShareTypeCircle = Share::TypeCircle,
        ShareTypeRoom = Share::TypeRoom,
        ShareTypePlaceholderLink = Share::TypePlaceholderLink,
    };
    Q_ENUM(ShareType);

    explicit ShareModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, const int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    AccountState *accountState() const;
    QString localPath() const;

    bool accountConnected() const;
    bool sharingEnabled() const;
    bool publicLinkSharesEnabled() const;
    bool userGroupSharingEnabled() const;
    bool canShare() const;

    bool fetchOngoing() const;
    bool hasInitialShareFetchCompleted() const;

signals:
    void localPathChanged();
    void accountStateChanged();
    void accountConnectedChanged();
    void sharingEnabledChanged();
    void publicLinkSharesEnabledChanged();
    void userGroupSharingEnabledChanged();
    void sharePermissionsChanged();
    void lockExpireStringChanged();
    void fetchOngoingChanged();
    void hasInitialShareFetchCompletedChanged();

    void serverError(const int code, const QString &message);
    void passwordSetError(const QString &shareId);
    void requestPasswordForLinkShare();
    void requestPasswordForEmailSharee(const ShareePtr &sharee);

    void sharesChanged();

public slots:
    void setAccountState(AccountState *accountState);
    void setLocalPath(const QString &localPath);

    void createNewLinkShare() const;
    void createNewLinkShareWithPassword(const QString &password) const;
    void createNewUserGroupShare(const ShareePtr &sharee);
    void createNewUserGroupShareFromQml(const QVariant &sharee);
    void createNewUserGroupShareWithPassword(const ShareePtr &sharee, const QString &password) const;
    void createNewUserGroupShareWithPasswordFromQml(const QVariant &sharee, const QString &password) const;

    void deleteShare(const SharePtr &share) const;
    void deleteShareFromQml(const QVariant &share) const;

    void toggleShareAllowEditing(const SharePtr &share, const bool enable) const;
    void toggleShareAllowEditingFromQml(const QVariant &share, const bool enable) const;
    void toggleShareAllowResharing(const SharePtr &share, const bool enable) const;
    void toggleShareAllowResharingFromQml(const QVariant &share, const bool enable) const;
    void toggleSharePasswordProtect(const SharePtr &share, const bool enable);
    void toggleSharePasswordProtectFromQml(const QVariant &share, const bool enable);
    void toggleShareExpirationDate(const SharePtr &share, const bool enable) const;
    void toggleShareExpirationDateFromQml(const QVariant &share, const bool enable) const;
    void toggleShareNoteToRecipient(const SharePtr &share, const bool enable) const;
    void toggleShareNoteToRecipientFromQml(const QVariant &share, const bool enable) const;

    void setLinkShareLabel(const QSharedPointer<LinkShare> &linkShare, const QString &label) const;
    void setLinkShareLabelFromQml(const QVariant &linkShare, const QString &label) const;
    void setShareExpireDate(const SharePtr &share, const qint64 milliseconds) const;
    // Needed as ints in QML are 32 bits so we need to use a QVariant
    void setShareExpireDateFromQml(const QVariant &share, const QVariant milliseconds) const;
    void setSharePassword(const SharePtr &share, const QString &password);
    void setSharePasswordFromQml(const QVariant &share, const QString &password);
    void setShareNote(const SharePtr &share, const QString &note) const;
    void setShareNoteFromQml(const QVariant &share, const QString &note) const;

private slots:
    void resetData();
    void updateData();
    void initShareManager();

    void slotPropfindReceived(const QVariantMap &result);
    void slotServerError(const int code, const QString &message);
    void slotAddShare(const SharePtr &share);
    void slotRemoveShareWithId(const QString &shareId);
    void slotSharesFetched(const QList<SharePtr> &shares);

    void slotSharePermissionsSet(const QString &shareId);
    void slotSharePasswordSet(const QString &shareId);
    void slotShareNoteSet(const QString &shareId);
    void slotShareNameSet(const QString &shareId);
    void slotShareLabelSet(const QString &shareId);
    void slotShareExpireDateSet(const QString &shareId);

private:
    QString displayStringForShare(const SharePtr &share) const;
    QString iconUrlForShare(const SharePtr &share) const;
    QString avatarUrlForShare(const SharePtr &share) const;
    long long enforcedMaxExpireDateForShare(const SharePtr &share) const;
    bool expireDateEnforcedForShare(const SharePtr &share) const;

    bool _fetchOngoing = false;
    bool _hasInitialShareFetchCompleted = false;
    SharePtr _placeholderLinkShare;

    // DO NOT USE QSHAREDPOINTERS HERE.
    // QSharedPointers MUST NOT be used with pointers already assigned to other shared pointers.
    // This is because they do not share reference counters, and as such are not aware of another
    // smart pointer's use of the same object.
    //
    // We cannot pass objects instantiated in QML using smart pointers through the property interface
    // so we have to pass the pointer here. If we kill the dialog using a smart pointer then
    // these objects will be deallocated for the entire application. We do not want that!!
    AccountState *_accountState;
    Folder *_folder;

    QString _localPath;
    QString _sharePath;
    SharePermissions _maxSharingPermissions;
    QByteArray _numericFileId;
    SyncJournalFileLockInfo _filelockState;
    QString _privateLinkUrl;

    QSharedPointer<ShareManager> _manager;

    QVector<SharePtr> _shares;
    QHash<QString, QPersistentModelIndex> _shareIdIndexHash;
    QHash<QString, QString> _shareIdRecentlySetPasswords;
};

} // namespace OCC
