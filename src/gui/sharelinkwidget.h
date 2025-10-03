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

#include "accountfwd.h"
#include "sharepermissions.h"
#include "QProgressIndicator.h"
#include <QDialog>
#include <QSharedPointer>
#include <QList>
#include <QToolButton>

class QMenu;
class QTableWidgetItem;

namespace OCC {

namespace Ui {
    class ShareLinkWidget;
}

class AbstractCredentials;
class SyncResult;
class LinkShare;
class Share;

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
    ~ShareLinkWidget();

    void toggleButton(bool show);
    void setupUiOptions();

    void setLinkShare(QSharedPointer<LinkShare> linkShare);
    QSharedPointer<LinkShare> getLinkShare();

    void focusPasswordLineEdit();

public slots:
    void slotDeleteShareFetched();
    void slotToggleShareLinkAnimation(bool start);
    void slotToggleButtonAnimation(QToolButton *button, QProgressIndicator *progressIndicator, bool optionEnabled, bool start);
    void slotServerError(int code, const QString &message);
    void slotCreateShareRequiresPassword(const QString &message);
    void slotStyleChanged();

private slots:
    void slotCreateShareLink(bool clicked);

    void slotCreatePassword();
    void slotPasswordSet();
    void slotPasswordSetError(int code, const QString &message);

	void slotCreateNote();
    void slotNoteSet();

    void slotSetExpireDate();
    void slotExpireDateSet();

    void slotContextMenuButtonClicked();
    void slotLinkContextMenuActionTriggered(QAction *action);

    void slotDeleteAnimationFinished();
    void slotAnimationFinished();

signals:
    void createLinkShare();
    void deleteLinkShare();
    void resizeRequested();
    void visualDeletionDone();
    void createPassword(const QString &password);
    void createPasswordProcessed();

private:
    void displayError(const QString &errMsg);

    void showPasswordOptions(bool show);
    void togglePasswordOptions(bool enable);

	void showNoteOptions(bool show);
    void toggleNoteOptions(bool enable);
    void setNote(const QString &note);

    void showExpireDateOptions(bool show);
    void toggleExpireDateOptions(bool enable);

    void slotCopyLinkShare(bool clicked);

    /** Confirm with the user and then delete the share */
    void confirmAndDeleteShare();

    /** Retrieve a share's name, accounting for _namesSupported */
    QString shareName() const;

    void startAnimation(const int start, const int end);

    void customizeStyle();

    Ui::ShareLinkWidget *_ui;
    AccountPtr _account;
    QString _sharePath;
    QString _localPath;
    QString _shareUrl;

    QSharedPointer<LinkShare> _linkShare;

    bool _isFile;
    bool _passwordRequired;
    bool _expiryRequired;
    bool _namesSupported;
    bool _noteRequired;

    QMenu *_linkContextMenu;
    QAction *_readOnlyLinkAction;
    QAction *_allowEditingLinkAction;
    QAction *_allowUploadEditingLinkAction;
    QAction *_allowUploadLinkAction;
    QAction *_passwordProtectLinkAction;
    QAction *_expirationDateLinkAction;
    QAction *_unshareLinkAction;
    QAction *_addAnotherLinkAction;
    QAction *_noteLinkAction;
};
}

#endif // SHARELINKWIDGET_H
