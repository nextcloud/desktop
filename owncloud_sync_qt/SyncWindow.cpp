/******************************************************************************
 *    Copyright 2011 Juan Carlos Cornejo jc2@paintblack.com
 *
 *    This file is part of owncloud_sync_qt.
 *
 *    owncloud_sync is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    owncloud_sync is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with owncloud_sync.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "SyncGlobal.h"
#include "SyncWindow.h"
#include "ui_SyncWindow.h"
#include "sqlite3_util.h"
#include "QWebDAV.h"
#include "OwnCloudSync.h"
#include "OwnPasswordManager.h"

#include <QFile>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QFileSystemWatcher>
#include <QFileDialog>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QCheckBox>
#include <QSignalMapper>
#include <QPushButton>
#include <QListView>
#include <QStringListModel>
#include <QMessageBox>
#include <QCloseEvent>
#include <QMenu>
#include <QSettings>

SyncWindow::SyncWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SyncWindow)
{
    hide();
    mProcessedPasswordManager = false;
    mSharedFilters = new QSet<QString>();
    mIncludedFilters = g_GetIncludedFilterList();
    mQuitAction = false;
    mBusy = false;
    ui->setupUi(this);
    setWindowTitle("OwnCloud Sync");
    mEditingConfig = -1;
    mConflictsExist = false;
    loadApplicationSettings();

    // Before anything else, connect to the SyncDebug
    connect(getSyncDebug(),SIGNAL(debugMessage(const QString)),
            this,SLOT(processDebugMessage(const QString)));

    mCurrentAccountEdit = 0;
    mTotalSyncs = 0;

    // Setup icons
    mDefaultIcon = QIcon(":images/owncloud.png");
    mSyncIcon = QIcon(":images/owncloud_sync.png");
    mDefaultConflictIcon = QIcon(":images/owncloud_conflict.png");
    mSyncConflictIcon = QIcon(":images/owncloud_sync_conflict.png");
    setWindowIcon(mDefaultIcon);

    // Add the tray, if available
    mSystemTray = new QSystemTrayIcon(this);
    mSystemTrayMenu = new QMenu(this);
    mSystemTrayMenu->addAction(ui->action_Quit);
    mSystemTray->setContextMenu(mSystemTrayMenu);
    mSystemTray->setIcon(mDefaultIcon);
    mSystemTray->setToolTip(tr("OwnCloud Sync Version %1").arg(_OCS_VERSION));
    mSystemTray->show();
    connect(mSystemTray,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this,SLOT(systemTrayActivated(QSystemTrayIcon::ActivationReason)));

    // Start with the default tab
    ui->stackedWidget->setCurrentIndex(0);
    ui->listFilterView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->listIncludedFilterView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->listGlobalFilterView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Create the Accounts SignalMapper
    mAccountsSignalMapper = new QSignalMapper(this);
    connect(mAccountsSignalMapper, SIGNAL(mapped(int)), this,
            SLOT(slotAccountsSignalMapper(int)));

    mConfigDirectory = QDir::toNativeSeparators(QDir::homePath());
#ifdef Q_OS_LINUX
    // In linux, we will store all databases in
    // $HOME/.local/share/data/owncloud_sync
    mConfigDirectory += QDir::toNativeSeparators("/.local/share/data/");
#endif
#ifdef Q_OS_WIN
    mConfigDirectory += QDir::toNativeSeparators("/AppData/Local/");
#endif
    mConfigDirectory += "owncloud_sync";
    QDir configDir(mConfigDirectory);
    configDir.mkpath(mConfigDirectory);
    QDir logsDir(QDir::toNativeSeparators(mConfigDirectory+"/logs"));
    logsDir.mkpath(QDir::toNativeSeparators(mConfigDirectory+"/logs"));
    importGlobalFilters(true);
    updateSharedFilterList();

    // Get the password manager
    mPasswordManager = new OwnPasswordManager(this,winId());
    connect(mPasswordManager,SIGNAL(managerReady()),
            this,SLOT(passwordManagerReady()));

    // Check if the passwordManager is already ready
    if(mPasswordManager->isReady() && !mProcessedPasswordManager) {
        passwordManagerReady();
    }
}

void SyncWindow::passwordManagerReady()
{
    mProcessedPasswordManager = true;
    // Look for accounts that already exist
    QDir configDir(mConfigDirectory);
    QStringList filters;
    filters << "*.db";
    QStringList files = configDir.entryList(filters);
    for( int i = 0; i < files.size(); i++ ) {
        QString name = files[i].replace(".db","");
        addAccount(name);
        syncDebug() << "Found account: " << name;
    }
    if( files.size() == 0 ) {
        on_buttonNewAccount_clicked();
    }
    rebuildAccountsTable();

    updateStatus();
    ui->actionEnable_Delete_Account->setVisible(false);

    if( mAccounts.size() > 0 && mHideOnStart ) {
        hide();
    } else {
        show();
    }
}

void SyncWindow::updateStatus()
{
    //if( !isVisible() )
    //    return;

    if( !mBusy ) {
        ui->statusBar->showMessage(tr("Version %1: Waiting...").arg(_OCS_VERSION));
        ui->status->setText(tr("Waiting..."));
        ui->progressFile->setValue(0);
        ui->progressTotal->setValue(0);
        if(mConflictsExist) {
             ui->labelImage->setPixmap(mDefaultConflictIcon.pixmap(129,129));
             mSystemTray->setIcon(mDefaultConflictIcon);
        } else {
            ui->labelImage->setPixmap(mDefaultIcon.pixmap(129,129));
            mSystemTray->setIcon(mDefaultIcon);
        }
    } else {
        ui->status->setText(mTransferState+mCurrentFile+" out of "
                            + QString("%1").arg(mCurrentFileSize));
        if(mConflictsExist) {
            mSystemTray->setIcon(mSyncConflictIcon);
            ui->labelImage->setPixmap(mSyncConflictIcon.pixmap(129,129));
        } else {
            mSystemTray->setIcon(mSyncIcon);
            ui->labelImage->setPixmap(mSyncIcon.pixmap(129,129));
        }
    }
}

SyncWindow::~SyncWindow()
{
    delete ui;
    delete mSystemTray;
    delete mSystemTrayMenu;
    delete mAccountsSignalMapper;
    mAccounts.clear();
}

void SyncWindow::saveLogs()
{
    QString name =
            QDateTime::currentDateTime().toString("yyyyMMdd_hh_mm_ss.log");
    QFile file(QDir::toNativeSeparators(mConfigDirectory+"/logs/"+name));
    if( !file.open(QIODevice::WriteOnly)) {
        syncDebug() << "Could not open log file for writting!"
                       << QDir::toNativeSeparators(mConfigDirectory+"/logs/"+name)
                       << file.errorString();
        return;
    }

    QTextStream out(&file);
    out << ui->textBrowser->toPlainText();
    out.flush();
    file.close();
    ui->textBrowser->clear();

}

void SyncWindow::transferProgress(qint64 current, qint64 total)
{
    // First update the current file progress bar
    qint64 percent;
    if ( total > 0 ) {
        percent = 100*current/total;
        ui->progressFile->setValue(percent);
    } else {
        percent = 0;
    }

    // Then update the total progress bar
    qint64 additional = (mCurrentFileSize*percent)/100;
    if (mTotalToTransfer > 0) {
        ui->progressTotal->setValue(100*(mTotalTransfered+additional)
                                    /mTotalToTransfer);
    }
}

void SyncWindow::systemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if( reason == QSystemTrayIcon::Trigger ) {
        // Just toggle this window's display!
        if(isVisible()) {
            hide();
        } else {
            show();
        }
    }
}

OwnCloudSync* SyncWindow::addAccount(QString name)
{
    OwnCloudSync *account = new OwnCloudSync(name,mPasswordManager,
                                             mSharedFilters,mConfigDirectory);
    mAccounts.append(account);
    mAccountNames.append(name);

    // Connect the signals
    connect(account,SIGNAL(conflictExists(OwnCloudSync*)),
            this,SLOT(slotConflictExists(OwnCloudSync*)));
    connect(account,SIGNAL(conflictResolved(OwnCloudSync*)),
            this,SLOT(slotConflictResolved(OwnCloudSync*)));
    connect(account,SIGNAL(progressFile(qint64)),
            this,SLOT(slotProgressFile(qint64)));
    connect(account,SIGNAL(progressTotal(qint64)),
            this,SLOT(slotProgressTotal(qint64)));
    connect(account,SIGNAL(readyToSync(OwnCloudSync*)),
            this,SLOT(slotReadyToSync(OwnCloudSync*)));
    connect(account,SIGNAL(toLog(QString)),
            this,SLOT(slotToLog(QString)));
    connect(account,SIGNAL(toStatus(QString)),
            this,SLOT(slotToStatus(QString)));
    connect(account,SIGNAL(finishedSync(OwnCloudSync*)),
            this,SLOT(slotFinishedSync(OwnCloudSync*)));
    connect(account,SIGNAL(toMessage(QString,QString,
                                     QSystemTrayIcon::MessageIcon)),
            this,SLOT(slotToMessage(QString,QString,
                                    QSystemTrayIcon::MessageIcon)));
    return account;
}

void SyncWindow::on_buttonSave_clicked()
{
    QFileInfo info(mConfigDirectory+"/"+ui->lineName->text()+".db");
    bool okToEdit = true;
    ui->buttonSave->setDisabled(true);
    QString host = ui->lineHost->text();
    host.replace("http://","");
    host.replace("https://","");
    host.replace("webdav://","");
    host.replace("webdavs://","");
    // clean up double or trailing slashes..
    QString remoteDir = QDir::cleanPath(ui->lineRemoteDir->text());
    QString localDir = QDir::cleanPath(ui->lineLocalDir->text());
    if( mEditingConfig >= 0 ) { // Editing an account
        // If we are renaming the account, make sure that the new name
        // does not already exist
        if( ui->lineName->text() != mAccounts[mEditingConfig]->getName() ) {
            if( info.exists() ) {
                syncDebug() << "New account name already taken!!";
                okToEdit = false;
            }
        }
        if( okToEdit ) {
            mAccounts[mEditingConfig]->initialize(
                        ui->labelHttp->text()+host,
                        ui->lineUser->text(),
                        ui->linePassword->text(),
                        remoteDir,
                        localDir,
                        ui->time->value());
        }
    } else { // New account
        // First, check to see if this name is already taken
        if(info.exists()) { // Account name taken!
            syncDebug() << "Account name already taken!!";
            ui->lineName->setFocus();
        } else { // Good, create a new account
            OwnCloudSync *account = addAccount(ui->lineName->text());
            account->initialize(ui->labelHttp->text()+host,
                                ui->lineUser->text(),
                                ui->linePassword->text(),
                                remoteDir,
                                localDir,
                                ui->time->value());
        }
    }
    mEditingConfig = -1;
    ui->stackedWidget->setCurrentIndex(0);
    ui->actionEnable_Delete_Account->setVisible(false);
    rebuildAccountsTable();
}

void SyncWindow::closeEvent(QCloseEvent *event)
{
    if(mQuitAction || !mHideOnClose) {
        // Ask the user for confirmation before closing!
        QMessageBox box(this);
        box.setText(tr("Are you sure you want to quit? "
                       "Program will not quit until all required syncs are made."));
        box.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
        if( box.exec() == QMessageBox::No ) {
            event->ignore();
            mQuitAction = false;
            return;
        }
                exportGlobalFilters(true);

        // We definitely don't want to quit when we are synchronizing!
        for( int i = 0; i < mAccounts.size(); i++ ) {
            mAccounts[i]->deleteWatcher();    // Delete the file watcher
            mAccounts[i]->stop(); // Stop the counter
        }

//        for( int i = 0; i < mAccounts.size(); i++ ) {
//            while( mAccounts[i]->needsSync() ) {
//                //syncDebug() << "Waiting for " + mAccounts[i]->getName()
//                //            + " to sync before close.";
//                QCoreApplication::processEvents();
//            }
//        }


        // Before closing, save the database!!!
        for( int i = 0; i < mAccounts.size(); i++ ) {
            mAccounts[i]->saveConfigToDB();
            mAccounts[i]->saveDBToFile();
        }

        saveLogs();
        saveApplicationSettings();
        syncDebug() << "All ready to close!";
        QMainWindow::closeEvent(event);
    } else {
        if(mSystemTray->isVisible()) {
            hide();
            event->ignore();
        }
    }
}

void SyncWindow::on_buttonSyncDir_clicked()
{
    QString syncDir = QFileDialog::getExistingDirectory(this);
    if( syncDir != "" ) {
        ui->lineLocalDir->setText(syncDir);
        ui->buttonSave->setEnabled(true);
    }
}

void SyncWindow::on_linePassword_textEdited(QString text)
{
    ui->buttonSave->setEnabled(true);
}

void SyncWindow::on_lineHost_textEdited(QString text)
{
    ui->buttonSave->setEnabled(true);
}

void SyncWindow::on_lineLocalDir_textEdited(QString text)
{
    ui->buttonSave->setEnabled(true);
}

void SyncWindow::on_lineRemoteDir_textEdited(QString text)
{
    ui->buttonSave->setEnabled(true);
}

void SyncWindow::on_lineUser_textEdited(QString text)
{
    ui->buttonSave->setEnabled(true);
}

void SyncWindow::on_time_valueChanged(int value)
{
    ui->buttonSave->setEnabled(true);
}

void SyncWindow::on_checkBoxHostnameEncryption_clicked()
{
   ui->buttonSave->setEnabled(true);
}

void SyncWindow::on_conflict_clicked()
{
    // Clear the table
    ui->tableConflict->clear();
    QTableWidgetItem *account;
    QTableWidgetItem *name;
    QTableWidgetItem *serverTime;
    QTableWidgetItem *localTime;
    QComboBox *combo;
    int row = 0;
    QStringList headers;
    headers.append(tr("Account"));
    headers.append(tr("File Name"));
    headers.append(tr("Server Modified"));
    headers.append(tr("Local Modifed"));
    headers.append(tr("Which wins?"));
    ui->tableConflict->setHorizontalHeaderLabels(headers);
    for( int i = 0; i < mAccounts.size(); i++ ) {
        QSqlQuery query = mAccounts[i]->getConflicts();
        query.exec("SELECT * from conflicts;");
        while( query.next() ) {
            ui->tableConflict->setRowCount(row+1);
            account = new QTableWidgetItem(mAccounts[i]->getName());
            name = new QTableWidgetItem(query.value(0).toString());
            serverTime = new QTableWidgetItem(query.value(2).toString());
            localTime = new QTableWidgetItem(query.value(3).toString());
            combo = new QComboBox(ui->tableConflict);
            combo->addItem(tr("Choose:"));
            combo->addItem("server");
            combo->addItem("local");
            ui->tableConflict->setItem(row,0, account);
            ui->tableConflict->setItem(row, 1, name);
            ui->tableConflict->setItem(row, 2, serverTime);
            ui->tableConflict->setItem(row, 3, localTime);
            ui->tableConflict->setCellWidget(row,4,combo);
            row++;
        }
        ui->tableConflict->resizeColumnsToContents();
        ui->tableConflict->horizontalHeader()->setStretchLastSection(true);
        ui->stackedWidget->setCurrentIndex(1);
    }

}

OwnCloudSync* SyncWindow::getAccount(QString name)
{
    // Get the account by looping through the list until we find the index
    for( int i = 0; i < mAccountNames.size(); i++ ) {
        if( mAccountNames[i] == name )
            return mAccounts[i];
    }
    return 0;
}

void SyncWindow::on_buttonBox_accepted()
{
    bool allConflictsResolved = true;
    for( int row = 0; row < ui->tableConflict->rowCount(); row++ ) {
        QComboBox *combo = (QComboBox*)ui->tableConflict->cellWidget(row,4);
        if( combo->currentIndex() == 0 ) {
            allConflictsResolved = false;
            continue; // This conflict has not been resolved
        }
        getAccount(ui->tableConflict->takeItem(row,0)->text())->
                processFileConflict(ui->tableConflict->takeItem(row,1)->text(),
                                    combo->currentText());
    }
    if( allConflictsResolved) {
        ui->conflict->setEnabled(false);
        mConflictsExist = false;
        updateStatus();
    }
    ui->stackedWidget->setCurrentIndex(0);
}

void SyncWindow::on_buttonBox_rejected()
{
    //mSyncTimer->start(mUpdateTime);
    ui->stackedWidget->setCurrentIndex(0);
}

void SyncWindow::on_lineName_textEdited(QString text)
{
    ui->buttonSave->setEnabled(true);
}

void SyncWindow::rebuildAccountsTable()
{
    // Clear the table
    ui->tableAccounts->clear();
    QTableWidgetItem *name;
    QCheckBox *checkbox;
    QPushButton *button;
    QTableWidgetItem *lastSync;
    QStringList headers;
    headers.append(tr("Name"));
    headers.append(tr("Enabled"));
    headers.append(tr("Last Sync"));
    ui->tableAccounts->setHorizontalHeaderLabels(headers);

    ui->tableAccounts->setRowCount(mAccounts.size());
    for( int row = 0; row < mAccountNames.size(); row++ ) {
        button = new QPushButton(mAccounts[row]->getName());
        checkbox = new QCheckBox();
        checkbox->setCheckState(mAccounts[row]->isEnabled()?Qt::Checked :
                                                           Qt::Unchecked);
        lastSync = new QTableWidgetItem(mAccounts[row]->getLastSync());
        lastSync->setFlags(Qt::ItemIsEnabled);
        ui->tableAccounts->setCellWidget(row,0,button);
        ui->tableAccounts->setCellWidget(row,1,checkbox);
        ui->tableAccounts->setItem(row,2,lastSync);
        connect(checkbox, SIGNAL(stateChanged(int)),
                mAccountsSignalMapper, SLOT(map()));
        connect(button,SIGNAL(clicked()),
                mAccountsSignalMapper,SLOT(map()));
        mAccountsSignalMapper->setMapping(checkbox, row+1);
        mAccountsSignalMapper->setMapping(button,-row-1);
    }
    ui->tableAccounts->resizeColumnsToContents();
    ui->tableAccounts->horizontalHeader()->setStretchLastSection(true);
    ui->tableAccounts->setShowGrid(false);
}

void SyncWindow::accountEnabledChanged(int row)
{
    mAccounts[row]->setEnabled(!mAccounts[row]->isEnabled());
}
void SyncWindow::slotConflictExists(OwnCloudSync* oc)
{
    ui->conflict->setEnabled(true);
    mConflictsExist = true;
    updateStatus();
}

void SyncWindow::slotConflictResolved(OwnCloudSync* oc)
{

}

void SyncWindow::slotProgressFile(qint64 value)
{
    ui->progressFile->setValue(value);
}

void SyncWindow::slotProgressTotal(qint64 value)
{
    ui->progressTotal->setValue(value);
}

void SyncWindow::on_buttonCancel_clicked()
{
    mEditingConfig = -1;
    ui->stackedWidget->setCurrentIndex(0);
    ui->actionEnable_Delete_Account->setVisible(false);
}

void SyncWindow::slotReadyToSync(OwnCloudSync* oc)
{
    mAccountsReadyToSync.enqueue(oc);
    syncDebug() << oc->getName() << " is ready to sync!";
    if(!mBusy) {
        processNextStep();
    }
}

void SyncWindow::slotToLog(QString text)
{
    ui->textBrowser->append(text);
}

void SyncWindow::slotToStatus(QString text) {
    ui->status->setText(text);
}

void SyncWindow::processNextStep()
{
    if( mAccountsReadyToSync.size() != 0 ) {
        mBusy = true;
        mTotalSyncs++;
        OwnCloudSync *account = mAccountsReadyToSync.dequeue();
        ui->statusBar->showMessage(tr("Version %1: Synchronizing %2")
                                   .arg(_OCS_VERSION).arg(account->getName()));
        account->sync();
    } else {
        mBusy = false;
        updateStatus();
    }
    if(mTotalSyncs%mSaveLogCounter == 0 ) {
        saveLogs();
    }
}

void SyncWindow::slotFinishedSync(OwnCloudSync *oc)
{
    syncDebug() << oc->getName() << " just finishied.";
    rebuildAccountsTable();
    processNextStep();
}

void SyncWindow::slotToMessage(QString caption, QString body,
                               QSystemTrayIcon::MessageIcon icon)
{
    mSystemTray->showMessage(caption, body, icon);
}

void SyncWindow::slotAccountsSignalMapper(int row)
{
    // The actual row number is offset by 1
    if( row < 0 ) { // Button presses
        editConfig(-row-1);
    } else if ( row > 0 ) { // Checkbox presses
        accountEnabledChanged(row-1);
    }
}

void SyncWindow::editConfig(int row)
{
    QString host = mAccounts[row]->getHost();
    if(host.contains("https://")) {
        ui->checkBoxHostnameEncryption->setChecked(true);
    }
    host.replace("https://","");
    host.replace("http://","");
    mEditingConfig = row;
    // Switch to the account editing widget
    ui->stackedWidget->setCurrentIndex(2);
    ui->lineName->setText(mAccounts[row]->getName());
    ui->lineHost->setText(host);
    ui->lineUser->setText(mAccounts[row]->getUserName());
    ui->linePassword->setText(mAccounts[row]->getPassword());
    ui->lineRemoteDir->setText(mAccounts[row]->getRemoteDirectory());
    ui->lineLocalDir->setText(mAccounts[row]->getLocalDirectory());
    ui->time->setValue(mAccounts[row]->getUpdateTime());
    ui->buttonDeleteAccount->setEnabled(false);
    ui->actionEnable_Delete_Account->setVisible(true);
    listFilters(row);
    ui->frameFilter->setEnabled(true);
}

void SyncWindow::listFilters(int row)
{
    if(row<0) {
        // Show an empty filter list
        ui->listFilterView->setModel(
                   new QStringListModel());
    } else {
        // Show the filter list
        ui->listFilterView->setModel(
                   new QStringListModel(mAccounts[row]->getFilterList()));
        // Create the filterView signals
        connect(ui->listFilterView->selectionModel(),
               SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                      this,SLOT(listFiltersSelectionChanged(QItemSelection,
                                                       QItemSelection)));
        ui->lineFilter->setText("");
        ui->buttonFilterInsert->setEnabled(false);
        ui->buttonFilterRemove->setEnabled(false);
    }
}

void SyncWindow::on_buttonNewAccount_clicked()
{
    mEditingConfig = -1;

    // Switch to the account editing widget
    ui->stackedWidget->setCurrentIndex(2);
    ui->lineName->setText("");
    ui->lineHost->setText("");
    ui->lineUser->setText("");
    ui->linePassword->setText("");
    ui->lineRemoteDir->setText("");
    ui->lineLocalDir->setText("");
    ui->buttonDeleteAccount->setEnabled(false);
    ui->actionEnable_Delete_Account->setVisible(false);
    ui->frameFilter->setEnabled(false);
    ui->time->setValue(15);
    listFilters(mEditingConfig);
}

void SyncWindow::on_lineFilter_textEdited(QString text)
{
    if(text == "") {
        ui->buttonFilterInsert->setDisabled(true);
    } else {
        ui->buttonFilterInsert->setEnabled(true);
    }

}

void SyncWindow::listFiltersSelectionChanged(QItemSelection selected,
                                             QItemSelection deselected)
{
    ui->buttonFilterRemove->setEnabled(true);
    //syncDebug() << "Selected: " << selected.indexes()[0].row();
}

void SyncWindow::on_buttonFilterRemove_clicked()
{
    int index = ui->listFilterView->selectionModel()
                ->selection().indexes()[0].row();
    syncDebug() << "Will remove: " << ui->listFilterView->model()->index(index,0)
                .data(Qt::DisplayRole ).toString();
    mAccounts[mEditingConfig]->removeFilter(
                ui->listFilterView->model()->index(index,0)
                                            .data(Qt::DisplayRole ).toString());
    listFilters(mEditingConfig);
}

void SyncWindow::on_buttonFilterInsert_clicked()
{
    mAccounts[mEditingConfig]->addFilter(
                ui->lineFilter->text());
    listFilters(mEditingConfig);
}

void SyncWindow::on_action_Quit_triggered()
{
    mQuitAction = true;
    close();
}

void SyncWindow::saveApplicationSettings()
{
    QSettings settings("paintblack.com","OwnCloud Sync");
    settings.beginGroup("SyncWindow");
    settings.setValue("hide_on_start",mHideOnStart);
    settings.setValue("hide_when_closed",mHideOnClose);
    settings.setValue("display_debug",mDisplayDebug);
    settings.setValue("save_log_count",mSaveLogCounter);
    settings.setValue("save_db_time",mSaveDBTime);
    settings.setValue("last_run_version",_OCS_VERSION);
    settings.endGroup();
    settings.beginGroup("DisabledIncludedFilters");
    for(int i = 0; i < mIncludedFilters.size(); i++ ) {
        if( !mIncludedFilters[i].enabled ) {
            settings.setValue(mIncludedFilters[i].name,true);
        } else {
            settings.remove(mIncludedFilters[i].name);
        }
    }
    settings.endGroup();
}

void SyncWindow::loadApplicationSettings()
{
    QSettings settings("paintblack.com","OwnCloud Sync");
    settings.beginGroup("SyncWindow");
    mHideOnStart = settings.value("hide_on_start",true).toBool();
    mHideOnClose = settings.value("hide_when_closed",true).toBool();
    mDisplayDebug = settings.value("display_debug",false).toBool();
    mSaveLogCounter = settings.value("save_log_count",1000).toLongLong();
    mSaveDBTime = settings.value("save_db_time",370).toLongLong();
    QString lastRunVersion = settings.value("last_run_version","").toString();
    if( lastRunVersion != _OCS_VERSION ) { // Need to display what's new
        // message
        displayWhatsNew();
    }
    settings.endGroup();
    settings.beginGroup("DisabledIncludedFilters");
    for(int i = 0; i < mIncludedFilters.size(); i++ ) {
        mIncludedFilters[i].enabled =
                !settings.value(mIncludedFilters[i].name).toBool();
    }
    settings.endGroup();

    // Now update the configuration
    on_configurationBox_rejected();
}

void SyncWindow::on_actionEnable_Delete_Account_triggered()
{
    if(mEditingConfig >= 0 ) {
        ui->buttonDeleteAccount->setEnabled(true);
    } // Else we are not editing an account!
}

void SyncWindow::on_buttonDeleteAccount_clicked()
{
    deleteAccount();
}

void SyncWindow::deleteAccount()
{
    // Confirm from user if they want to delete the account
    QMessageBox box(this);
    box.setText(tr("Are you sure you want to delete account: %1").arg(
                mAccounts[mEditingConfig]->getName()));
    box.setStandardButtons(QMessageBox::Yes|QMessageBox::No);
    if( box.exec() == QMessageBox::Yes ) { // Delete the account
        ui->textBrowser->append(tr("Deleted account: %1").
                                arg(mAccounts[mEditingConfig]->getName()));
        mAccounts[mEditingConfig]->deleteAccount();
        mAccounts.removeAt(mEditingConfig);
        rebuildAccountsTable();
    }
    ui->stackedWidget->setCurrentIndex(0);
}

void SyncWindow::on_pushButton_clicked()
{
    ui->textBrowser->setText("");
}

void SyncWindow::on_pushButton_2_clicked()
{
    saveLogs();
}

void SyncWindow::on_checkBoxHostnameEncryption_stateChanged(int arg1)
{
    if( arg1 == 0 ) {
        ui->labelHttp->setText("http://");
    } else {
        ui->labelHttp->setText("https://");
    }
}

void SyncWindow::processDebugMessage(const QString msg)
{
    if(mDisplayDebug)
        ui->textBrowser->append(msg);
}

void SyncWindow::on_actionDisplay_Debug_Messages_toggled(bool arg1)
{
    mDisplayDebug = arg1;
}

void SyncWindow::on_buttonResume_clicked()
{
    for( int i = 0; i < mAccounts.size(); i++ ) {
        mAccounts[i]->resume();

    }
    ui->buttonPause->setEnabled(true);
    ui->buttonResume->setEnabled(false);
        ui->textBrowser->append(tr("...Resuming"));
}

void SyncWindow::on_buttonPause_clicked()
{
    for( int i = 0; i < mAccounts.size(); i++ ) {
        mAccounts[i]->pause();
    }
    ui->buttonPause->setEnabled(false);
    ui->buttonResume->setEnabled(true);
    ui->textBrowser->append(tr("Pausing..."));
}

void SyncWindow::on_actionConfigure_triggered()
{
    ui->stackedWidget->setCurrentIndex(3);
    ui->listIncludedFilterView->clear();
    QListWidgetItem *item;
    for( int i = 0; i < mIncludedFilters.size(); i++ ) {
        item = new QListWidgetItem(mIncludedFilters[i].filter+
                                   "     [" + mIncludedFilters[i].description + "]",
                ui->listIncludedFilterView,i);
        if( mIncludedFilters[i].canBeDisabled ) {
            if ( mIncludedFilters[i].enabled ) {
                item->setCheckState(Qt::Checked);
            } else {
                item->setCheckState(Qt::Unchecked);
            }
        } else {
            item->setFlags(Qt::NoItemFlags);
        }
    }
    connect(ui->listIncludedFilterView,SIGNAL(itemChanged(QListWidgetItem *)),
            this,SLOT(includedFilterListItemChanged(QListWidgetItem*)));

    // Now populate the Global Filters list
    listGlobalFilters();
}

void SyncWindow::includedFilterListItemChanged(QListWidgetItem *item)
{
    if(item->checkState() == Qt::Checked ) {
        mIncludedFilters[item->type()].enabled = true;
    } else {
        mIncludedFilters[item->type()].enabled = false;
    }
    updateSharedFilterList();
//    syncDebug() << "Item changed: " << mIncludedFilters[item->type()].name
//                << "State: " << item->checkState() << " Enabled?: " <<
//                   mIncludedFilters[item->type()].enabled;
}

void SyncWindow::on_buttonEnableAllIncludedFilters_clicked()
{
    for(int i = 0; i<mIncludedFilters.size(); i++) {
        if(mIncludedFilters[i].canBeDisabled) {
            ui->listIncludedFilterView->item(i)->setCheckState(Qt::Checked);
        }
    }
    updateSharedFilterList();
}

void SyncWindow::on_buttonDisableAllIncludedFilters_clicked()
{
    for(int i = 0; i<mIncludedFilters.size(); i++) {
        if(mIncludedFilters[i].canBeDisabled) {
            ui->listIncludedFilterView->item(i)->setCheckState(Qt::Unchecked);
        }
    }
    updateSharedFilterList();
}

void SyncWindow::updateSharedFilterList()
{
    mSharedFilters->clear();
    for(int i = 0; i < mIncludedFilters.size(); i++ ) {
        if(mIncludedFilters[i].enabled)
            mSharedFilters->insert(mIncludedFilters[i].filter);
    }
    QStringList list = mGlobalFilters.toList();
    for(int i =0; i < list.size(); i++ ) {
        mSharedFilters->insert(list[i]);
    }
}


void SyncWindow::on_buttonReturn_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void SyncWindow::on_lineGlobalFilter_textEdited(const QString &text)
{
    if(text == "") {
        ui->buttonGlobalFilterInsert->setDisabled(true);
    } else {
        ui->buttonGlobalFilterInsert->setEnabled(true);
    }
}

void SyncWindow::on_buttonGlobalFilterInsert_clicked()
{
    mGlobalFilters.insert(ui->lineGlobalFilter->text());
    listGlobalFilters();
    updateSharedFilterList();
}

void SyncWindow::on_buttonGlobalFilterRemove_clicked()
{
    int index = ui->listGlobalFilterView->selectionModel()
                ->selection().indexes()[0].row();
    mGlobalFilters.remove(ui->listGlobalFilterView->model()->index(index,0)
                          .data(Qt::DisplayRole ).toString());
    listGlobalFilters();
    updateSharedFilterList();
}

void SyncWindow::listGlobalFilters()
{
    // Show the filter list
    ui->listGlobalFilterView->setModel(
                new QStringListModel(mGlobalFilters.toList()));
    // Create the filterView signals
    connect(ui->listGlobalFilterView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                   this,SLOT(listGlobalFiltersSelectionChanged(QItemSelection,
                                                    QItemSelection)));
    ui->lineGlobalFilter->setText("");
    ui->buttonGlobalFilterInsert->setEnabled(false);
    ui->buttonGlobalFilterRemove->setEnabled(false);
}

void SyncWindow::listGlobalFiltersSelectionChanged(QItemSelection selected,
                                             QItemSelection deselected)
{
    ui->buttonGlobalFilterRemove->setEnabled(true);
}

void SyncWindow::on_buttonImport_clicked()
{
    importGlobalFilters();
}

void SyncWindow::on_buttonExport_clicked()
{
    exportGlobalFilters();
}

void SyncWindow::importGlobalFilters(bool isDefault)
{
    QString fileName;
    QFile file;
    if(isDefault) {
        fileName = mConfigDirectory+"/global_filters.txt";
        file.setFileName(fileName);
        if(!file.exists()) { // One has not been created yet.
            return;
        }
    } else { // Ask user
        fileName = QFileDialog::getOpenFileName(this);
        if( fileName == "" ) { // User cancelled
            return;
        }
        file.setFileName(fileName);
    }

    if(!file.open(QIODevice::ReadOnly|QIODevice::Text)) {
        syncDebug() << "Failed to open filter file for reading: " << fileName;
        return;
    }

    // Read the list
    QTextStream in(&file);
    while(!in.atEnd()) {
        mGlobalFilters.insert(in.readLine());
    }
    file.close();
    listGlobalFilters();
    updateSharedFilterList();
}

void SyncWindow::exportGlobalFilters( bool isDefault)
{
    QString fileName;
    QFile file;
    if(isDefault) {
        fileName = mConfigDirectory+"/global_filters.txt";
        file.setFileName(fileName);
    } else { // Ask user
        fileName = QFileDialog::getSaveFileName(this);
        if( fileName == "" ) { // User cancelled
            return;
        }
        file.setFileName(fileName);
    }

    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {
        syncDebug() << "Failed to open filter file for writting: " << fileName;
        return;
    }

    // Write the list
    QTextStream out(&file);
    QStringList list = mGlobalFilters.toList();
    for(int i = 0; i < list.size(); i++ ) {
        out << list[i] << "\n";
    }
    out.flush();
    file.close();
}

void SyncWindow::displayWhatsNew()
{
    QMessageBox::information(this,tr("What's new?"),
                             tr("<center><b>WARNING!!</b></center><br />This software is still in beta. It may eat your files!!!<br /><br />"
                                "<center><b>New features</b>:</center><br />"
                                "- Soft Pause/Resume of synchronization<br />"
                                "- Global filters<br />"
                                "- Selection of common filters<br />"),
                             QMessageBox::Ok);
}

void SyncWindow::on_configurationBox_accepted()
{
    mSaveLogCounter = ui->spinSaveLogs->value();
    mSaveDBTime = ui->spinSaveDB->value();
    mHideOnClose = ui->checkCloseButton->isChecked();
    mDisplayDebug = ui->checkShowDebug->isChecked();
    mHideOnStart = ui->checkHideOnStart->isChecked();
    for(int i = 0; i < mAccounts.size(); i++ ) {
        mAccounts[i]->setSaveDBTime(mSaveDBTime);
    }

    // Finally return to the main window
    ui->stackedWidget->setCurrentIndex(0);
}

void SyncWindow::on_configurationBox_rejected()
{
    // Reset the GUI
    ui->spinSaveLogs->setValue(mSaveLogCounter);
    ui->spinSaveDB->setValue(mSaveDBTime);
    ui->checkCloseButton->setChecked(mHideOnClose);
    ui->checkShowDebug->setChecked(mDisplayDebug);
    ui->checkHideOnStart->setChecked(mHideOnStart);

    // Finally return to the main window
    ui->stackedWidget->setCurrentIndex(0);
}

