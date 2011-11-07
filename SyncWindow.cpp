#include "SyncWindow.h"
#include "ui_SyncWindow.h"
#include "sqlite3_util.h"
#include "QWebDAV.h"


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

SyncWindow::SyncWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SyncWindow)
{
    mBusy = false;
    ui->setupUi(this);

    // Set the pointers so we can delete them without worrying :)
    mSyncTimer = 0;
    mFileWatcher = 0;

    mIsFirstRun = true;
    mDownloadingConflictingFile = false;
    mFileAccessBusy = false;
    mConflictsExist = false;
    mSettingsCheck = true;
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

    // Create a QWebDAV instance
    mWebdav = new QWebDAV();

    // Create a File System Watcher
    /*mFileWatcher = new QFileSystemWatcher(this);
    connect(mFileWatcher,SIGNAL(fileChanged(QString)),
            this, SLOT(localFileChanged(QString)));
    connect(mFileWatcher,SIGNAL(directoryChanged(QString)),
            this, SLOT(localDirectoryChanged(QString)));*/

    // Connect to QWebDAV signals
    connect(mWebdav,SIGNAL(directoryListingError(QString)),
            this, SLOT(directoryListingError(QString)));
    connect(mWebdav,SIGNAL(directoryListingReady(QList<QWebDAV::FileInfo>)),
            this, SLOT(processDirectoryListing(QList<QWebDAV::FileInfo>)));
    connect(mWebdav,SIGNAL(fileReady(QByteArray,QString)),
            this, SLOT(processFileReady(QByteArray,QString)));

    connect(mWebdav,SIGNAL(uploadComplete(QString)),
            this, SLOT(updateDBUpload(QString)));

    mDownloadingFiles.clear();
    mDownloadConflict.clear();
    mUploadingFiles.clear();

    // Initialize the Database
    mDB = QSqlDatabase::addDatabase("QSQLITE");
#ifdef Q_OS_LINUX
    // In linux, we will store all databases in
    // $HOME/.local/share/data/owncloud_sync
    mHomeDirectory = QDir::home().path();
    mConfigDirectory = mHomeDirectory+"/.local/share/data/owncloud_sync";
    QDir configDir(mConfigDirectory);
    configDir.mkpath(mConfigDirectory);
    QDir logsDir(mConfigDirectory+"/logs");
    logsDir.mkpath(mConfigDirectory+"/logs");
    //mDB.setDatabaseName(mConfigDirectory+"/owncloud_sync.db");
    mDB.setDatabaseName(":memory:"); // Use memory for now
    mDBFileName = mConfigDirectory+"/owncloud_sync.db";

/*    mSyncDirectory = mHomeDirectory + QString("/tmp/sync/files");
    QString path(mHomeDirectory);
    path.append("/tmp/sync/owncloud_sync.db");
    path = QDir::toNativeSeparators(path);
    //qDebug() << "Database path: " << path;
//    mDB.setDatabaseName(path);*/
#endif
    // Find out if the database exists.
    QFile dbFile(mConfigDirectory+"/owncloud_sync.db");
    if( dbFile.exists() ) {
        if(!mDB.open()) {
            qDebug() << "Cannot open database!";
            qDebug() << mDB.lastError().text();
            mDBOpen = false;
            show();
        } else {
            mDBOpen = true;
            loadDBFromFile();
            readConfigFromDB();
            // If we are already configured, just hide the window
            if( QSystemTrayIcon::isSystemTrayAvailable()) {
                hide();
            } else {
                show();
            }
            ui->buttonSave->setDisabled(true);
            qDebug() << "Checking configuration!";
            initialize();
        }
    } else {
      createDataBase(); // Create the database in memory
      show();
    }

    mSaveDBTimer = new QTimer(this);
    connect(mSaveDBTimer, SIGNAL(timeout()), this, SLOT(saveDBToFile()));
    mSaveDBTimer->start(370000);

    updateStatus();
}

void SyncWindow::directoryListingError(QString url)
{
    if(mSettingsCheck) {
        qDebug() << "Something wrong with the settings, please check.";
        ui->textBrowser->append("Settings could not be confirmed. Please "
                                "confirm your settings and try again.");
    }
}

void SyncWindow::updateStatus()
{
    //if( !isVisible() )
    //    return;

    if( !mBusy ) {
        ui->status->setText("Waiting " + QString("%1").arg(mUpdateTime) +
                            " seconds...");
        ui->progressFile->setValue(0);
        ui->progressTotal->setValue(0);
        if(mConflictsExist) {
             ui->labelImage->setPixmap(mDefaultConflictIcon.pixmap(129,129));
        } else {
            ui->labelImage->setPixmap(mDefaultIcon.pixmap(129,129));
        }
    } else {
        mSyncTimer->stop();
        ui->status->setText(mTransferState+mCurrentFile+" out of "
                            + QString("%1").arg(mCurrentFileSize));
        if(mConflictsExist) {
            ui->labelImage->setPixmap(mSyncConflictIcon.pixmap(129,129));
        } else {
            ui->labelImage->setPixmap(mSyncIcon.pixmap(129,129));
        }
    }
}

void SyncWindow::timeToSync()
{
    if ( mBusy ) {
        ui->textBrowser->append(
                    "Ooops, looks like it is busy, we'll try again later");
        return;
    }

    if( !mDBOpen ) {
        ui->status->setText("Database is not open. Aborting sync!");
        return;
    }

    // Announce we are busy!
    mBusy = true;

    ui->textBrowser->append("Synchronizing on: "+QDateTime::currentDateTime().toString());

    // If this is the first run, scan the directory, otherwise just wait
    // for the watcher to update us :)
    if(mIsFirstRun) {
        scanLocalDirectory(mSyncDirectory);
    }

    if ( mScanDirectoriesSet.size() != 0 ) {
        while( mScanDirectories.size() > 0 ) {
            QString relativeName = mScanDirectories.dequeue();
            QString name(relativeName);
            name.replace("_sssspace_"," ");
            scanLocalDirectoryForNewFiles(name);
            mScanDirectoriesSet.remove(relativeName);
        }
    }

    // List all the watched files for now
    /*
    QStringList list = mFileWatcher->files();
    for(int i = 0; i < list.size(); i++ ) {
        qDebug() << "Watching: " << list[i];
    }
    list = mFileWatcher->directories();
    for(int i = 0; i < list.size(); i++ ) {
        qDebug() << "Watching: " << list[i];
    } */

    // Then scan the base directory of the WebDAV server
    mWebdav->dirList("/");

    // Set the icon to sync
    if( mConflictsExist) {
        mSystemTray->setIcon(mSyncConflictIcon);
    } else {
        mSystemTray->setIcon(mSyncIcon);
    }
}

SyncWindow::~SyncWindow()
{
    delete ui;
    delete mWebdav;
    mDB.close();
}

void SyncWindow::processDirectoryListing(QList<QWebDAV::FileInfo> fileInfo)
{
    //qDebug() << "Processing Directory Listing";
    if( mSettingsCheck ) {
        // Great, we were just checking
        mSettingsCheck = false;
        settingsAreFine();
        return;
    }
    // Compare against the database of known files
    QSqlQuery query;
    QSqlQuery add;
    for(int i = 0; i < fileInfo.size(); i++ ){
        query = queryDBFileInfo(fileInfo[i].fileName,"server_files");
        if(query.next()) { // File exists confirm no conflict, then update
            if( query.value(7).toString() == "" ) {
                //ui->textBrowser->append("File " + fileInfo[i].fileName +
                //                        " exists. Comparing!");
                QString prevModified = query.value(4).toString();
                QString updateStatement =
                        QString("UPDATE server_files SET file_size='%1',"
                                "last_modified='%2',found='yes',prev_modified='%3'"
                                " where file_name='%4'")
                        .arg(fileInfo[i].size)
                        .arg(fileInfo[i].lastModified)
                        .arg(prevModified)
                        .arg(fileInfo[i].fileName);
                add.exec(updateStatement);
                //qDebug() << "SQuery: " << updateStatement;
            } else if ( !mUploadingConflictFilesSet.contains(
                            fileInfo[i].fileName.replace(" ","_sssspace_")) ) {
                // Enable the conflict resolution window
                ui->conflict->setEnabled(true);
                mConflictsExist = true;
                qDebug() << "SFile still conflicts: " << fileInfo[i].fileName;
            }
        } else { // File does not exist, so just add this info to the DB
            //ui->textBrowser->append("File " + fileInfo[i].fileName +
            //                        " does not exist. Adding to DB");
            QString addStatement = QString("INSERT INTO server_files(file_name,"
                                 "file_size,file_type,last_modified,found,conflict) "
                                           "values('%1','%2','%3','%4','yes','');")
                    .arg(fileInfo[i].fileName).arg(fileInfo[i].size)
                    .arg(fileInfo[i].type).arg(fileInfo[i].lastModified);
            //qDebug() << "Query: " << addStatement;
            add.exec(addStatement);
        }
        // If a collection, list those contents too
        if(fileInfo[i].type == "collection") {
            mDirectoryQueue.enqueue(fileInfo[i].fileName);
        }
    }
    if(!mDirectoryQueue.empty()) {
        mWebdav->dirList(mDirectoryQueue.dequeue());
    } else {
        syncFiles();
    }
}

void SyncWindow::processFileReady(QByteArray data,QString fileName)
{
    //ui->textBrowser->append("Processing file " + fileName);
    //qDebug() << "Processing File " + mSyncDirectory << fileName;
    // Temporarily remove this watcher so we don't get a message when
    // we modify it.
    mFileWatcher->removePath(mSyncDirectory+fileName);
    QString finalName;
    if(mDownloadingConflictingFile) {
        finalName = getConflictName(fileName);
        qDebug() << "Downloading conflicting file " << fileName;
    } else {
        finalName = fileName;
    }
    QFile file(mSyncDirectory+finalName);
    if (!file.open(QIODevice::WriteOnly))
            return;
    QDataStream out(&file);
    out.writeRawData(data.constData(),data.length());
    file.flush();
    file.close();
    updateDBDownload(fileName);
    mFileWatcher->addPath(mSyncDirectory+fileName); // Add the watcher back!
    processNextStep();
}

void SyncWindow::processNextStep()
{
    // Check if there is another file to dowload, if so, start that process
    if( mDownloadingFiles.size() != 0 ) {
        download(mDownloadingFiles.dequeue());
    } else if ( mUploadingFiles.size() != 0 ) { // Maybe an upload?
        upload(mUploadingFiles.dequeue());
    } else if ( mUploadingConflictFiles.size() !=0 ) { // Upload conflict files
        FileInfo info = mUploadingConflictFiles.dequeue();
        upload(info);
        clearFileConflict(info.name);
        mUploadingConflictFilesSet.remove(info.name.replace(" ","_sssspace_"));
    } else if ( mDownloadConflict.size() != 0 ) { // Download conflicting files
        mDownloadingConflictingFile = true;
        download(mDownloadConflict.dequeue());
        ui->conflict->setEnabled(true);
    } else { // We are done! Start the sync clock
        mDownloadingConflictingFile = false;
        mBusy = false;
        mSyncTimer->start();
        ui->textBrowser->append(tr("Finished: ") +
                                QDateTime::currentDateTime().toString());
        if(mConflictsExist) {
            mSystemTray->setIcon(mDefaultConflictIcon);
        } else {
            mSystemTray->setIcon(mDefaultIcon);
        }
        mTotalSyncs++;
        if(mTotalSyncs%1000 == 0) {
            saveLogs();
        }
    }
    updateStatus();
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

void SyncWindow::scanLocalDirectory( QString dirPath)
{
    QDir dir(dirPath);
    QStringList list = dir.entryList();
    QString type;
    QString append;
    for( int i = 0; i < list.size(); i++ ) {
        QString name = list.at(i);
        if( name == "." || name == ".." ||
                name.contains("_ocs_serverconflict.")) {
            continue;
        }

        //qDebug() << "Relative Path: " << relativeName;
        processLocalFile(dirPath + "/" + name);

        // Check if it is a directory, and if so, process it
    }
}

void SyncWindow::processLocalFile(QString name)
{
    QFileInfo file( name );
    QString type;
    QString append;
    // Check if it is a directory, and if so, process it
    if ( file.isDir() ) {
        type = "collection";
        append = "/";
    } else {
        type = "file";
        append = "";
    }

    // Add to the watcher
    mFileWatcher->addPath(name+append);
    updateDBLocalFile(name + append,
                      file.size(),file.lastModified().toUTC()
                      .toMSecsSinceEpoch(),type);

    if ( file.isDir() ) {
        scanLocalDirectory(file.absoluteFilePath() );
    }
}

void SyncWindow::updateDBLocalFile(QString name, qint64 size, qint64 last,
                                   QString type )
{
    // Do not upload the server conflict files
    if( name.contains("_ocs_serverconflict.") ) {
        return;
    }
    // Get the relative name of the file
    name.replace(mSyncDirectory,"");
    // Check against the database
    QSqlQuery query = queryDBFileInfo(name,"local_files");
    if (query.next() ) { // We already knew about this file. Update info.
        QString prevModified = query.value(4).toString();
        if ( query.value(8).toString() == "") {
            QString updateStatement =
                    QString("UPDATE local_files SET file_size='%1',"
                            "last_modified='%2',found='yes',prev_modified='%3' "
                            "where file_name='%4'")
                    .arg(size)
                    .arg(last)
                    .arg(prevModified)
                    .arg(name);
            //qDebug() << "Query:   " << updateStatement;
            query.exec(updateStatement);
        } else {
            // Enable the conflict resolution button
            ui->conflict->setEnabled(true);
            mConflictsExist = true;
            qDebug() << "LFile still conflicts: " << name;
        }
    } else { // We did not know about this file, add
        QString addStatement = QString("INSERT INTO local_files (file_name,"
                             "file_size,file_type,last_modified,found,conflict) "
                                       "values('%1','%2','%3','%4','yes','');")
                .arg(name).arg(size).arg(type).arg(last);
        //qDebug() << "Query: " << addStatement;
        query.exec(addStatement);
    }
    //qDebug() << "Processing: " << mSyncDirectory + relativeName << " Size: "
    //         << file.size();
}

QSqlQuery SyncWindow::queryDBFileInfo(QString fileName, QString table)
{
    QSqlQuery query;
    query.exec("SELECT * FROM " + table + " WHERE file_name = '" +
                     fileName + "';");
    return query;
}

QSqlQuery SyncWindow::queryDBAllFiles(QString table)
{
    QSqlQuery query;
    query.exec("SELECT * FROM " + table + ";");
    return query;
}

void SyncWindow::syncFiles()
{
    QList<QString> serverDirs;
    QList<QString> localDirs;
    QSqlQuery localQuery = queryDBAllFiles("local_files");
    QSqlQuery serverQuery = queryDBAllFiles("server_files");

    // Reset the progress trackers
    mTotalToDownload = 0;
    mTotalToUpload = 0;
    mTotalToTransfer = 0;
    mTotalDownloaded = 0;
    mTotalUploaded = 0;
    mTotalTransfered = 0;
    //mUploadingFiles.clear();
    //mDownloadingFiles.clear();


    // Find out which local files need to be uploaded
    while ( localQuery.next() ) {
        QString localName = localQuery.value(1).toString();
        qint64 localSize = localQuery.value(2).toString().toLongLong();
        QString localType = localQuery.value(3).toString();
        qint64 localModified = localQuery.value(4).toString().toLongLong();
        qint64 lastSync = localQuery.value(5).toString().toLongLong();
        qint64 localPrevModified = localQuery.value(7).toString().toLongLong();
        QDateTime localModifiedTime;
        localModifiedTime.setTimeSpec(Qt::UTC);
        localModifiedTime.setMSecsSinceEpoch(localModified);
        QDateTime localPrevModifiedTime;
        localPrevModifiedTime.setTimeSpec(Qt::UTC);
        localPrevModifiedTime.setMSecsSinceEpoch(localPrevModified);
        QDateTime lastSyncTime;
        lastSyncTime.setTimeSpec(Qt::UTC);
        lastSyncTime.setMSecsSinceEpoch(lastSync);
        //qDebug() << "LFile: " << localName << " Size: " << localSize << " vs "
        //         << localQuery.value(2).toString() << " type: " << localType ;
        // Query the database and look for this file
        QSqlQuery query = queryDBFileInfo(localName,"server_files");
        if( query.next() ) {
            // Check when this file was last modified, and check to see
            // when we last synced
            //QString serverType = query.value(3).toString();
            qint64 serverSize = query.value(2).toString().toLongLong();
            qint64 serverModified = query.value(4).toString().toLongLong();
            qint64 serverPrevModified = query.value(6).toString().toLongLong();
            QDateTime serverModifiedTime;
            serverModifiedTime.setTimeSpec(Qt::UTC);
            serverModifiedTime.setMSecsSinceEpoch(serverModified);
            QDateTime serverPrevModifiedTime;
            serverPrevModifiedTime.setTimeSpec(Qt::UTC);
            serverPrevModifiedTime.setMSecsSinceEpoch(serverPrevModified);

/*
            if(localName == "/pretty.xml") {
                qDebug() << localModifiedTime << localPrevModifiedTime
                            << serverModifiedTime << serverPrevModifiedTime
                               << lastSyncTime;
                localName = localName;
            }
*/
            if( serverModifiedTime < localModifiedTime &&
                    localModifiedTime > lastSyncTime  ) { // Server is older!
                // Now check to see if the server too modified the file
                if( localType == "collection" ) { // But already exists!
                    //serverDirs.append(localName);
                } else {
                    if( serverPrevModifiedTime != serverModifiedTime &&
                            serverModifiedTime > lastSyncTime) {
                        // There is a conflict, both files got changed since the
                        // last time we synced
                        qDebug() << "Conflict with sfile " << localName
                                 << serverModifiedTime << serverPrevModifiedTime
                                 << localModifiedTime << lastSyncTime;
                        setFileConflict(localName,localSize,
                                        serverModifiedTime.toString(),
                                        localModifiedTime.toString());
                        //qDebug() << "UPLOAD:   " << localName;
                    } else { // There is no conflict
                        mUploadingFiles.enqueue(FileInfo(localName,localSize));
                        mTotalToUpload +=localSize;
                        //qDebug() << "File " << localName << " is newer than server!";
                    }
                }
            } else if ( serverModifiedTime > localModifiedTime &&
                        serverModifiedTime > lastSyncTime ) { // Server is newer
                // Check to see if local file was also modified
                if( localType == "collection" ) {
                    //localDirs.append(localName);
                } else {
                    if( localPrevModifiedTime != localModifiedTime
                            && localModifiedTime > lastSyncTime) {
                        // There is a conflict, both files got changed since the
                        // last time we synced
                        qDebug() << "Conflict with lfile " << localName
                                 << serverModifiedTime << serverPrevModifiedTime
                                 << localModifiedTime << lastSyncTime;
                        setFileConflict(localName,serverSize,
                                        serverModifiedTime.toString(),
                                        localModifiedTime.toString());
                    } else { // There is no conflict
                        mDownloadingFiles.enqueue(FileInfo(localName,serverSize));
                        mTotalToDownload += serverSize;
                        //qDebug() << "OLDER:    " << localName;
                    }
                }
            } else { // The same! (I highly doubt that!)
                //qDebug() << "SAME:     " << localName;
            }
        } else { // Does not exist on server! Upload!
            //qDebug() << "NEW:      " << localName;
            if ( localType == "collection") {
                serverDirs.append(localName);
            } else {
                mUploadingFiles.enqueue(FileInfo(localName,localSize));
                //uploads.append(localName);
                //uploadsSizes.append(localSize);
                mTotalToUpload += localSize;
            }
        }
    }

    // Find out which remote files need to be downloaded (only the ones
    // that don't exist)
    while ( serverQuery.next() ) {
        QString serverName = serverQuery.value(1).toString();
        qint64 serverSize = serverQuery.value(2).toString().toLongLong();
        QString serverType = serverQuery.value(3).toString();
        //qDebug() << "SFile: " << serverName << " Size: " << serverSize << " vs "
        //         << serverQuery.value(2).toString() << " type: " << serverType ;
        QSqlQuery query = queryDBFileInfo(serverName,"local_files");
        if( !query.next() ) {
            if( serverType == "collection") {
                localDirs.append(serverName);
            } else {
                mDownloadingFiles.enqueue(FileInfo(serverName,serverSize));
                //downloads.append(serverName);
                //downloadsSizes.append(serverSize);
                mTotalToDownload += serverSize;
            }
            //qDebug() << "DOWNLOAD: " << serverName;
        }
    }
    mTotalToTransfer = mTotalToDownload+mTotalToUpload;

    // Make local dirs and downloads
    for(int i = 0; i < localDirs.size(); i++ ) {
        QDir dir;
        if (!dir.mkdir(mSyncDirectory+localDirs[i]) ) {
            qDebug() << "Could not make directory "+mSyncDirectory+localDirs[i];
        } else {
            //qDebug() << "Made directory "+mSyncDirectory+localDirs[i];
        }
    }

    // Now make remote dirs
    for(int i = 0; i < serverDirs.size(); i++ ) {
        mWebdav->mkdir(serverDirs[i]);
        //qDebug() << "Making the following directories on server: " <<
        //            serverDirs[i];
    }

    // Delete removed files and reset the file status
    deleteRemovedFiles();
    QSqlQuery query;
    query.exec("UPDATE  local_files SET found='' WHERE conflict='';");
    query.exec("UPDATE server_files SET found='' WHERE conflict='';");

     mIsFirstRun = false;

    // Let's get the ball rolling!
    processNextStep();
}

void SyncWindow::setFileConflict(QString name, qint64 size, QString server_last,
                                 QString local_last)
{
    QSqlQuery conflict;
    QString conflictText = QString("UPDATE server_files SET conflict='yes'"
                    " WHERE file_name='%1';").arg(name);
    conflict.exec(conflictText);
    conflictText = QString("UPDATE local_files SET conflict='yes'"
                    " WHERE file_name='%1';").arg(name);
    conflict.exec(conflictText);
    conflictText = QString("INSERT INTO conflicts values('%1','','%2','%3');")
            .arg(name).arg(server_last).arg(local_last);
    conflict.exec(conflictText);
    mDownloadConflict.enqueue(FileInfo(name,size));
    mConflictsExist = true;
    mSystemTray->showMessage(tr("A conflict has been found!"),
                             tr("File %1 conflicts.").arg(name),
                             QSystemTrayIcon::Warning);
}

void SyncWindow::download( FileInfo file )
{
    mCurrentFileSize = file.size;
    mCurrentFile = file.name;
    if(mDownloadingConflictingFile) {
        mTransferState = "Downloading conflicting file ";
    } else {
        mTransferState = "Downloading ";
    }
    QNetworkReply *reply = mWebdav->get(file.name);
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
            this, SLOT(transferProgress(qint64,qint64)));
    updateStatus();
}

void SyncWindow::upload( FileInfo fileInfo)
{
    mCurrentFileSize = fileInfo.size;
    mCurrentFile = fileInfo.name;
    mTransferState = "Uploading ";
    //ui->textBrowser->append("Uploading file " + fileName);
    //qDebug() << "Uploading File " +mSyncDirectory + fileName;
    QFile file(mSyncDirectory+fileInfo.name);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "File read error " + mSyncDirectory+fileInfo.name+" Code: "
                    << file.error();
        return;
    }
    QByteArray data = file.readAll();
    //in.writeRawData(data.constData(),data.length());
    //ui->textBrowser->append(data);
    QNetworkReply *reply = mWebdav->put(fileInfo.name,data);
    connect(reply, SIGNAL(uploadProgress(qint64,qint64)),
            this, SLOT(transferProgress(qint64,qint64)));
    updateStatus();
}

void SyncWindow::updateDBDownload(QString name)
{
    // This seems redundant, a little, really.
    QString fileName = mSyncDirectory+name;
    QFileInfo file(fileName);

    // Check against the database
    QSqlQuery query = queryDBFileInfo(name,"local_files");
    if (query.next() ) { // We already knew about this file. Update.
        QString updateStatement =
                QString("UPDATE local_files SET file_size='%1',"
                        "last_modified='%2',last_sync='%3' where file_name='%4'")
                        .arg(file.size())
                        .arg(file.lastModified().toUTC()
                             .toMSecsSinceEpoch())
                        .arg(file.lastModified().toUTC()
                              .toMSecsSinceEpoch())
                        .arg(name);
        query.exec(updateStatement);
    } else { // We did not know about this file, add
        QString addStatement = QString("INSERT INTO local_files (file_name,"
                             "file_size,file_type,last_modified,last_sync) "
                                       "values('%1','%2','%3','%4','%5');")
                .arg(name).arg(file.size())
                .arg("file")
                .arg(file.lastModified().toUTC().toMSecsSinceEpoch())
                .arg(file.lastModified().toUTC().toMSecsSinceEpoch());
        query.exec(addStatement);
    }
    QString downloadText;
    if( mDownloadingConflictingFile ) {
        downloadText = "Downloaded conflicting file: " + name;
    } else {
        downloadText = "Downloaded file: " + name;
    }
    ui->textBrowser->append(downloadText);
    //qDebug() << "Did this get called?";
    mTotalTransfered += mCurrentFileSize;
}

void SyncWindow::updateDBUpload(QString name)
{
    QString fileName = mSyncDirectory+name;
    QFileInfo file(fileName);
    qint64 time = QDateTime::currentMSecsSinceEpoch();
    //qDebug() << "Debug: File: " << name << " Size: " << file.size();

    // Check against the database
    QSqlQuery query = queryDBFileInfo(name,"server_files");
    if (query.next() ) { // We already knew about this file. Update.
        QString updateStatement =
                QString("UPDATE server_files SET file_size='%1',"
                        "last_modified='%2' where file_name='%3'")
                        .arg(file.size())
                        .arg(time).arg(name);
        //qDebug() << "Query: " << updateStatement;
        query.exec(updateStatement);
        updateStatement =
                QString("UPDATE local_files SET last_sync='%1'"
                        "where file_name='%2'")
                .arg(time).arg(name);
        query.exec(updateStatement);
        //qDebug() << "Query: " << updateStatement;
    } else { // We did not know about this file, add
        QString addStatement = QString("INSERT INTO server_files (file_name,"
                             "file_size,file_type,last_modified) "
                                       "values('%1','%2','%3','%4');")
                .arg(name).arg(file.size())
                .arg("file")
                .arg(time);
        query.exec(addStatement);
        QString updateStatement =
                QString("UPDATE local_files SET file_size='%1',"
                        "last_modified='%2',last_sync='%3' where file_name='%4'")
                        .arg(file.size()).arg(time).arg(time).arg(name);
        query.exec(updateStatement);
    }
    ui->textBrowser->append("Uploaded file: " + name );
    mTotalTransfered += mCurrentFileSize;
    processNextStep();
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

void SyncWindow::createDataBase()
{
    //qDebug() << "Creating Database!";
    if(!mDB.open()) {
        qDebug() << "Cannot open database for creation!";
        qDebug() << mDB.lastError().text();
        mDBOpen = false;
    } else {
        mDBOpen = true;
    }
    QString createLocal("create table local_files(\n"
                        "id INTEGER PRIMARY KEY ASC,\n"
                        "file_name text unique,\n"
                        "file_size text,\n"
                        "file_type text,\n"
                        "last_modified text,\n"
                        "last_sync text,\n"
                        "found text,\n"
                        "prev_modified text,\n"
                        "conflict text\n"
                        ");");
    QString createServer("create table server_files(\n"
                        "id INTEGER PRIMARY KEY ASC,\n"
                        "file_name text unique,\n"
                        "file_size text,\n"
                        "file_type text,\n"
                        "last_modified text,\n"
                        "found text,\n"
                        "prev_modified text,\n"
                        "conflict text\n"
                        ");");

    QString createConflicts("create table conflicts(\n"
                        "file_name text unique,\n"
                        "resolution text,\n"
                        "server_modified text,\n"
                        "local_modified text\n"
                        ");");

    QString createConfig("create table config(\n"
                        "host text,\n"
                        "username text,\n"
                        "password text,\n"
                        "syncdir text,\n"
                        "updatetime text\n"
                        ");");
    QSqlQuery query;
    query.exec(createLocal);
    query.exec(createServer);
    query.exec(createConfig);
    query.exec(createConflicts);

}

void SyncWindow::on_buttonSave_clicked()
{
    ui->buttonSave->setDisabled(true);
    QString host = ui->lineHost->text();
    // Add /files/webdav.php but remove what the user may have added
    mHost = host.replace("/files/webdav.php","") + "/files/webdav.php";
    mUsername = ui->lineUser->text();
    mPassword = ui->linePassword->text();
    mSyncDirectory = ui->lineSyncDir->text();
    mUpdateTime = ui->time->value();
    saveConfigToDB();
    saveDBToFile();
    initialize();
}

void SyncWindow::readConfigFromDB()
{
    QSqlQuery query;
    query.exec("SELECT * from config;");
    if(query.next()) {
        mHost = query.value(0).toString();
        mUsername = query.value(1).toString();
        mPassword = query.value(2).toString();
        mSyncDirectory = query.value(3).toString();
        mUpdateTime = query.value(4).toString().toLongLong();

        // Update the display
        QString host = mHost;
        ui->lineHost->setText(host.replace("/files/webdav.php",""));
        ui->lineUser->setText(mUsername);
        ui->linePassword->setText(mPassword);
        ui->lineSyncDir->setText(mSyncDirectory);
        ui->time->setValue(mUpdateTime);
    } else {
        // There is no configuration on the db
        mDBOpen = false;
    }
}

void SyncWindow::saveConfigToDB()
{
    QSqlQuery query;
    query.exec("SELECT * from config;");
    if(query.next()) { // Update
        QString update = QString("UPDATE config SET host='%1',username='%2',"
                       "password='%3',syncdir='%4',updatetime='%5';").arg(mHost)
                       .arg(mUsername).arg(mPassword).arg(mSyncDirectory)
                       .arg(mUpdateTime);
        query.exec(update);
    } else { // Insert
        QString add = QString("INSERT INTO config values('%1','%2',"
                              "'%3','%4','%5');").arg(mHost)
                       .arg(mUsername).arg(mPassword).arg(mSyncDirectory)
                       .arg(mUpdateTime);
        query.exec(add);
    }
}

void SyncWindow::initialize()
{
    // Initialize WebDAV
    mWebdav->initialize(mHost,mUsername,mPassword,"/files/webdav.php");

    // Create a File System Watcher

    delete mFileWatcher;
    mFileWatcher = new QFileSystemWatcher(this);
    connect(mFileWatcher,SIGNAL(fileChanged(QString)),
            this, SLOT(localFileChanged(QString)));
    connect(mFileWatcher,SIGNAL(directoryChanged(QString)),
            this, SLOT(localDirectoryChanged(QString)));

    mFileWatcher->addPath(mSyncDirectory+"/");
    mSettingsCheck = true;
    mWebdav->dirList("/");
}

void SyncWindow::settingsAreFine()
{
    // Synchronize (only if it is not synchronizing right now) then start the timer
    if(mIsFirstRun) {
        timeToSync();
    }
    mIsFirstRun = true;
    if(mSyncTimer) {
        mSyncTimer->stop();
    }
    delete mSyncTimer;
    mSyncTimer = new QTimer(this);
    connect(mSyncTimer, SIGNAL(timeout()), this, SLOT(timeToSync()));
    mSyncTimer->start(mUpdateTime*1000);
}

void SyncWindow::localDirectoryChanged(QString name)
{
    // Maybe this was caused by us renaming a file, just wait it out
    while (mFileAccessBusy ) {
        sleep(1);
    }
    // Since we don't want to be scanning the directories every single
    // time a file is changed (since temporary files could be the cause)
    // instead we'll add them to a list and have a separate timer
    // randomly go through them
    QString relativeName(name);
    relativeName.replace(mSyncDirectory,"");
    // Replace spaces because it may confuse QSet
    relativeName.replace(" ","_sssspace_");
    if( !mScanDirectoriesSet.contains(relativeName) ) {
        // Add to the list
        mScanDirectoriesSet.insert(relativeName);
        mScanDirectories.enqueue(relativeName);
    }
}

void SyncWindow::localFileChanged(QString name)
{
    //qDebug() << "Checking file status: " << name;
    QFileInfo info(name);
    name.replace(mSyncDirectory,"");
    //qDebug() << "File " << name << " changed.";
    if( info.exists() ) { // Ok, file did not get deleted
        updateDBLocalFile(name,info.size(),
                        info.lastModified().toUTC().toMSecsSinceEpoch(),"file");
    } else { // File got deleted (or moved!) I can't do anything about
        // the moves for now. But I can delete! So do that for now :)
        ui->textBrowser->append("Local file was deleted: " + name);
        deleteFromServer(name.replace(mSyncDirectory,""));
    }
}

void SyncWindow::scanLocalDirectoryForNewFiles(QString path)
{
    //qDebug() << "Scanning local directory: " << path;
    QDir dir(mSyncDirectory+path);
    QStringList list = dir.entryList();
    for( int i = 0; i < list.size(); i++ ) {

        // Skip current and previous directory and conflict related files
        QString name = list.at(i);
        if( name == "." || name == ".." ||
                name.contains("_ocs_serverconflict.") ) {
            continue;
        }

        QSqlQuery query;
        query.exec(QString("SELECT * from local_files where file_name='%1'")
                   .arg(path+list[i]));
        if( !query.next() ) { // Ok, this file does not exist. It might be a
            // directory, however, so let's check again!
            query.exec(QString("SELECT * from local_files where "
                               "file_name='%1/'").arg(path+list[i]));
            if( !query.next() ) { // Definitely does not exist! Good!
                // File really doesn't exist!!!
                processLocalFile(mSyncDirectory+path+list[i]);
            }
        }
        //QString name = pathi+"/"+list[i];
    }
}

void SyncWindow::closeEvent(QCloseEvent *event)
{
    // We definitely don't want to quit when we are synchronizing!
    while(mBusy) {
        sleep(5);
    }

    // Before closing, save the database!!!
    saveDBToFile();
    saveLogs();
    qDebug() << "All ready to close!";
    QMainWindow::closeEvent(event);
}

void SyncWindow::saveDBToFile()
{
    if( sqlite3_util::sqliteDBMemFile( mDB, mDBFileName, true ) ) {
        qDebug() << "Successfully saved DB to file!";
    } else {
        qDebug() << "Failed to save DB to file!";
    }
}

void SyncWindow::loadDBFromFile()
{
    if( sqlite3_util::sqliteDBMemFile( mDB, mDBFileName, false ) ) {
        qDebug() << "Successfully loaded DB from file!";
    } else {
        qDebug() << "Failed to load DB from file!";
    }
}

void SyncWindow::deleteRemovedFiles()
{
    // Any file that has not been found will be deleted!

    if( mIsFirstRun ) {
        //qDebug() << "Looking for server files to delete!";
        // Since we don't always query local files except for the first run
        // only do this if it is the first run
        QSqlQuery local;

        // First delete the files
        local.exec("SELECT file_name from local_files where found='' "
                   "AND file_type='file';");
        while(local.next()) {
            // Local files were deleted. Delete from server too.
            //qDebug() << "Deleting file from server: " << local.value(0).toString();
            ui->textBrowser->append(tr("Deleted server file %1").arg(
                                        local.value(0).toString()));
            deleteFromServer(local.value(0).toString());
        }

        // Then delete the collections
        local.exec("SELECT file_name from local_files where found='' "
                   "AND file_type='collection';");
        while(local.next()) {
            // Local files were deleted. Delete from server too.
            //qDebug() << "Deleting directory from server: " << local.value(0).toString();
            deleteFromServer(local.value(0).toString());
        }
    }

    //qDebug() << "Looking for local files to delete!";
    QSqlQuery server;
    // First delete the files
    server.exec("SELECT file_name from server_files where found=''"
                "AND file_type='file';");
    while(server.next()) {
        // Server files were deleted. Delete from local too.
        qDebug() << "Deleting file from local:  " << server.value(0).toString();
        ui->textBrowser->append("Deleting local file: "+
                                server.value(0).toString());
        deleteFromLocal(server.value(0).toString(),false);
    }

    // Then delete the collections
    server.exec("SELECT file_name from server_files where found=''"
                "AND file_type='collection';");
    while(server.next()) {
        // Server files were deleted. Delete from local too.
        //qDebug() << "Deleting directory from local:  " << server.value(0).toString();
        deleteFromLocal(server.value(0).toString(),true);
    }
}

void SyncWindow::deleteFromLocal(QString name, bool isDir)
{
    // Remove the watcher before deleting.
    mFileWatcher->removePath(mSyncDirectory+name);
    if( !QFile::remove(mSyncDirectory+name ) ) {
            qDebug() << "File deletion failed: " << mSyncDirectory+name;
            return;
    }
    ui->textBrowser->append("Deleting local file: " + name);
    dropFromDB("local_files","file_name",name);
    dropFromDB("server_files","file_name",name);
}

void SyncWindow::deleteFromServer(QString name)
{
    // Delete from server
    mWebdav->deleteFile(name);
    ui->textBrowser->append("Deleting from server: " + name) ;
    dropFromDB("server_files","file_name",name);
    dropFromDB("local_files","file_name",name);
}

void SyncWindow::dropFromDB(QString table, QString column, QString condition)
{
    QSqlQuery drop;
    drop.exec("DELETE FROM "+table+" WHERE "+column+"='"+condition+"';");
}

void SyncWindow::on_buttonSyncDir_clicked()
{
    QString syncDir = QFileDialog::getExistingDirectory(this);
    if( syncDir != "" ) {
        ui->lineSyncDir->setText(syncDir);
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

void SyncWindow::on_lineSyncDir_textEdited(QString text)
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
    ui->tableWidget->clear();
    QTableWidgetItem *name;
    QTableWidgetItem *serverTime;
    QTableWidgetItem *localTime;
    QComboBox *combo;
    int row = 0;
    QStringList headers;
    headers.append("File Name");
    headers.append("Server Modified");
    headers.append("Local Modifed");
    headers.append("Which wins?");
    ui->tableWidget->setHorizontalHeaderLabels(headers);



    QSqlQuery query;
    query.exec("SELECT * from conflicts;");
    while( query.next() ) {
        ui->tableWidget->setRowCount(row+1);
        name = new QTableWidgetItem(query.value(0).toString());
        serverTime = new QTableWidgetItem(query.value(2).toString());
        localTime = new QTableWidgetItem(query.value(3).toString());
        combo = new QComboBox(ui->tableWidget);
        combo->addItem("Choose:");
        combo->addItem("server");
        combo->addItem("local");
        ui->tableWidget->setItem(row, 0, name);
        ui->tableWidget->setItem(row, 1, serverTime);
        ui->tableWidget->setItem(row, 2, localTime);
        ui->tableWidget->setCellWidget(row,3,combo);
        row++;
    }
    ui->tableWidget->resizeColumnsToContents();
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->stackedWidget->setCurrentIndex(1);
}

void SyncWindow::on_buttonBox_accepted()
{
    // Check the selections are valid
    //mSyncTimer->start(mUpdateTime);
    mFileAccessBusy = true;
    bool allConflictsResolved = true;
    for( int row = 0; row < ui->tableWidget->rowCount(); row++ ) {
        QComboBox *combo = (QComboBox*)ui->tableWidget->cellWidget(row,3);
        if( combo->currentIndex() == 0 ) {
            allConflictsResolved = false;
            continue;
        }
        processFileConflict(ui->tableWidget->takeItem(row,0)->text(),
                            combo->currentText());

        //qDebug() << ui->tableWidget->takeItem(row,0)->text()
        //            << combo->currentIndex();
    }
    if( allConflictsResolved) {
        ui->conflict->setEnabled(false);
        mConflictsExist = false;
    }
    mFileAccessBusy = false;
    ui->stackedWidget->setCurrentIndex(0);
}

void SyncWindow::on_buttonBox_rejected()
{
    //mSyncTimer->start(mUpdateTime);
    ui->stackedWidget->setCurrentIndex(0);
}

void SyncWindow::processFileConflict(QString name, QString wins)
{
    if( wins == "local" ) {
        QFileInfo info(mSyncDirectory+name);
        QFile::remove(mSyncDirectory+getConflictName(name));
        mUploadingConflictFiles.enqueue(FileInfo(name,info.size()));
        mUploadingConflictFilesSet.insert(name.replace(" ","_sssspace_"));
    } else {
        // Stop watching the old file, since it will get removed
        mFileWatcher->removePath(mSyncDirectory+name);
        QFileInfo info(mSyncDirectory+getConflictName(name));
        qint64 last = info.lastModified().toMSecsSinceEpoch();
        QFile::remove(mSyncDirectory+name);
        QFile::rename(mSyncDirectory+getConflictName(name),
                      mSyncDirectory+name);
        QSqlQuery query;
        QString statement = QString("UPDATE local_files SET last_sync='%1'"
                                  "WHERE file_name='%2';").arg(last).arg(name);
        query.exec(statement);

        // Add back to the watcher
        mFileWatcher->addPath(mSyncDirectory+name);
        clearFileConflict(name);
    }
}

void SyncWindow::clearFileConflict(QString name)
{
    QSqlQuery query;
    QString statement = QString("DELETE FROM conflicts where file_name='%1';")
            .arg(name);
    query.exec(statement);
    statement = QString("UPDATE local_files set conflict='' where file_name='%1';")
            .arg(name);
    query.exec(statement);
    statement = QString("UPDATE server_files set conflict='' where file_name='%1';")
            .arg(name);
    query.exec(statement);
}

QString SyncWindow::getConflictName(QString name)
{
    QFileInfo info(name);
    return QString(info.absolutePath()+
                   "/_ocs_serverconflict."+info.fileName());
}
