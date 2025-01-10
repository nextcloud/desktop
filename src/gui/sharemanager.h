/*
 * Copyright (C) by Roeland Jago Douma <rullzer@owncloud.com>
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

#ifndef SHAREMANAGER_H
#define SHAREMANAGER_H

#include "accountfwd.h"
#include "sharee.h"
#include "sharepermissions.h"

#include <QObject>
#include <QDate>
#include <QString>
#include <QList>
#include <QSharedPointer>
#include <QUrl>

class QJsonDocument;
class QJsonObject;

namespace OCC {

class OcsShareJob;

class Share : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AccountPtr account READ account CONSTANT)
    Q_PROPERTY(QString path READ path CONSTANT)
    Q_PROPERTY(QString id READ getId CONSTANT)
    Q_PROPERTY(QString uidOwner READ getUidOwner CONSTANT)
    Q_PROPERTY(QString ownerDisplayName READ getOwnerDisplayName CONSTANT)
    Q_PROPERTY(ShareType shareType READ getShareType CONSTANT)
    Q_PROPERTY(ShareePtr shareWith READ getShareWith CONSTANT)
    Q_PROPERTY(Permissions permissions READ getPermissions WRITE setPermissions NOTIFY permissionsSet)
    Q_PROPERTY(bool isPasswordSet READ isPasswordSet NOTIFY passwordSet)

public:
    /**
     * Possible share types
     * Need to be in sync with Sharee::Type
     */
    enum ShareType {
        TypeSecureFileDropPlaceholderLink = -3,
        TypeInternalLink = -2,
        TypePlaceholderLink = -1,
        TypeUser = Sharee::User,
        TypeGroup = Sharee::Group,
        TypeLink = 3,
        TypeEmail = Sharee::Email,
        TypeRemote = Sharee::Federated,
        TypeCircle = Sharee::Circle,
        TypeRoom = Sharee::Room,
    };
    Q_ENUM(ShareType);

    using Permissions = SharePermissions;

    Q_ENUM(Permissions);

    /*
     * Constructor for shares
     */
    explicit Share(AccountPtr account,
                   const QString &id,
                   const QString &owner,
                   const QString &ownerDisplayName,
                   const QString &path,
                   const ShareType shareType,
                   bool isPasswordSet = false,
                   const Permissions permissions = SharePermissionAll,
                   const ShareePtr shareWith = ShareePtr(nullptr));

    /**
     * The account the share is defined on.
     */
    [[nodiscard]] AccountPtr account() const;

    [[nodiscard]] QString path() const;

    /*
     * Get the id
     */
    [[nodiscard]] QString getId() const;

    /*
     * Get the uid_owner
     */
    [[nodiscard]] QString getUidOwner() const;

    /*
     * Get the owner display name
     */
    [[nodiscard]] QString getOwnerDisplayName() const;

    /*
     * Get the shareType
     */
    [[nodiscard]] ShareType getShareType() const;

    /*
     * Get the shareWith
     */
    [[nodiscard]] ShareePtr getShareWith() const;

    /*
     * Get permissions
     */
    [[nodiscard]] Permissions getPermissions() const;

    /*
     * Get whether the share has a password set
     */
    [[nodiscard]] Q_REQUIRED_RESULT bool isPasswordSet() const;

     /*
     * Is it a share with a user or group (local or remote)
     */
    [[nodiscard]] static bool isShareTypeUserGroupEmailRoomOrRemote(const ShareType type);

signals:
    void permissionsSet();
    void shareDeleted();
    void serverError(int code, const QString &message);
    void passwordSet();
    void hideDownloadSet();
    void passwordSetError(int statusCode, const QString &message);    

public slots:
    /*
     * Deletes a share
     *
     * On success the shareDeleted signal is emitted
     * In case of a server error the serverError signal is emitted.
     */
    void deleteShare();

    /*
     * Set the permissions of a share
     *
     * On success the permissionsSet signal is emitted
     * In case of a server error the serverError signal is emitted.
     */
    void setPermissions(OCC::Share::Permissions permissions);

    /*
     * Set the password for remote share
     *
     * On success the passwordSet signal is emitted
     * In case of a server error the passwordSetError signal is emitted.
     */
    void setPassword(const QString &password);

protected:
    AccountPtr _account;
    QString _id;
    QString _uidowner;
    QString _ownerDisplayName;
    QString _path;
    ShareType _shareType;
    bool _isPasswordSet;
    Permissions _permissions;
    ShareePtr _shareWith;

protected slots:
    void slotOcsError(int statusCode, const QString &message);
    void slotPasswordSet(const QJsonDocument &, const QVariant &value);
    void slotSetPasswordError(int statusCode, const QString &message);

private slots:
    void slotDeleted();
    void slotPermissionsSet(const QJsonDocument &, const QVariant &value);
};

using SharePtr = QSharedPointer<Share>;

/**
 * A Link share is just like a regular share but then slightly different.
 * There are several methods in the API that either work differently for
 * link shares or are only available to link shares.
 */
class LinkShare : public Share
{
    Q_OBJECT
    Q_PROPERTY(QUrl link READ getLink CONSTANT)
    Q_PROPERTY(QUrl directDownloadLink READ getDirectDownloadLink CONSTANT)
    Q_PROPERTY(bool publicCanUpload READ getPublicUpload CONSTANT)
    Q_PROPERTY(bool publicCanReadDirectory READ getShowFileListing CONSTANT)
    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameSet)
    Q_PROPERTY(QString note READ getNote WRITE setNote NOTIFY noteSet)
    Q_PROPERTY(QString label READ getLabel WRITE setLabel NOTIFY labelSet)
    Q_PROPERTY(bool hideDownload READ getHideDownload WRITE setHideDownload NOTIFY hideDownloadSet)
    Q_PROPERTY(QDate expireDate READ getExpireDate WRITE setExpireDate NOTIFY expireDateSet)
    Q_PROPERTY(QString token READ getToken CONSTANT)

public:
    explicit LinkShare(AccountPtr account,
        const QString &id,
        const QString &uidowner,
        const QString &ownerDisplayName,
        const QString &path,
        const QString &name,
        const QString &token,
        const Permissions permissions,
        bool isPasswordSet,
        const QUrl &url,
        const QDate &expireDate,
        const QString &note,
        const QString &label,
        const bool hideDownload);

    /*
     * Get the share link
     */
    [[nodiscard]] QUrl getLink() const;

    /*
     * The share's link for direct downloading.
     */
    [[nodiscard]] QUrl getDirectDownloadLink() const;

    /*
     * Get the publicUpload status of this share
     */
    [[nodiscard]] bool getPublicUpload() const;

    /*
     * Whether directory listings are available (READ permission)
     */
    [[nodiscard]] bool getShowFileListing() const;

    /*
     * Returns the name of the link share. Can be empty.
     */
    [[nodiscard]] QString getName() const;

    /*
     * Returns the note of the link share.
     */
    [[nodiscard]] QString getNote() const;
    
    /*
     * Returns the label of the link share.
     */
    [[nodiscard]] QString getLabel() const;

    /*
     * Returns if the link share's hideDownload is true or false
     */
    [[nodiscard]] bool getHideDownload() const;

    /*
     * Returns the token of the link share.
     */
    [[nodiscard]] QString getToken() const;

    /*
     * Get the expiration date
     */
    [[nodiscard]] QDate getExpireDate() const;
    
    /*
     * Create OcsShareJob and connect to signal/slots
     */
    template <typename LinkShareSlot>
    OcsShareJob *createShareJob(const LinkShareSlot slotFunction);
    
public slots:
    /*
     * Set the name of the link share.
     *
     * Emits either nameSet() or serverError().
     */
    void setName(const QString &name);

    /*
     * Set the note of the link share.
     */
    void setNote(const QString &note);

    /*
     * Set the expiration date
     *
     * On success the expireDateSet signal is emitted
     * In case of a server error the serverError signal is emitted.
     */
    void setExpireDate(const QDate &expireDate);

    /*
     * Set the label of the share link.
     */
    void setLabel(const QString &label);

    /*
     * Set the hideDownload flag of the share link.
     */
    void setHideDownload(const bool hideDownload);
    
signals:
    void expireDateSet();
    void noteSet();
    void nameSet();
    void labelSet();

private slots:
    void slotNoteSet(const QJsonDocument &, const QVariant &value);
    void slotExpireDateSet(const QJsonDocument &reply, const QVariant &value);
    void slotNameSet(const QJsonDocument &, const QVariant &value);
    void slotLabelSet(const QJsonDocument &, const QVariant &value);
    void slotHideDownloadSet(const QJsonDocument &jsonDoc, const QVariant &hideDownload);

private:
    QString _name;
    QString _token;
    QString _note;
    QDate _expireDate;
    QUrl _url;
    QString _label;
    bool _hideDownload = false;
};

class UserGroupShare : public Share
{
    Q_OBJECT
    Q_PROPERTY(QString note READ getNote WRITE setNote NOTIFY noteSet)
    Q_PROPERTY(QDate expireDate READ getExpireDate WRITE setExpireDate NOTIFY expireDateSet)
public:
    UserGroupShare(AccountPtr account,
        const QString &id,
        const QString &owner,
        const QString &ownerDisplayName,
        const QString &path,
        const ShareType shareType,
        bool isPasswordSet,
        const Permissions permissions,
        const ShareePtr shareWith,
        const QDate &expireDate,
        const QString &note);

    [[nodiscard]] QString getNote() const;
    [[nodiscard]] QDate getExpireDate() const;

public slots:
    void setNote(const QString &note);
    void setExpireDate(const QDate &date);

signals:
    void noteSet();
    void noteSetError();
    void expireDateSet();

private slots:
     void slotNoteSet(const QJsonDocument &json, const QVariant &note);
     void slotExpireDateSet(const QJsonDocument &reply, const QVariant &value);

private:
    QString _note;
    QDate _expireDate;
};

/**
 * The share manager allows for creating, retrieving and deletion
 * of shares. It abstracts away from the OCS Share API, all the usages
 * shares should talk to this manager and not use OCS Share Job directly
 */
class ShareManager : public QObject
{
    Q_OBJECT
public:
    explicit ShareManager(AccountPtr _account, QObject *parent = nullptr);

    /**
     * Tell the manager to create a link share
     *
     * @param path The path of the linkshare relative to the user folder on the server
     * @param name The name of the created share, may be empty
     * @param password The password of the share, may be empty
     *
     * On success the signal linkShareCreated is emitted
     * For older server the linkShareRequiresPassword signal is emitted when it seems appropriate
     * In case of a server error the serverError signal is emitted
     */
    void createLinkShare(const QString &path,
        const QString &name,
        const QString &password);

    void createSecureFileDropShare(const QString &path, const QString &name, const QString &password);

    /**
     * Tell the manager to create a new share
     *
     * @param path The path of the share relative to the user folder on the server
     * @param shareType The type of share (TypeUser, TypeGroup, TypeRemote)
     * @param Permissions The share permissions
     *
     * On success the signal shareCreated is emitted
     * In case of a server error the serverError signal is emitted
     */
    void createShare(const QString &path,
        const Share::ShareType shareType,
        const QString shareWith,
        const Share::Permissions permissions,
        const QString &password = "");

        /**
     * Tell the manager to create and start new UpdateE2eeShareMetadataJob job
     *
     * @param fullRemotePath The path of the share relative to the user folder on the server
     * @param shareType The type of share (TypeUser, TypeGroup, TypeRemote)
     * @param Permissions The share permissions
     * @param folderId The id for an E2EE folder
     * @param password An optional password for a share
     *
     * On success the signal shareCreated is emitted
     * In case of a server error the serverError signal is emitted
     */
    void createE2EeShareJob(const QString &fullRemotePath,
                            const ShareePtr sharee,
                            const Share::Permissions permissions,
                            const QString &password = "");

    /**
     * Fetch all the shares for path
     *
     * @param path The path to get the shares for relative to the users folder on the server
     *
     * On success the sharesFetched signal is emitted
     * In case of a server error the serverError signal is emitted
     */
    void fetchShares(const QString &path);

    /**
     * Fetch shares with the current user for path
     *
     * @param path The path to get the shares for relative to the users folder on the server
     *
     * On success the sharedWithMeFetched signal is emitted
     * In case of a server error the serverError signal is emitted
     */
    void fetchSharedWithMe(const QString &path);

signals:
    void shareCreated(const OCC::SharePtr &share);
    void linkShareCreated(const QSharedPointer<OCC::LinkShare> &share);
    void sharesFetched(const QList<OCC::SharePtr> &shares);
    void sharedWithMeFetched(const QList<OCC::SharePtr> &shares);
    void serverError(int code, const QString &message);

    /** Emitted when creating a link share with password fails.
     *
     * @param message the error message reported by the server
     *
     * See createLinkShare().
     */
    void linkShareRequiresPassword(const QString &message);

private slots:
    void slotSharesFetched(const QJsonDocument &reply);
    void slotSharedWithMeFetched(const QJsonDocument &reply);
    void slotLinkShareCreated(const QJsonDocument &reply);
    void slotShareCreated(const QJsonDocument &reply);
    void slotOcsError(int statusCode, const QString &message);
    void slotCreateE2eeShareJobFinised(int statusCode, const QString &message);

private:
    QSharedPointer<LinkShare> parseLinkShare(const QJsonObject &data) const;
    QSharedPointer<UserGroupShare> parseUserGroupShare(const QJsonObject &data) const;
    SharePtr parseShare(const QJsonObject &data) const;
    const QList<OCC::SharePtr> parseShares(const QJsonDocument &reply) const;

    AccountPtr _account;
};
}

Q_DECLARE_METATYPE(OCC::SharePtr)

#endif // SHAREMANAGER_H
