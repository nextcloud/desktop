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

ActivityWidget::ActivityWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ActivityWidget)
    , _notificationRequestsRunning(0)
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
    QWidget *w = new QWidget;
    _notificationsLayout = new QVBoxLayout;
    w->setLayout(_notificationsLayout);
    _notificationsLayout->setAlignment(Qt::AlignTop);
    _ui->_notifyScroll->setAlignment(Qt::AlignTop);
    _ui->_notifyScroll->setWidget(w);

    showLabels();

    connect(_model, SIGNAL(activityJobStatusCode(AccountState *, int)),
        this, SLOT(slotAccountActivityStatus(AccountState *, int)));

    _copyBtn = _ui->_dialogButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    _copyBtn->setToolTip(tr("Copy the activity list to the clipboard."));
    connect(_copyBtn, SIGNAL(clicked()), SIGNAL(copyToClipboard()));

    connect(_model, SIGNAL(rowsInserted(QModelIndex, int, int)), SIGNAL(rowsInserted()));

    connect(_ui->_activityList, SIGNAL(activated(QModelIndex)), this,
        SLOT(slotOpenFile(QModelIndex)));

    connect(&_removeTimer, SIGNAL(timeout()), this, SLOT(slotCheckToCleanWidgets()));
    _removeTimer.setInterval(1000);
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
    if (_notificationRequestsRunning == 0) {
        ServerNotificationHandler *snh = new ServerNotificationHandler;
        connect(snh, SIGNAL(newNotificationList(ActivityList)), this,
            SLOT(slotBuildNotificationDisplay(ActivityList)));

        snh->slotFetchNotifications(ptr);
    } else {
        qCWarning(lcActivity) << "Notification request counter not zero.";
    }
}

void ActivityWidget::slotRemoveAccount(AccountState *ptr)
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
    while (i.hasNext()) {
        t.append(tr("<br/>Account %1 does not have activities enabled.").arg(i.next()));
    }
    _ui->_bottomLabel->setTextFormat(Qt::RichText);
    _ui->_bottomLabel->setText(t);
}

void ActivityWidget::slotAccountActivityStatus(AccountState *ast, int statusCode)
{
    if (!(ast && ast->account())) {
        return;
    }
    if (statusCode == 999) {
        _accountsWithoutActivities.insert(ast->account()->displayName());
    } else {
        _accountsWithoutActivities.remove(ast->account()->displayName());
    }

    checkActivityTabVisibility();
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

void ActivityWidget::storeActivityList(QTextStream &ts)
{
    ActivityList activities = _model->activityList();

    foreach (Activity activity, activities) {
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

void ActivityWidget::checkActivityTabVisibility()
{
    int accountCount = AccountManager::instance()->accounts().count();
    bool hasAccountsWithActivity =
        _accountsWithoutActivities.count() != accountCount;
    bool hasNotifications = !_widgetForNotifId.isEmpty();

    _ui->_headerLabel->setVisible(hasAccountsWithActivity);
    _ui->_activityList->setVisible(hasAccountsWithActivity);

    _ui->_notifyLabel->setVisible(hasNotifications);
    _ui->_notifyScroll->setVisible(hasNotifications);

    emit hideActivityTab(!hasAccountsWithActivity && !hasNotifications);
}

void ActivityWidget::slotOpenFile(QModelIndex indx)
{
    qCDebug(lcActivity) << indx.isValid() << indx.data(ActivityItemDelegate::PathRole).toString() << QFile::exists(indx.data(ActivityItemDelegate::PathRole).toString());
    if (indx.isValid()) {
        QString fullPath = indx.data(ActivityItemDelegate::PathRole).toString();

        if (QFile::exists(fullPath)) {
            showInFileManager(fullPath);
        }
    }
}

// GUI: Display the notifications.
// All notifications in list are coming from the same account
// but in the _widgetForNotifId hash widgets for all accounts are
// collected.
void ActivityWidget::slotBuildNotificationDisplay(const ActivityList &list)
{
    QHash<QString, int> accNotified;
    QString listAccountName;

    // Whether a new notification widget was added to the notificationLayout.
    bool newNotificationShown = false;

    foreach (auto activity, list) {
        if (_blacklistedNotifications.contains(activity)) {
            qCInfo(lcActivity) << "Activity in blacklist, skip";
            continue;
        }

        NotificationWidget *widget = 0;

        if (_widgetForNotifId.contains(activity.ident())) {
            widget = _widgetForNotifId[activity.ident()];
        } else {
            widget = new NotificationWidget(this);
            connect(widget, SIGNAL(sendNotificationRequest(QString, QString, QByteArray)),
                this, SLOT(slotSendNotificationRequest(QString, QString, QByteArray)));
            connect(widget, SIGNAL(requestCleanupAndBlacklist(Activity)),
                this, SLOT(slotRequestCleanupAndBlacklist(Activity)));

            _notificationsLayout->addWidget(widget);
// _ui->_notifyScroll->setMinimumHeight( widget->height());
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
            _ui->_notifyScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
#endif
            _widgetForNotifId[activity.ident()] = widget;
            newNotificationShown = true;
        }

        widget->setActivity(activity);

        // remember the list account name for the strayCat handling below.
        listAccountName = activity._accName;

        // handle gui logs. In order to NOT annoy the user with every fetching of the
        // notifications the notification id is stored in a Set. Only if an id
        // is not in the set, it qualifies for guiLog.
        // Important: The _guiLoggedNotifications set must be wiped regularly which
        // will repeat the gui log.

        // after one hour, clear the gui log notification store
        if (_guiLogTimer.elapsed() > 60 * 60 * 1000) {
            _guiLoggedNotifications.clear();
        }
        if (!_guiLoggedNotifications.contains(activity._id)) {
            QString host = activity._accName;
            // store the name of the account that sends the notification to be
            // able to add it to the tray notification
            // remove the user name from the account as that is not accurate here.
            int indx = host.indexOf(QChar('@'));
            if (indx > -1) {
                host.remove(0, 1 + indx);
            }
            if (!host.isEmpty()) {
                if (accNotified.contains(host)) {
                    accNotified[host] = accNotified[host] + 1;
                } else {
                    accNotified[host] = 1;
                }
            }
            _guiLoggedNotifications.insert(activity._id);
        }
    }

    // check if there are widgets that have no corresponding activity from
    // the server any more. Collect them in a list
    QList<Activity::Identifier> strayCats;
    foreach (auto id, _widgetForNotifId.keys()) {
        NotificationWidget *widget = _widgetForNotifId[id];

        bool found = false;
        // do not mark widgets of other accounts to delete.
        if (widget->activity()._accName != listAccountName) {
            continue;
        }

        foreach (auto activity, list) {
            if (activity.ident() == id) {
                // found an activity
                found = true;
                break;
            }
        }
        if (!found) {
            // the activity does not exist any more.
            strayCats.append(id);
        }
    }

    // .. and now delete all these stray cat widgets.
    foreach (auto strayCatId, strayCats) {
        NotificationWidget *widgetToGo = _widgetForNotifId[strayCatId];
        scheduleWidgetToRemove(widgetToGo, 0);
    }

    checkActivityTabVisibility();

    int newGuiLogCount = accNotified.count();

    if (newGuiLogCount > 0) {
        // restart the gui log timer now that we show a notification
        _guiLogTimer.restart();

        // Assemble a tray notification
        QString msg = tr("You received %n new notification(s) from %2.", "", accNotified[accNotified.keys().at(0)]).arg(accNotified.keys().at(0));

        if (newGuiLogCount >= 2) {
            QString acc1 = accNotified.keys().at(0);
            QString acc2 = accNotified.keys().at(1);
            if (newGuiLogCount == 2) {
                int notiCount = accNotified[acc1] + accNotified[acc2];
                msg = tr("You received %n new notification(s) from %1 and %2.", "", notiCount).arg(acc1, acc2);
            } else {
                msg = tr("You received new notifications from %1, %2 and other accounts.").arg(acc1, acc2);
            }
        }

        const QString log = tr("%1 Notifications - Action Required").arg(Theme::instance()->appNameGUI());
        emit guiLog(log, msg);
    }

    if (newNotificationShown) {
        emit newNotification();
    }
}

void ActivityWidget::slotSendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb)
{
    qCInfo(lcActivity) << "Server Notification Request " << verb << link << "on account" << accountName;
    NotificationWidget *theSender = qobject_cast<NotificationWidget *>(sender());

    const QStringList validVerbs = QStringList() << "GET"
                                                 << "PUT"
                                                 << "POST"
                                                 << "DELETE";

    if (validVerbs.contains(verb)) {
        AccountStatePtr acc = AccountManager::instance()->account(accountName);
        if (acc) {
            NotificationConfirmJob *job = new NotificationConfirmJob(acc->account());
            QUrl l(link);
            job->setLinkAndVerb(l, verb);
            job->setWidget(theSender);
            connect(job, SIGNAL(networkError(QNetworkReply *)),
                this, SLOT(slotNotifyNetworkError(QNetworkReply *)));
            connect(job, SIGNAL(jobFinished(QString, int)),
                this, SLOT(slotNotifyServerFinished(QString, int)));
            job->start();

            // count the number of running notification requests. If this member var
            // is larger than zero, no new fetching of notifications is started
            _notificationRequestsRunning++;
        }
    } else {
        qCWarning(lcActivity) << "Notification Links: Invalid verb:" << verb;
    }
}

void ActivityWidget::endNotificationRequest(NotificationWidget *widget, int replyCode)
{
    _notificationRequestsRunning--;
    if (widget) {
        widget->slotNotificationRequestFinished(replyCode);
    }
}

void ActivityWidget::slotNotifyNetworkError(QNetworkReply *reply)
{
    NotificationConfirmJob *job = qobject_cast<NotificationConfirmJob *>(sender());
    if (!job) {
        return;
    }

    int resultCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    endNotificationRequest(job->widget(), resultCode);
    qCWarning(lcActivity) << "Server notify job failed with code " << resultCode;
}

void ActivityWidget::slotNotifyServerFinished(const QString &reply, int replyCode)
{
    NotificationConfirmJob *job = qobject_cast<NotificationConfirmJob *>(sender());
    if (!job) {
        return;
    }

    endNotificationRequest(job->widget(), replyCode);
    // FIXME: remove the  widget after a couple of seconds
    qCInfo(lcActivity) << "Server Notification reply code" << replyCode << reply;

    // if the notification was successful start a timer that triggers
    // removal of the done widgets in a few seconds
    // Add 200 millisecs to the predefined value to make sure that the timer in
    // widget's method readyToClose() has elapsed.
    if (replyCode == OCS_SUCCESS_STATUS_CODE) {
        scheduleWidgetToRemove(job->widget());
    }
}

// blacklist the activity coming in here.
void ActivityWidget::slotRequestCleanupAndBlacklist(const Activity &blacklistActivity)
{
    if (!_blacklistedNotifications.contains(blacklistActivity)) {
        _blacklistedNotifications.append(blacklistActivity);
    }

    NotificationWidget *widget = _widgetForNotifId[blacklistActivity.ident()];
    scheduleWidgetToRemove(widget);
}

void ActivityWidget::scheduleWidgetToRemove(NotificationWidget *widget, int milliseconds)
{
    if (!widget) {
        return;
    }
    // in five seconds from now, remove the widget.
    QDateTime removeTime = QDateTime::currentDateTimeUtc().addMSecs(milliseconds);
    QDateTime &it = _widgetsToRemove[widget];
    if (!it.isValid() || it > removeTime) {
        it = removeTime;
    }
    if (!_removeTimer.isActive()) {
        _removeTimer.start();
    }
}

// Called every second to see if widgets need to be removed.
void ActivityWidget::slotCheckToCleanWidgets()
{
    auto currentTime = QDateTime::currentDateTimeUtc();
    auto it = _widgetsToRemove.begin();
    while (it != _widgetsToRemove.end()) {
        // loop over all widgets in the to-remove queue
        QDateTime t = it.value();
        NotificationWidget *widget = it.key();

        if (currentTime > t) {
            // found one to remove!
            Activity::Identifier id = widget->activity().ident();
            _widgetForNotifId.remove(id);
            widget->deleteLater();
            it = _widgetsToRemove.erase(it);
        } else {
            ++it;
        }
    }

    if (_widgetsToRemove.isEmpty()) {
        _removeTimer.stop();
    }

    // check to see if the whole notification pane should be hidden
    if (_widgetForNotifId.isEmpty()) {
        _ui->_notifyLabel->setHidden(true);
        _ui->_notifyScroll->setHidden(true);
    }
}


/* ==================================================================== */

ActivitySettings::ActivitySettings(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *hbox = new QHBoxLayout(this);
    setLayout(hbox);

    // create a tab widget for the three activity views
    _tab = new QTabWidget(this);
    hbox->addWidget(_tab);
    _activityWidget = new ActivityWidget(this);
    _activityTabId = _tab->insertTab(0, _activityWidget, Theme::instance()->applicationIcon(), tr("Server Activity"));
    connect(_activityWidget, SIGNAL(copyToClipboard()), this, SLOT(slotCopyToClipboard()));
    connect(_activityWidget, SIGNAL(hideActivityTab(bool)), this, SLOT(setActivityTabHidden(bool)));
    connect(_activityWidget, SIGNAL(guiLog(QString, QString)), this, SIGNAL(guiLog(QString, QString)));
    connect(_activityWidget, SIGNAL(newNotification()), SLOT(slotShowActivityTab()));

    _protocolWidget = new ProtocolWidget(this);
    _tab->insertTab(1, _protocolWidget, Theme::instance()->syncStateIcon(SyncResult::Success), tr("Sync Protocol"));
    connect(_protocolWidget, SIGNAL(copyToClipboard()), this, SLOT(slotCopyToClipboard()));
    connect(_protocolWidget, SIGNAL(issueItemCountUpdated(int)),
        this, SLOT(slotShowIssueItemCount(int)));

    // Add the not-synced list into the tab
    QWidget *w = new QWidget;
    QVBoxLayout *vbox2 = new QVBoxLayout(w);
    vbox2->addWidget(new QLabel(tr("List of ignored or erroneous files"), this));
    vbox2->addWidget(_protocolWidget->issueWidget());
    QDialogButtonBox *dlgButtonBox = new QDialogButtonBox(this);
    vbox2->addWidget(dlgButtonBox);
    QPushButton *_copyBtn = dlgButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    _copyBtn->setToolTip(tr("Copy the activity list to the clipboard."));
    _copyBtn->setEnabled(true);
    connect(_copyBtn, SIGNAL(clicked()), this, SLOT(slotCopyToClipboard()));

    w->setLayout(vbox2);
    _syncIssueTabId = _tab->insertTab(2, w, Theme::instance()->syncStateIcon(SyncResult::Problem), QString());
    slotShowIssueItemCount(0); // to display the label.

    // Add a progress indicator to spin if the acitivity list is updated.
    _progressIndicator = new QProgressIndicator(this);
    _tab->setCornerWidget(_progressIndicator);

    connect(&_notificationCheckTimer, SIGNAL(timeout()),
        this, SLOT(slotRegularNotificationCheck()));

    // connect a model signal to stop the animation.
    connect(_activityWidget, SIGNAL(rowsInserted()), _progressIndicator, SLOT(stopAnimation()));

    // We want the protocol be the default
    _tab->setCurrentIndex(1);
}

void ActivitySettings::setNotificationRefreshInterval(quint64 interval)
{
    qCDebug(lcActivity) << "Starting Notification refresh timer with " << interval / 1000 << " sec interval";
    _notificationCheckTimer.start(interval);
}

void ActivitySettings::setActivityTabHidden(bool hidden)
{
    if (hidden && _activityTabId > -1) {
        _tab->removeTab(_activityTabId);
        _activityTabId = -1;
    }

    if (!hidden && _activityTabId == -1) {
        _activityTabId = _tab->insertTab(0, _activityWidget, Theme::instance()->applicationIcon(), tr("Server Activity"));
    }
}

void ActivitySettings::slotShowIssueItemCount(int cnt)
{
    QString cntText = tr("Not Synced");
    if (cnt) {
        //: %1 is the number of not synced files.
        cntText = tr("Not Synced (%1)").arg(cnt);
    }
    _tab->setTabText(_syncIssueTabId, cntText);
}

void ActivitySettings::slotShowActivityTab()
{
    if (_activityTabId != -1) {
        _tab->setCurrentIndex(_activityTabId);
    }
}

void ActivitySettings::slotCopyToClipboard()
{
    QString text;
    QTextStream ts(&text);

    int idx = _tab->currentIndex();
    QString message;

    if (idx == 0) {
        // the activity widget
        _activityWidget->storeActivityList(ts);
        message = tr("The server activity list has been copied to the clipboard.");
    } else if (idx == 1) {
        // the protocol widget
        _protocolWidget->storeSyncActivity(ts);
        message = tr("The sync activity list has been copied to the clipboard.");
    } else if (idx == 2) {
        // issues Widget
        message = tr("The list of unsynced items has been copied to the clipboard.");
        _protocolWidget->storeSyncIssues(ts);
    }

    QApplication::clipboard()->setText(text);
    emit guiLog(tr("Copied to clipboard"), message);
}

void ActivitySettings::slotRemoveAccount(AccountState *ptr)
{
    _activityWidget->slotRemoveAccount(ptr);
}

void ActivitySettings::slotRefresh(AccountState *ptr)
{
    // QElapsedTimer isn't actually constructed as invalid.
    if (!_timeSinceLastCheck.contains(ptr)) {
        _timeSinceLastCheck[ptr].invalidate();
    }
    QElapsedTimer &timer = _timeSinceLastCheck[ptr];

    // Fetch Activities only if visible and if last check is longer than 15 secs ago
    if (timer.isValid() && timer.elapsed() < NOTIFICATION_REQUEST_FREE_PERIOD) {
        qCDebug(lcActivity) << "Do not check as last check is only secs ago: " << timer.elapsed() / 1000;
        return;
    }
    if (ptr && ptr->isConnected()) {
        if (isVisible() || !timer.isValid()) {
            _progressIndicator->startAnimation();
            _activityWidget->slotRefreshActivities(ptr);
        }
        _activityWidget->slotRefreshNotifications(ptr);
        timer.start();
    }
}

void ActivitySettings::slotRegularNotificationCheck()
{
    AccountManager *am = AccountManager::instance();
    foreach (AccountStatePtr a, am->accounts()) {
        slotRefresh(a.data());
    }
}

bool ActivitySettings::event(QEvent *e)
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
