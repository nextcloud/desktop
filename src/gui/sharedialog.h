/*
 * Copyright (C) by
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
#include "QProgressIndicator.h"
#include <QDialog>
#include <QTreeWidgetItem>

namespace OCC {

class OcsShareJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit OcsShareJob(const QByteArray& verb, const QUrl& url, const QUrl& postData, AccountPtr account, QObject* parent = 0);
public slots:
    void start() Q_DECL_OVERRIDE;
signals:
    void jobFinished(QString reply);
private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
private:
    QByteArray _verb;
    QUrl _url;
    QUrl _postData;
};

namespace Ui {
class ShareDialog;
}

class AbstractCredentials;
class Account;
class QuotaInfo;
class MirallAccessManager;

class ShareDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareDialog(const QString &path, const bool &isDir, QWidget *parent = 0);
    ~ShareDialog();
    void getShares();
    void setPath(const QString &path, const bool &isDir);
    QString getPath();
private slots:
    void slotSharesFetched(const QString &reply);
    void slotCreateShareFetched(const QString &reply);
    void slotDeleteShareFetched(const QString &reply);
    void slotPasswordSet(const QString &reply);
    void slotExpireSet(const QString &reply);
    void slotCalendarClicked(const QDate &date);
    void slotCheckBoxShareLinkClicked();
    void slotCheckBoxPasswordClicked();
    void slotCheckBoxExpireClicked();
    void slotPasswordReturnPressed();

    void slotUserShareWidgetClicked(QTreeWidgetItem *item, const int column);
    void slotUpdateShareReply(const QString &reply);
    void slotAddUserShareClicked();
    void slotAddShareReply(const QString &reply);
    void slotDeleteUserShareClicked();
    void slotDeleteShareReply(const QString &reply);
    void slotGroupShareWidgetClicked(QTreeWidgetItem *item, const int column);
    void slotAddGroupShareClicked();
    void slotDeleteGroupShareClicked();
private:
    Ui::ShareDialog *_ui;
    QString _path;
    bool _isDir;
    QList<QVariant> _shares;
    qulonglong _public_share_id;
    void setPassword(QString password);
    void setExpireDate(const QString &date);
    int checkJsonReturnCode(const QString &reply);

    QProgressIndicator *_pi_link;
    QProgressIndicator *_pi_password;
    QProgressIndicator *_pi_date;
};

}

#endif // SHAREDIALOG_H
