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

#ifndef SHAREDIALOG_H
#define SHAREDIALOG_H

#include "accountstate.h"

#include <QPointer>
#include <QString>
#include <QDialog>
#include <QWidget>

namespace OCC {

namespace Ui {
class ShareDialog;
}

class ShareLinkWidget;
class ShareUserGroupWidget;

class ShareDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareDialog(QPointer<AccountState> accountState,
                         const QString &sharePath,
                         const QString &localPath,
                         bool resharingAllowed,
                         QWidget *parent = 0);
    ~ShareDialog();

    void getShares();

private slots:
    void done( int r );
    void slotThumbnailFetched(const int &statusCode, const QByteArray &reply);
    void slotAccountStateChanged(int state);

private:
    Ui::ShareDialog *_ui;
    QPointer<AccountState> _accountState;
    QString _sharePath;
    QString _localPath;

    bool _resharingAllowed;

    ShareLinkWidget *_linkWidget;
    ShareUserGroupWidget *_userGroupWidget;
};

}

#endif // SHAREDIALOG_H
