/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef OCSSHAREJOB_H
#define OCSSHAREJOB_H

#include "ocsjob.h"
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
     * Support sharetypes
     */
    enum class SHARETYPE : int {
        LINK = 3
    };

    /**
     * Possible permissions
     */
    enum class PERMISSION : int {
        READ = 1,
        UPDATE = 2,
        CREATE = 4,
        DELETE = 8,
        SHARE = 16,
        ALL = 31
    };

    /**
     * Constructor for new shares or listing of shares
     */
    explicit OcsShareJob(AccountPtr account, QObject *parent = 0);

    /**
     * Constructors for existing shares of which we know the shareId
     */
    explicit OcsShareJob(int shareId, AccountPtr account, QObject *parent = 0);

    /**
     * Get all the shares
     *
     * @param path Path to request shares for (default all shares)
     */
    void getShares(const QString& path = "");

    /**
     * Delete the current Share
     */
    void deleteShare();

    /**
     * Set the expiration date of a share
     *
     * @param date The expire date, if this date is invalid the expire date
     * will be removed
     */
    void setExpireDate(const QDate& date);

    /**
     * Set the password of a share
     *
     * @param password The password of the share, if the password is empty the
     * share will be removed
     */
    void setPassword(const QString& password);

    /**
     * Void set the a share to be public upload
     * 
     * @param publicUpload Set or remove public upload
     */
    void setPublicUpload(bool publicUpload);

    /**
     * Create a new share
     *
     * @param path The path of the file/folder to share
     * @param shareType The type of share (user/group/link/federated)
     * @param password Optionally a password for the share
     * @param date Optionally an expire date for the share
     */
    void createShare(const QString& path, SHARETYPE shareType, const QString& password = "", const QDate& date = QDate());
};

}

#endif // OCSSHAREJOB_H
