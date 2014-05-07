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
#include "mirall/folderwizard.h"
#include "mirall/folderstatusmodel.h"
#include "mirall/utility.h"
#include "mirall/application.h"
#include "mirall/owncloudsetupwizard.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/ignorelisteditor.h"
#include "mirall/account.h"
#include "mirall/quotainfo.h"
#include "creds/abstractcredentials.h"

#include <math.h>

#include <QDebug>
#include <QDesktopServices>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QAction>
#include <QKeySequence>
#include <QIcon>
#include <QVariant>

#include "mirall/account.h"

namespace Mirall {

static const char progressBarStyleC[] =
        "QProgressBar {"
        "border: 1px solid grey;"
        "border-radius: 5px;"
        "text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "background-color: %1; width: 1px;"
        "}";

AccountSettings::AccountSettings(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AccountSettings),
    _wasDisabledBefore(false),
    _account(AccountManager::instance()->account())
{
    ui->setupUi(this);

    _model = new FolderStatusModel;
    _model->setParent(this);
    FolderStatusDelegate *delegate = new FolderStatusDelegate;
    delegate->setParent(this);

    ui->_folderList->setItemDelegate( delegate );
    ui->_folderList->setModel( _model );
#if defined(Q_OS_MAC)
    ui->_folderList->setMinimumWidth( 400 );
#else
    ui->_folderList->setMinimumWidth( 300 );
#endif
    ui->_folderList->setEditTriggers( QAbstractItemView::NoEditTriggers );

    ui->_buttonRemove->setEnabled(false);
    ui->_buttonEnable->setEnabled(false);
    ui->_buttonAdd->setEnabled(true);

    QAction *resetFolderAction = new QAction(this);
    resetFolderAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(resetFolderAction, SIGNAL(triggered()), SLOT(slotResetCurrentFolder()));
    addAction(resetFolderAction);

    QAction *syncNowAction = new QAction(this);
    syncNowAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(syncNowAction, SIGNAL(triggered()), SLOT(slotSyncCurrentFolderNow()));
    addAction(syncNowAction);

    connect(ui->_buttonRemove, SIGNAL(clicked()), this, SLOT(slotRemoveCurrentFolder()));
    connect(ui->_buttonEnable, SIGNAL(clicked()), this, SLOT(slotEnableCurrentFolder()));
    connect(ui->_buttonAdd,    SIGNAL(clicked()), this, SLOT(slotAddFolder()));
    connect(ui->modifyAccountButton, SIGNAL(clicked()), SLOT(slotOpenAccountWizard()));
    connect(ui->ignoredFilesButton, SIGNAL(clicked()), SLOT(slotIgnoreFilesEditor()));;

    connect(ui->_folderList, SIGNAL(clicked(QModelIndex)), SLOT(slotFolderActivated(QModelIndex)));
    connect(ui->_folderList, SIGNAL(doubleClicked(QModelIndex)),SLOT(slotDoubleClicked(QModelIndex)));

    QColor color = palette().highlight().color();
    ui->quotaProgressBar->setStyleSheet(QString::fromLatin1(progressBarStyleC).arg(color.name()));
    ui->connectLabel->setWordWrap(true);
    ui->connectLabel->setOpenExternalLinks(true);
    ui->quotaLabel->setWordWrap(true);

    ui->connectLabel->setText(tr("No account configured."));
    ui->_buttonAdd->setEnabled(false);

    connect(AccountManager::instance(), SIGNAL(accountChanged(Account*,Account*)),
            this, SLOT(slotAccountChanged(Account*,Account*)));
    slotAccountChanged(AccountManager::instance()->account(), 0);

    connect(FolderMan::instance(), SIGNAL(folderListLoaded(Folder::Map)),
            this, SLOT(setFolderList(Folder::Map)));
    setFolderList(FolderMan::instance()->map());
}

void AccountSettings::slotAccountChanged(Account *newAccount, Account *oldAccount)
{
    if (oldAccount) {
        disconnect(oldAccount, SIGNAL(stateChanged(int)), this, SLOT(slotAccountStateChanged(int)));
        disconnect(oldAccount->quotaInfo(), SIGNAL(quotaUpdated(qint64,qint64)),
                    this, SLOT(slotUpdateQuota(qint64,qint64)));
    }

    _account = newAccount;
    if (_account) {
        connect(_account, SIGNAL(stateChanged(int)), SLOT(slotAccountStateChanged(int)));
        slotAccountStateChanged(_account->state());

        QuotaInfo *quotaInfo = _account->quotaInfo();
        connect( quotaInfo, SIGNAL(quotaUpdated(qint64,qint64)),
                this, SLOT(slotUpdateQuota(qint64,qint64)));
        slotUpdateQuota(quotaInfo->lastQuotaTotalBytes(), quotaInfo->lastQuotaUsedBytes());
    }

}

void AccountSettings::slotFolderActivated( const QModelIndex& indx )
{
  bool isValid = indx.isValid();

  bool haveFolders = ui->_folderList->model()->rowCount() > 0;

  ui->_buttonRemove->setEnabled(isValid);
  if( Theme::instance()->singleSyncFolder() ) {
      // only one folder synced folder allowed.
      ui->_buttonAdd->setVisible(!haveFolders);
  } else {
      ui->_buttonAdd->setVisible(true);
  }
  ui->_buttonAdd->setEnabled(_account && _account->state() == Account::Connected);
  ui->_buttonEnable->setEnabled( isValid );

  if ( isValid ) {
    bool folderEnabled = _model->data( indx, FolderStatusDelegate::FolderSyncEnabled).toBool();
    if ( folderEnabled ) {
      ui->_buttonEnable->setText( tr( "Pause" ) );
    } else {
      ui->_buttonEnable->setText( tr( "Resume" ) );
    }
    ui->_buttonEnable->setEnabled( _account && _account->state() == Account::Connected);
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
    slotButtonsSetEnabled();
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
    // in order to update the enabled state of the "Sync now" button
    connect(folder, SIGNAL(syncStateChange()), this, SLOT(slotFolderSyncStateChange()), Qt::UniqueConnection);
}

void AccountSettings::slotButtonsSetEnabled()
{
    QModelIndex selected = ui->_folderList->currentIndex();

    slotFolderActivated(selected);
}

void AccountSettings::setGeneralErrors( const QStringList& errors )
{
    _generalErrors = errors;
    if (_account) {
        // this will update the message
        slotAccountStateChanged(_account->state());
    }
}

void AccountSettings::folderToModelItem( QStandardItem *item, Folder *f )
{
    if( ! item || !f ) return;

    item->setData( f->nativePath(),        FolderStatusDelegate::FolderPathRole );
    item->setData( f->remotePath(),        FolderStatusDelegate::FolderSecondPathRole );
    item->setData( f->alias(),             FolderStatusDelegate::FolderAliasRole );
    item->setData( f->syncEnabled(),       FolderStatusDelegate::FolderSyncEnabled );

    SyncResult res = f->syncResult();
    SyncResult::Status status = res.status();

    QStringList errorList = res.errorStrings();

    Theme *theme = Theme::instance();
    item->setData( theme->statusHeaderText( status ),  Qt::ToolTipRole );
    if( f->syncEnabled() ) {
        if( status == SyncResult::SyncPrepare ) {
            if( _wasDisabledBefore ) {
                // if the folder was disabled before, set the sync icon
                item->setData( theme->syncStateIcon( SyncResult::SyncRunning), FolderStatusDelegate::FolderStatusIconRole );
            }  // we keep the previous icon for the SyncPrepare state.
        } else if( status == SyncResult::Undefined ) {
            // startup, the sync was never done.
            qDebug() << "XXX FIRST time sync, setting icon to sync running!";
            item->setData( theme->syncStateIcon( SyncResult::SyncRunning), FolderStatusDelegate::FolderStatusIconRole );
        } else {
            // kepp the previous icon for the prepare phase.
            if( status == SyncResult::Problem) {
                item->setData( theme->syncStateIcon( SyncResult::Success), FolderStatusDelegate::FolderStatusIconRole );
            } else {
                item->setData( theme->syncStateIcon( status ), FolderStatusDelegate::FolderStatusIconRole );
            }
        }
    } else {
        item->setData( theme->folderDisabledIcon( ), FolderStatusDelegate::FolderStatusIconRole ); // size 48 before
        _wasDisabledBefore = false;
    }
    item->setData( theme->statusHeaderText( status ), FolderStatusDelegate::FolderStatus );

    if( errorList.isEmpty() ) {
        if( (status == SyncResult::Error ||
             status == SyncResult::SetupError ||
             status == SyncResult::SyncAbortRequested ||
             status == SyncResult::Unavailable)) {
            errorList <<  theme->statusHeaderText(status);
        }
    }

    item->setData( errorList, FolderStatusDelegate::FolderErrorMsg);

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
        int row = selected.row();

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
            /* Remove the selected item from the timer hash. */
            QStandardItem *item = NULL;
            if( selected.isValid() )
                item = _model->itemFromIndex(selected);

            if( selected.isValid() && item && _hideProgressTimers.contains(item) ) {
                QTimer *t = _hideProgressTimers[item];
                t->stop();
                _hideProgressTimers.remove(item);
                delete(t);
            }

            FolderMan *folderMan = FolderMan::instance();
            folderMan->slotRemoveFolder( alias );
            _model->removeRow(row);

            // single folder fix to show add-button and hide remove-button
            slotButtonsSetEnabled();

            emit folderChanged();
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
                                            "<p><b>Note:</b> This function is designed for maintenance purposes only. "
                                            "No files will be removed, but this can cause significant data traffic and "
                                            "take several minutes or hours to complete, depending on the size of the folder. "
                                            "Only use this option if advised by your administrator.</p>").arg(alias),
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

void AccountSettings::showConnectionLabel( const QString& message, const QString& tooltip )
{
    const QString errStyle = QLatin1String("color:#ffffff; background-color:#bb4d4d;padding:5px;"
                                           "border-width: 1px; border-style: solid; border-color: #aaaaaa;"
                                           "border-radius:5px;");
    if( _generalErrors.isEmpty() ) {
        ui->connectLabel->setText( message );
        ui->connectLabel->setToolTip(tooltip);
        ui->connectLabel->setStyleSheet(QString());
    } else {
        const QString msg = _generalErrors.join(QLatin1String("\n"));
        ui->connectLabel->setText( msg );
        ui->connectLabel->setToolTip(QString());
        ui->connectLabel->setStyleSheet(errStyle);
    }
}

void AccountSettings::setFolderList( const Folder::Map &folders )
{
    _model->clear();

    foreach(QTimer *t, _hideProgressTimers) {
        t->stop();
        delete t;
    }
    _hideProgressTimers.clear();

    foreach( Folder *f, folders ) {
        slotAddFolder( f );
    }

    QModelIndex idx = _model->index(0, 0);
    if (idx.isValid()) {
        ui->_folderList->setCurrentIndex(idx);
    }
    slotButtonsSetEnabled();

}

// move from Application
void AccountSettings::slotFolderOpenAction( const QString& alias )
{
    Folder *f = FolderMan::instance()->folder(alias);
    qDebug() << "opening local url " << f->path();
    if( f ) {
        QUrl url(f->path(), QUrl::TolerantMode);
        url.setScheme( QLatin1String("file") );

#ifdef Q_OS_WIN
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
            if( f && folderEnabled ) {
                // check if a sync is still running and if so, ask if we should terminate.
                if( f->isBusy() ) { // its still running
#if defined(Q_OS_MAC)
                    QWidget *parent = this;
                    Qt::WindowFlags flags = Qt::Sheet;
#else
                    QWidget *parent = 0;
                    Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint; // default flags
#endif
                    QMessageBox msgbox(QMessageBox::Question, tr("Sync Running"),
                                       tr("The syncing operation is running.<br/>Do you want to terminate it?"),
                                       QMessageBox::Yes | QMessageBox::No, parent, flags);
                    msgbox.setDefaultButton(QMessageBox::Yes);
                    int reply = msgbox.exec();
                    if ( reply == QMessageBox::Yes )
                        terminate = true;
                    else
                        return; // do nothing
                }
            }

            // message box can return at any time while the thread keeps running,
            // so better check again after the user has responded.
            if ( f->isBusy() && terminate ) {
                f->slotTerminateSync();
            }

            folderMan->slotEnableFolder( alias, !folderEnabled );

            // keep state for the icon setting.
            if( !folderEnabled ) _wasDisabledBefore = true;

            slotUpdateFolderState (f);
            // set the button text accordingly.
            slotFolderActivated( selected );
        }
    }
}

void AccountSettings::slotSyncCurrentFolderNow()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if( !selected.isValid() )
        return;
    QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
    FolderMan *folderMan = FolderMan::instance();

    folderMan->slotScheduleSync(alias);
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

    if( item ) {
        folderToModelItem( item, folder );
    } else {
        // the dialog is not visible.
    }
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
            QString remotePathUrl = f->remoteUrl().toString();
            shortFile.remove(Utility::toCSyncScheme(remotePathUrl));
        }
    }
    return shortFile;
}

void AccountSettings::slotSetProgress(const QString& folder, const Progress::Info &progress )
{
    QStandardItem *item = itemForFolder( folder );
    if( !item ) return;

    if(!progress._lastCompletedItem.isEmpty()
            && Progress::isWarningKind(progress._lastCompletedItem._status)) {
        int warnCount = item->data(FolderStatusDelegate::WarningCount).toInt();
        warnCount++;
        item->setData( QVariant(warnCount), FolderStatusDelegate::WarningCount );
    }

    // find the single item to display:  This is going to be the bigger item, or the last completed
    // item if no items are in progress.
    SyncFileItem curItem = progress._lastCompletedItem;
    qint64 curItemProgress = -1; // -1 means finished
    quint64 biggerItemSize = -1;
    foreach(const Progress::Info::ProgressItem &citm, progress._currentItems) {
        if (curItemProgress == -1 || (Progress::isSizeDependent(citm._item._instruction)
                                      && biggerItemSize < citm._item._size)) {
            curItemProgress = citm._completedSize;
            curItem = citm._item;
            biggerItemSize = citm._item._size;
        }
    }
    if (curItemProgress == -1) {
        curItemProgress = curItem._size;
    }

    QString itemFileName = shortenFilename(folder, curItem._file);
    QString kindString = Progress::asActionString(curItem);

    // switch on extra space.
    item->setData( QVariant(true), FolderStatusDelegate::AddProgressSpace );

    QString fileProgressString;
    if (Progress::isSizeDependent(curItem._instruction)) {
        QString s1 = Utility::octetsToString( curItemProgress );
        QString s2 = Utility::octetsToString( curItem._size );
        //: Example text: "uploading foobar.png (1MB of 2MB)"
        fileProgressString = tr("%1 %2 (%3 of %4)").arg(kindString, itemFileName, s1, s2);
    } else {
        //: Example text: "uploading foobar.png"
        fileProgressString = tr("%1 %2").arg(kindString, itemFileName);
    }
    item->setData( fileProgressString,FolderStatusDelegate::SyncProgressItemString);

    // overall progress
    quint64 completedSize = progress.completedSize();
    quint64 currentFile =  progress._completedFileCount + progress._currentItems.count();
    QString s1 = Utility::octetsToString( completedSize );
    QString s2 = Utility::octetsToString( progress._totalSize );
    QString overallSyncString = tr("%1 of %2, file %3 of %4").arg(s1, s2)
            .arg(currentFile).arg(progress._totalFileCount);
    item->setData( overallSyncString, FolderStatusDelegate::SyncProgressOverallString );

    int overallPercent = 0;
    if( progress._totalFileCount > 0 ) {
        // Add one 'byte' for each files so the percentage is moving when deleting or renaming files
        overallPercent = qRound(double(completedSize + progress._completedFileCount)/double(progress._totalSize + progress._totalFileCount) * 100.0);
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

            /* Check if this item is still existing */
            bool ok = false;
            for( int r = 0; !ok && r < _model->rowCount(); r++) {
                if( item == _model->item(r,0) ) {
                    ok = true;
                }
            }

            if( ok ) {
                item->setData( false,     FolderStatusDelegate::AddProgressSpace );
                item->setData( QString(), FolderStatusDelegate::SyncProgressOverallString );
                item->setData( QString(), FolderStatusDelegate::SyncProgressItemString );
                item->setData( 0,         FolderStatusDelegate::SyncProgressOverallPercent );
            }
            _hideProgressTimers.remove(item);
            break;
        }
        ++i;
    }

    send_timer->deleteLater();
}

void AccountSettings::slotFolderSyncStateChange()
{
    slotButtonsSetEnabled();
    Folder* folder = qobject_cast<Folder *>(sender());
    if (!folder) return;

    QStandardItem *item = itemForFolder( folder->alias() );
    if( !item ) return;

    SyncResult::Status state = folder->syncResult().status();
    if (state == SyncResult::SyncPrepare)  {
        item->setData( QVariant(0), FolderStatusDelegate::WarningCount );
    } else if (state == SyncResult::Success || state == SyncResult::Problem) {
        // start a timer to stop the progress display
        QTimer *timer;
        if( _hideProgressTimers.contains(item) ) {
            timer = _hideProgressTimers[item];
            // there is already one timer running.
        } else {
            timer = new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(slotHideProgress()));
            timer->setSingleShot(true);
            _hideProgressTimers.insert(item, timer);
        }
        timer->start(5000);
    }
}


void AccountSettings::slotUpdateQuota(qint64 total, qint64 used)
{
    if( total > 0 ) {
        ui->quotaProgressBar->setVisible(true);
        ui->quotaInfoLabel->setVisible(true);
        ui->quotaProgressBar->setEnabled(true);
        // workaround the label only accepting ints (which may be only 32 bit wide)
        ui->quotaProgressBar->setMaximum(100);
        int qVal = qRound(used/(double)total * 100);
        if( qVal > 100 ) qVal = 100;
        ui->quotaProgressBar->setValue(qVal);
        QString usedStr = Utility::octetsToString(used);
        QString totalStr = Utility::octetsToString(total);
        double percent = used/(double)total*100;
        QString percentStr = Utility::compactFormatDouble(percent, 1);
        ui->quotaLabel->setText(tr("%1 (%3%) of %2 server space in use.").arg(usedStr, totalStr, percentStr));
    } else {
        ui->quotaProgressBar->setVisible(false);
        ui->quotaInfoLabel->setVisible(false);
        ui->quotaLabel->setText(tr("Currently there is no storage usage information available."));
    }
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

void AccountSettings::slotAccountStateChanged(int state)
{
    if (_account) {
        ui->sslButton->updateAccountInfo(_account);
        QUrl safeUrl(_account->url());
        safeUrl.setPassword(QString()); // Remove the password from the URL to avoid showing it in the UI
        slotButtonsSetEnabled();
        if (state == Account::Connected) {
            QString user;
            if (AbstractCredentials *cred = _account->credentials()) {
               user = cred->user();
            }
            if (user.isEmpty()) {
                showConnectionLabel( tr("Connected to <a href=\"%1\">%2</a>.").arg(_account->url().toString(), safeUrl.toString())
                                 /*, tr("Version: %1 (%2)").arg(versionStr).arg(version) */ );
            } else {
                showConnectionLabel( tr("Connected to <a href=\"%1\">%2</a> as <i>%3</i>.").arg(_account->url().toString(), safeUrl.toString(), user)
                                 /*, tr("Version: %1 (%2)").arg(versionStr).arg(version) */ );
            }
        } else {
            showConnectionLabel( tr("No connection to %1 at <a href=\"%2\">%3</a>.")
                                 .arg(Theme::instance()->appNameGUI(),
                                      _account->url().toString(),
                                      safeUrl.toString()) );
        }
    } else {
        // ownCloud is not yet configured.
        showConnectionLabel( tr("No %1 connection configured.").arg(Theme::instance()->appNameGUI()) );
        ui->_buttonAdd->setEnabled( false);
    }
}

AccountSettings::~AccountSettings()
{
    delete ui;
}

} // namespace Mirall
