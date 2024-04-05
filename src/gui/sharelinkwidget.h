/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 * Copyright (C) 2015 by Klaas Freitag <freitag@owncloud.com>
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

#ifndef SHARELINKWIDGET_H
#define SHARELINKWIDGET_H

#include "libsync/accountfwd.h"

#include "gui/sharemanager.h"
#include "gui/sharepermissions.h"

#include "QProgressIndicator.h"

#include <QDialog>
#include <QSharedPointer>
#include <QList>

class QMenu;
class QTableWidgetItem;

namespace OCC {

namespace Ui {
    class ShareLinkWidget;
}

class AbstractCredentials;
class SyncResult;
class LinkShare;
class ShareManager;

/**
 * @brief The ShareDialog class
 * @ingroup gui
 */
class ShareLinkWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ShareLinkWidget(AccountPtr account,
        const QString &sharePath,
        const QString &localPath,
        SharePermissions maxSharingPermissions,
        QWidget *parent = nullptr);
    ~ShareLinkWidget() override;
    void getShares();

private Q_SLOTS:
    void slotSharesFetched(const QList<QSharedPointer<Share>> &shares);
    void slotShareSelectionChanged();

    void slotShareNameEntered();
    void slotDeleteShareClicked();
    void slotCheckBoxPasswordClicked();
    void slotCheckBoxExpireClicked();
    void slotPasswordReturnPressed();
    void slotPermissionsClicked();
    void slotExpireDateChanged(const QDate &date);
    void slotPasswordChanged(const QString &newText);
    void slotNameEdited(QTableWidgetItem *item);

    void slotContextMenuButtonClicked();
    void slotLinkContextMenuActionTriggered(QAction *action);

    void slotDeleteShareFetched();
    void slotCreateShareFetched(const QSharedPointer<LinkShare> &share);
    void slotCreateShareForbidden(const QString &message);
    void slotPasswordSet();
    void slotExpireSet();
    void slotPermissionsSet();

    void slotServerError(int code, const QString &message);
    void slotPasswordSetError(int code, const QString &message);

private:
    void displayError(const QString &errMsg);

    void setPassword(const QString &password);
    void setExpireDate(const QDate &date);

    void copyShareLink(const QUrl &url);
    void emailShareLink(const QUrl &url);
    void openShareLink(const QUrl &url);

    /** Confirm with the user and then delete the share */
    void confirmAndDeleteShare(const QSharedPointer<LinkShare> &share);

    /** Retrieve a share's name, accounting for _namesSupported */
    QString shareName(const LinkShare &share) const;

    /** Permission implied by current ui state */
    SharePermissions uiPermissionState() const;

    /**
     * Retrieve the selected share, returning 0 if none.
     *
     * Returning 0 means that the "Create new..." item is selected.
     */
    QSharedPointer<LinkShare> selectedShare() const;

    /** Returns the default expire date as requested by the server, invalid otherwise */
    QDate capabilityDefaultExpireDate() const;

    Ui::ShareLinkWidget *_ui;
    AccountPtr _account;
    QString _sharePath;
    QString _localPath;
    QString _shareUrl;

    QProgressIndicator *_pi_create;
    QProgressIndicator *_pi_password;
    QProgressIndicator *_pi_date;
    QProgressIndicator *_pi_editing;

    ShareManager *_manager;

    bool _isFile;
    bool _expiryRequired;
    bool _namesSupported;

    // For maintaining the selection and temporary ui state
    // when getShares() finishes, but the selection didn't
    // change.
    QString _selectedShareId;

    // When a new share is created, we want to select it
    // the next time getShares() finishes. This stores its id.
    QString _newShareOverrideSelectionId;

    QMenu *_linkContextMenu = nullptr;
    QAction *_deleteLinkAction = nullptr;
    QAction *_openLinkAction = nullptr;
    QAction *_copyLinkAction = nullptr;
    QAction *_copyDirectLinkAction = nullptr;
    QAction *_emailLinkAction = nullptr;
    QAction *_emailDirectLinkAction = nullptr;
};
}

#endif // SHARELINKWIDGET_H
