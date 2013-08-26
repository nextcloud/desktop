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

#include "mirall/itemprogressdialog.h"
#include "mirall/syncresult.h"
#include "mirall/logger.h"
#include "mirall/utility.h"
#include "mirall/theme.h"
#include "mirall/folderman.h"
#include "mirall/syncfileitem.h"

#include "ui_itemprogressdialog.h"

namespace Mirall {

ItemProgressDialog::ItemProgressDialog(Application*, QWidget *parent) :
    QDialog(parent),
    ErrorIndicatorRole( Qt::UserRole +1 ),
    _ui(new Ui::ItemProgressDialog)
{
    _ui->setupUi(this);
    connect(_ui->_dialogButtonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()),
            this, SLOT(accept()));

    connect(ProgressDispatcher::instance(), SIGNAL(progressInfo(QString,Progress::Info)),
            this, SLOT(slotProgressInfo(QString,Progress::Info)));
    connect(ProgressDispatcher::instance(), SIGNAL(progressSyncProblem(const QString&,const Progress::SyncProblem&)),
            this, SLOT(slotProgressErrors(const QString&, const Progress::SyncProblem&)));

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

    connect(this, SIGNAL(guiLog(QString,QString)), Logger::instance(), SIGNAL(guiLog(QString,QString)));

    QPushButton *copyBtn = _ui->_dialogButtonBox->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    connect(copyBtn, SIGNAL(clicked()), SLOT(copyToClipboard()));

    setWindowTitle(tr("Sync Protocol"));

}

void ItemProgressDialog::setSyncResultStatus(const SyncResult& result )
{
     if( result.errorStrings().count() ) {
        _ui->_errorLabel->setVisible(true);
        _ui->_errorLabel->setTextFormat(Qt::RichText);

        QString errStr;
        QStringList errors = result.errorStrings();
        int cnt = errors.size();
        bool appendDots = false;
        if( cnt > 3 ) {
            cnt = 3;
            appendDots = true;
        }

        for( int i = 0; i < cnt; i++) {
            errStr.append(QString("%1<br/>").arg(errors.at(i)));
        }
        if( appendDots ) {
            errStr.append(QString("..."));
        }
        _ui->_errorLabel->setText(errStr);
    } else {
        _ui->_errorLabel->setText(QString::null);
        _ui->_errorLabel->setVisible(false);
    }

}

void ItemProgressDialog::setSyncResult( const SyncResult& result )
{
    setSyncResultStatus(result);

    const QString& folder = result.folder();
    qDebug() << "Setting sync result for folder " << folder;

    SyncFileItemVector::const_iterator i;
    const SyncFileItemVector& items = result.syncFileItemVector();
    QDateTime dt = QDateTime::currentDateTime();

    for (i = items.begin(); i != items.end(); ++i) {
         const SyncFileItem& item = *i;
         QString errMsg;
         QString tooltip;
         // handle ignored files here.

         if( item._instruction == CSYNC_INSTRUCTION_IGNORE
                 || item._instruction == CSYNC_INSTRUCTION_CONFLICT ) {
             QStringList columns;
             QString timeStr = timeString(dt);
             QString longTimeStr = timeString(dt, QLocale::LongFormat);

             columns << timeStr;
             columns << item._file;
             columns << folder;
             if( item._instruction == CSYNC_INSTRUCTION_IGNORE) {
                 if( item._type == SyncFileItem::File ) {
                     errMsg = tr("File ignored.");
                     tooltip = tr("The file was ignored because it is listed in the clients ignore list\n"
                                  "or the filename contains characters that are not syncable\nin a cross platform "
                                  "environment.");
                 } else if( item._type == SyncFileItem::Directory ){
                     errMsg = tr("Directory ignored.");
                     tooltip = tr("The directory was ignored because it is listed in the clients\nignore list "
                                  "or the directory name contains\ncharacters that are not syncable in a cross  "
                                  "platform environment.");
                 } else if( item._type == SyncFileItem::SoftLink ) {
                     errMsg = tr("Soft Link ignored.");
                     tooltip = tr("Softlinks break the semantics of synchronization.\nPlease do not "
                                  "use them in synced directories.");
                 } else {
                     errMsg = tr("Ignored.");
                 }
             } else if(  item._instruction == CSYNC_INSTRUCTION_CONFLICT ) {
                 errMsg = tr("Conflict file.");
                 tooltip = tr("The file was changed on server and local repository and as a result it\n"
                              "created a so called conflict. The local change is copied to the conflict\n"
                              "file while the file from the server side is available under the original\n"
                              "name");
             } else {
                 Q_ASSERT(!"unhandled instruction.");
             }
             columns << errMsg;

             QTreeWidgetItem *twitem = new QTreeWidgetItem(columns);
             twitem->setData(0, ErrorIndicatorRole, QVariant(true) );
             twitem->setToolTip(0, longTimeStr);
             twitem->setToolTip(3, tooltip);
             twitem->setIcon(0, Theme::instance()->syncStateIcon(SyncResult::Problem, true));
             _ui->_treeWidget->insertTopLevelItem(0, twitem);

         }
    }
}

void ItemProgressDialog::setupList()
{
  // get the folders to set up the top level list.
  Folder::Map map = FolderMan::instance()->map();
  SyncResult lastResult;
  QDateTime dt;
  bool haveSyncResult = false;

  foreach( Folder *f, map.values() ) {
    if( f->syncResult().syncTime() > dt ) {
        dt = f->syncResult().syncTime();
        lastResult = f->syncResult();
        haveSyncResult = true;
    }
  }

  if( haveSyncResult ) {
      setSyncResult(lastResult);
  }

  QList<Progress::Info> progressList = ProgressDispatcher::instance()->recentChangedItems(0); // All.

  QHash <QString, int> folderHash;

  foreach( Progress::Info info, progressList ) {
    slotProgressInfo( info.folder, info );
    folderHash[info.folder] = 1;
  }

  QList<Progress::SyncProblem> problemList = ProgressDispatcher::instance()->recentProblems(0);
  foreach( Progress::SyncProblem prob, problemList ) {
    slotProgressErrors(prob.folder, prob);
    folderHash[prob.folder] = 1;
  }
}

ItemProgressDialog::~ItemProgressDialog()
{
    delete _ui;
}

void ItemProgressDialog::copyToClipboard()
{
    QString text;
    QTextStream ts(&text);

    int topLevelItems = _ui->_treeWidget->topLevelItemCount();
    for (int i = 0; i < topLevelItems; i++) {
        QTreeWidgetItem *child = _ui->_treeWidget->topLevelItem(i);
        // time stamp
        ts << left << qSetFieldWidth(10)
            << child->data(0,Qt::DisplayRole).toString()
                // file name
            << qSetFieldWidth(64)
            << child->data(1,Qt::DisplayRole).toString()
            << qSetFieldWidth(0) << ' '
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
    emit guiLog(tr("Copied to clipboard"), tr("The sync protocol has been copied to the clipboard."));
}

void ItemProgressDialog::accept()
{
    QDialog::accept();
}

void ItemProgressDialog::cleanErrors( const QString& /* folder */ ) // FIXME: Use the folder to detect which errors can be deleted.
{
    _problemCounter = 0;
    QList<QTreeWidgetItem*> wipeList;

    int itemCnt = _ui->_treeWidget->topLevelItemCount();

    for( int cnt = 0; cnt < itemCnt; cnt++ ) {
        QTreeWidgetItem *item = _ui->_treeWidget->topLevelItem(cnt);
        bool isErrorItem = item->data(0, ErrorIndicatorRole).toBool();
        if( isErrorItem ) {
            wipeList.append(item);
        }
    }
    qDeleteAll(wipeList.begin(), wipeList.end());
}

QString ItemProgressDialog::timeString(QDateTime dt, QLocale::FormatType format) const
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

void ItemProgressDialog::slotProgressErrors( const QString& folder, const Progress::SyncProblem& problem )
{
  QStringList columns;
  QString timeStr = timeString(problem.timestamp);
  QString longTimeStr = timeString(problem.timestamp, QLocale::LongFormat);

  columns << timeStr;
  columns << problem.current_file;
  columns << folder;
  QString errMsg = tr("Problem: %1").arg(problem.error_message);
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
  item->setIcon(0, Theme::instance()->syncStateIcon(SyncResult::Error, true));
  item->setToolTip(0, longTimeStr);
  _ui->_treeWidget->insertTopLevelItem(0, item);
  Q_UNUSED(item);
}

void ItemProgressDialog::slotProgressInfo( const QString& folder, const Progress::Info& progress )
{
    if( progress.kind == Progress::StartSync ) {
      cleanErrors( folder );
    }

    if( progress.kind == Progress::EndSync ) {
      // decorateFolderItem( folder );
    }

    // Ingore other events than finishing an individual up- or download.
    if( !(progress.kind == Progress::EndDownload || progress.kind == Progress::EndUpload || progress.kind == Progress::EndDelete)) {
        return;
    }

    QStringList columns;
    QString timeStr = timeString(progress.timestamp);
    QString longTimeStr = timeString(progress.timestamp, QLocale::LongFormat);

    columns << timeStr;
    columns << progress.current_file;
    columns << progress.folder;
    columns << Progress::asResultString(progress.kind);
    columns << Utility::octetsToString( progress.file_size );

    QTreeWidgetItem *item = new QTreeWidgetItem(columns);
    item->setToolTip(0, longTimeStr);
    _ui->_treeWidget->insertTopLevelItem(0, item);
    Q_UNUSED(item);
}


}
