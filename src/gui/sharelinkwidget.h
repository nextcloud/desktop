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
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QWidgetAction>

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
class ElidedLabel;

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

    void toggleButton(bool show);
    void setupUiOptions();

    void setLinkShare(QSharedPointer<LinkShare> linkShare);
    QSharedPointer<LinkShare> getLinkShare();

    void focusPasswordLineEdit();

public slots:
    void slotDeleteShareFetched();
    void slotToggleShareLinkAnimation(const bool start);
    void slotServerError(const int code, const QString &message);
    void slotCreateShareRequiresPassword(const QString &message);
    void slotStyleChanged();

private slots:
    void slotCreateShareLink(const bool clicked);
    void slotCopyLinkShare(const bool clicked) const;    

    void slotCreatePassword();
    void slotPasswordSet();
    void slotPasswordSetError(const int code, const QString &message);

    void slotCreateNote();
    void slotNoteSet();

    void slotSetExpireDate();
    void slotExpireDateSet();

    void slotContextMenuButtonClicked();
    void slotLinkContextMenuActionTriggered(QAction *action);
    
    void slotCreateLabel();
    void slotLabelSet();

signals:
    void createLinkShare();
    void deleteLinkShare();
    void visualDeletionDone();
    void createPassword(const QString &password);
    void createPasswordProcessed();

private:
    void displayError(const QString &errMsg);
    
    void togglePasswordOptions(const bool enable = true);
    void toggleNoteOptions(const bool enable = true);
    void toggleExpireDateOptions(const bool enable = true);
    void toggleButtonAnimation(QToolButton *button, QProgressIndicator *progressIndicator, const QAction *checkedAction) const;

    /** Confirm with the user and then delete the share */
    void confirmAndDeleteShare();

    /** Retrieve a share's name, accounting for _namesSupported */
    QString shareName() const;

    void customizeStyle();
    
    void displayShareLinkLabel();

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
    QScopedPointer<QAction> _unshareLinkAction;
    QScopedPointer<QAction> _addAnotherLinkAction;
    QAction *_noteLinkAction;
    QHBoxLayout *_shareLinkLayout{};
    QLabel *_shareLinkLabel{};
    ElidedLabel *_shareLinkElidedLabel{};
    QLineEdit *_shareLinkEdit{};
    QToolButton *_shareLinkButton{};
    QProgressIndicator *_shareLinkProgressIndicator{};
    QWidget *_shareLinkDefaultWidget{};
    QWidgetAction *_shareLinkWidgetAction{};
};
}

#endif // SHARELINKWIDGET_H
