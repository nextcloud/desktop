/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef OCSSHAREJOB_H
#define OCSSHAREJOB_H

#include "networkjobs/jsonjob.h"
#include "sharemanager.h"
#include <QList>
#include <QPair>
#include <QVector>

class QJsonDocument;

namespace OCC {

/**
 * @brief The OcsShareJob class
 * @ingroup gui
 *
 * Handle talking to the OCS Share API. 
 * For creation, deletion and modification of shares.
 */
namespace OcsShareJob {
    /**
     * Get all the shares
     *
     * @param path Path to request shares for (default all shares)
     */
    JsonApiJob *getShares(AccountPtr account, QObject *parent, const QString &path);

    /**
     * Delete the current Share
     */
    JsonApiJob *deleteShare(AccountPtr account, QObject *parent, const QString &shareId);

    /**
     * Set the expiration date of a share
     *
     * @param date The expire date, if this date is invalid the expire date
     * will be removed
     */
    JsonApiJob *setExpireDate(AccountPtr account, QObject *parent, const QString &shareId, const QDate &date);

    /**
     * Set the password of a share
     *
     * @param password The password of the share, if the password is empty the
     * share will be removed
     */
    JsonApiJob *setPassword(AccountPtr account, QObject *parent, const QString &shareId, const QString &password);

    /**
     * Set the share to be public upload
     * 
     * @param publicUpload Set or remove public upload
     */
    JsonApiJob *setPublicUpload(AccountPtr account, QObject *parent, const QString &shareId, bool publicUpload);

    /**
     * Change the name of a share
     */
    JsonApiJob *setName(AccountPtr account, QObject *parent, const QString &shareId, const QString &name);

    /**
     * Set the permissions
     *
     * @param permissions
     */
    JsonApiJob *setPermissions(AccountPtr account, QObject *parent, const QString &shareId,
        const Share::Permissions permissions);

    /**
     * Create a new link share
     *
     * @param path The path of the file/folder to share
     * @param name The name of the link share, empty name auto-generates one
     * @param password Optionally a password for the share
     * @param expireDate Target expire data (may be null)
     * @param permissions Desired permissions (SharePermissionDefault leaves to server)
     */
    JsonApiJob *createLinkShare(AccountPtr account, QObject *parent, const QString &path,
        const QString &name,
        const QString &password,
        const QDate &expireDate,
        const Share::Permissions permissions);

    /**
     * Create a new share
     *
     * @param path The path of the file/folder to share
     * @param shareType The type of share (user/group/link/federated)
     * @param shareWith The uid/gid/federated id to share with
     * @param permissions The permissions the share will have
     */
    JsonApiJob *createShare(AccountPtr account, QObject *parent, const QString &path,
        const Share::ShareType shareType,
        const QString &shareWith = QString(),
        const Share::Permissions permissions = SharePermissionRead);

    /**
     * Returns information on the items shared with the current user.
     */
    JsonApiJob *getSharedWithMe(AccountPtr account, QObject *parent);

};
}

#endif // OCSSHAREJOB_H
