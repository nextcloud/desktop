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
        list = FolderMan::instance()->findFileInLocalFolders(a._file);
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
bool ActivityListModel::canFetchMore(const QModelIndex& ) const
{
    if( _activityLists.count() == 0 ) return true;

    QMap<AccountState*, ActivityList>::const_iterator i = _activityLists.begin();
    while (i != _activityLists.end()) {
        AccountState *ast = i.key();
        if( !ast->isConnected() ) {
            return false;
        }
        ActivityList activities = i.value();
        if( activities.count() == 0 &&
                ! _currentlyFetching.contains(ast) ) {
            return true;
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
    AccountState* ai = qvariant_cast<AccountState*>(sender()->property("AccountStatePtr"));
    _currentlyFetching.remove(ai);
    list.setAccountName( ai->account()->displayName());

    foreach( auto activ, activities ) {
        auto json = activ.toMap();

        Activity a;
        a._accName  = ai->account()->displayName();
        a._id       = json.value("id").toLongLong();
        a._subject  = json.value("subject").toString();
        a._message  = json.value("message").toString();
        a._file     = json.value("file").toString();
        a._link     = json.value("link").toUrl();
        a._dateTime = json.value("date").toDateTime();
        list.append(a);
    }

    _activityLists[ai] = list;

    if( statusCode == 999 ) {
        emit accountWithoutActivityApp(ai);
    }

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
        // if the account is not yet managed, add an empty list.
        if( !_activityLists.contains(asp.data()) ) {
            _activityLists[asp.data()] = ActivityList();
            newItem = true;
        }
        ActivityList activities = _activityLists[asp.data()];
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

    showLabels();

    connect(_model, SIGNAL(accountWithoutActivityApp(AccountState*)),
            this, SLOT(slotAccountWithoutActivityApp(AccountState*)));

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

void ActivityWidget::slotRefresh(AccountState *ptr)
{
    _model->slotRefreshActivity(ptr);
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

    t.clear();
    QSetIterator<QString> i(_accountsWithoutActivities);
    while (i.hasNext() ) {
        t.append( tr("<br/>Account %1 does not have activities enabled.").arg(i.next()));
    }
    _ui->_bottomLabel->setTextFormat(Qt::RichText);
    _ui->_bottomLabel->setText(t);
}

void ActivityWidget::slotAccountWithoutActivityApp(AccountState *ast)
{
    if( ast && ast->account() ) {
        _accountsWithoutActivities.insert(ast->account()->displayName());
    }

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
        ts << left
              // account name
           << qSetFieldWidth(30)
           << activity._accName
              // date and time
           << qSetFieldWidth(34)
           << activity._dateTime.toString()
              // subject
           << qSetFieldWidth(10)
           << activity._subject
              // file
           << qSetFieldWidth(30)
           << activity._file
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
    if( indx.isValid() ) {
        QString fullPath = indx.data(ActivityItemDelegate::PathRole).toString();

        if (QFile::exists(fullPath)) {
            showInFileManager(fullPath);
        }
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
    _tab->addTab(_activityWidget, Theme::instance()->applicationIcon(), tr("Server Activity"));
    connect(_activityWidget, SIGNAL(copyToClipboard()), this, SLOT(slotCopyToClipboard()));


    _protocolWidget = new ProtocolWidget(this);
    _tab->addTab(_protocolWidget, Theme::instance()->syncStateIcon(SyncResult::Success), tr("Sync Protocol"));
    connect(_protocolWidget, SIGNAL(copyToClipboard()), this, SLOT(slotCopyToClipboard()));

    // Add the not-synced list into the tab
    QWidget *w = new QWidget;
    QVBoxLayout *vbox2 = new QVBoxLayout(w);
    vbox2->addWidget(new QLabel(tr("List of ignored or errornous files"), this));
    vbox2->addWidget(_protocolWidget->issueWidget());
    QDialogButtonBox *dlgButtonBox = new QDialogButtonBox(this);
    vbox2->addWidget(dlgButtonBox);
    QPushButton *_copyBtn = dlgButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    _copyBtn->setToolTip( tr("Copy the activity list to the clipboard."));
    _copyBtn->setEnabled(true);
    connect(_copyBtn, SIGNAL(clicked()), this, SLOT(slotCopyToClipboard()));

    w->setLayout(vbox2);
    _tab->addTab(w, Theme::instance()->syncStateIcon(SyncResult::Problem), tr("Not Synced"));

    // Add a progress indicator to spin if the acitivity list is updated.
    _progressIndicator = new QProgressIndicator(this);
    _tab->setCornerWidget(_progressIndicator);

    // connect a model signal to stop the animation.
    connect(_activityWidget, SIGNAL(rowsInserted()), _progressIndicator, SLOT(stopAnimation()));
}

void ActivitySettings::slotCopyToClipboard()
{
    QString text;
    QTextStream ts(&text);

    int idx = _tab->currentIndex();
    QString theSubject;

    if( idx == 0 ) {
        // the activity widget
        _activityWidget->storeActivityList(ts);
        theSubject = tr("server activity list");
    } else if(idx == 1 ) {
        // the protocol widget
        _protocolWidget->storeSyncActivity(ts);
        theSubject = tr("sync activity list");
    } else if(idx == 2 ) {
        // issues Widget
        theSubject = tr("not syned items list");
       _protocolWidget->storeSyncIssues(ts);
    }

    QApplication::clipboard()->setText(text);
    emit guiLog(tr("Copied to clipboard"), tr("The %1 has been copied to the clipboard.").arg(theSubject));
}

void ActivitySettings::slotRemoveAccount( AccountState *ptr )
{
    _activityWidget->slotRemoveAccount(ptr);
}

void ActivitySettings::slotRefresh( AccountState* ptr )
{
    if( ptr && ptr->isConnected() && isVisible()) {
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
