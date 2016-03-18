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

#include <QtGui>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtWidgets>
#endif

#include "activitylistmodel.h"
#include "activitywidget.h"
#include "syncresult.h"
#include "logger.h"
#include "utility.h"
#include "theme.h"
#include "folderman.h"
#include "syncfileitem.h"
#include "folder.h"
#include "openfilemanager.h"
#include "owncloudpropagator.h"
#include "account.h"
#include "accountstate.h"
#include "accountmanager.h"
#include "activityitemdelegate.h"
#include "protocolwidget.h"
#include "QProgressIndicator.h"
#include "notificationwidget.h"
#include "notificationconfirmjob.h"
#include "servernotificationhandler.h"
#include "theme.h"
#include "ocsjob.h"

#include "ui_activitywidget.h"

#include <climits>

// time span in milliseconds which has to be between two
// refreshes of the notifications
#define NOTIFICATION_REQUEST_FREE_PERIOD 15000

namespace OCC {


/* ==================================================================== */

ActivityWidget::ActivityWidget(QWidget *parent) :
    QWidget(parent),
    _ui(new Ui::ActivityWidget),
    _notificationRequestsRunning(0)
{
    _ui->setupUi(this);

    // Adjust copyToClipboard() when making changes here!
#if defined(Q_OS_MAC)
    _ui->_activityList->setMinimumWidth(400);
#endif

    _model = new ActivityListModel(this);
    ActivityItemDelegate *delegate = new ActivityItemDelegate;
    delegate->setParent(this);
    _ui->_activityList->setItemDelegate(delegate);
    _ui->_activityList->setAlternatingRowColors(true);
    _ui->_activityList->setModel(_model);

    _ui->_notifyLabel->hide();
    _ui->_notifyScroll->hide();

    // Create a widget container for the notifications. The ui file defines
    // a scroll area that get a widget with a layout as children
    QWidget *w = new QWidget(this);
    _notificationsLayout = new QVBoxLayout(this);
    w->setLayout(_notificationsLayout);
    _ui->_notifyScroll->setWidget(w);

    showLabels();

    connect(_model, SIGNAL(activityJobStatusCode(AccountState*,int)),
            this, SLOT(slotAccountActivityStatus(AccountState*,int)));

    _copyBtn = _ui->_dialogButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    _copyBtn->setToolTip( tr("Copy the activity list to the clipboard."));
    connect(_copyBtn, SIGNAL(clicked()), SIGNAL(copyToClipboard()));

    connect(_model, SIGNAL(rowsInserted(QModelIndex,int,int)), SIGNAL(rowsInserted()));

    connect( _ui->_activityList, SIGNAL(activated(QModelIndex)), this,
             SLOT(slotOpenFile(QModelIndex)));
}

ActivityWidget::~ActivityWidget()
{
    delete _ui;
}

void ActivityWidget::slotRefreshActivities(AccountState *ptr)
{
    _model->slotRefreshActivity(ptr);
}

void ActivityWidget::slotRefreshNotifications(AccountState *ptr)
{
    // start a server notification handler if no notification requests
    // are running
    if( _notificationRequestsRunning == 0 ) {
        ServerNotificationHandler *snh = new ServerNotificationHandler;
        connect(snh, SIGNAL(newNotificationList(ActivityList)), this,
                SLOT(slotBuildNotificationDisplay(ActivityList)));

        snh->slotFetchNotifications(ptr);
    } else {
        qDebug() << Q_FUNC_INFO << "========> notification request counter not zero.";
    }
}

void ActivityWidget::slotRemoveAccount( AccountState *ptr )
{
    _model->slotRemoveAccount(ptr);
}

void ActivityWidget::showLabels()
{
    QString t = tr("Server Activities");
    _ui->_headerLabel->setTextFormat(Qt::RichText);
    _ui->_headerLabel->setText(t);

    _ui->_notifyLabel->setText(tr("Action Required: Notifications"));

    t.clear();
    QSetIterator<QString> i(_accountsWithoutActivities);
    while (i.hasNext() ) {
        t.append( tr("<br/>Account %1 does not have activities enabled.").arg(i.next()));
    }
    _ui->_bottomLabel->setTextFormat(Qt::RichText);
    _ui->_bottomLabel->setText(t);
}

void ActivityWidget::slotAccountActivityStatus(AccountState *ast, int statusCode)
{
    if( !(ast && ast->account()) ) {
        return;
    }
    if( statusCode == 999 ) {
        _accountsWithoutActivities.insert(ast->account()->displayName());
    } else {
        _accountsWithoutActivities.remove(ast->account()->displayName());
    }

    int accountCount = AccountManager::instance()->accounts().count();
    emit hideAcitivityTab(_accountsWithoutActivities.count() == accountCount);

    showLabels();
}

// FIXME: Reused from protocol widget. Move over to utilities.
QString ActivityWidget::timeString(QDateTime dt, QLocale::FormatType format) const
{
    const QLocale loc = QLocale::system();
    QString dtFormat = loc.dateTimeFormat(format);
    static const QRegExp re("(HH|H|hh|h):mm(?!:s)");
    dtFormat.replace(re, "\\1:mm:ss");
    return loc.toString(dt, dtFormat);
}

void ActivityWidget::storeActivityList( QTextStream& ts )
{
    ActivityList activities = _model->activityList();

    foreach( Activity activity, activities ) {
        ts << right
              // account name
           << qSetFieldWidth(30)
           << activity._accName
              // separator
           << qSetFieldWidth(0) << ","

              // date and time
           << qSetFieldWidth(34)
           << activity._dateTime.toString()
              // separator
           << qSetFieldWidth(0) << ","

              // file
           << qSetFieldWidth(30)
           << activity._file
              // separator
           << qSetFieldWidth(0) << ","

              // subject
           << qSetFieldWidth(100)
           << activity._subject
              // separator
           << qSetFieldWidth(0) << ","

              // message (mostly empty)
           << qSetFieldWidth(55)
           << activity._message
              //
           << qSetFieldWidth(0)
           << endl;
    }
}

void ActivityWidget::slotOpenFile(QModelIndex indx)
{
    qDebug() << Q_FUNC_INFO << indx.isValid() << indx.data(ActivityItemDelegate::PathRole).toString() << QFile::exists(indx.data(ActivityItemDelegate::PathRole).toString());
    if( indx.isValid() ) {
        QString fullPath = indx.data(ActivityItemDelegate::PathRole).toString();

        if (QFile::exists(fullPath)) {
            showInFileManager(fullPath);
        }
    }
}

// GUI: Display the notifications
void ActivityWidget::slotBuildNotificationDisplay(const ActivityList& list)
{
    QHash<QString, int> accNotified;

    foreach( auto activity, list ) {
        NotificationWidget *widget = 0;

        if( _widgetForNotifId.contains(activity._id) ) {
            widget = _widgetForNotifId[activity._id];
        } else {
            widget = new NotificationWidget(this);
            connect(widget, SIGNAL(sendNotificationRequest(QString, QString, QString)),
                    this, SLOT(slotSendNotificationRequest(QString, QString, QString)));
            _notificationsLayout->addWidget(widget);
            // _ui->_notifyScroll->setMinimumHeight( widget->height());
            _ui->_notifyScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
            _widgetForNotifId[activity._id] = widget;
        }

        widget->setActivity( activity );

        // handle gui logs. In order to NOT annoy the user with every fetching of the
        // notifications the notification id is stored in a Set. Only if an id
        // is not in the set, it qualifies for guiLog.
        // Important: The _guiLoggedNotifications set must be wiped regularly which
        // will repeat the gui log.

        // after one hour, clear the gui log notification store
        if( _guiLogTimer.elapsed() > 60*60*1000 ) {
            _guiLoggedNotifications.clear();
        }
        if( !_guiLoggedNotifications.contains(activity._id)) {
            QString host = activity._accName;
            // store the name of the account that sends the notification to be
            // able to add it to the tray notification
            // remove the user name from the account as that is not accurate here.
            int indx = host.indexOf(QChar('@'));
            if( indx>-1 ) {
                host.remove(0, 1+indx);
            }
            if( !host.isEmpty() ) {
                if( accNotified.contains(host)) {
                    accNotified[host] = accNotified[host]+1;
                } else {
                    accNotified[host] = 1;
                }
            }
            _guiLoggedNotifications.insert(activity._id);
        }
    }

    _ui->_notifyLabel->setHidden( _widgetForNotifId.isEmpty() );
    _ui->_notifyScroll->setHidden( _widgetForNotifId.isEmpty() );

    int newGuiLogCount = accNotified.count();

    if( newGuiLogCount > 0 ) {
        // restart the gui log timer now that we show a notification
        _guiLogTimer.restart();

        // Assemble a tray notification
        QString msg = tr("You received %n new notification(s) from %2.", "", accNotified[accNotified.keys().at(0)]).
                arg(accNotified.keys().at(0));

        if( newGuiLogCount >= 2 ) {
            QString acc1 = accNotified.keys().at(0);
            QString acc2 = accNotified.keys().at(1);
            if( newGuiLogCount == 2 ) {
                int notiCount = accNotified[ acc1 ] + accNotified[ acc2 ];
                msg = tr("You received %1 new notifications from %2 and %3.").arg(notiCount).arg(acc1).arg(acc2);
            } else {
                msg = tr("You received new notifications from %1, %2 and other accounts.").arg(acc1).arg(acc2);
            }
        }

        emit guiLog(Theme::instance()->appNameGUI() + QLatin1String(" ") + tr("Notifications - Action Required"),
                    msg);
    }
}

void ActivityWidget::slotSendNotificationRequest(const QString& accountName, const QString& link, const QString& verb)
{
    qDebug() << Q_FUNC_INFO << "Server Notification Request " << verb << link << "on account" << accountName;
    NotificationWidget *theSender = qobject_cast<NotificationWidget*>(sender());

    const QStringList validVerbs = QStringList() << "GET" << "PUT" << "POST" << "DELETE";

    if( validVerbs.contains(verb)) {
        AccountStatePtr acc = AccountManager::instance()->account(accountName);
        if( acc ) {
            NotificationConfirmJob *job = new NotificationConfirmJob(acc->account());
            QUrl l(link);
            job->setLinkAndVerb(l, verb);
            job->setWidget(theSender);
            connect( job, SIGNAL( networkError(QNetworkReply*)),
                                  this, SLOT(slotNotifyNetworkError(QNetworkReply*)));
            connect( job, SIGNAL( jobFinished(QString, int)),
                     this, SLOT(slotNotifyServerFinished(QString, int)) );
            job->start();

            // count the number of running notification requests. If this member var
            // is larger than zero, no new fetching of notifications is started
            _notificationRequestsRunning++;
        }
    } else {
        qDebug() << Q_FUNC_INFO << "Notification Links: Invalid verb:" << verb;
    }
}

void ActivityWidget::endNotificationRequest( NotificationWidget *widget, int replyCode )
{
    _notificationRequestsRunning--;
    if( widget ) {
        widget->slotNotificationRequestFinished(replyCode);
    }
}

void ActivityWidget::slotNotifyNetworkError( QNetworkReply *reply)
{
    NotificationConfirmJob *job = qobject_cast<NotificationConfirmJob*>(sender());
    if( !job ) {
        return;
    }

    int resultCode =0;
    if( reply ) {
        resultCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }

    endNotificationRequest(job->widget(), resultCode);
    qDebug() << Q_FUNC_INFO << "Server notify job failed with code " << resultCode;

}

void ActivityWidget::slotNotifyServerFinished( const QString& reply, int replyCode )
{
    NotificationConfirmJob *job = qobject_cast<NotificationConfirmJob*>(sender());
    if( !job ) {
        return;
    }

    endNotificationRequest(job->widget(), replyCode);
    // FIXME: remove the  widget after a couple of seconds
    qDebug() << Q_FUNC_INFO << "Server Notification reply code"<< replyCode << reply;

    // if the notification was successful start a timer that triggers
    // removal of the done widgets in a few seconds
    // Add 200 millisecs to the predefined value to make sure that the timer in
    // widget's method readyToClose() has elapsed.
    if( replyCode == OCS_SUCCESS_STATUS_CODE ) {
        QTimer::singleShot(NOTIFICATION_WIDGET_CLOSE_AFTER_MILLISECS+200, this, SLOT(slotCleanWidgetList()));
    }
}

void ActivityWidget::slotCleanWidgetList()
{
    foreach( int id, _widgetForNotifId.keys() ) {
        Q_ASSERT(_widgetForNotifId[id]);
        if( _widgetForNotifId[id]->readyToClose() ) {
            auto *widget = _widgetForNotifId[id];
            _widgetForNotifId.remove(id);
            delete widget;
        }
    }

    if( _widgetForNotifId.isEmpty() ) {
        _ui->_notifyLabel->setHidden(true);
        _ui->_notifyScroll->setHidden(true);
    }
}


/* ==================================================================== */

ActivitySettings::ActivitySettings(QWidget *parent)
    :QWidget(parent)
{
    QHBoxLayout *hbox = new QHBoxLayout(this);
    setLayout(hbox);

    // create a tab widget for the three activity views
    _tab = new QTabWidget(this);
    hbox->addWidget(_tab);
    _activityWidget = new ActivityWidget(this);
    _activityTabId = _tab->insertTab(0, _activityWidget, Theme::instance()->applicationIcon(), tr("Server Activity"));
    connect(_activityWidget, SIGNAL(copyToClipboard()), this, SLOT(slotCopyToClipboard()));
    connect(_activityWidget, SIGNAL(hideAcitivityTab(bool)), this, SLOT(setActivityTabHidden(bool)));
    connect(_activityWidget, SIGNAL(guiLog(QString,QString)), this, SIGNAL(guiLog(QString,QString)));

    _protocolWidget = new ProtocolWidget(this);
    _tab->insertTab(1, _protocolWidget, Theme::instance()->syncStateIcon(SyncResult::Success), tr("Sync Protocol"));
    connect(_protocolWidget, SIGNAL(copyToClipboard()), this, SLOT(slotCopyToClipboard()));

    // Add the not-synced list into the tab
    QWidget *w = new QWidget;
    QVBoxLayout *vbox2 = new QVBoxLayout(w);
    vbox2->addWidget(new QLabel(tr("List of ignored or erroneous files"), this));
    vbox2->addWidget(_protocolWidget->issueWidget());
    QDialogButtonBox *dlgButtonBox = new QDialogButtonBox(this);
    vbox2->addWidget(dlgButtonBox);
    QPushButton *_copyBtn = dlgButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    _copyBtn->setToolTip( tr("Copy the activity list to the clipboard."));
    _copyBtn->setEnabled(true);
    connect(_copyBtn, SIGNAL(clicked()), this, SLOT(slotCopyToClipboard()));

    w->setLayout(vbox2);
    _tab->insertTab(2, w, Theme::instance()->syncStateIcon(SyncResult::Problem), tr("Not Synced"));

    // Add a progress indicator to spin if the acitivity list is updated.
    _progressIndicator = new QProgressIndicator(this);
    _tab->setCornerWidget(_progressIndicator);

    connect(&_notificationCheckTimer, SIGNAL(timeout()),
            this, SLOT(slotRegularNotificationCheck()));

    // connect a model signal to stop the animation.
    connect(_activityWidget, SIGNAL(rowsInserted()), _progressIndicator, SLOT(stopAnimation()));
}

void ActivitySettings::setNotificationRefreshInterval( quint64 interval )
{
    qDebug() << "Starting Notification refresh timer with " << interval/1000 << " sec interval";
    _notificationCheckTimer.start(interval);
}

void ActivitySettings::setActivityTabHidden(bool hidden)
{
    if( hidden && _activityTabId > -1 ) {
        _tab->removeTab(_activityTabId);
        _activityTabId = -1;
    }

    if( !hidden && _activityTabId == -1 ) {
        _activityTabId = _tab->insertTab(0, _activityWidget, Theme::instance()->applicationIcon(), tr("Server Activity"));
    }
}

void ActivitySettings::slotCopyToClipboard()
{
    QString text;
    QTextStream ts(&text);

    int idx = _tab->currentIndex();
    QString message;

    if( idx == 0 ) {
        // the activity widget
        _activityWidget->storeActivityList(ts);
        message = tr("The server activity list has been copied to the clipboard.");
    } else if(idx == 1 ) {
        // the protocol widget
        _protocolWidget->storeSyncActivity(ts);
        message = tr("The sync activity list has been copied to the clipboard.");
    } else if(idx == 2 ) {
        // issues Widget
        message = tr("The list of unsynched items has been copied to the clipboard.");
       _protocolWidget->storeSyncIssues(ts);
    }

    QApplication::clipboard()->setText(text);
    emit guiLog(tr("Copied to clipboard"), message);
}

void ActivitySettings::slotRemoveAccount( AccountState *ptr )
{
    _activityWidget->slotRemoveAccount(ptr);
}

void ActivitySettings::slotRefresh( AccountState* ptr )
{
    // Fetch Activities only if visible and if last check is longer than 15 secs ago
    if( _timeSinceLastCheck.isValid() && _timeSinceLastCheck.elapsed() < NOTIFICATION_REQUEST_FREE_PERIOD ) {
        qDebug() << Q_FUNC_INFO << "do not check as last check is only secs ago: " << _timeSinceLastCheck.elapsed() / 1000;
        return;
    }
    if( ptr && ptr->isConnected() ) {
        if( isVisible() ) {
            _progressIndicator->startAnimation();
            _activityWidget->slotRefreshActivities( ptr);
        }
        _activityWidget->slotRefreshNotifications(ptr);
        if( !_timeSinceLastCheck.isValid() ) {
            _timeSinceLastCheck.start();
        } else {
            _timeSinceLastCheck.restart();
        }
    }
}

void ActivitySettings::slotRegularNotificationCheck()
{
    AccountManager *am = AccountManager::instance();
    foreach (AccountStatePtr a, am->accounts()) {
        slotRefresh(a.data());
    }
}

bool ActivitySettings::event(QEvent* e)
{
    if (e->type() == QEvent::Show) {
        AccountManager *am = AccountManager::instance();
        foreach (AccountStatePtr a, am->accounts()) {
            slotRefresh(a.data());
        }
    }
    return QWidget::event(e);
}

ActivitySettings::~ActivitySettings()
{

}


}
