/*
 * Copyright (C) by Roeland Jago Douma <rullzer@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef SHARE_H
#define SHARE_H

#include "accountfwd.h"

#include <QObject>
#include <QDate>
#include <QString>
#include <QList>
#include <QSharedPointer>
#include <QUrl>

namespace OCC {

class Share : public QObject {
    Q_OBJECT

public:

    /*
     * Constructor for link shares
     */
    explicit Share(AccountPtr account,
                   const QString& id,
                   const QString& path,
                   int shareType,
                   int permissions,
                   QObject *parent = NULL);

    /*
     * Get the id
     */
    const QString getId();

    /*
     * Get the shareType
     */
    int getShareType();

    /*
     * Get permissions
     */
    int getPermissions();

    /*
     * Set the permissions of a share
     *
     * On success the permissionsSet signal is emitted
     * In case of a server error the serverError signal is emitted.
     */
    void setPermissions(int permissions);

    /**
     * Deletes a share
     *
     * On success the shareDeleted signal is emitted
     * In case of a server error the serverError signal is emitted.
     */
    void deleteShare();

signals:
    void permissionsSet();
    void shareDeleted();
    void serverError(int code, const QString &message);

protected:
    AccountPtr _account;
    QString _id;
    QString _path;
    int _shareType;
    int _permissions;

private slots:
    void slotDeleted(const QVariantMap &reply);

};

/**
 * A Link share is just like a regular share but then slightly different.
 * There are several methods in the API that either work differently for
 * link shares or are only available to link shares.
 */
class LinkShare : public Share {
    Q_OBJECT
public:
 
    explicit LinkShare(AccountPtr account,
                       const QString& id,
                       const QString& path,
                       int shareType,
                       int permissions,
                       bool passwordSet,
                       const QUrl& url,
                       const QDate& expireDate,
                       QObject *parent = NULL);

    /*
     * Get the share link
     */
    const QUrl getLink();

    /*
     * Get the publicUpload status of this share
     */
    bool getPublicUpload();

    /*
     * Set a share to be public upload
     * This function can only be called on link shares
     *
     * On success the publicUploadSet signal is emitted
     * In case of a server error the serverError signal is emitted.
     */
    void setPublicUpload(bool publicUpload);
    
    /*
     * Set the password
     *
     * On success the passwordSet signal is emitted
     * In case of a server error the serverError signal is emitted.
     */
    void setPassword(const QString& password);

    /*
     * Is the password set?
     */
    bool isPasswordSet();

    /*
     * Get the expiration date
     */
    const QDate getExpireDate();

    /*
     * Set the expiration date
     *
     * On success the expireDateSet signal is emitted
     * In case of a server error the serverError signal is emitted.
     */
    void setExpireDate(const QDate& expireDate);

signals:
    void expireDateSet();
    void publicUploadSet();
    void passwordSet();

private slots:
    void slotPasswordSet(const QVariantMap &reply, const QVariant &value);
    void slotPublicUploadSet(const QVariantMap &reply, const QVariant &value);
    void slotExpireDateSet(const QVariantMap &reply, const QVariant &value);

private:
    bool _passwordSet;
    QDate _expireDate;
    QUrl _url;
};

/**
 * The share manager allows for creating, retrieving and deletion
 * of shares. It abstracts away from the OCS Share API, all the usages
 * shares should talk to this manager and not use OCS Share Job directly
 */
class ShareManager : public QObject {
    Q_OBJECT
public:
    explicit ShareManager(AccountPtr _account, QObject *parent = NULL);

    /**
     * Tell the manager to create a link share
     *
     * @param path The path of the linkshare relative to the user folder on the server
     * @param password The password of the share
     *
     * On success the signal linkShareCreated is emitted
     * For older server the linkShareRequiresPassword signal is emitted when it seems appropiate
     * In case of a server error the serverError signal is emitted
     */
    void createLinkShare(const QString& path,
                         const QString& password="");

    /**
     * Fetch all the shares for path
     *
     * @param path The path to get the shares for relative to the users folder on the server
     *
     * On success the sharesFetched signal is emitted
     * In case of a server error the serverError signal is emitted
     */
    void fetchShares(const QString& path);

signals:
    void linkShareCreated(const QSharedPointer<LinkShare> share);
    void linkShareRequiresPassword();
    void sharesFetched(QList<QSharedPointer<Share>>);
    void serverError(int code, const QString &message);

private slots:
    void slotSharesFetched(const QVariantMap &reply);
    void slotLinkShareCreated(const QVariantMap &reply);

private:
    LinkShare *parseLinkShare(const QVariantMap &data);

    AccountPtr _account;
};


}

#endif // SHARE_H
