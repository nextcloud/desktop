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

#include "ocsjob.h"
#include "sharemanager.h"
#include <QVector>
#include <QList>
#include <QPair>

namespace OCC {

/**
 * @brief The OcsShareJob class
 * @ingroup gui
 *
 * Handle talking to the OCS Share API. 
 * For creation, deletion and modification of shares.
 */
class OcsShareJob : public OcsJob {
    Q_OBJECT
public:

    /**
     * Constructor for new shares or listing of shares
     */
    explicit OcsShareJob(AccountPtr account);

    /**
     * Get all the shares
     *
     * @param path Path to request shares for (default all shares)
     */
    void getShares(const QString& path = "");

    /**
     * Delete the current Share
     */
    void deleteShare(const QString &shareId);

    /**
     * Set the expiration date of a share
     *
     * @param date The expire date, if this date is invalid the expire date
     * will be removed
     */
    void setExpireDate(const QString &shareId, const QDate& date);

    /**
     * Set the password of a share
     *
     * @param password The password of the share, if the password is empty the
     * share will be removed
     */
    void setPassword(const QString &shareId, const QString& password);

    /**
     * Void set the share to be public upload
     * 
     * @param publicUpload Set or remove public upload
     */
    void setPublicUpload(const QString &shareId, bool publicUpload);

    /**
     * Set the permissions
     *
     * @param permissions
     */
    void setPermissions(const QString &shareId, 
                        const Share::Permissions permissions);

    /**
     * Create a new link share
     *
     * @param path The path of the file/folder to share
     * @param name The name of the link share, empty name auto-generates one
     * @param password Optionally a password for the share
     */
    void createLinkShare(const QString& path, 
                         const QString& name,
                         const QString& password);

    /**
     * Create a new share
     *
     * @param path The path of the file/folder to share
     * @param shareType The type of share (user/group/link/federated)
     * @param shareWith The uid/gid/federated id to share with
     * @param permissions The permissions the share will have
     */
    void createShare(const QString& path, 
                     const Share::ShareType shareType,
                     const QString& shareWith = "",
                     const Share::Permissions permissions = SharePermissionRead);

    /**
     * Returns information on the items shared with the current user.
     */
    void getSharedWithMe();

signals:
    /**
     * Result of the OCS request
     * The value parameter is only set if this was a put request.
     * e.g. if we set the password to 'foo' the QVariant will hold a QString with 'foo'.
     * This is needed so we can update the share objects properly
     *
     * @param reply The reply
     * @param value To what did we set a variable (if we set any).
     */
    void shareJobFinished(QVariantMap reply, QVariant value);

private slots:
    void jobDone(QVariantMap reply);

private:
    QVariant _value;
};

}

#endif // OCSSHAREJOB_H
