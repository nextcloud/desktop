/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 * Copyright (C) 2015 by Klaas Freitag <freitag@owncloud.com>
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

#ifndef SHAREDIALOG_H
#define SHAREDIALOG_H

#include "accountfwd.h"
#include "QProgressIndicator.h"
#include <QDialog>
#include <QVariantMap>
#include <QSharedPointer>
#include <QList>

#include "share.h"

namespace OCC {

namespace Ui {
class ShareDialog;
}

class AbstractCredentials;
class QuotaInfo;
class SyncResult;

/**
 * @brief The ShareDialog class
 * @ingroup gui
 */
class ShareDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareDialog(AccountPtr account, const QString &sharePath, const QString &localPath,
                         bool resharingAllowed, QWidget *parent = 0);
    ~ShareDialog();
    void getShares();

private slots:
    void slotSharesFetched(const QList<QSharedPointer<Share>> &shares);
    void slotCreateShareFetched(const QSharedPointer<LinkShare> share);
    void slotCreateShareRequiresPassword();
    void slotDeleteShareFetched();
    void slotPasswordSet();
    void slotExpireSet();
    void slotCalendarClicked(const QDate &date);
    void slotCheckBoxShareLinkClicked();
    void slotCheckBoxPasswordClicked();
    void slotCheckBoxExpireClicked();
    void slotPasswordReturnPressed();
    void slotPasswordChanged(const QString& newText);
    void slotPushButtonCopyLinkPressed();
    void slotThumbnailFetched(const int &statusCode, const QByteArray &reply);
    void slotCheckBoxEditingClicked();
    void slotPublicUploadSet();

    void done( int r );
private:
    void setShareCheckBoxTitle(bool haveShares);
    void displayError(int code);
    void displayError(const QString& errMsg);
    void setShareLink( const QString& url );
    void resizeEvent(QResizeEvent *e);
    void redrawElidedUrl();
    void setPublicUpload(bool publicUpload);

    Ui::ShareDialog *_ui;
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

    bool _resharingAllowed;
    bool _isFile;
};

}

#endif // SHAREDIALOG_H
