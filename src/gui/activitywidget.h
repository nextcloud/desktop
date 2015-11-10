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

namespace Ui {
  class ActivityWidget;
}
class Application;

/**
 * @brief The ActivityListModel
 * @ingroup gui
 */

class Activity
{
public:
    qlonglong _id;
    QString   _subject;
    QString   _message;
    QString   _file;
    QUrl      _link;
    QDateTime _dateTime;
    QString   _accName;

    /**
     * @brief Sort operator to sort the list youngest first.
     * @param val
     * @return
     */
    bool operator<( const Activity& val ) const {
        return _dateTime.toMSecsSinceEpoch() > val._dateTime.toMSecsSinceEpoch();
    }

};

class ActivityList:public QList<Activity>
{
    // explicit ActivityList();
public:
    void setAccountName( const QString& name );
    QString accountName() const;

private:
    QString _accountName;
};



class ActivityListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit ActivityListModel(QWidget *parent=0);

    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;

    bool canFetchMore(const QModelIndex& ) const;
    void fetchMore(const QModelIndex&);

    ActivityList activityList() { return _finalList; }

public slots:
    void slotRefreshActivity(AccountState* ast);

private slots:
    void slotActivitiesReceived(const QVariantMap& json);

private:
    void startFetchJob(AccountState* s);
    void combineActivityLists();
    QString timeSpanFromNow(const QDateTime& dt) const;

    QMap<AccountState*, ActivityList> _activityLists;
    ActivityList _finalList;
    QSet<AccountState*> _currentlyFetching;
};

/**
 * @brief The ActivityWidget class
 * @ingroup gui
 */

class ActivityWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ActivityWidget(QWidget *parent = 0);
    ~ActivityWidget();
    QSize sizeHint() const { return ownCloudGui::settingsDialogSize(); }
    void storeActivityList(QTextStream &ts);

public slots:
    void slotOpenFile();
    void slotRefresh(AccountState* ptr);

signals:
    void guiLog(const QString&, const QString&);
    void copyToClipboard();
    void rowsInserted();

private:
    QString timeString(QDateTime dt, QLocale::FormatType format) const;
    Ui::ActivityWidget *_ui;
    QPushButton *_copyBtn;

    ActivityListModel *_model;
};


class ActivitySettings : public QWidget
{
    Q_OBJECT
public:
    explicit ActivitySettings(QWidget *parent = 0);
    ~ActivitySettings();
    QSize sizeHint() const { return ownCloudGui::settingsDialogSize(); }

public slots:
    void slotRefresh( AccountState* ptr );
    void slotCopyToClipboard();

signals:
    void guiLog(const QString&, const QString&);

private:
    QTabWidget *_tab;
    ActivityWidget *_activityWidget;
    ProtocolWidget *_protocolWidget;
    QProgressIndicator *_progressIndicator;

};

}
#endif // ActivityWIDGET_H
