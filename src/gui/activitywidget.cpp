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
#include <QtWidgets>

#include "QProgressIndicator.h"

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "activitywidget.h"
#include "commonstrings.h"
#include "issueswidget.h"
#include "notificationconfirmjob.h"
#include "notificationwidget.h"
#include "openfilemanager.h"
#include "protocolwidget.h"
#include "servernotificationhandler.h"
#include "syncresult.h"
#include "theme.h"

#include "models/activitylistmodel.h"
#include "models/expandingheaderview.h"
#include "models/models.h"

#include "ui_activitywidget.h"

#include <climits>

using namespace std::chrono;
using namespace std::chrono_literals;

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

    _model = new ActivityListModel(this);
    _sortModel = new Models::SignalledQSortFilterProxyModel(this);
    _sortModel->setSourceModel(_model);
    _ui->_activityList->setModel(_sortModel);
    _sortModel->setSortRole(Models::UnderlyingDataRole);
    auto header = new ExpandingHeaderView(QStringLiteral("ActivityListModelHeader"), _ui->_activityList);
    _ui->_activityList->setHorizontalHeader(header);
    header->hideSection(static_cast<int>(ActivityListModel::ActivityRole::Path));
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSortIndicator(static_cast<int>(ActivityListModel::ActivityRole::PointInTime), Qt::DescendingOrder);

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

    connect(_model, &ActivityListModel::activityJobStatusCode,
        this, &ActivityWidget::slotAccountActivityStatus);

    connect(AccountManager::instance(), &AccountManager::accountRemoved, this, [this](AccountStatePtr ast) {
        if (_accountsWithoutActivities.remove(ast->account()->displayName())) {
            showLabels();
        }

        for (const auto widget : qAsConst(_widgetForNotifId)) {
            if (widget->activity().accountUuid() == ast->account()->uuid()) {
                scheduleWidgetToRemove(widget);
            }
        }
    });

    connect(_model, &ActivityListModel::activityJobStatusCode, this, &ActivityWidget::dataChanged);
    connect(_ui->_activityList, &QListView::customContextMenuRequested, this, &ActivityWidget::slotItemContextMenu);
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QListView::customContextMenuRequested, header, [header, this] {
        auto menu = ProtocolWidget::showFilterMenu(header, _sortModel, static_cast<int>(ActivityListModel::ActivityRole::Account), tr("Account"));
        menu->addSeparator();
        header->addResetActionToMenu(menu);
    });

    connect(_ui->_filterButton, &QAbstractButton::clicked, this, [this] {
        ProtocolWidget::showFilterMenu(_ui->_filterButton, _sortModel, static_cast<int>(ActivityListModel::ActivityRole::Account), tr("Account"));
    });
    connect(_sortModel, &Models::SignalledQSortFilterProxyModel::filterChanged, this,
        [this]() { _ui->_filterButton->setText(CommonStrings::filterButtonText(_sortModel->filterRegularExpression().pattern().isEmpty() ? 0 : 1)); });

    connect(&_removeTimer, &QTimer::timeout, this, &ActivityWidget::slotCheckToCleanWidgets);
    _removeTimer.setInterval(1000);
}

ActivityWidget::~ActivityWidget()
{
    delete _ui;
}

void ActivityWidget::slotRefreshActivities(const AccountStatePtr &ptr)
{
    _model->slotRefreshActivity(ptr);
}

void ActivityWidget::slotRefreshNotifications(const AccountStatePtr &ptr)
{
    // start a server notification handler if no notification requests
    // are running
    if (_notificationRequestsRunning == 0) {
        ServerNotificationHandler *snh = new ServerNotificationHandler;
        connect(snh, &ServerNotificationHandler::newNotificationList,
            this, &ActivityWidget::slotBuildNotificationDisplay);

        snh->slotFetchNotifications(ptr);
    } else {
        qCWarning(lcActivity) << "Notification request counter not zero.";
    }
}

void ActivityWidget::slotRemoveAccount(const AccountStatePtr &ptr)
{
    _model->slotRemoveAccount(ptr);
}

void ActivityWidget::showLabels()
{
    QString t = tr("Server Activities");
    _ui->_headerLabel->setTextFormat(Qt::RichText);
    _ui->_headerLabel->setText(t);

    _ui->_notifyLabel->setText(tr("Notifications"));

    t.clear();
    QSetIterator<QString> i(_accountsWithoutActivities);
    while (i.hasNext()) {
        t.append(tr("<br/>%1 does not provide activities.").arg(i.next()));
    }
    _ui->_bottomLabel->setTextFormat(Qt::RichText);
    _ui->_bottomLabel->setText(t);
}

void ActivityWidget::slotAccountActivityStatus(AccountStatePtr ast, int statusCode)
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

    Q_EMIT hideActivityTab(!hasAccountsWithActivity && !hasNotifications);
}

// GUI: Display the notifications.
// All notifications in list are coming from the same account
// but in the _widgetForNotifId hash widgets for all accounts are
// collected.
void ActivityWidget::slotBuildNotificationDisplay(const ActivityList &list)
{
    if (list.empty()) {
        return;
    }
    // compute the count to display later
    QMap<QString, int> accNotified;

    // Whether a new notification widget was added to the notificationLayout.
    bool newNotificationShown = false;

    for (const auto &activity : list) {
        if (_blacklistedNotifications.contains(activity)) {
            qCInfo(lcActivity) << "Activity in blacklist, skip";
            continue;
        }

        NotificationWidget *widget = nullptr;

        if (_widgetForNotifId.contains(activity.id())) {
            widget = _widgetForNotifId[activity.id()];
        } else {
            widget = new NotificationWidget(this);
            connect(widget, &NotificationWidget::sendNotificationRequest,
                this, &ActivityWidget::slotSendNotificationRequest);
            connect(widget, &NotificationWidget::requestCleanupAndBlacklist,
                this, &ActivityWidget::slotRequestCleanupAndBlacklist);

            _notificationsLayout->addWidget(widget);
            // _ui->_notifyScroll->setMinimumHeight( widget->height());
            _ui->_notifyScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);
            _widgetForNotifId[activity.id()] = widget;
            newNotificationShown = true;
        }

        widget->setActivity(activity);

        // handle gui logs. In order to NOT annoy the user with every fetching of the
        // notifications the notification id is stored in a Set. Only if an id
        // is not in the set, it qualifies for guiLog.
        // Important: The _guiLoggedNotifications set must be wiped regularly which
        // will repeat the gui log.

        // after one hour, clear the gui log notification store
        if (_guiLogTimer.elapsed() > duration_cast<microseconds>(1h).count()) {
            _guiLoggedNotifications.clear();
        }
        if (!_guiLoggedNotifications.contains(activity.id())) {
            // store the uui of the account that sends the notification to be
            // able to add it to the tray notification
            accNotified[activity.accName()]++;
            _guiLoggedNotifications.insert(activity.id());
        }
    }

    // check if there are widgets that have no corresponding activity from
    // the server any more. Collect them in a list

    const auto accId = list.first().accountUuid();
    QList<QString> strayCats;
    for (auto it = _widgetForNotifId.cbegin(); it != _widgetForNotifId.cend(); ++it) {
        bool found = false;
        // do not mark widgets of other accounts to delete.
        if (it.value()->activity().accountUuid() != accId) {
            continue;
        }

        for (const auto &activity : list) {
            if (activity.id() == it.key()) {
                // found an activity
                found = true;
                break;
            }
        }
        if (!found) {
            // the activity does not exist any more.
            strayCats.append(it.key());
        }
    }

    // .. and now delete all these stray cat widgets.
    for (const auto &strayCatId : strayCats) {
        NotificationWidget *widgetToGo = _widgetForNotifId[strayCatId];
        scheduleWidgetToRemove(widgetToGo, 0);
    }

    checkActivityTabVisibility();

    const int newGuiLogCount = accNotified.count();

    if (newGuiLogCount > 0) {
        // restart the gui log timer now that we show a notification
        _guiLogTimer.start();

        // Assemble a tray notification
        QString msg;
        if (newGuiLogCount == 1) {
            msg = tr("%n notifications(s) for %1.", "", accNotified.begin().value()).arg(accNotified.begin().key());
        } else if (newGuiLogCount >= 2) {
            const auto acc1 = accNotified.begin();
            const auto acc2 = ++accNotified.begin();
            if (newGuiLogCount == 2) {
                const int notiCount = acc1.value() + acc2.value();
                msg = tr("%n notifications(s) for %1 and %2.", "", notiCount).arg(acc1.key(), acc2.key());
            } else {
                msg = tr("New notifications for %1, %2 and other accounts.").arg(acc1.key(), acc2.key());
            }
        }
        const QString log = tr("Open the activity view for details.");
        Q_EMIT guiLog(msg, log);
    }

    if (newNotificationShown) {
        Q_EMIT newNotification();
    }
}

void ActivityWidget::slotSendNotificationRequest(const QString &accountName, const QString &link, const QByteArray &verb)
{
    qCInfo(lcActivity) << "Server Notification Request " << verb << link << "on account" << accountName;
    NotificationWidget *theSender = qobject_cast<NotificationWidget *>(sender());

    const QList<QByteArray> validVerbs = {
        QByteArrayLiteral("GET"),
        QByteArrayLiteral("PUT"),
        QByteArrayLiteral("POST"),
        QByteArrayLiteral("DELETE")
    };

    if (validVerbs.contains(verb)) {
        if (auto acc = AccountManager::instance()->account(accountName)) {
            // TODO: host validation?
            auto *job = new NotificationConfirmJob(acc->account(), QUrl(link), verb, this);
            job->setWidget(theSender);
            connect(job, &NotificationConfirmJob::finishedSignal,
                this, [job, this] {
                    if (job->reply()->error() == QNetworkReply::NoError) {
                        endNotificationRequest(job->widget(), job->ocsSuccess());
                        qCInfo(lcActivity) << "Server Notification reply code" << job->ocsStatus();

                        // if the notification was successful start a timer that triggers
                        // removal of the done widgets in a few seconds
                        // Add 200 millisecs to the predefined value to make sure that the timer in
                        // widget's method readyToClose() has elapsed.
                        if (job->ocsSuccess()) {
                            scheduleWidgetToRemove(job->widget());
                        }
                    } else {
                        endNotificationRequest(job->widget(), job->ocsSuccess());
                        qCWarning(lcActivity) << "Server notify job failed with code " << job->ocsStatus();
                    }
                });
            job->start();

            // count the number of running notification requests. If this member var
            // is larger than zero, no new fetching of notifications is started
            _notificationRequestsRunning++;
        }
    } else {
        qCWarning(lcActivity) << "Notification Links: Invalid verb:" << verb;
    }
}

void ActivityWidget::endNotificationRequest(NotificationWidget *widget, bool success)
{
    _notificationRequestsRunning--;
    if (widget) {
        widget->slotNotificationRequestFinished(success);
    }
}

// blacklist the activity coming in here.
void ActivityWidget::slotRequestCleanupAndBlacklist(const Activity &blacklistActivity)
{
    if (!_blacklistedNotifications.contains(blacklistActivity)) {
        _blacklistedNotifications.append(blacklistActivity);
    }

    NotificationWidget *widget = _widgetForNotifId[blacklistActivity.id()];
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
            _widgetForNotifId.remove(widget->activity().id());
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

void ActivityWidget::slotItemContextMenu()
{
    auto rows = _ui->_activityList->selectionModel()->selectedRows();
    auto menu = new QMenu(this);
    menu->setAccessibleName(tr("Activity item menu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // keep in sync with ProtocolWidget::showContextMenu
    menu->addAction(CommonStrings::copyToClipBoard(), this, [text = Models::formatSelection(rows, Models::UnderlyingDataRole)] {
        QApplication::clipboard()->setText(text);
    });

    if (rows.size() == 1) {
        // keep in sync with ProtocolWidget::showContextMenu
        const auto localPath = rows.first().siblingAtColumn(static_cast<int>(ActivityListModel::ActivityRole::Path)).data(Models::UnderlyingDataRole).toString();
        if (!localPath.isEmpty()) {
            menu->addAction(CommonStrings::showInFileBrowser(), this, [localPath] {
                if (QFileInfo::exists(localPath)) {
                    showInFileManager(localPath);
                }
            });
        }
    }
    menu->popup(QCursor::pos());
    // accassebility
    menu->setFocus();
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
    _activityTabId = _tab->addTab(_activityWidget, Theme::instance()->applicationIcon(), tr("Server Activity"));
    connect(_activityWidget, &ActivityWidget::hideActivityTab, this, &ActivitySettings::setActivityTabHidden);
    connect(_activityWidget, &ActivityWidget::guiLog, this, &ActivitySettings::guiLog);
    connect(_activityWidget, &ActivityWidget::newNotification, this, &ActivitySettings::slotShowActivityTab);

    _protocolWidget = new ProtocolWidget(this);
    _protocolTabId = _tab->addTab(_protocolWidget, Resources::getCoreIcon(QStringLiteral("states/ok")), tr("Sync Protocol"));

    _issuesWidget = new IssuesWidget(this);
    _syncIssueTabId = _tab->addTab(_issuesWidget, Resources::getCoreIcon(QStringLiteral("states/error")), QString());
    slotShowIssueItemCount(0); // to display the label.
    connect(_issuesWidget, &IssuesWidget::issueCountUpdated,
        this, &ActivitySettings::slotShowIssueItemCount);

    // Add a progress indicator to spin if the acitivity list is updated.
    _progressIndicator = new QProgressIndicator(this);
    _tab->setCornerWidget(_progressIndicator);

    connect(&_notificationCheckTimer, &QTimer::timeout,
        this, &ActivitySettings::slotRegularNotificationCheck);

    // connect a model signal to stop the animation.
    connect(_activityWidget, &ActivityWidget::dataChanged, _progressIndicator, &QProgressIndicator::stopAnimation);

    // We want the protocol be the default
    _tab->setCurrentIndex(1);

    connect(AccountManager::instance(), &AccountManager::accountRemoved, this,
        [this](const AccountStatePtr &accountStatePtr) { _timeSinceLastCheck.take(accountStatePtr); });
}

void ActivitySettings::setNotificationRefreshInterval(std::chrono::milliseconds interval)
{
    qCDebug(lcActivity) << "Starting Notification refresh timer with " << interval.count() / 1000 << " sec interval";
    _notificationCheckTimer.start(interval.count());
}

void ActivitySettings::setActivityTabHidden(bool hidden)
{
    if (hidden && _activityTabId > -1) {
        _tab->removeTab(_activityTabId);
        _activityTabId = -1;
        _protocolTabId -= 1;
        _syncIssueTabId -= 1;
    }

    if (!hidden && _activityTabId == -1) {
        _activityTabId = _tab->insertTab(0, _activityWidget, Theme::instance()->applicationIcon(), tr("Server Activity"));
        _protocolTabId += 1;
        _syncIssueTabId += 1;
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

void ActivitySettings::slotShowIssuesTab()
{
    if (_syncIssueTabId == -1)
        return;
    _tab->setCurrentIndex(_syncIssueTabId);
}

void ActivitySettings::slotRemoveAccount(const AccountStatePtr &ptr)
{
    _activityWidget->slotRemoveAccount(ptr);
}

void ActivitySettings::slotRefresh(AccountStatePtr ptr)
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
    for (const auto &a : AccountManager::instance()->accounts()) {
        slotRefresh(a);
    }
}

bool ActivitySettings::event(QEvent *e)
{
    if (e->type() == QEvent::Show) {
        slotRegularNotificationCheck();
    }
    return QWidget::event(e);
}

ActivitySettings::~ActivitySettings()
{
}
}
