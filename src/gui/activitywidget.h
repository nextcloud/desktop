/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef ACTIVITYWIDGET_H
#define ACTIVITYWIDGET_H

#include <QDialog>
#include <QDateTime>
#include <QLocale>
#include <QAbstractListModel>
#include <chrono>

#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "account.h"
#include "activitydata.h"
#include "accountmanager.h"

#include "ui_activitywidget.h"

class QPushButton;
class QProgressIndicator;

namespace OCC {

class Account;
class AccountStatusPtr;
class JsonApiJob;
class ActivityListModel;

namespace Ui {
    class ActivityWidget;
}
class Application;

/**
 * @brief The ActivityWidget class
 * @ingroup gui
 *
 * The list widget to display the activities, contained in the
 * subsequent ActivitySettings widget.
 */

class ActivityWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ActivityWidget(AccountState *accountState, QWidget *parent = nullptr);
    ~ActivityWidget();
    QSize sizeHint() const Q_DECL_OVERRIDE { return ownCloudGui::settingsDialogSize(); }
    void storeActivityList(QTextStream &ts);

    /**
     * Adjusts the activity tab's and some widgets' visibility
     *
     * Based on whether activities are enabled and whether notifications are
     * available.
     */
    void checkActivityWidgetVisibility();

public slots:
    void slotOpenFile(QModelIndex indx);
    void slotRefreshActivities();
    void slotRefreshNotifications();
    void slotRemoveAccount();
    void slotAccountActivityStatus(int statusCode);
    void addError(const QString &folderAlias, const QString &message, ErrorCategory category);
    void slotProgressInfo(const QString &folder, const ProgressInfo &progress);
    void slotItemCompleted(const QString &folder, const SyncFileItemPtr &item);

signals:
    void guiLog(const QString &, const QString &);
    void rowsInserted();
    void hideActivityTab(bool);
    void sendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row);

private slots:
    void slotBuildNotificationDisplay(const ActivityList &list);
    void slotSendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb, int row);
    void slotNotifyNetworkError(QNetworkReply *);
    void slotNotifyServerFinished(const QString &reply, int replyCode);
    void endNotificationRequest(int replyCode);
    void slotNotificationRequestFinished(int statusCode);
    void slotPrimaryButtonClickedOnListView(const QModelIndex &index);
    void slotSecondaryButtonClickedOnListView(const QModelIndex &index);

private:
    void showLabels();
    QString timeString(QDateTime dt, QLocale::FormatType format) const;
    Ui::ActivityWidget *_ui;
    QSet<QString> _accountsWithoutActivities;
    QElapsedTimer _guiLogTimer;
    QSet<int> _guiLoggedNotifications;
    ActivityList _blacklistedNotifications;

    QTimer _removeTimer;

    // number of currently running notification requests. If non zero,
    // no query for notifications is started.
    int _notificationRequestsRunning;

    ActivityListModel *_model;
    AccountState *_accountState;
    const QString _accept;
    const QString _remote_share;
};


/**
 * @brief The ActivitySettings class
 * @ingroup gui
 *
 * Implements a tab for the settings dialog, displaying the three activity
 * lists.
 */
class ActivitySettings : public QWidget
{
    Q_OBJECT
public:
    explicit ActivitySettings(AccountState *accountState, QWidget *parent = nullptr);

    ~ActivitySettings();
    QSize sizeHint() const Q_DECL_OVERRIDE { return ownCloudGui::settingsDialogSize(); }

public slots:
    void slotRefresh();
    void slotRemoveAccount();
    void setNotificationRefreshInterval(std::chrono::milliseconds interval);

private slots:
    void slotRegularNotificationCheck();
    void slotDisplayActivities();

signals:
    void guiLog(const QString &, const QString &);

private:
    bool event(QEvent *e) Q_DECL_OVERRIDE;

    ActivityWidget *_activityWidget;
    QProgressIndicator *_progressIndicator;
    QVBoxLayout *_vbox;
    QTimer _notificationCheckTimer;
    QHash<AccountState *, QElapsedTimer> _timeSinceLastCheck;

    AccountState *_accountState;
};
}
#endif // ActivityWIDGET_H
