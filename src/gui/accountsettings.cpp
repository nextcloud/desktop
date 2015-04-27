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

#include "theme.h"
#include "folderman.h"
#include "folderwizard.h"
#include "folderstatusmodel.h"
#include "utility.h"
#include "application.h"
#include "configfile.h"
#include "account.h"
#include "accountstate.h"
#include "quotainfo.h"
#include "creds/abstractcredentials.h"

#include <math.h>

#include <QDebug>
#include <QDesktopServices>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QAction>
#include <QVBoxLayout>
#include <QTreeView>
#include <QKeySequence>
#include <QIcon>
#include <QVariant>
#include <qstringlistmodel.h>
#include <qpropertyanimation.h>

#include "account.h"

namespace OCC {

static const char progressBarStyleC[] =
        "QProgressBar {"
        "border: 1px solid grey;"
        "border-radius: 5px;"
        "text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "background-color: %1; width: 1px;"
        "}";

AccountSettings::AccountSettings(AccountState *accountState, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AccountSettings),
    _wasDisabledBefore(false),
    _accountState(accountState)
{
    ui->setupUi(this);

    _model = new FolderStatusModel;
    _model->setAccount(_accountState->account());
    _model->setParent(this);
    FolderStatusDelegate *delegate = new FolderStatusDelegate;
    delegate->setParent(this);

    ui->_folderList->header()->hide();
    ui->_folderList->setItemDelegate( delegate );
    ui->_folderList->setModel( _model );
#if defined(Q_OS_MAC)
    ui->_folderList->setMinimumWidth( 400 );
#else
    ui->_folderList->setMinimumWidth( 300 );
#endif
    connect(ui->_folderList, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(slotCustomContextMenuRequested(QPoint)));

    connect(ui->_folderList, SIGNAL(expanded(QModelIndex)) , this, SLOT(refreshSelectiveSyncStatus()));
    connect(ui->_folderList, SIGNAL(collapsed(QModelIndex)) , this, SLOT(refreshSelectiveSyncStatus()));
    connect(_model, SIGNAL(dirtyChanged()), this, SLOT(refreshSelectiveSyncStatus()));
    ui->selectiveSyncStatus->hide();

    QAction *resetFolderAction = new QAction(this);
    resetFolderAction->setShortcut(QKeySequence(Qt::Key_F5));
    connect(resetFolderAction, SIGNAL(triggered()), SLOT(slotResetCurrentFolder()));
    addAction(resetFolderAction);

    QAction *syncNowAction = new QAction(this);
    syncNowAction->setShortcut(QKeySequence(Qt::Key_F6));
    connect(syncNowAction, SIGNAL(triggered()), SLOT(slotSyncCurrentFolderNow()));
    addAction(syncNowAction);

    connect(ui->_folderList, SIGNAL(clicked(QModelIndex)), SLOT(slotFolderActivated(QModelIndex)));
    connect(ui->_folderList, SIGNAL(doubleClicked(QModelIndex)),SLOT(slotDoubleClicked(QModelIndex)));

    connect(ui->selectiveSyncApply, SIGNAL(clicked()), _model, SLOT(slotApplySelectiveSync()));
    connect(ui->selectiveSyncCancel, SIGNAL(clicked()), _model, SLOT(resetFolders()));
    connect(FolderMan::instance(), SIGNAL(folderListLoaded(Folder::Map)), _model, SLOT(resetFolders()));
    connect(this, SIGNAL(folderChanged()), _model, SLOT(resetFolders()));


    QColor color = palette().highlight().color();
    ui->quotaProgressBar->setStyleSheet(QString::fromLatin1(progressBarStyleC).arg(color.name()));
    ui->connectLabel->setWordWrap(true);
    ui->connectLabel->setOpenExternalLinks(true);
    QFont smallFont = ui->quotaInfoLabel->font();
    smallFont.setPointSize(smallFont.pointSize() * 0.8);
    ui->quotaInfoLabel->setFont(smallFont);

    _quotaLabel = new QLabel(ui->quotaProgressBar);
    (new QVBoxLayout(ui->quotaProgressBar))->addWidget(_quotaLabel);

    ui->connectLabel->setText(tr("No account configured."));

    connect(_accountState, SIGNAL(stateChanged(int)), SLOT(slotAccountStateChanged(int)));
    slotAccountStateChanged(_accountState->state());

    QuotaInfo *quotaInfo = _accountState->quotaInfo();
    connect( quotaInfo, SIGNAL(quotaUpdated(qint64,qint64)),
            this, SLOT(slotUpdateQuota(qint64,qint64)));
    slotUpdateQuota(quotaInfo->lastQuotaTotalBytes(), quotaInfo->lastQuotaUsedBytes());

    connect( ProgressDispatcher::instance(), SIGNAL(progressInfo(QString, Progress::Info)),
             this, SLOT(slotSetProgress(QString, Progress::Info)) );

}

void AccountSettings::slotCustomContextMenuRequested(const QPoint &pos)
{
    QTreeView *tv = ui->_folderList;
    QModelIndex index = tv->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    QString alias = _model->data( index, FolderStatusDelegate::FolderAliasRole ).toString();
    if (alias.isEmpty()) {
        return;
    }

    tv->setCurrentIndex(index);
    bool folderPaused = _model->data( index, FolderStatusDelegate::FolderSyncPaused).toBool();

    QMenu *menu = new QMenu(tv);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("Remove folder")), SIGNAL(triggered(bool)),
            this, SLOT(slotRemoveCurrentFolder()));
    connect(menu->addAction(folderPaused ? tr("Resume") : tr("Pause")), SIGNAL(triggered(bool)),
            this, SLOT(slotEnableCurrentFolder()));
    menu->exec(tv->mapToGlobal(pos));
}

void AccountSettings::slotFolderActivated( const QModelIndex& indx )
{
    if (indx.data(FolderStatusDelegate::AddButton).toBool()) {
        slotAddFolder();
        return;
    }
}

void AccountSettings::slotAddFolder()
{
    FolderMan *folderMan = FolderMan::instance();
    folderMan->setSyncEnabled(false); // do not start more syncs.

    FolderWizard *folderWizard = new FolderWizard(_accountState->account(), this);

    connect(folderWizard, SIGNAL(accepted()), SLOT(slotFolderWizardAccepted()));
    connect(folderWizard, SIGNAL(rejected()), SLOT(slotFolderWizardRejected()));
    folderWizard->open();
}


void AccountSettings::slotFolderWizardAccepted()
{
    FolderWizard *folderWizard = qobject_cast<FolderWizard*>(sender());
    FolderMan *folderMan = FolderMan::instance();

    qDebug() << "* Folder wizard completed";

    FolderDefinition definition;
    definition.alias        = folderWizard->field(QLatin1String("alias")).toString();
    definition.localPath    = folderWizard->field(QLatin1String("sourceFolder")).toString();
    definition.targetPath   = folderWizard->property("targetPath").toString();
    definition.selectiveSyncBlackList = folderWizard->property("selectiveSyncBlackList").toStringList();

    Folder *f = folderMan->addFolder(_accountState, definition);
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

void AccountSettings::setGeneralErrors( const QStringList& errors )
{
    _generalErrors = errors;
    if (_accountState) {
        // this will update the message
        slotAccountStateChanged(_accountState->state());
    }
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

            FolderMan *folderMan = FolderMan::instance();
            folderMan->slotRemoveFolder( alias );
            _model->removeRow(row);

            // single folder fix to show add-button and hide remove-button

            emit folderChanged();
        }
    }
}

void AccountSettings::slotResetCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();
    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();
        if (alias.isEmpty())
            return;
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
    if (alias.isEmpty()) return;

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
    ui->accountStatus->setVisible(!message.isEmpty());
}

void AccountSettings::slotEnableCurrentFolder()
{
    QModelIndex selected = ui->_folderList->selectionModel()->currentIndex();

    if( selected.isValid() ) {
        QString alias = _model->data( selected, FolderStatusDelegate::FolderAliasRole ).toString();

        if( alias.isEmpty() ) {
            qDebug() << "Empty alias to enable.";
            return;
        }

        FolderMan *folderMan = FolderMan::instance();

        qDebug() << "Application: enable folder with alias " << alias;
        bool terminate = false;
        bool currentlyPaused = false;

        // this sets the folder status to disabled but does not interrupt it.
        Folder *f = folderMan->folder( alias );
        if (!f) {
            return;
        }
        currentlyPaused = f->syncPaused();
        if( ! currentlyPaused ) {
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
        f->setSyncPaused(!currentlyPaused); // toggle the pause setting
        folderMan->slotSetFolderPaused( alias, !currentlyPaused );

        // keep state for the icon setting.
        if( currentlyPaused ) _wasDisabledBefore = true;

        slotUpdateFolderState (f);
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
    if( ! folder ) return;

    auto folderList = FolderMan::instance()->map().values();
    auto folderIndex = folderList.indexOf(folder);
    if (folderIndex < 0) { return; }
    emit _model->dataChanged(_model->index(folderIndex), _model->index(folderIndex),
                             QVector<int>() << FolderStatusDelegate::AddProgressSpace);
}

void AccountSettings::slotOpenOC()
{
  if( _OCUrl.isValid() )
    QDesktopServices::openUrl( _OCUrl );
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

void AccountSettings::slotSetProgress(const QString& folder, const ProgressInfo &progress )
{
    if (!isVisible()) {
        return; // for https://github.com/owncloud/client/issues/2648#issuecomment-71377909
    }

    Folder *f = FolderMan::instance()->folder(folder);
    if( !f ) { return; }

    auto folderList = FolderMan::instance()->map().values();
    auto folderIndex = folderList.indexOf(f);
    if (folderIndex < 0) { return; }

    if (_model->_progresses.size() <= folderIndex) {
        _model->_progresses.resize(folderIndex + 1);
    }
    FolderStatusModel::ProgressInfo *progressInfo = &_model->_progresses[folderIndex];

    QVector<int> roles;
    roles << FolderStatusDelegate::AddProgressSpace << FolderStatusDelegate::SyncProgressItemString
        << FolderStatusDelegate::WarningCount;

    if (!progress._currentDiscoveredFolder.isEmpty()) {
        progressInfo->_progressString = tr("Discovering '%1'").arg(progress._currentDiscoveredFolder);
        emit _model->dataChanged(_model->index(folderIndex), _model->index(folderIndex), roles);
        return;
    }

    if(!progress._lastCompletedItem.isEmpty()
            && Progress::isWarningKind(progress._lastCompletedItem._status)) {
        progressInfo->_warningCount++;
    }

    // find the single item to display:  This is going to be the bigger item, or the last completed
    // item if no items are in progress.
    SyncFileItem curItem = progress._lastCompletedItem;
    qint64 curItemProgress = -1; // -1 means finished
    quint64 biggerItemSize = -1;
    foreach(const ProgressInfo::ProgressItem &citm, progress._currentItems) {
        if (curItemProgress == -1 || (ProgressInfo::isSizeDependent(citm._item)
                                      && biggerItemSize < citm._item._size)) {
            curItemProgress = citm._progress.completed();
            curItem = citm._item;
            biggerItemSize = citm._item._size;
        }
    }
    if (curItemProgress == -1) {
        curItemProgress = curItem._size;
    }

    QString itemFileName = shortenFilename(folder, curItem._file);
    QString kindString = Progress::asActionString(curItem);



    QString fileProgressString;
    if (ProgressInfo::isSizeDependent(curItem)) {
        QString s1 = Utility::octetsToString( curItemProgress );
        QString s2 = Utility::octetsToString( curItem._size );
        quint64 estimatedBw = progress.fileProgress(curItem).estimatedBandwidth;
        if (estimatedBw) {
            //: Example text: "uploading foobar.png (1MB of 2MB) time left 2 minutes at a rate of 24Kb/s"
            fileProgressString = tr("%1 %2 (%3 of %4) %5 left at a rate of %6/s")
                .arg(kindString, itemFileName, s1, s2,
                    Utility::timeToDescriptiveString(progress.fileProgress(curItem).estimatedEta, 3, " ", true),
                    Utility::octetsToString(estimatedBw) );
        } else {
            //: Example text: "uploading foobar.png (2MB of 2MB)"
            fileProgressString = tr("%1 %2 (%3 of %4)") .arg(kindString, itemFileName, s1, s2);
        }
    } else if (!kindString.isEmpty()) {
        //: Example text: "uploading foobar.png"
        fileProgressString = tr("%1 %2").arg(kindString, itemFileName);
    }
    progressInfo->_progressString = fileProgressString;

    // overall progress
    quint64 completedSize = progress.completedSize();
    quint64 completedFile = progress.completedFiles();
    quint64 currentFile = progress.currentFile();
    if (currentFile == ULLONG_MAX)
        currentFile = 0;
    quint64 totalSize = qMax(completedSize, progress.totalSize());
    quint64 totalFileCount = qMax(currentFile, progress.totalFiles());
    QString overallSyncString;
    if (totalSize > 0) {
        QString s1 = Utility::octetsToString( completedSize );
        QString s2 = Utility::octetsToString( totalSize );
        overallSyncString = tr("%1 of %2, file %3 of %4\nTotal time left %5")
            .arg(s1, s2)
            .arg(currentFile).arg(totalFileCount)
            .arg( Utility::timeToDescriptiveString(progress.totalProgress().estimatedEta, 3, " ", true) );
    } else if (totalFileCount > 0) {
        // Don't attemt to estimate the time left if there is no kb to transfer.
        overallSyncString = tr("file %1 of %2") .arg(currentFile).arg(totalFileCount);
    }

    progressInfo->_overallSyncString =  overallSyncString;

    int overallPercent = 0;
    if( totalFileCount > 0 ) {
        // Add one 'byte' for each files so the percentage is moving when deleting or renaming files
        overallPercent = qRound(double(completedSize + completedFile)/double(totalSize + totalFileCount) * 100.0);
    }
    progressInfo->_overallPercent = qBound(0, overallPercent, 100);
    emit _model->dataChanged(_model->index(folderIndex), _model->index(folderIndex), roles);
}

void AccountSettings::slotHideProgress()
{
    auto folderIndex = sender()->property("owncloud_folderIndex").toInt();
    if (folderIndex < 0) { return; }

    if (_model->_progresses.size() <= folderIndex) {
        return;
    }

    _model->_progresses[folderIndex] = FolderStatusModel::ProgressInfo();
    emit _model->dataChanged(_model->index(folderIndex), _model->index(folderIndex),
                             QVector<int>() << FolderStatusDelegate::AddProgressSpace);
}

void AccountSettings::slotFolderSyncStateChange()
{
    Folder* folder = qobject_cast<Folder *>(sender());
    if (!folder) return;
    auto folderList = FolderMan::instance()->map().values();
    auto folderIndex = folderList.indexOf(folder);
    if (folderIndex < 0) { return; }

    SyncResult::Status state = folder->syncResult().status();
    if (state == SyncResult::SyncPrepare)  {
        if (_model->_progresses.size() > folderIndex) {
            _model->_progresses[folderIndex] = FolderStatusModel::ProgressInfo();
        }
    } else if (state == SyncResult::Success || state == SyncResult::Problem) {
        // start a timer to stop the progress display
        QTimer *timer;
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(slotHideProgress()));
        connect(timer, SIGNAL(timeout()), timer, SLOT(deleteLater()));
        timer->setSingleShot(true);
        timer->setProperty("owncloud_folderIndex", folderIndex);
        timer->start(5000);
    }
}

void AccountSettings::slotUpdateQuota(qint64 total, qint64 used)
{
    if( total > 0 ) {
        ui->storageGroupBox->setVisible(true);
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
        _quotaLabel->setText(tr("%1 (%3%) of %2 server space in use.").arg(usedStr, totalStr, percentStr));
    } else {
        ui->storageGroupBox->setVisible(false);
        ui->quotaInfoLabel->setVisible(false);
        ui->quotaProgressBar->setMaximum(0);
        _quotaLabel->setText(tr("Currently there is no storage usage information available."));
    }
}

void AccountSettings::slotAccountStateChanged(int state)
{
    if (_accountState) {
        ui->sslButton->updateAccountState(_accountState);
        AccountPtr account = _accountState->account();
        QUrl safeUrl(account->url());
        safeUrl.setPassword(QString()); // Remove the password from the URL to avoid showing it in the UI
        FolderMan *folderMan = FolderMan::instance();
        foreach (Folder *folder, folderMan->map().values()) {
            slotUpdateFolderState(folder);
        }
        if (state == AccountState::Connected || state == AccountState::ServerMaintenance) {
            QString user;
            if (AbstractCredentials *cred = account->credentials()) {
               user = cred->user();
            }
            if (user.isEmpty()) {
                showConnectionLabel( tr("Connected to <a href=\"%1\">%2</a>.").arg(account->url().toString(), safeUrl.toString())
                                 /*, tr("Version: %1 (%2)").arg(versionStr).arg(version) */ );
            } else {
                showConnectionLabel( tr("Connected to <a href=\"%1\">%2</a> as <i>%3</i>.").arg(account->url().toString(), safeUrl.toString(), user)
                                 /*, tr("Version: %1 (%2)").arg(versionStr).arg(version) */ );
            }
        } else {
            showConnectionLabel( tr("No connection to %1 at <a href=\"%2\">%3</a>.")
                                 .arg(Theme::instance()->appNameGUI(),
                                      account->url().toString(),
                                      safeUrl.toString()) );
        }
    } else {
        // ownCloud is not yet configured.
        showConnectionLabel( tr("No %1 connection configured.").arg(Theme::instance()->appNameGUI()) );
    }
}

AccountSettings::~AccountSettings()
{
    delete ui;
}

void AccountSettings::refreshSelectiveSyncStatus()
{
    ui->selectiveSyncApply->setEnabled(_model->isDirty());
    ui->selectiveSyncCancel->setEnabled(_model->isDirty());
    bool shouldBeVisible = _model->isDirty();
    for (int i = 0; !shouldBeVisible && i < _model->rowCount(); ++i) {
        if (ui->_folderList->isExpanded(_model->index(i)))
            shouldBeVisible = true;
    }
    bool wasVisible = ui->selectiveSyncApply->isVisible();
    if (wasVisible != shouldBeVisible) {
        QSize hint = ui->selectiveSyncStatus->sizeHint();
        if (shouldBeVisible) {
            ui->selectiveSyncStatus->setMaximumHeight(0);
            ui->selectiveSyncStatus->setVisible(true);
        }
        auto anim = new QPropertyAnimation(ui->selectiveSyncStatus, "maximumHeight", ui->selectiveSyncStatus);
        anim->setEndValue(shouldBeVisible ? hint.height() : 0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
        if (!shouldBeVisible) {
            connect(anim, SIGNAL(finished()), ui->selectiveSyncStatus, SLOT(hide()));
        }
    }
}

} // namespace OCC
