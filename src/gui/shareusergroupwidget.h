/*
 * Copyright (C) by Roeland Jago Douma <roeland@owncloud.com>
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

#ifndef SHAREUSERGROUPWIDGET_H
#define SHAREUSERGROUPWIDGET_H

#include "accountfwd.h"
#include "sharepermissions.h"
#include "QProgressIndicator.h"
#include <QDialog>
#include <QWidget>
#include <QSharedPointer>
#include <QList>
#include <QVector>
#include <QTimer>

class QAction;
class QCompleter;
class QModelIndex;

namespace OCC {

namespace Ui {
    class ShareUserGroupWidget;
    class ShareUserLine;
}

class AbstractCredentials;
class SyncResult;
class Share;
class Sharee;
class ShareManager;
class ShareeModel;

/**
 * @brief The ShareDialog (user/group) class
 * @ingroup gui
 */
class ShareUserGroupWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ShareUserGroupWidget(AccountPtr account,
        const QString &sharePath,
        const QString &localPath,
        SharePermissions maxSharingPermissions,
        const QString &privateLinkUrl,
        QWidget *parent = nullptr);
    ~ShareUserGroupWidget() override;

public slots:
    void getShares();

private slots:
    void slotSharesFetched(const QList<QSharedPointer<Share>> &shares);

    void on_shareeLineEdit_textChanged(const QString &text);
    void searchForSharees();
    void slotLineEditTextEdited(const QString &text);

    void slotLineEditReturn();
    void slotCompleterActivated(const QModelIndex &index);
    void slotCompleterHighlighted(const QModelIndex &index);
    void slotShareesReady();
    void slotAdjustScrollWidgetSize();
    void slotPrivateLinkShare();
    void displayError(int code, const QString &message);

    void slotPrivateLinkOpenBrowser();
    void slotPrivateLinkCopy();
    void slotPrivateLinkEmail();

private:
    Ui::ShareUserGroupWidget *_ui;
    AccountPtr _account;
    QString _sharePath;
    QString _localPath;
    SharePermissions _maxSharingPermissions;
    QString _privateLinkUrl;

    QCompleter *_completer;
    ShareeModel *_completerModel;
    QTimer _completionTimer;

    bool _isFile;
    bool _disableCompleterActivated; // in order to avoid that we share the contents twice
    ShareManager *_manager;

    QProgressIndicator _pi_sharee;
};

/**
 * The widget displayed for each user/group share
 */
class ShareUserLine : public QWidget
{
    Q_OBJECT

public:
    explicit ShareUserLine(QSharedPointer<Share> Share,
        SharePermissions maxSharingPermissions,
        bool isFile,
        QWidget *parent = nullptr);
    ~ShareUserLine() override;

    QSharedPointer<Share> share() const;

signals:
    void visualDeletionDone();
    void resizeRequested();

private slots:
    void on_deleteShareButton_clicked();
    void slotPermissionsChanged();
    void slotEditPermissionsChanged();
    void slotDeleteAnimationFinished();

    void slotShareDeleted();
    void slotPermissionsSet();

    void slotAvatarLoaded(const QPixmap &avatar);

private:
    void displayPermissions();
    void loadAvatar();

    Ui::ShareUserLine *_ui;
    QSharedPointer<Share> _share;
    bool _isFile;

    QAction *_permissionCreate;
    QAction *_permissionUpdate;
    QAction *_permissionDelete;
};
}

#endif // SHAREUSERGROUPWIDGET_H
