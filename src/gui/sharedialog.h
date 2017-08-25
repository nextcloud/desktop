/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef SHAREDIALOG_H
#define SHAREDIALOG_H

#include "accountstate.h"
#include "sharepermissions.h"

#include <QPointer>
#include <QString>
#include <QDialog>
#include <QWidget>

class QProgressIndicator;

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
        SharePermissions maxSharingPermissions,
        const QByteArray &numericFileId,
        QWidget *parent = 0);
    ~ShareDialog();

private slots:
    void done(int r);
    void slotMaxSharingPermissionsReceived(const QVariantMap &result);
    void slotMaxSharingPermissionsError();
    void slotThumbnailFetched(const int &statusCode, const QByteArray &reply);
    void slotAccountStateChanged(int state);

private:
    void showSharingUi();

    Ui::ShareDialog *_ui;
    QPointer<AccountState> _accountState;
    QString _sharePath;
    QString _localPath;
    SharePermissions _maxSharingPermissions;
    QByteArray _numericFileId;

    ShareLinkWidget *_linkWidget;
    ShareUserGroupWidget *_userGroupWidget;
    QProgressIndicator *_progressIndicator;
};
}

#endif // SHAREDIALOG_H
