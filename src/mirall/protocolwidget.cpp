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

#include "mirall/protocolwidget.h"
#include "mirall/syncresult.h"
#include "mirall/logger.h"
#include "mirall/utility.h"
#include "mirall/theme.h"
#include "mirall/folderman.h"
#include "mirall/syncfileitem.h"
#include "mirall/folder.h"

#include "ui_protocolwidget.h"

namespace Mirall {

ProtocolWidget::ProtocolWidget(QWidget *parent) :
    QWidget(parent),
    ErrorIndicatorRole( Qt::UserRole +1 ),
    _ui(new Ui::ProtocolWidget)
{
    _ui->setupUi(this);

    connect(ProgressDispatcher::instance(), SIGNAL(progressInfo(QString,Progress::Info)),
            this, SLOT(slotProgressInfo(QString,Progress::Info)));
    connect(ProgressDispatcher::instance(), SIGNAL(progressSyncProblem(const QString&,const Progress::SyncProblem&)),
            this, SLOT(slotProgressProblem(const QString&, const Progress::SyncProblem&)));

    connect(_ui->_treeWidget, SIGNAL(itemActivated(QTreeWidgetItem*,int)), SLOT(slotOpenFile(QTreeWidgetItem*,int)));

    // Adjust copyToClipboard() when making changes here!
    QStringList header;
    header << tr("Time");
    header << tr("File");
    header << tr("Folder");
    header << tr("Action");
    header << tr("Size");

    _ui->_treeWidget->setHeaderLabels( header );
    _ui->_treeWidget->setColumnWidth(1, 180);
    _ui->_treeWidget->setColumnCount(5);
    _ui->_treeWidget->setRootIsDecorated(false);
    _ui->_treeWidget->setTextElideMode(Qt::ElideMiddle);

    connect(this, SIGNAL(guiLog(QString,QString)), Logger::instance(), SIGNAL(guiLog(QString,QString)));

    _clearBlacklistBtn = _ui->_dialogButtonBox->addButton(tr("Retry Sync"), QDialogButtonBox::ActionRole);
    _clearBlacklistBtn->setEnabled(false);
    connect(_clearBlacklistBtn, SIGNAL(clicked()), SLOT(slotClearBlacklist()));

    QPushButton *copyBtn = _ui->_dialogButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    copyBtn->setToolTip( tr("Copy the activity list to the clipboard."));
    connect(copyBtn, SIGNAL(clicked()), SLOT(copyToClipboard()));

}

void ProtocolWidget::initializeList()
{
  QList<Progress::Info> progressList = ProgressDispatcher::instance()->recentChangedItems(0); // All.
  QList<QTreeWidgetItem*> items;
  QTreeWidgetItem *item;

  _ui->_treeWidget->clear();

  QList<Progress::SyncProblem> problemList = ProgressDispatcher::instance()->recentProblems(0);
  items.clear();
  foreach( Progress::SyncProblem prob, problemList ) {
      item = createProblemTreewidgetItem(prob);
      if (item) {
          items.append(item);
      }
  }
  _ui->_treeWidget->addTopLevelItems(items);

  foreach( Progress::Info info, progressList ) {
      item = createProgressTreewidgetItem(info);
      if(item) {
          items.append(item);
      }
  }
  _ui->_treeWidget->addTopLevelItems(items);

  computeResyncButtonEnabled();

}

ProtocolWidget::~ProtocolWidget()
{
    delete _ui;
}

void ProtocolWidget::copyToClipboard()
{
    QString text;
    QTextStream ts(&text);

    int topLevelItems = _ui->_treeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItems; i++) {
        QTreeWidgetItem *child = _ui->_treeWidget->topLevelItem(i);
        ts << left
                // time stamp
            << qSetFieldWidth(10)
            << child->data(0,Qt::DisplayRole).toString()
                // file name
            << qSetFieldWidth(64)
            << child->data(1,Qt::DisplayRole).toString()
                // folder
            << qSetFieldWidth(15)
            << child->data(2, Qt::DisplayRole).toString()
                // action
            << qSetFieldWidth(15)
            << child->data(3, Qt::DisplayRole).toString()
                // size
            << qSetFieldWidth(10)
            << child->data(4, Qt::DisplayRole).toString()
            << qSetFieldWidth(0)
            << endl;
    }

    QApplication::clipboard()->setText(text);
    emit guiLog(tr("Copied to clipboard"), tr("The sync status has been copied to the clipboard."));
}

void ProtocolWidget::slotClearBlacklist()
{
    FolderMan *folderMan = FolderMan::instance();

    Folder::Map folders = folderMan->map();

    foreach( Folder *f, folders ) {
        int num = f->slotWipeBlacklist();
        qDebug() << num << "entries were removed from"<< f->alias() << "blacklist";
    }

    folderMan->slotScheduleAllFolders();
}

QList<QTreeWidgetItem*> ProtocolWidget::errorItems( const QString& folder )
{
    QList<QTreeWidgetItem*> list;

    int itemCnt = _ui->_treeWidget->topLevelItemCount();

    for( int cnt = 0; cnt < itemCnt; cnt++ ) {
        QTreeWidgetItem *item = _ui->_treeWidget->topLevelItem(cnt);
        bool isErrorItem = item->data(0, ErrorIndicatorRole).toBool();
        QString itemFolder = item->data(2, Qt::DisplayRole).toString();
        if( isErrorItem && itemFolder == folder ) {
            list.append(item);
        }
    }
    return list;
}

void ProtocolWidget::cleanErrorItems( const QString& folder ) // FIXME: Use the folder to detect which errors can be deleted.
{
    QList<QTreeWidgetItem*> wipeList = errorItems(folder);
    if( wipeList.count() > 0 ) {
        qDeleteAll(wipeList.begin(), wipeList.end());
    }
}

QString ProtocolWidget::timeString(QDateTime dt, QLocale::FormatType format) const
{
    QLocale loc = QLocale::system();
    QString timeStr;
    QDate today = QDate::currentDate();

    if( format == QLocale::NarrowFormat ) {
        if( dt.date().day() == today.day() ) {
            timeStr = loc.toString(dt.time(), QLocale::NarrowFormat);
        } else {
            timeStr = loc.toString(dt, QLocale::NarrowFormat);
        }
    } else {
        timeStr = loc.toString(dt, format);
    }
    return timeStr;
}

QTreeWidgetItem *ProtocolWidget::createProblemTreewidgetItem( const Progress::SyncProblem& problem)
{
    QStringList columns;
    QString timeStr = timeString(problem.timestamp);
    QString longTimeStr = timeString(problem.timestamp, QLocale::LongFormat);

    columns << timeStr;
    columns << problem.current_file;
    columns << problem.folder;
    QString errMsg = problem.error_message;
  #if 0
    if( problem.error_code == 507 ) {
        errMsg = tr("No more storage space available on server.");
    }
  #endif
    columns << errMsg;

    QTreeWidgetItem *item = new QTreeWidgetItem(columns);
    item->setData(0, ErrorIndicatorRole, QVariant(true) );
    // Maybe we should not set the error icon for all problems but distinguish
    // by error_code. A quota problem is considered an error, others might not??
    if( problem.kind == Progress::SoftError ) {
        item->setIcon(0, Theme::instance()->syncStateIcon(SyncResult::Problem, true));
    } else {
        item->setIcon(0, Theme::instance()->syncStateIcon(SyncResult::Error, true));
    }
    item->setToolTip(0, longTimeStr);
    item->setToolTip(1, problem.current_file);
    item->setToolTip(3, errMsg );

    return item;
}

void ProtocolWidget::slotProgressProblem( const QString& folder, const Progress::SyncProblem& problem)
{
    Q_UNUSED(folder);
    QTreeWidgetItem *item = createProblemTreewidgetItem(problem);
    _ui->_treeWidget->insertTopLevelItem(0, item);
}

void ProtocolWidget::slotOpenFile( QTreeWidgetItem *item, int )
{
    QString folderName = item->text(2);
    QString fileName = item->text(1);

    Folder *folder = FolderMan::instance()->folder(folderName);
    if (folder) {
        QString fullPath = folder->path() + '/' + fileName;
        if (QFile(fullPath).exists()) {
            Utility::showInFileManager(fullPath);
        }
    }
}

QTreeWidgetItem* ProtocolWidget::createProgressTreewidgetItem( const Progress::Info& progress )
{
    QStringList columns;
    const QString timeStr = timeString(progress.timestamp);
    const QString longTimeStr = timeString(progress.timestamp, QLocale::LongFormat);
    const QString actionStr = Progress::asResultString(progress);

    columns << timeStr;
    columns << progress.current_file;
    columns << progress.folder;
    columns << actionStr;
    columns << Utility::octetsToString( progress.file_size );

    QTreeWidgetItem *item = new QTreeWidgetItem(columns);
    item->setToolTip(0, longTimeStr);
    item->setToolTip(3, actionStr);
    return item;
}

void ProtocolWidget::computeResyncButtonEnabled()
{
    FolderMan *folderMan = FolderMan::instance();
    Folder::Map folders = folderMan->map();

    int cnt = 0;
    foreach( Folder *f, folders ) {
        cnt += f->blackListEntryCount();
    }

    QString t = tr("Currently no files are ignored because of previous errors.");
    if(cnt > 0) {
        t = tr("%1 files are ignored because of previous errors.\n Try to sync these again.").arg(cnt);
    }

    _clearBlacklistBtn->setEnabled(cnt > 0);
    _clearBlacklistBtn->setToolTip(t);

}

void ProtocolWidget::slotProgressInfo( const QString& folder, const Progress::Info& progress )
{
    if( progress.kind == Progress::StartSync ) {
      cleanErrorItems( folder );
      _clearBlacklistBtn->setEnabled(false);
    }

    if( progress.kind == Progress::EndSync ) {
        computeResyncButtonEnabled();
    }

    // Ingore other events than finishing an individual up- or download.
    if( !(progress.kind == Progress::EndDownload || progress.kind == Progress::EndUpload
          || progress.kind == Progress::EndDelete || progress.kind == Progress::EndRename)) {
        return;
    }

    QTreeWidgetItem *item = createProgressTreewidgetItem(progress);
    if(item) {
        _ui->_treeWidget->insertTopLevelItem(0, item);
    }
}


}
