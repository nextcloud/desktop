/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OCSSHAREJOB_H
#define OCSSHAREJOB_H

#include "ocsjob.h"
#include "sharemanager.h"
#include <QVector>
#include <QList>
#include <QPair>

class QJsonDocument;

namespace OCC {

/**
 * @brief The OcsShareJob class
 * @ingroup gui
 *
 * Handle talking to the OCS Share API.
 * For creation, deletion and modification of shares.
 */
class OcsShareJob : public OcsJob
{
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
    void getShares(const QString &path = "", const QMap<QString, QString> &params = {});

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
    void setExpireDate(const QString &shareId, const QDate &date);

	 /**
     * Set note a share
     *
     * @param note The note to a share, if the note is empty the
     * share will be removed
     */
    void setNote(const QString &shareId, const QString &note);

    /**
     * Set the password of a share
     *
     * @param password The password of the share, if the password is empty the
     * share will be removed
     */
    void setPassword(const QString &shareId, const QString &password);

    /**
     * Set the share to be public upload
     *
     * @param publicUpload Set or remove public upload
     */
    void setPublicUpload(const QString &shareId, bool publicUpload);

    /**
     * Change the name of a share
     */
    void setName(const QString &shareId, const QString &name);

    /**
     * Set the permissions
     *
     * @param permissions
     */
    void setPermissions(const QString &shareId,
        const Share::Permissions permissions);
    
    /**
     * Set share link label
     */
    void setLabel(const QString &shareId, const QString &label);

    /**
     * Set share hideDownload flag
     */
    void setHideDownload(const QString &shareId, const bool hideDownload);

    /**
     * Create a new link share
     *
     * @param path The path of the file/folder to share
     * @param password Optionally a password for the share
     */
    void createLinkShare(const QString &path, const QString &name,
        const QString &password);

    void createSecureFileDropLinkShare(const QString &path, const QString &name, const QString &password);

    /**
     * Create a new share
     *
     * @param path The path of the file/folder to share
     * @param shareType The type of share (user/group/link/federated)
     * @param shareWith The uid/gid/federated id to share with
     * @param permissions The permissions the share will have
     * @param password The password to protect the share with
     */
    void createShare(const QString &path,
        const Share::ShareType shareType,
        const QString &shareWith = "",
        const Share::Permissions permissions = SharePermissionRead,
        const QString &password = "");

    /**
     * Returns information on the items shared with the current user.
     * @param path Path to request shares for (default all shares)
     */
    void getSharedWithMe(const QString &path = "");

    static const QString _pathForSharesRequest;

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
    void shareJobFinished(QJsonDocument reply, QVariant value);

private slots:
    void jobDone(QJsonDocument reply);

private:
    QVariant _value;
};
}

#endif // OCSSHAREJOB_H
