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

// ========================================================================

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

    if (role == Qt::EditRole)
        return QVariant();

    switch (role) {
    case Qt::ToolTipRole:
        return QVariant();
    case Qt::DisplayRole:
        // return tr("Account %1 at %2: %3").arg(a._accName).arg(a._dateTime.toString(Qt::SystemLocaleShortDate)).arg(a._subject);
        break;
    case Qt::DecorationRole:
        return QVariant();
        break;
    case ActivityItemDelegate::ActionIconRole:
        return QVariant(); // FIXME once the action can be quantified, display on Icon
        break;
    case ActivityItemDelegate::UserIconRole:
        return QIcon(QLatin1String(":/client/resources/account.png"));
        break;
    case ActivityItemDelegate::ActionTextRole:
        return a._subject;
        break;
    case ActivityItemDelegate::PathRole:
            return a._file;
        break;
    case ActivityItemDelegate::LinkRole:
            return a._link;
        break;
    case ActivityItemDelegate::AccountRole:
            return a._accName;
        break;
    case ActivityItemDelegate::PointInTimeRole:
        return timeSpanFromNow(a._dateTime);

    }
    return QVariant();

}

QString ActivityListModel::timeSpanFromNow(const QDateTime& dt) const
{
    QDateTime now = QDateTime::currentDateTime();

    if( dt.daysTo(now)>0 ) {
        return tr("%1 day(s) ago").arg(dt.daysTo(now));
    } else {
        qint64 secs = dt.secsTo(now);

        if( floor(secs / 3600.0) > 0 ) {
            int hours = floor(secs/3600.0);
            return( tr("%1 hour(s) ago").arg(hours));
        } else {
            int minutes = qRound(secs/60.0);
            return( tr("%1 minute(s) ago").arg(minutes));
        }
    }
    return tr("Some time ago");
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
        ActivityList activities = i.value();
        if( ast->isConnected() && activities.count() == 0 &&
                ! _currentlyFetching.contains(ast) ) {
            return true;
        }
        ++i;
    }

    return false;
}

void ActivityListModel::startFetchJob(AccountState* s)
{
    JsonApiJob *job = new JsonApiJob(s->account(), QLatin1String("ocs/v1.php/cloud/activity"), this);
    QObject::connect(job, SIGNAL(jsonRecieved(QVariantMap)), this, SLOT(slotActivitiesReceived(QVariantMap)));
    job->setProperty("AccountStatePtr", QVariant::fromValue<AccountState*>(s));

    QList< QPair<QString,QString> > params;
    params.append(qMakePair(QLatin1String("page"), QLatin1String("0")));
    params.append(qMakePair(QLatin1String("pagesize"), QLatin1String("100")));
    job->addQueryParams(params);

    _currentlyFetching.insert(s);
    job->start();
}

void ActivityListModel::slotActivitiesReceived(const QVariantMap& json)
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

    // if all activity lists were received, assemble the whole list
    // otherwise wait until the others are finished
    bool allAreHere = true;
    foreach( ActivityList list, _activityLists.values() ) {
        if( list.count() == 0 ) {
            allAreHere = false;
            break;
        }
    }

    // FIXME: Be more efficient,
    if( allAreHere ) {
        combineActivityLists();
    }
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

        // if the account is not yet managed, add an empty list.
        if( !_activityLists.contains(asp.data()) ) {
            _activityLists[asp.data()] = ActivityList();
        }
        ActivityList activities = _activityLists[asp.data()];
        if( activities.count() == 0 ) {
            startFetchJob(asp.data());
        }
    }
}

void ActivityListModel::slotRefreshActivity(AccountState *ast)
{
    qDebug() << "**** Refreshing" << ast->account()->displayName();
    if(ast && _activityLists.contains(ast)) {
        _activityLists[ast].clear();
    }
    startFetchJob(ast);
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

    _ui->_headerLabel->setText(tr("Server Activities"));

    _copyBtn = _ui->_dialogButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    _copyBtn->setToolTip( tr("Copy the activity list to the clipboard."));
    connect(_copyBtn, SIGNAL(clicked()), SIGNAL(copyToClipboard()));

    connect(_model, SIGNAL(rowsInserted(QModelIndex,int,int)), SIGNAL(rowsInserted()));
}

ActivityWidget::~ActivityWidget()
{
    delete _ui;
}

void ActivityWidget::slotRefresh(AccountState *ptr)
{
    _model->slotRefreshActivity(ptr);
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

void ActivityWidget::slotOpenFile( )
{
    // FIXME make work at all.
#if 0
    QString folderName = item->data(2, Qt::UserRole).toString();
    QString fileName = item->text(1);

    Folder *folder = FolderMan::instance()->folder(folderName);
    if (folder) {
        // folder->path() always comes back with trailing path
        QString fullPath = folder->path() + fileName;
        if (QFile(fullPath).exists()) {
            showInFileManager(fullPath);
        }
    }
#endif
}


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
    QVBoxLayout *vbox2 = new QVBoxLayout(this);
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

void ActivitySettings::slotRefresh( AccountState* ptr )
{
    _progressIndicator->startAnimation();
    _activityWidget->slotRefresh(ptr);
}

ActivitySettings::~ActivitySettings()
{

}


}
