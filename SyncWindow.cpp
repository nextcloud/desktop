#include "SyncWindow.h"
#include "ui_SyncWindow.h"
#include "sqlite3_util.h"
#include "QWebDAV.h"
#include "OwnCloudSync.h"

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

SyncWindow::SyncWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SyncWindow)
{
    mBusy = false;
    ui->setupUi(this);
    mEditingConfig = -1;

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
    mSystemTray->setIcon(mDefaultIcon);
    mSystemTray->show();
    connect(mSystemTray,SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this,SLOT(systemTrayActivated(QSystemTrayIcon::ActivationReason)));

    // Start with the default tab
    ui->stackedWidget->setCurrentIndex(0);

    // Create the Accounts SignalMapper
    mAccountsSignalMapper = new QSignalMapper(this);
    connect(mAccountsSignalMapper, SIGNAL(mapped(int)), this,
            SLOT(slotAccountsSignalMapper(int)));

#ifdef Q_OS_LINUX
    // In linux, we will store all databases in
    // $HOME/.local/share/data/owncloud_sync
    mConfigDirectory = QDir::home().path()+"/.local/share/data/owncloud_sync";
    QDir configDir(mConfigDirectory);
    configDir.mkpath(mConfigDirectory);
    QDir logsDir(mConfigDirectory+"/logs");
    logsDir.mkpath(mConfigDirectory+"/logs");
#endif

    // Look for accounts that already exist
    QStringList filters;
    filters << "*.db";
    QStringList files = configDir.entryList(filters);
    for( int i = 0; i < files.size(); i++ ) {
        QString name = files[i].replace(".db","");
        qDebug() << "Found " << name << " config.";
        addAccount(name);
    }
    if( files.size() == 0 ) {
        ui->stackedWidget->setCurrentIndex(2);
        show();
    }
    rebuildAccountsTable();

//    updateStatus();
}

void SyncWindow::updateStatus()
{
    //if( !isVisible() )
    //    return;

    if( !mBusy ) {
        ui->status->setText("Waiting ");
        ui->progressFile->setValue(0);
        ui->progressTotal->setValue(0);
        if(mConflictsExist) {
             ui->labelImage->setPixmap(mDefaultConflictIcon.pixmap(129,129));
        } else {
            ui->labelImage->setPixmap(mDefaultIcon.pixmap(129,129));
        }
    } else {
        ui->status->setText(mTransferState+mCurrentFile+" out of "
                            + QString("%1").arg(mCurrentFileSize));
        if(mConflictsExist) {
            ui->labelImage->setPixmap(mSyncConflictIcon.pixmap(129,129));
        } else {
            ui->labelImage->setPixmap(mSyncIcon.pixmap(129,129));
        }
    }
}

SyncWindow::~SyncWindow()
{
    delete ui;
}

void SyncWindow::saveLogs()
{
    QString name =
            QDateTime::currentDateTime().toString("yyyyMMdd:hh:mm:ss.log");
    QFile file(mConfigDirectory+"/logs/"+name);
    if( !file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open log file for writting!\n";
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
    OwnCloudSync *account = new OwnCloudSync(name);
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
    if( mEditingConfig >= 0 ) { // Editing an account
        // If we are renaming the account, make sure that the new name
        // does not already exist
        if( ui->lineName->text() != mAccounts[mEditingConfig]->getName() ) {
            if( info.exists() ) {
                qDebug() << "New account name already taken!!";
                okToEdit = false;
            }
        }
        if( okToEdit ) {
            mAccounts[mEditingConfig]->initialize(
                        ui->lineHost->text(),
                        ui->lineUser->text(),
                        ui->linePassword->text(),
                        ui->lineRemoteDir->text(),
                        ui->lineLocalDir->text(),
                        ui->time->value());
        }
    } else { // New account
        // First, check to see if this name is already taken
        if(info.exists()) { // Account name taken!
            qDebug() << "Account name already taken!!";
            ui->lineName->setFocus();
        } else { // Good, create a new account
            OwnCloudSync *account = addAccount(ui->lineName->text());
            account->initialize(ui->lineHost->text(),
                                ui->lineUser->text(),
                                ui->linePassword->text(),
                                ui->lineRemoteDir->text(),
                                ui->lineLocalDir->text(),
                                ui->time->value());
        }
    }
    mEditingConfig = -1;
    ui->stackedWidget->setCurrentIndex(0);
    rebuildAccountsTable();
}

void SyncWindow::closeEvent(QCloseEvent *event)
{
    // We definitely don't want to quit when we are synchronizing!
    for( int i = 0; i < mAccounts.size(); i++ ) {
        mAccounts[i]->deleteWatcher();    // Delete the file watcher
        mAccounts[i]->stop(); // Stop the counter
        while( mAccounts[i]->needsSync() ) {
            qDebug() << "Waiting for " + mAccounts[i]->getName()
                        + " to sync before close.";
            sleep(1); // Just wait until this thing syncs
        }
    }
    while(mBusy) {
        sleep(5);
        qDebug() << "Still busy!";
    }

    // Before closing, save the database!!!
    for( int i = 0; i < mAccounts.size(); i++ ) {
        mAccounts[i]->saveConfigToDB();
        mAccounts[i]->saveDBToFile();
    }

    saveLogs();
    qDebug() << "All ready to close!";
    QMainWindow::closeEvent(event);
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
    headers.append("Account");
    headers.append("File Name");
    headers.append("Server Modified");
    headers.append("Local Modifed");
    headers.append("Which wins?");
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
            combo->addItem("Choose:");
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
    for( int row = 0; row < ui->tableAccounts->rowCount(); row++ ) {
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
    /*
    // Check the selections are valid
    //mSyncTimer->start(mUpdateTime);
    mFileAccessBusy = true;
    for( int row = 0; row < ui->tableConflict->rowCount(); row++ ) {
        QComboBox *combo = (QComboBox*)ui->tableConflict->cellWidget(row,3);
        if( combo->currentIndex() == 0 ) {
            allConflictsResolved = false;
            continue;
        }
        processFileConflict(ui->tableConflict->takeItem(row,0)->text(),
                            combo->currentText());

        //qDebug() << ui->tableConflict->takeItem(row,0)->text()
        //            << combo->currentIndex();
    }
    if( allConflictsResolved) {
        ui->conflict->setEnabled(false);
        mConflictsExist = false;
    }
    mFileAccessBusy = false;
    */
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
    headers.append("Name");
    headers.append("Enabled");
    headers.append("Last Sync");
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

}

void SyncWindow::slotProgressTotal(qint64 value)
{

}

void SyncWindow::on_buttonCancel_clicked()
{
    mEditingConfig = -1;
    ui->stackedWidget->setCurrentIndex(0);
}

void SyncWindow::slotReadyToSync(OwnCloudSync* oc)
{
    mAccountsReadyToSync.enqueue(oc);
    qDebug() << oc->getName() << " is ready to sync!";
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
        mAccountsReadyToSync.dequeue()->sync();
    } else {
        mBusy = false;
    }
    if(mTotalSyncs%10 == 0 ) {
        saveLogs();
    }
}

void SyncWindow::slotFinishedSync(OwnCloudSync *oc)
{
    qDebug() << oc->getName() << " just finishied.";
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
    mEditingConfig = row;
    // Switch to the account editing widget
    ui->stackedWidget->setCurrentIndex(2);
    ui->lineName->setText(mAccounts[row]->getName());
    ui->lineHost->setText(mAccounts[row]->getHost());
    ui->lineUser->setText(mAccounts[row]->getUserName());
    ui->linePassword->setText(mAccounts[row]->getPassword());
    ui->lineRemoteDir->setText(mAccounts[row]->getRemoteDirectory());
    ui->lineLocalDir->setText(mAccounts[row]->getLocalDirectory());
    ui->time->setValue(mAccounts[row]->getUpdateTime());
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
    ui->time->setValue(15);
}
