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

#include "networkjobs.h"
#include "accountfwd.h"
#include "QProgressIndicator.h"
#include <QDialog>
#include <QTreeWidgetItem>

namespace OCC {

/**
 * @brief The OcsShareJob class
 * @ingroup gui
 */
class OcsShareJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit OcsShareJob(const QByteArray& verb, const QUrl& url, AccountPtr account, QObject* parent = 0);

    void setPostParams(const QList<QPair<QString, QString> >& postParams);
    void addPassStatusCode(int code);

public slots:
    void start() Q_DECL_OVERRIDE;
signals:
    void jobFinished(QVariantMap reply);
private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
private:
    QByteArray _verb;
    QUrl _url;
    QList<QPair<QString, QString> > _postParams;
    QVector<int> _passStatusCodes;
};


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
    void slotSharesFetched(const QVariantMap &reply);
    void slotCreateShareFetched(const QVariantMap &reply);
    void slotDeleteShareFetched(const QVariantMap &reply);
    void slotPasswordSet(const QVariantMap &reply);
    void slotExpireSet(const QVariantMap &reply);
    void slotCalendarClicked(const QDate &date);
    void slotCheckBoxShareLinkClicked();
    void slotCheckBoxPasswordClicked();
    void slotCheckBoxExpireClicked();
    void slotPasswordReturnPressed();
    void slotPasswordChanged(const QString& newText);
    void slotPushButtonCopyLinkPressed();
    void slotThumbnailFetched(const int &statusCode, const QByteArray &reply);

    void done( int r );
private:
    void setShareCheckBoxTitle(bool haveShares);
    void displayError(int code);
    void displayError(const QString& errMsg);
    void setShareLink( const QString& url );
    void resizeEvent(QResizeEvent *e);
    void redrawElidedUrl();

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
    QList<QVariant> _shares;
    qulonglong _public_share_id;
    void setPassword(const QString &password);
    void setExpireDate(const QDate &date);

    QProgressIndicator *_pi_link;
    QProgressIndicator *_pi_password;
    QProgressIndicator *_pi_date;

    bool _resharingAllowed;
};

}

#endif // SHAREDIALOG_H
