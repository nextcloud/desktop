/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef ACTIVITYWIDGET_H
#define ACTIVITYWIDGET_H

#include <QDialog>
#include <QDateTime>
#include <QLocale>
#include <QAbstractListModel>

#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "account.h"

#include "ui_activitywidget.h"

class QPushButton;
class QProgressIndicator;

namespace OCC {

class Account;
class AccountStatusPtr;
class ProtocolWidget;
class JsonApiJob;
class NotificationWidget;

namespace Ui {
  class ActivityWidget;
}
class Application;

/**
 * @brief The ActivityLink class describes actions of an activity
 *
 * These are part of notifications which are mapped into activities.
 */

class ActivityLink
{
public:
    QHash <QString, QVariant> toVariantHash();

    QString _label;
    QString _link;
    QString _verb;
    bool _isPrimary;
};

/**
 * @brief Activity Structure
 * @ingroup gui
 *
 * contains all the information describing a single activity.
 */

class Activity
{
public:
    enum Type {
        ActivityType,
        NotificationType
    };
    Type      _type;
    qlonglong _id;
    QString   _subject;
    QString   _message;
    QString   _file;
    QUrl      _link;
    QDateTime _dateTime;
    QString   _accName;

    QVector <ActivityLink> _links;
    /**
     * @brief Sort operator to sort the list youngest first.
     * @param val
     * @return
     */
    bool operator<( const Activity& val ) const {
        return _dateTime.toMSecsSinceEpoch() > val._dateTime.toMSecsSinceEpoch();
    }

};

/**
 * @brief The ActivityList
 * @ingroup gui
 *
 * A QList based list of Activities
 */
class ActivityList:public QList<Activity>
{
public:
    void setAccountName( const QString& name );
    QString accountName() const;

private:
    QString _accountName;
};


/**
 * @brief The ActivityListModel
 * @ingroup gui
 *
 * Simple list model to provide the list view with data.
 */
class ActivityListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ActivityListModel(QWidget *parent=0);

    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;

    bool canFetchMore(const QModelIndex& ) const Q_DECL_OVERRIDE;
    void fetchMore(const QModelIndex&) Q_DECL_OVERRIDE;

    ActivityList activityList() { return _finalList; }

public slots:
    void slotRefreshActivity(AccountState* ast);
    void slotRemoveAccount( AccountState *ast );

private slots:
    void slotActivitiesReceived(const QVariantMap& json, int statusCode);

signals:
    void activityJobStatusCode(AccountState* ast, int statusCode);

private:
    void startFetchJob(AccountState* s);
    void combineActivityLists();

    QMap<AccountState*, ActivityList> _activityLists;
    ActivityList _finalList;
    QSet<AccountState*> _currentlyFetching;
};

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
    explicit ActivityWidget(QWidget *parent = 0);
    ~ActivityWidget();
    QSize sizeHint() const Q_DECL_OVERRIDE { return ownCloudGui::settingsDialogSize(); }
    void storeActivityList(QTextStream &ts);

public slots:
    void slotOpenFile(QModelIndex indx);
    void slotRefresh(AccountState* ptr);
    void slotRemoveAccount( AccountState *ptr );
    void slotAccountActivityStatus(AccountState *ast, int statusCode);

signals:
    void guiLog(const QString&, const QString&);
    void copyToClipboard();
    void rowsInserted();
    void hideAcitivityTab(bool);
    void newNotificationList(const ActivityList& list);

private slots:
    void slotBuildNotificationDisplay(const ActivityList& list);
    void slotSendNotificationRequest(const QString &accountName, const QString& link, const QString& verb);
    void slotNotifyNetworkError( QNetworkReply* );
    void slotNotifyServerFinished( const QString& reply, int replyCode );
    void endNotificationRequest(NotificationWidget *widget , int replyCode);

private:
    void showLabels();
    QString timeString(QDateTime dt, QLocale::FormatType format) const;
    Ui::ActivityWidget *_ui;
    QPushButton *_copyBtn;

    QSet<QString> _accountsWithoutActivities;
    QMap<int, NotificationWidget*> _widgetForNotifId;
    QElapsedTimer _guiLogTimer;
    QSet<int> _guiLoggedNotifications;
    int _notificationRequests;

    ActivityListModel *_model;
    QVBoxLayout *_notificationsLayout;

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
    explicit ActivitySettings(QWidget *parent = 0);
    ~ActivitySettings();
    QSize sizeHint() const Q_DECL_OVERRIDE { return ownCloudGui::settingsDialogSize(); }

public slots:
    void slotRefresh( AccountState* ptr );
    void slotRemoveAccount( AccountState *ptr );

private slots:
    void slotCopyToClipboard();
    void setActivityTabHidden(bool hidden);

signals:
    void guiLog(const QString&, const QString&);

private:
    bool event(QEvent* e) Q_DECL_OVERRIDE;

    QTabWidget *_tab;
    int _activityTabId;

    ActivityWidget *_activityWidget;
    ProtocolWidget *_protocolWidget;
    QProgressIndicator *_progressIndicator;

};

}
#endif // ActivityWIDGET_H
