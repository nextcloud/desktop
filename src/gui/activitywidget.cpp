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

#include "activitywidget.h"
#include "configfile.h"
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

#include "ui_activitywidget.h"

#include <climits>

namespace OCC {

void ActivityList::setAccountName( const QString& name )
{
    _accountName = name;
}

QString ActivityList::accountName() const
{
    return _accountName;
}

/* ==================================================================== */

QHash <QString, QVariant> ActivityLink::toVariantHash()
{
    QHash<QString, QVariant> hash;

    hash["label"] = _label;
    hash["link"]  = _link;
    hash["verb"]  = _verb;
    hash["primary"] = _isPrimary;

    return hash;
}

/* ==================================================================== */

ActivityListModel::ActivityListModel(QWidget *parent)
    :QAbstractListModel(parent)
{
}

QVariant ActivityListModel::data(const QModelIndex &index, int role) const
{
    Activity a;

    if (!index.isValid())
        return QVariant();

    a = _finalList.at(index.row());
    AccountStatePtr ast = AccountManager::instance()->account(a._accName);
    QStringList list;

    if (role == Qt::EditRole)
        return QVariant();

    switch (role) {
    case ActivityItemDelegate::PathRole:
        list = FolderMan::instance()->findFileInLocalFolders(a._file, ast->account());
        if( list.count() > 0 ) {
            return QVariant(list.at(0));
        }
        // File does not exist anymore? Let's try to open its path
        list = FolderMan::instance()->findFileInLocalFolders(QFileInfo(a._file).path(), ast->account());
        if( list.count() > 0 ) {
            return QVariant(list.at(0));
        }
        return QVariant();
        break;
    case ActivityItemDelegate::ActionIconRole:
        return QVariant(); // FIXME once the action can be quantified, display on Icon
        break;
    case ActivityItemDelegate::UserIconRole:
        return QIcon(QLatin1String(":/client/resources/account.png"));
        break;
    case Qt::ToolTipRole:
    case ActivityItemDelegate::ActionTextRole:
        return a._subject;
        break;
    case ActivityItemDelegate::LinkRole:
        return a._link;
        break;
    case ActivityItemDelegate::AccountRole:
        return a._accName;
        break;
    case ActivityItemDelegate::PointInTimeRole:
        return Utility::timeAgoInWords(a._dateTime);
        break;
    case ActivityItemDelegate::AccountConnectedRole:
        return (ast && ast->isConnected());
        break;
    default:
        return QVariant();

    }
    return QVariant();

}

int ActivityListModel::rowCount(const QModelIndex&) const
{
    return _finalList.count();
}

// current strategy: Fetch 100 items per Account
// ATTENTION: This method is const and thus it is not possible to modify
// the _activityLists hash or so. Doesn't make it easier...
bool ActivityListModel::canFetchMore(const QModelIndex& ) const
{
    if( _activityLists.count() == 0 ) return true;

    QMap<AccountState*, ActivityList>::const_iterator i = _activityLists.begin();
    while (i != _activityLists.end()) {
        AccountState *ast = i.key();
        if( ast && ast->isConnected() ) {
            ActivityList activities = i.value();
            if( activities.count() == 0 &&
                    ! _currentlyFetching.contains(ast) ) {
                return true;
            }
        }
        ++i;
    }

    return false;
}

void ActivityListModel::startFetchJob(AccountState* s)
{
    if( !s->isConnected() ) {
        return;
    }
    JsonApiJob *job = new JsonApiJob(s->account(), QLatin1String("ocs/v1.php/cloud/activity"), this);
    QObject::connect(job, SIGNAL(jsonReceived(QVariantMap, int)),
                     this, SLOT(slotActivitiesReceived(QVariantMap, int)));
    job->setProperty("AccountStatePtr", QVariant::fromValue<AccountState*>(s));

    QList< QPair<QString,QString> > params;
    params.append(qMakePair(QString::fromLatin1("page"),     QString::fromLatin1("0")));
    params.append(qMakePair(QString::fromLatin1("pagesize"), QString::fromLatin1("100")));
    job->addQueryParams(params);

    _currentlyFetching.insert(s);
    qDebug() << "Start fetching activities for " << s->account()->displayName();
    job->start();
}

void ActivityListModel::slotActivitiesReceived(const QVariantMap& json, int statusCode)
{
    auto activities = json.value("ocs").toMap().value("data").toList();
    qDebug() << "*** activities" << activities;

    ActivityList list;
    AccountState* ast = qvariant_cast<AccountState*>(sender()->property("AccountStatePtr"));
    _currentlyFetching.remove(ast);
    list.setAccountName( ast->account()->displayName());

    foreach( auto activ, activities ) {
        auto json = activ.toMap();

        Activity a;
        a._type = Activity::ActivityType;
        a._accName  = ast->account()->displayName();
        a._id       = json.value("id").toLongLong();
        a._subject  = json.value("subject").toString();
        a._message  = json.value("message").toString();
        a._file     = json.value("file").toString();
        a._link     = json.value("link").toUrl();
        a._dateTime = json.value("date").toDateTime();
        a._dateTime.setTimeSpec(Qt::UTC);
        list.append(a);
    }

    _activityLists[ast] = list;

    emit activityJobStatusCode(ast, statusCode);

    combineActivityLists();
}


void ActivityListModel::combineActivityLists()
{
    ActivityList resultList;

    foreach( ActivityList list, _activityLists.values() ) {
        resultList.append(list);
    }

    std::sort( resultList.begin(), resultList.end() );

    beginInsertRows(QModelIndex(), 0, resultList.count()-1);
    _finalList = resultList;
    endInsertRows();
}

void ActivityListModel::fetchMore(const QModelIndex &)
{
    QList<AccountStatePtr> accounts = AccountManager::instance()->accounts();

    foreach (AccountStatePtr asp, accounts) {
        bool newItem = false;

        if( !_activityLists.contains(asp.data()) && asp->isConnected() ) {
            _activityLists[asp.data()] = ActivityList();
            newItem = true;
        }
        if( newItem ) {
            startFetchJob(asp.data());
        }
    }
}

void ActivityListModel::slotRefreshActivity(AccountState *ast)
{
    if(ast && _activityLists.contains(ast)) {
        qDebug() << "**** Refreshing Activity list for" << ast->account()->displayName();
        _activityLists.remove(ast);
    }
    startFetchJob(ast);
}

void ActivityListModel::slotRemoveAccount(AccountState *ast )
{
    if( _activityLists.contains(ast) ) {
        int i = 0;
        const QString accountToRemove = ast->account()->displayName();

        QMutableListIterator<Activity> it(_finalList);

        while (it.hasNext()) {
            Activity activity = it.next();
            if( activity._accName == accountToRemove ) {
                beginRemoveRows(QModelIndex(), i, i+1);
                it.remove();
                endRemoveRows();
            }
        }
        _activityLists.remove(ast);
        _currentlyFetching.remove(ast);
    }
}

/* ==================================================================== */

ActivityWidget::ActivityWidget(QWidget *parent) :
    QWidget(parent),
    _ui(new Ui::ActivityWidget)
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

    connect( this, SIGNAL(newNotificationList(ActivityList)), this,
             SLOT(slotBuildNotificationDisplay(ActivityList)) );
}

ActivityWidget::~ActivityWidget()
{
    delete _ui;
}

void ActivityWidget::slotRefresh(AccountState *ptr)
{
    _model->slotRefreshActivity(ptr);
    slotFetchNotifications(ptr);
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
    qDebug() << indx.isValid() << indx.data(ActivityItemDelegate::PathRole).toString() << QFile::exists(indx.data(ActivityItemDelegate::PathRole).toString());
    if( indx.isValid() ) {
        QString fullPath = indx.data(ActivityItemDelegate::PathRole).toString();

        if (QFile::exists(fullPath)) {
            showInFileManager(fullPath);
        }
    }
}

void ActivityWidget::slotFetchNotifications(AccountState *ptr)
{
    /* start the notification fetch job as well */
    if( !ptr) {
        return;
    }

    // if the previous notification job has finished, start next.
    if( !_notificationJob ) {
        _notificationJob = new JsonApiJob( ptr->account(), QLatin1String("ocs/v2.php/apps/notifications/api/v1/notifications"), this );
        QObject::connect(_notificationJob.data(), SIGNAL(jsonReceived(QVariantMap, int)),
                         this, SLOT(slotNotificationsReceived(QVariantMap, int)));
        _notificationJob->setProperty("AccountStatePtr", QVariant::fromValue<AccountState*>(ptr));

        qDebug() << "Start fetching notifications for " << ptr->account()->displayName();
        _notificationJob->start();
    } else {
        qDebug() << "Notification Job still running, not starting a new one.";
    }
}


void ActivityWidget::slotNotificationsReceived(const QVariantMap& json, int statusCode)
{
    if( statusCode != 200 ) {
        qDebug() << "Failed for Notifications";
        return;
    }

    auto notifies = json.value("ocs").toMap().value("data").toList();

    AccountState* ai = qvariant_cast<AccountState*>(sender()->property("AccountStatePtr"));

    qDebug() << "Notifications for " << ai->account()->displayName() << notifies;

    ActivityList list;

    foreach( auto element, notifies ) {
        Activity a;
        auto json   = element.toMap();
        a._type     = Activity::NotificationType;
        a._accName  = ai->account()->displayName();
        a._id       = json.value("notification_id").toLongLong();
        a._subject  = json.value("subject").toString();
        a._message  = json.value("message").toString();
        QString s   = json.value("link").toString();
        if( !s.isEmpty() ) {
            a._link     = QUrl(s);
        }
        a._dateTime = json.value("datetime").toDateTime();
        a._dateTime.setTimeSpec(Qt::UTC);

        auto actions = json.value("actions").toList();
        foreach( auto action, actions) {
            auto actionJson = action.toMap();
            ActivityLink al;
            al._label = QUrl::fromPercentEncoding(actionJson.value("label").toByteArray());
            al._link  = actionJson.value("link").toString();
            al._verb  = actionJson.value("type").toString();
            al._isPrimary = actionJson.value("primary").toBool();

            a._links.append(al);
        }

        list.append(a);
    }
    emit newNotificationList( list );
}

// GUI: Display the notifications
void ActivityWidget::slotBuildNotificationDisplay(const ActivityList& list)
{
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

        widget->setAccountName( activity._accName );
        widget->setActivity( activity );
    }
    _ui->_notifyLabel->setHidden( list.count() == 0 );
    _ui->_notifyScroll->setHidden( list.count() == 0 );
}

void ActivityWidget::slotSendNotificationRequest(const QString& accountName, const QString& link, const QString& verb)
{
    qDebug() << "Server Notification Request " << verb << link << "on account" << accountName;

    const QStringList validVerbs = QStringList() << "GET" << "PUT" << "POST" << "DELETE";

    if( validVerbs.contains(verb)) {
        AccountStatePtr acc = AccountManager::instance()->account(accountName);
        if( acc ) {
            NotificationConfirmJob *job = new NotificationConfirmJob(acc->account());
            QString myLink(link);
            QUrl l(myLink);
            job->setLinkAndVerb(l, verb);
            connect( job, SIGNAL( networkError(QNetworkReply*)),
                                  this, SLOT(slotNotifyNetworkError(QNetworkReply*)));
            connect( job, SIGNAL( jobFinished(QString, int)),
                     this, SLOT(slotNotifyServerFinished(QString, int)) );
            job->start();
        }
    } else {
        qDebug() << "Invalid verb:" << verb;
    }
}


void ActivityWidget::slotNotifyNetworkError( QNetworkReply* )
{
    qDebug() << "Server notify job failed.";
}

void ActivityWidget::slotNotifyServerFinished( const QString& reply, int replyCode )
{
    // FIXME: remove the  widget after a couple of seconds
    qDebug() << "Server Notification reply code"<< replyCode << reply;

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

    // connect a model signal to stop the animation.
    connect(_activityWidget, SIGNAL(rowsInserted()), _progressIndicator, SLOT(stopAnimation()));
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
    if( ptr && ptr->isConnected() && isVisible()) {
        qDebug() << "Refreshing Activity list for " << ptr->account()->displayName();
        _progressIndicator->startAnimation();
        _activityWidget->slotRefresh(ptr);
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
