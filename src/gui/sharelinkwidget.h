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
#include <QVariantMap>
#include <QSharedPointer>
#include <QList>

namespace OCC {

namespace Ui {
class ShareLinkWidget;
}

class AbstractCredentials;
class QuotaInfo;
class SyncResult;
class LinkShare;
class Share;
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
                             bool autoShare = false,
                             QWidget *parent = 0);
    ~ShareLinkWidget();
    void getShares();

private slots:
    void slotSharesFetched(const QList<QSharedPointer<Share>> &shares);
    void slotCreateShareFetched(const QSharedPointer<LinkShare> share);
    void slotCreateShareRequiresPassword(const QString& message);
    void slotDeleteShareFetched();
    void slotPasswordSet();
    void slotExpireSet();
    void slotExpireDateChanged(const QDate &date);
    void slotCheckBoxShareLinkClicked();
    void slotCheckBoxPasswordClicked();
    void slotCheckBoxExpireClicked();
    void slotPasswordReturnPressed();
    void slotPasswordChanged(const QString& newText);
    void slotPushButtonCopyLinkPressed();
    void slotPushButtonMailLinkPressed();
    void slotCheckBoxEditingClicked();
    void slotPublicUploadSet();

    void slotServerError(int code, const QString &message);
    void slotPasswordSetError(int code, const QString &message);

private:
    void setShareCheckBoxTitle(bool haveShares);
    void displayError(const QString& errMsg);
    void setShareLink( const QString& url );
    void resizeEvent(QResizeEvent *e);
    void redrawElidedUrl();
    void setPublicUpload(bool publicUpload);

    Ui::ShareLinkWidget *_ui;
    AccountPtr _account;
    QString _sharePath;
    QString _localPath;
    QString _shareUrl;
#if 0
    QString _folderAlias;
    int     _uploadFails;
    QString _expectedSyncFile;
#endif

    bool _passwordJobRunning;
    void setPassword(const QString &password);
    void setExpireDate(const QDate &date);

    QProgressIndicator *_pi_link;
    QProgressIndicator *_pi_password;
    QProgressIndicator *_pi_date;
    QProgressIndicator *_pi_editing;

    ShareManager *_manager;
    QSharedPointer<LinkShare> _share;

    SharePermissions _maxSharingPermissions;
    bool _isFile;
    bool _autoShare;
    bool _passwordRequired;
};

}

#endif // SHARELINKWIDGET_H
