/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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


#include "accountsettings.h"
#include "ui_accountsettings.h"

#include "mirall/theme.h"
#include "mirall/folderman.h"
#include "mirall/owncloudinfo.h"
#include "mirall/folderwizard.h"
#include "mirall/folderstatusmodel.h"
#include "mirall/utility.h"
#include "mirall/application.h"
#include "mirall/fileitemdialog.h"
#include "mirall/owncloudsetupwizard.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/ignorelisteditor.h"
#include "mirall/itemprogressdialog.h"

#include <math.h>

#include <QDebug>
#include <QDesktopServices>
#include <QListWidgetItem>
#include <QMessageBox>

namespace Mirall {

AccountSettings::AccountSettings(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AccountSettings),
    _item(0)
{
    ui->setupUi(this);

    _model = new FolderStatusModel;
    _model->setParent(this);
    FolderStatusDelegate *delegate = new FolderStatusDelegate;

    ui->_folderList->setItemDelegate( delegate );
    ui->_folderList->setModel( _model );
    ui->_folderList->setMinimumWidth( 300 );
    ui->_folderList->setEditTriggers( QAbstractItemView::NoEditTriggers );

    ui->_ButtonRemove->setEnabled(false);
    ui->_ButtonReset->setEnabled(false);
    ui->_ButtonEnable->setEnabled(false);
    ui->_ButtonInfo->setEnabled(false);
    ui->_ButtonAdd->setEnabled(true);

    connect(ui->_ButtonRemove, SIGNAL(clicked()), this, SLOT(slotRemoveCurrentFolder()));
    connect(ui->_ButtonReset,  SIGNAL(clicked()), this, SLOT(slotResetCurrentFolder()));
    connect(ui->_ButtonEnable, SIGNAL(clicked()), this, SLOT(slotEnableCurrentFolder()));
    connect(ui->_ButtonInfo,   SIGNAL(clicked()), this, SLOT(slotInfoAboutCurrentFolder()));
    connect(ui->_ButtonAdd,    SIGNAL(clicked()), this, SLOT(slotAddFolder()));
    connect(ui->modifyAccountButton, SIGNAL(clicked()), SLOT(slotOpenAccountWizard()));
    connect(ui->ignoredFilesButton, SIGNAL(clicked()), SLOT(slotIgnoreFilesEditor()));;

    connect(ui->_folderList, SIGNAL(clicked(QModelIndex)), SLOT(slotFolderActivated(QModelIndex)));
    connect(ui->_folderList, SIGNAL(doubleClicked(QModelIndex)),SLOT(slotDoubleClicked(QModelIndex)));

    ownCloudInfo *ocInfo = ownCloudInfo::instance();
    slotUpdateQuota(ocInfo->lastQuotaTotalBytes(), ocInfo->lastQuotaUsedBytes());
    connect(ocInfo, SIGNAL(quotaUpdated(qint64,qint64)), SLOT(slotUpdateQuota(qint64,qint64)));

    ui->connectLabel->setWordWrap( true );

    setFolderList(FolderMan::instance()->map());

    slotCheckConnection();
}

void AccountSettings::slotFolderActivated( const QModelIndex& indx )
{
  bool state = indx.isValid();

  ui->_ButtonRemove->setEnabled( state );
  ui->_ButtonReset->setEnabled( state );
  ui->_ButtonReset->setEnabled( state );
  ui->_ButtonEnable->setEnabled( state );
  ui->_ButtonInfo->setEnabled( state );

  if ( state ) {
    bool folderEnabled = _model->data( indx, FolderStatusDelegate::FolderSyncEnabled).toBool();
    qDebug() << "folder is sync enabled: " << folderEnabled;
    if ( folderEnabled ) {
      ui->_ButtonEnable->setText( tr( "Pause" ) );
    } else {
      ui->_ButtonEnable->setText( tr( "Resume" ) );
    }
  }
}



void AccountSettings::slotAddFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(false); // do not start more syncs.

    FolderWizard *folderWizard = new FolderWizard(this);
    Folder::Map folderMap = folderMan->map();
    folderWizard->setFolderMap( folderMap );

    connect(folderWizard, SIGNAL(accepted()), SLOT(slotFolderWizardAccepted()));
    connect(folderWizard, SIGNAL(rejected()), SLOT(slotFolderWizardRejected()));
    folderWizard->open();
}


void AccountSettings::slotFolderWizardAccepted()
{
    FolderWizard *folderWizard = qobject_cast<FolderWizard*>(sender());
    FolderMan *folderMan = FolderMan::instance();

    qDebug() << "* Folder wizard completed";

    QString alias        = folderWizard->field(QLatin1String("alias")).toString();
    QString sourceFolder = folderWizard->field(QLatin1String("sourceFolder")).toString();
    QString targetPath   = folderWizard->property("targetPath").toString();

    if (!FolderMan::ensureJournalGone( sourceFolder ))
        return;
    folderMan->addFolderDefinition(alias, sourceFolder, targetPath );
    Folder *f = folderMan->setupFolderFromConfigFile( alias );
    slotAddFolder( f );
    folderMan->setSyncEnabled(true);
    if( f ) {
        folderMan->slotScheduleAllFolders();
        emit folderChanged();
    }
}

void AccountSettings::slotFolderWizardRejected()
{
    qDebug() << "* Folder wizard cancelled";
    FolderMan *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(true);
    folderMan->slotScheduleAllFolders();
}

void AccountSettings::slotOpenAccountWizard()
{
    this->topLevelWidget()->close();
    OwncloudSetupWizard::runWizard(qApp, SLOT(slotownCloudWizardDone(int)), 0);
}

void AccountSettings::slotAddFolder( Folder *folder )
{
    if( ! folder || folder->alias().isEmpty() ) return;

    QStandardItem *item = new QStandardItem();
    folderToModelItem( item, folder );
    _model->appendRow( item );
    slotCheckConnection();
}



void AccountSettings::buttonsSetEnabled()
{
    bool haveFolders = ui->_folderList->model()->rowCount() > 0;

    ui->_ButtonRemove->setEnabled(false);
    if( Theme::instance()->singleSyncFolder() ) {
        // only one folder synced folder allowed.
        ui->_ButtonAdd->setVisible(!haveFolders);
    } else {
        ui->_ButtonAdd->setVisible(true);
        ui->_ButtonAdd->setEnabled(true);
    }

    QModelIndex selected = ui->_folderList->currentIndex();
    bool isSelected = selected.isValid();

    ui->_ButtonEnable->setEnabled(isSelected);
    ui->_ButtonReset->setEnabled(isSelected);
    ui->_ButtonRemove->setEnabled(isSelected);
    ui->_ButtonInfo->setEnabled(isSelected);
}

void AccountSettings::setListWidgetItem( QListWidgetItem *item )
{
    _item = item;
}

void AccountSettings::folderToModelItem( QStandardItem *item, Folder *f )
{
    if( ! item || !f ) return;

    item->setData( f->nativePath(),        FolderStatusDelegate::FolderPathRole );
    item->setData( f->secondPath(),        FolderStatusDelegate::FolderSecondPathRole );
    item->setData( f->alias(),             FolderStatusDelegate::FolderAliasRole );
    item->setData( f->syncEnabled(),       FolderStatusDelegate::FolderSyncEnabled );

    SyncResult res = f->syncResult();
    SyncResult::Status status = res.status();

    QString errors = res.errorStrings().join(QLatin1String("<br/>"));

    Theme *theme = Theme::instance();
    item->setData( theme->statusHeaderText( status ),  Qt::ToolTipRole );
    if( f->syncEnabled() ) {
        item->setData( theme->syncStateIcon( status ), FolderStatusDelegate::FolderStatusIconRole );
    } else {
        item->setData( theme->folderDisabledIcon( ),   FolderStatusDelegate::FolderStatusIconRole ); // size 48 before
    }
    item->setData( theme->statusHeaderText( status ),  FolderStatusDelegate::FolderStatus );
    item->setData( errors,                             FolderStatusDelegate::FolderErrorMsg );

    bool ongoing = false;
    item->setData( QVariant(res.warnCount()), FolderStatusDelegate::WarningCount );
    if( status == SyncResult::SyncRunning ) {
        ongoing = true;
    }
    item->setData( ongoing, FolderStatusDelegate::SyncRunning);

}

void AccountSettings::slotRemoveCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
        qDebug() << "Remove Folder alias " << alias;
        if( !alias.isEmpty() ) {
            // remove from file system through folder man
            // _model->removeRow( selected.row() );
            int ret = QMessageBox::question( this, tr("Confirm Folder Remove"),
                                             tr("<p>Do you really want to stop syncing the folder <i>%1</i>?</p>"
                                                "<p><b>Note:</b> This will not remove the files from your client.</p>").arg(alias),
                                             QMessageBox::Yes|QMessageBox::No );

            if( ret == QMessageBox::No ) {
                return;
            }
            FolderMan *folderMan = FolderMan::instance();
            folderMan->slotRemoveFolder( alias );
            setFolderList(folderMan->map());
            emit folderChanged();
            slotCheckConnection();
        }
    }
}
void AccountSettings::slotResetCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
        int ret = QMessageBox::question( 0, tr("Confirm Folder Reset"),
                                         tr("<p>Do you really want to reset folder <i>%1</i> and rebuild your client database?</p>"
                                            "<p><b>Note:</b> While no files will be removed, this can cause significant data "
                                            "traffic and take several minutes to hours, depending on the size of the folder.</p>").arg(alias),
                                         QMessageBox::Yes|QMessageBox::No );
        if( ret == QMessageBox::Yes ) {
            FolderMan *folderMan = FolderMan::instance();
            Folder *f = folderMan->folder(alias);
            f->slotTerminateSync();
            f->wipe();
            folderMan->slotScheduleAllFolders();
        }
    }
}

void AccountSettings::slotDoubleClicked( const QModelIndex& indx )
{
    if( ! indx.isValid() ) return;
    QString alias = _model->data( indx, FolderStatusDelegate::FolderAliasRole ).toString();

    emit openFolderAlias( alias );
}

void AccountSettings::slotCheckConnection()
{
    if( ownCloudInfo::instance()->isConfigured() ) {
        connect(ownCloudInfo::instance(), SIGNAL(ownCloudInfoFound(const QString&, const QString&, const QString&, const QString&)),
                this, SLOT(slotOCInfo( const QString&, const QString&, const QString&, const QString& )));
        connect(ownCloudInfo::instance(), SIGNAL(noOwncloudFound(QNetworkReply*)),
                this, SLOT(slotOCInfoFail(QNetworkReply*)));

        ui->connectLabel->setText( tr("Checking %1 connection...").arg(Theme::instance()->appNameGUI()));
        qDebug() << "Check status.php from statusdialog.";
        ownCloudInfo::instance()->checkInstallation();
    } else {
        // ownCloud is not yet configured.
        ui->connectLabel->setText( tr("No %1 connection configured.").arg(Theme::instance()->appNameGUI()));
        ui->_ButtonAdd->setEnabled( false);
    }
}

void AccountSettings::setFolderList( const Folder::Map &folders )
{
    _model->clear();
    foreach( Folder *f, folders ) {
        qDebug() << "Folder: " << f;
        slotAddFolder( f );
    }

   QModelIndex idx = _model->index(0, 0);
   if (idx.isValid())
        ui->_folderList->setCurrentIndex(idx);
    buttonsSetEnabled();

}

// move from Application
void AccountSettings::slotFolderOpenAction( const QString& alias )
{
    Folder *f = FolderMan::instance()->folder(alias);
    qDebug() << "opening local url " << f->path();
    if( f ) {
        QUrl url(f->path(), QUrl::TolerantMode);
        url.setScheme( QLatin1String("file") );

#ifdef Q_OS_WIN32
        // work around a bug in QDesktopServices on Win32, see i-net
        QString filePath = f->path();

        if (filePath.startsWith(QLatin1String("\\\\")) || filePath.startsWith(QLatin1String("//")))
            url.setUrl(QDir::toNativeSeparators(filePath));
        else
            url = QUrl::fromLocalFile(filePath);
#endif
        QDesktopServices::openUrl(url);
    }
}

void AccountSettings::slotEnableCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();

    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
        bool folderEnabled = _model->data( selected, FolderStatusDelegate::FolderSyncEnabled).toBool();
        qDebug() << "Toggle enabled/disabled Folder alias " << alias << " - current state: " << folderEnabled;
        if( !alias.isEmpty() ) {
            FolderMan *folderMan = FolderMan::instance();

            qDebug() << "Application: enable folder with alias " << alias;
            bool terminate = false;

            // this sets the folder status to disabled but does not interrupt it.
            Folder *f = folderMan->folder( alias );
            if( f && !folderEnabled ) {
                // check if a sync is still running and if so, ask if we should terminate.
                if( f->isBusy() ) { // its still running
                    int reply = QMessageBox::question( 0, tr("Sync Running"),
                                                       tr("The syncing operation is running.<br/>Do you want to terminate it?"),
                                                       QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
                    if ( reply == QMessageBox::Yes )
                        terminate = true;
                    else
                        return; // do nothing
                }
            }

            // message box can return at any time while the thread keeps running,
            // so better check again after the user has responded.
            if ( f->isBusy() && terminate )
                folderMan->terminateSyncProcess( alias );

            folderMan->slotEnableFolder( alias, !folderEnabled );
            slotUpdateFolderState (f);
            // set the button text accordingly.
            slotFolderActivated( selected );
        }
    }
}

void AccountSettings::slotUpdateFolderState( Folder *folder )
{
    QStandardItem *item = 0;
    int row = 0;

    if( ! folder ) return;

    item = _model->item( row );

    while( item ) {
        if( item->data( FolderStatusDelegate::FolderAliasRole ) == folder->alias() ) {
            // its the item to update!
            break;
        }
        item = _model->item( ++row );
    }

#if 0
    if( !_fileItemDialog.isNull() && _fileItemDialog->isVisible() ) {
        _fileItemDialog->setSyncResult( FolderMan::instance()->syncResult(folder) );
    }
#endif
    if( item ) {
        folderToModelItem( item, folder );
    } else {
        // the dialog is not visible.
    }
    slotCheckConnection();
}

void AccountSettings::slotOCInfo( const QString& url, const QString& versionStr, const QString& version, const QString& )
{
#ifdef Q_OS_WIN32
        // work around a bug in QDesktopServices on Win32, see i-net
        QString filePath = url;

        if (filePath.startsWith("\\\\") || filePath.startsWith("//"))
            _OCUrl.setUrl(QDir::toNativeSeparators(filePath));
        else
            _OCUrl = QUrl::fromLocalFile(filePath);
#else
    _OCUrl = QUrl::fromLocalFile(url);
#endif

    qDebug() << "#-------# oC found on " << url;
    /* enable the open button */
    ui->connectLabel->setOpenExternalLinks(true);
    ui->connectLabel->setText( tr("Connected to <a href=\"%1\">%1</a>.").arg(url) );
    ui->connectLabel->setToolTip( tr("Version: %1 (%2)").arg(versionStr).arg(version));
    ui->_ButtonAdd->setEnabled(true);

    disconnect(ownCloudInfo::instance(), SIGNAL(ownCloudInfoFound(const QString&, const QString&, const QString&, const QString&)),
            this, SLOT(slotOCInfo( const QString&, const QString&, const QString&, const QString& )));
    disconnect(ownCloudInfo::instance(), SIGNAL(noOwncloudFound(QNetworkReply*)),
            this, SLOT(slotOCInfoFail(QNetworkReply*)));
}

void AccountSettings::slotOCInfoFail( QNetworkReply *reply)
{
    QString errStr = tr("unknown problem.");
    if( reply ) errStr = reply->errorString();

    ui->connectLabel->setText( tr("<p>Failed to connect to %1: <tt>%2</tt></p>").arg(Theme::instance()->appNameGUI()).arg(errStr) );
    ui->_ButtonAdd->setEnabled( false);

    disconnect(ownCloudInfo::instance(), SIGNAL(ownCloudInfoFound(const QString&, const QString&, const QString&, const QString&)),
            this, SLOT(slotOCInfo( const QString&, const QString&, const QString&, const QString& )));
    disconnect(ownCloudInfo::instance(), SIGNAL(noOwncloudFound(QNetworkReply*)),
            this, SLOT(slotOCInfoFail(QNetworkReply*)));

}

void AccountSettings::slotOpenOC()
{
  if( _OCUrl.isValid() )
    QDesktopServices::openUrl( _OCUrl );
}

QStandardItem* AccountSettings::itemForFolder(const QString& folder)
{
    QStandardItem *item = NULL;

    if( folder.isEmpty() ) {
        return item;
    }

    int row = 0;

    item = _model->item( row );

    while( item ) {
        if( item->data( FolderStatusDelegate::FolderAliasRole ) == folder ) {
            // its the item to update!
            break;
        }
        item = _model->item( ++row );
    }
    return item;
}

QString AccountSettings::shortenFilename( const QString& folder, const QString& file ) const
{
    // strip off the server prefix from the file name
    QString shortFile(file);
    if( shortFile.isEmpty() ) {
        return QString::null;
    }

    if(shortFile.startsWith(QLatin1String("ownclouds://")) ||
            shortFile.startsWith(QLatin1String("owncloud://")) ) {
        // rip off the whole ownCloud URL.
        Folder *f = FolderMan::instance()->folder(folder);
        if( f ) {
            QString remotePathUrl = ownCloudInfo::instance()->webdavUrl() + QLatin1Char('/') + f->secondPath();
            shortFile.remove(Utility::toCSyncScheme(remotePathUrl));

        }
    }
    return shortFile;
}

void AccountSettings::slotProgressProblem(const QString& folder, const Progress::SyncProblem& problem)
{
    Q_UNUSED(problem);

    QStandardItem *item = itemForFolder( folder );
    if( !item ) return;

    int warnCount = qvariant_cast<int>( item->data(FolderStatusDelegate::WarningCount) );
    warnCount++;
    item->setData( QVariant(warnCount), FolderStatusDelegate::WarningCount );
}

void AccountSettings::slotSetProgress(const QString& folder, const Progress::Info &progress )
{
    // qDebug() << "================================> Progress for folder " << folder << " file " << file << ": "<< p1;
    QStandardItem *item = itemForFolder( folder );
    qint64 prog1 = progress.current_file_bytes;
    qint64 prog2 = progress.file_size;

    if( item == NULL ) {
        return;
    }

    // Hotfix for a crash that I experienced in a very rare case/setup
    if (progress.kind == Mirall::Progress::Invalid) {
        qDebug() << "================================> INVALID Progress for folder " << folder;
        return;
    }

    QString itemFileName = shortenFilename(folder, progress.current_file);
    QString syncFileProgressString;

    // stay with the previous kind-string for Context.
    if( progress.kind != Progress::Context ) {
        _kindContext = Progress::asActionString(progress.kind);
    }
    QString kindString = _kindContext;

    switch( progress.kind ) {
    case Progress::StartSync:
        item->setData( QVariant(0), FolderStatusDelegate::WarningCount );
        break;
    case Progress::StartDownload:
    case Progress::StartUpload:
    case Progress::StartDelete:
        syncFileProgressString = tr("Start");
        if( _hideProgressTimers.contains(item) ) {
            // The timer is still running.
            QTimer *t = _hideProgressTimers.take(item);
            t->stop();
            t->deleteLater();
        }
        break;
    case Progress::Context:
        syncFileProgressString = tr("Currently");
        break;
    case Progress::EndDownload:
    case Progress::EndUpload:
        break;
    case Progress::EndSync:
        syncFileProgressString = tr("Completely");

        // start a timer to stop the progress display
        QTimer *timer;
        if( _hideProgressTimers.contains(item) ) {
            timer = _hideProgressTimers[item];
            // there is already one timer running.
        } else {
            timer = new QTimer;
            connect(timer, SIGNAL(timeout()), this, SLOT(slotHideProgress()));
            timer->setSingleShot(true);
            _hideProgressTimers.insert(item, timer);
        }
        timer->start(5000);
        break;
    case Progress::Invalid:
    case Progress::Download:
    case Progress::Upload:
    case Progress::Inactive:
    case Progress::Error:
        break;
    }

    QString fileProgressString;
    QString s1 = Utility::octetsToString( prog1 );
    QString s2 = Utility::octetsToString( prog2 );

    // switch on extra space.
    item->setData( QVariant(true), FolderStatusDelegate::AddProgressSpace );

    if( progress.kind != Progress::EndSync ) {
        // Example text: "Currently uploading foobar.png (1MB of 2MB)"
        fileProgressString = tr("%1 %2 %3 (%4 of %5)").arg(syncFileProgressString).arg(kindString).
                arg(itemFileName).arg(s1).arg(s2);
    } else {
        fileProgressString = tr("Completely finished.");
    }
    item->setData( fileProgressString,FolderStatusDelegate::SyncProgressItemString);

    // overall progress
    s1 = Utility::octetsToString( progress.overall_current_bytes );
    s2 = Utility::octetsToString( progress.overall_transmission_size );
    QString overallSyncString = tr("%1 of %2, file %3 of %4").arg(s1).arg(s2)
            .arg(progress.current_file_no).arg(progress.overall_file_count);
    item->setData( overallSyncString, FolderStatusDelegate::SyncProgressOverallString );

    int overallPercent = 0;
    if( progress.overall_transmission_size > 0 ) {
        overallPercent = qRound(double(progress.overall_current_bytes)/double(progress.overall_transmission_size) * 100.0);
    }
    item->setData( overallPercent, FolderStatusDelegate::SyncProgressOverallPercent);
}

void AccountSettings::slotHideProgress()
{
    QTimer *send_timer = qobject_cast<QTimer*>(this->sender());
    QHash<QStandardItem*, QTimer*>::const_iterator i = _hideProgressTimers.constBegin();
    while (i != _hideProgressTimers.constEnd()) {
        if( i.value() == send_timer ) {
            QStandardItem *item = i.key();
            item->setData( QVariant(false),  FolderStatusDelegate::AddProgressSpace );
            item->setData( QVariant(QString::null), FolderStatusDelegate::SyncProgressOverallString );
            item->setData( QVariant(QString::null), FolderStatusDelegate::SyncProgressItemString );
            item->setData( 0,                       FolderStatusDelegate::SyncProgressOverallPercent );

            ui->_folderList->repaint();
            _hideProgressTimers.remove(item);
            break;
        }
        ++i;
    }

    send_timer->deleteLater();
}

void AccountSettings::slotUpdateQuota(qint64 total, qint64 used)
{
    ui->quotaProgressBar->setEnabled(true);
    // workaround the label only accepting ints (which may be only 32 bit wide)
    ui->quotaProgressBar->setMaximum(100);
    ui->quotaProgressBar->setValue(round(used/(double)total * 100));
    QString usedStr = Utility::octetsToString(used);
    QString totalStr = Utility::octetsToString(total);
    ui->quotaLabel->setText(tr("You are using %1 of your available %2 storage.").arg(usedStr, totalStr));
}

void AccountSettings::slotIgnoreFilesEditor()
{
    if (_ignoreEditor.isNull()) {
        _ignoreEditor = new IgnoreListEditor(this);
        _ignoreEditor->setAttribute( Qt::WA_DeleteOnClose, true );
        _ignoreEditor->open();
    } else {
        Utility::raiseDialog(_ignoreEditor);
    }
}

void AccountSettings::slotInfoAboutCurrentFolder()
{
    emit(openProgressDialog());
}

AccountSettings::~AccountSettings()
{
    delete ui;
}

} // namespace Mirall
