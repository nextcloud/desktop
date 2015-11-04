/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
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

#ifndef SHAREDIALOG_UG_H
#define SHAREDIALOG_UG_H

#include "accountfwd.h"
#include "QProgressIndicator.h"
#include <QDialog>
#include <QWidget>
#include <QVariantMap>
#include <QSharedPointer>
#include <QList>


class QCompleter;

namespace OCC {

namespace Ui {
class ShareUserGroupDialog;
class ShareDialogShare;
}

class AbstractCredentials;
class QuotaInfo;
class SyncResult;
class Share;
class ShareManager;
class ShareeModel;

class ShareDialogShare : public QWidget
{
    Q_OBJECT

public:
    explicit ShareDialogShare(QSharedPointer<Share> Share, QWidget *parent = 0);
    ~ShareDialogShare();

signals:
    void shareDeleted(ShareDialogShare *share);

private slots:
    void on_deleteShareButton_clicked();
    void slotPermissionsChanged();

    void slotShareDeleted();
    void slotPermissionsSet();

private:
    Ui::ShareDialogShare *_ui;
    QSharedPointer<Share> _share;
};


/**
 * @brief The ShareDialog (user/group) class
 * @ingroup gui
 */
class ShareUserGroupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareUserGroupDialog(AccountPtr account, 
                                  const QString &sharePath,
                                  const QString &localPath,
                                  bool resharingAllowed,
                                  QWidget *parent = 0);
    ~ShareUserGroupDialog();

public slots:
    void getShares();

private slots:
    void slotSharesFetched(const QList<QSharedPointer<Share>> &shares);
    void done( int r );


    void on_shareeLineEdit_textChanged(const QString &text);
    void on_searchPushButton_clicked();

    void slotUpdateCompletion();
    void slotCompleterActivated(const QModelIndex & index);

private:
    Ui::ShareUserGroupDialog *_ui;
    AccountPtr _account;
    QString _sharePath;
    QString _localPath;

    QCompleter *_completer;
    ShareeModel *_completerModel;

    bool _resharingAllowed;
    bool _isFile;

    ShareManager *_manager;
};

}

#endif // SHAREDIALOG_UG_H
