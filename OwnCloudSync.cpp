/******************************************************************************
 *    Copyright 2011 Juan Carlos Cornejo jc2@paintblack.com
 *
 *    This file is part of owncloud_sync.
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
#include "OwnCloudSync.h"
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

#ifdef Q_OS_LINUX
    #include <kwallet.h>
#endif

OwnCloudSync::OwnCloudSync(QString name, WId id) : mAccountName(name),mWinId(id)
{
    mBusy = false;

    // Set the pointers so we can delete them without worrying :)
    mSyncTimer = 0;
    mFileWatcher = 0;
    mWallet = 0;

    mHardStop = false;
    mIsFirstRun = true;
    mDownloadingConflictingFile = false;
    mFileAccessBusy = false;
    mConflictsExist = false;
    mSettingsCheck = true;
    mIsEnabled = false;
    mAllowedToSync = false;
    mNeedsSync = false;
    mNotifySyncEmitted = false;
    mLastSyncAborted = SYNCFINISHED;
    mSyncPosition = SYNCFINISHED;

    mRequestTimer = new QTimer(this);
    connect(mRequestTimer,SIGNAL(timeout()),this,SLOT(requestTimedout()));

    // Create a QWebDAV instance
    mWebdav = new QWebDAV();

    // Connect to QWebDAV signals
    connect(mWebdav,SIGNAL(directoryListingError(QString)),
            this, SLOT(directoryListingError(QString)));
    connect(mWebdav,SIGNAL(directoryListingReady(QList<QWebDAV::FileInfo>)),
            this, SLOT(processDirectoryListing(QList<QWebDAV::FileInfo>)));
    connect(mWebdav,SIGNAL(fileReady(QByteArray,QString)),
            this, SLOT(processFileReady(QByteArray,QString)));

    connect(mWebdav,SIGNAL(uploadComplete(QString)),
            this, SLOT(updateDBUpload(QString)));
    connect(mWebdav,SIGNAL(directoryCreated(QString)),
            this, SLOT(serverDirectoryCreated(QString)));

    mDownloadingFiles.clear();
    mDownloadConflict.clear();
    mUploadingFiles.clear();

    // Initialize the Database
    mDB = QSqlDatabase::addDatabase("QSQLITE",mAccountName);
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
    mDBFileName = mConfigDirectory+"/"+mAccountName+".db";
#endif
    // Find out if the database exists.
    QFile dbFile(mConfigDirectory+"/"+mAccountName+".db");
    if( dbFile.exists() ) {
        if(!mDB.open()) {
            qDebug() << "Cannot open database!";
            qDebug() << mDB.lastError().text();
            mDBOpen = false;
        } else {
            mDBOpen = true;
            loadDBFromFile();
            readConfigFromDB();
            //ui->buttonSave->setDisabled(true);
            qDebug() << "Checking configuration!";
#ifdef Q_OS_LINUX
            // Wait until the password is set
#else
            initialize();
#endif
        }
    } else {
      createDataBase(); // Create the database in memory
    }

    mSaveDBTimer = new QTimer(this);
    connect(mSaveDBTimer, SIGNAL(timeout()), this, SLOT(saveDBToFile()));
    mSaveDBTimer->start(370000);
    updateStatus();

    // Now the password management
#ifdef Q_OS_LINUX
    mWallet= KWallet::Wallet::openWallet(
                KWallet::Wallet::NetworkWallet(),
                mWinId,KWallet::Wallet::Asynchronous);
    connect(mWallet, SIGNAL(walletOpened(bool)), SLOT(walletOpened(bool)));
#else
#endif
}

void OwnCloudSync::setEnabled( bool enabled)
{
    mIsEnabled = enabled;
    saveConfigToDB();
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    query.exec("SELECT * FROM config;");
    if(query.next() ) { // Update
        query.exec(QString("UPDATE config SET enabled='%1';")
                   .arg(mIsEnabled?"yes":"no"));
    } else {
        query.exec(QString("INSERT INTO config(enabled) values('%1');")
                   .arg(mIsEnabled?"yes":"no"));
    }

    if(mIsEnabled) {
        qDebug() << "Starting " << mAccountName;
        start();
    } else {
        qDebug() << "Stopping " << mAccountName;
        stop();
    }
}

void OwnCloudSync::directoryListingError(QString url)
{
    if(mSettingsCheck) {
        qDebug() << "Something wrong with the settings, please check.";
        emit toLog(tr("Settings could not be confirmed for account %1. Please "
                      "confirm your settings and try again.").arg(mAccountName));
    }
}

void OwnCloudSync::updateStatus()
{
    //if( !isVisible() )
    //    return;


    if( !mBusy ) {
        /*
        emit toStatus(tr("Waiting %1 seconds...") + QString("%1").arg(mUpdateTime) +
                            " seconds...");
        ui->progressFile->setValue(0);
        ui->progressTotal->setValue(0);
        if(mConflictsExist) {
             ui->labelImage->setPixmap(mDefaultConflictIcon.pixmap(129,129));
        } else {
            ui->labelImage->setPixmap(mDefaultIcon.pixmap(129,129));
        } */
    } else {
        if( mSyncTimer )
           mSyncTimer->stop();
        emit toStatus(tr("%1 out of %2 bytes").arg(mTransferState+mCurrentFile)
                      .arg(mCurrentFileSize));
        /*if(mConflictsExist) {
            ui->labelImage->setPixmap(mSyncConflictIcon.pixmap(129,129));
        } else {
            ui->labelImage->setPixmap(mSyncIcon.pixmap(129,129));
        }*/
    }
}

void OwnCloudSync::timeToSync()
{
    mNotifySyncEmitted = true;
    emit readyToSync(this);
}

void OwnCloudSync::sync()
{
    mNeedsSync = true;
    mNotifySyncEmitted = false;
    if(!mIsEnabled) {
        return;
    }

    if ( mBusy ) {
        emit toLog(tr("Ooops, looks like %1 is busy, we'll try again later")
                   .arg(mAccountName));
        return;
    }

    if( !mDBOpen ) {
        emit toStatus(tr("Database is not open. Aborting sync!"));
        return;
    }

    // Announce we are busy!
    mBusy = true;
    if(mSyncTimer)
        mSyncTimer->stop();

    emit toLog(tr("\nSynchronizing %1 on: %2")
               .arg(mAccountName)
               .arg(QDateTime::currentDateTime().toString()));

    // First, continue right where the last sync left off.
    if( mLastSyncAborted != SYNCFINISHED ) {
        switch(mLastSyncAborted) {
        emit toLog(tr("Last sync unsuccessful. Resumming."));
        case LISTLOCALDIR:
            scanLocalDirectory(mLocalDirectory);
            break;
        case LISTREMOTEDIR:
            restartRequestTimer();
            mWebdav->dirList(mRemoteDirectory+"/");
            return;
        case TRANSFER:
            processNextStep();
            return;
        default:
            break;
        }
    }

    // If this is the first run, scan the directory, otherwise just wait
    // for the watcher to update us :)
    if(mIsFirstRun) {
        //emit toLog("Clear files found!");
        QSqlQuery query(QSqlDatabase::database(mAccountName));
        query.exec("UPDATE  local_files SET found='' WHERE conflict='';");
        //qDebug() << "Scanning local directory: ";
        scanLocalDirectory(mLocalDirectory);
        //qDebug() << "Scanning local directory!!!";
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

    // Then scan the base directory of the WebDAV server
    //qDebug() << "Scanning server: " << mRemoteDirectory+"/";
    mWebdav->dirList(mRemoteDirectory+"/");
    mSyncPosition = LISTREMOTEDIR;
    restartRequestTimer();
}

OwnCloudSync::~OwnCloudSync()
{
    delete mWebdav;
    mDB.close();
}

void OwnCloudSync::processDirectoryListing(QList<QWebDAV::FileInfo> fileInfo)
{
    stopRequestTimer();
    if( mSettingsCheck ) {
        // Great, we were just checking
        mSettingsCheck = false;
        settingsAreFine();
        return;
    }
    // Compare against the database of known files
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    QSqlQuery add(QSqlDatabase::database(mAccountName));
    for(int i = 0; i < fileInfo.size(); i++ ){
        query = queryDBFileInfo(fileInfo[i].fileName,"server_files");
        if(query.next()) { // File exists confirm no conflict, then update
            if( query.value(7).toString() == "" ) {
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
                emit conflictExists(this);
                mConflictsExist = true;
                //qDebug() << "SFile still conflicts: " << fileInfo[i].fileName;
            }
        } else { // File does not exist, so just add this info to the DB
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
        mSyncPosition = LISTREMOTEDIR;
        restartRequestTimer();
    } else {
        syncFiles();
    }
}

void OwnCloudSync::processFileReady(QByteArray data,QString fileName)
{
    fileName = stringRemoveBasePath(fileName,mRemoteDirectory);
    // Temporarily remove this watcher so we don't get a message when
    // we modify it.
    if(mFileWatcher)
        mFileWatcher->removePath(mLocalDirectory+fileName);
    QString finalName;
    if(mDownloadingConflictingFile) {
        finalName = getConflictName(fileName);
        //qDebug() << "Downloading conflicting file " << fileName;
    } else {
        finalName = fileName;
    }
    QFile file(mLocalDirectory+finalName);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Could not open file " << file.fileName() << "for writting.";
        processNextStep();
        return;
    }
    QDataStream out(&file);
    out.writeRawData(data.constData(),data.length());
    file.flush();
    file.close();
    updateDBDownload(fileName);
    if(mFileWatcher)
        mFileWatcher->addPath(mLocalDirectory+fileName); // Add the watcher back!
    processNextStep();
}

void OwnCloudSync::processNextStep()
{
    stopRequestTimer();
    if(mHardStop) { // Hard stop, usually indicates account will be removed
        return;
    }

    mSyncPosition = TRANSFER;

    if( mMakeServerDirs.size() != 0 ) {
            mWebdav->mkdir(mMakeServerDirs.dequeue());
            restartRequestTimer();
            //qDebug() << "Making the following directories on server: " <<
          //            serverDirs[i];
    // Check if there is another file to dowload, if so, start that process
    }else if( mDownloadingFiles.size() != 0 ) {
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
        emit conflictExists(this);
    } else { // We are done! Start the sync clock
        mDownloadingConflictingFile = false;
        mBusy = false;
        if(mSyncTimer)
            mSyncTimer->start();
        emit toLog(tr("Finished %1: %2").arg(mAccountName)
                                .arg(QDateTime::currentDateTime().toString()));
        if(mConflictsExist) {
            //mSystemTray->setIcon(mDefaultConflictIcon);
        } else {
            //mSystemTray->setIcon(mDefaultIcon);
        }
        QSqlQuery query(QSqlDatabase::database(mAccountName));
        query.exec(QString("UPDATE config SET lastsync='%1';").arg(
                       QDateTime::currentDateTime().toString()));
        mNeedsSync = false;
        mLastSyncAborted = SYNCFINISHED;
        mSyncPosition = SYNCFINISHED;
        emit finishedSync(this);
    }
    updateStatus();
}

QString OwnCloudSync::getLastSync()
{
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    query.exec("SELECT lastsync FROM config;");
    if( query.next() ) {
        return query.value(0).toString();
    }
    return QString("");
}

void OwnCloudSync::scanLocalDirectory( QString dirPath)
{
    QDir dir(dirPath);
    dir.setFilter(QDir::Files|QDir::NoDot|QDir::NoDotDot|QDir::AllEntries
                  |QDir::Hidden);
    QStringList list = dir.entryList();
    for( int i = 0; i < list.size(); i++ ) {
        QString name = list.at(i);
        //qDebug() << "Processing local file: " + dirPath+"/"+list.at(i);
        if( isFileFiltered(name) ) {
            continue;
        }

        //qDebug() << "Relative Path: " << relativeName;
        processLocalFile(dirPath + "/" + name);

        // Check if it is a directory, and if so, process it
    }
}

void OwnCloudSync::processLocalFile(QString name)
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

void OwnCloudSync::updateDBLocalFile(QString name, qint64 size, qint64 last,
                                   QString type )
{
    // Do not upload the server conflict files
    if( isFileFiltered(name)) {
        return;
    }
    // Get the relative name of the file
    name = stringRemoveBasePath(name, mLocalDirectory);
    name = mRemoteDirectory + name;
    //qDebug() << "Local file name: " << name;
    // Check against the database
    QSqlQuery query = queryDBFileInfo(name,"local_files");
    if (query.next() ) { // We already knew about this file. Update info.
        qint64 prevModified = query.value(4).toString().toLongLong();
        // Sometimes the watcher goes crazy, though. So check to see
        // if last == previous, if so, then it never changed anything!
        //qDebug() << "Last: " << last << " Prev: " << prevModified;
        if( (last != prevModified) || mIsFirstRun ) {
            if ( query.value(8).toString() == "") {
                QString updateStatement =
                        QString("UPDATE local_files SET file_size='%1',"
                                "last_modified='%2',found='yes',"
                                "prev_modified='%3' where file_name='%4'")
                        .arg(size)
                        .arg(last)
                        .arg(prevModified)
                        .arg(name);
                //qDebug() << "Query:   " << updateStatement;
                query.exec(updateStatement);
            } else {
                // Enable the conflict resolution button
                emit conflictExists(this);
                mConflictsExist = true;
                //qDebug() << "LFile still conflicts: " << name;
            }
        }
    } else { // We did not know about this file, add
        QString addStatement = QString("INSERT INTO local_files (file_name,"
                             "file_size,file_type,last_modified,found,conflict) "
                                       "values('%1','%2','%3','%4','yes','');")
                .arg(name).arg(size).arg(type).arg(last);
        //qDebug() << "Query: " << addStatement;
        query.exec(addStatement);
    }
    mNeedsSync = true;  // Since a local file was changed, we need to sync
                        // before closing
    //qDebug() << "Processing: " << mLocalDirectory + relativeName << " Size: "
    //         << file.size();
}

QSqlQuery OwnCloudSync::queryDBFileInfo(QString fileName, QString table)
{
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    query.exec("SELECT * FROM " + table + " WHERE file_name = '" +
                     fileName + "';");
    return query;
}

QSqlQuery OwnCloudSync::queryDBAllFiles(QString table)
{
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    query.exec("SELECT * FROM " + table + ";");
    return query;
}

void OwnCloudSync::syncFiles()
{
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
            if(localName == "/more/numbers.txt") {
                qDebug() << localModifiedTime << localPrevModifiedTime
                            << serverModifiedTime << serverPrevModifiedTime
                               << lastSyncTime;
                localName = localName;
            } */

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
                        /*qDebug() << "Conflict with sfile " << localName
                                 << serverModifiedTime << serverPrevModifiedTime
                                 << localModifiedTime << lastSyncTime;*/
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
                mMakeServerDirs.enqueue(localName);
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
    for( int i = 0; i < mDownloadConflict.size(); i++ ) {
        mTotalToDownload += mDownloadConflict[i].size;
    }
    for( int i = 0; i < mUploadingConflictFiles.size(); i++ ) {
        mTotalToUpload += mUploadingConflictFiles[i].size;
    }
    mTotalToTransfer = mTotalToDownload+mTotalToUpload;

    // Make local dirs
    for(int i = 0; i < localDirs.size(); i++ ) {
        QDir dir;
        if (!dir.mkdir(mLocalDirectory+
                       stringRemoveBasePath(localDirs[i],mRemoteDirectory)) ) {
            qDebug() << "Could not make directory "+mLocalDirectory+
                        stringRemoveBasePath(localDirs[i],mRemoteDirectory);
        } else {
            emit toLog(tr("Created local directory: %1").arg(localDirs[i]));
            //qDebug() << "Made directory "+mLocalDirectory+localDirs[i];
        }
    }

    // Delete removed files and reset the file status
    deleteRemovedFiles();
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    //query.exec("UPDATE  local_files SET found='' WHERE conflict='';");
    query.exec("UPDATE server_files SET found='' WHERE conflict='';");

    mIsFirstRun = false;

    // Let's get the ball rolling!
    processNextStep();
}

void OwnCloudSync::setFileConflict(QString name, qint64 size, QString server_last,
                                 QString local_last)
{
    QSqlQuery conflict(QSqlDatabase::database(mAccountName));
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
    emit toMessage(tr("%1 has a conflict!").arg(mAccountName),
                   tr("File %1 conflicts.").arg(name),
                             QSystemTrayIcon::Warning);
    emit conflictExists(this);
}

void OwnCloudSync::download( FileInfo file )
{
    qDebug() << "Will download file: " << file.name;
    mCurrentFileSize = file.size;
    mCurrentFile = file.name;
    if(mDownloadingConflictingFile) {
        mTransferState = tr("Downloading conflicting file ");
    } else {
        mTransferState = tr("Downloading ");
    }
    QNetworkReply *reply = mWebdav->get(file.name);
    connect(reply, SIGNAL(downloadProgress(qint64,qint64)),
            this, SLOT(transferProgress(qint64,qint64)));
    restartRequestTimer();
    updateStatus();
}

void OwnCloudSync::upload( FileInfo fileInfo)
{
    QString localName = fileInfo.name;
    localName = stringRemoveBasePath(localName,mRemoteDirectory);
    mCurrentFileSize = fileInfo.size;
    mCurrentFile = fileInfo.name;
    mTransferState = tr("Uploading ");
    qDebug() << "Uploading File " +mLocalDirectory + mCurrentFile;
    QFile file(mLocalDirectory+localName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "File read error " + mLocalDirectory+localName+" Code: "
                    << file.error();
        return;
    }
    QByteArray data = file.readAll();
    QNetworkReply *reply = mWebdav->put(fileInfo.name,data);
    connect(reply, SIGNAL(uploadProgress(qint64,qint64)),
            this, SLOT(transferProgress(qint64,qint64)));
    restartRequestTimer();
    updateStatus();
}

void OwnCloudSync::updateDBDownload(QString name)
{
    // This seems redundant, a little, really.
    QString fileName = mLocalDirectory+name;
    QFileInfo file(fileName);
    QString dbName = name;
    if( mRemoteDirectory != "/") {
        dbName = mRemoteDirectory + name;
    }

    // Check against the database
    QSqlQuery query = queryDBFileInfo(dbName,"local_files");
    if (query.next() ) { // We already knew about this file. Update.
        QString updateStatement =
                QString("UPDATE local_files SET file_size='%1',"
                        "last_modified='%2',last_sync='%3' where file_name='%4'")
                        .arg(file.size())
                        .arg(file.lastModified().toUTC()
                             .toMSecsSinceEpoch())
                        .arg(file.lastModified().toUTC()
                              .toMSecsSinceEpoch())
                        .arg(dbName);
        query.exec(updateStatement);
    } else { // We did not know about this file, add
        QString addStatement = QString("INSERT INTO local_files (file_name,"
                             "file_size,file_type,last_modified,last_sync) "
                                       "values('%1','%2','%3','%4','%5');")
                .arg(dbName).arg(file.size())
                .arg("file")
                .arg(file.lastModified().toUTC().toMSecsSinceEpoch())
                .arg(file.lastModified().toUTC().toMSecsSinceEpoch());
        query.exec(addStatement);
    }
    QString downloadText;
    if( mDownloadingConflictingFile ) {
        downloadText = tr("Downloaded conflicting file: %1").arg(dbName);
    } else {
        downloadText = tr("Downloaded file: %1").arg(dbName);
    }
    emit toLog(downloadText);
    //qDebug() << "Did this get called?";
    mTotalTransfered += mCurrentFileSize;
}

void OwnCloudSync::updateDBUpload(QString name)
{
    QString fileName = mLocalDirectory+name;
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
    emit toLog(tr("Uploaded file: %1").arg(name));
    mTotalTransfered += mCurrentFileSize;
    processNextStep();
}

void OwnCloudSync::transferProgress(qint64 current, qint64 total)
{
    stopRequestTimer();
    // First update the current file progress bar
    qint64 percent;
    if ( total > 0 ) {
        percent = 100*current/total;
        emit progressFile(percent);
        //ui->progressFile->setValue(percent);
    } else {
        percent = 0;
    }

    // Then update the total progress bar
    qint64 additional = (mCurrentFileSize*percent)/100;
    if (mTotalToTransfer > 0) {
        emit progressTotal(100*(mTotalTransfered+additional)/mTotalToTransfer);
    }
    restartRequestTimer();
}

void OwnCloudSync::createDataBase()
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
                        "localdir text,\n"
                        "updatetime text,\n"
                        "enabled text,\n"
                        "remotedir text,\n"
                        "lastsync text\n"
                        ");");

    QString createFilters("create table filters(\n"
                          "filter text"
                          ");");
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    query.exec(createLocal);
    query.exec(createServer);
    query.exec(createConfig);
    query.exec(createConflicts);
    query.exec(createFilters);

}

void OwnCloudSync::readConfigFromDB()
{
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    query.exec("SELECT * from config;");
    if(query.next()) {
        mHost = query.value(0).toString();
        mUsername = query.value(1).toString();
        mReadPassword = true;
        mPassword = query.value(2).toString();
        mLocalDirectory = query.value(3).toString();
        mRemoteDirectory = query.value(6).toString();
        mUpdateTime = query.value(4).toString().toLongLong();
        if( query.value(5).toString() == "yes" ) {
            mIsEnabled = true;
        } else {
            mIsEnabled = false;
        }
    } else {
        // There is no configuration on the db
        mDBOpen = false;
    }

    // Now also read the filters on file
    query.exec("SELECT * from filters;");
    while(query.next()) {
        mFilters.insert(query.value(0).toString());
    }
}

void OwnCloudSync::removeFilter(QString filter)
{
    mFilters.remove(filter);
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    query.exec(QString("DELETE FROM filters WHERE filter='%1';").arg(filter));
}

void OwnCloudSync::addFilter(QString filter)
{
    if(!mFilters.contains(filter)) {
        mFilters.insert(filter);
        QSqlQuery query(QSqlDatabase::database(mAccountName));
        query.exec(QString("INSERT into filters values('%1');").arg(filter));
    }
}

void OwnCloudSync::saveConfigToDB()
{
    bool savePw = true;
#ifdef Q_OS_LINUX
    savePw = false;
#endif
    QSqlQuery query(QSqlDatabase::database(mAccountName));
    query.exec("SELECT * from config;");
    if(query.next()) { // Update
        QString update = QString("UPDATE config SET host='%1',username='%2',"
                       "password='%3',localdir='%4',updatetime='%5',"
                                 "enabled='%6',remotedir='%7';").arg(mHost)
                .arg(mUsername).arg(savePw?mPassword:"").arg(mLocalDirectory)
                       .arg(mUpdateTime).arg(mIsEnabled?"yes":"no")
                       .arg(mRemoteDirectory);
        query.exec(update);
    } else { // Insert
        QString add = QString("INSERT INTO config values('%1','%2',"
                              "'%3','%4','%5','%6','%7');").arg(mHost)
                .arg(mUsername).arg(savePw?mPassword:"").arg(mLocalDirectory)
                       .arg(mUpdateTime).arg(mIsEnabled?"yes":"no")
                       .arg(mRemoteDirectory);
        query.exec(add);
    }
}

void OwnCloudSync::initialize()
{
    initialize(mHost,mUsername,mPassword,mRemoteDirectory,mLocalDirectory,
               mUpdateTime);
}

void OwnCloudSync::settingsAreFine()
{
    if(mIsEnabled) {
        start();
    } else {
        stop();
    }
}

void OwnCloudSync::start()
{
    if(mSyncTimer)
        delete mSyncTimer;
    mSyncTimer = new QTimer(this);
    connect(mSyncTimer, SIGNAL(timeout()), this, SLOT(timeToSync()));
    mSyncTimer->start(mUpdateTime*1000);
}

void OwnCloudSync::stop()
{
    if( mNeedsSync && !mNotifySyncEmitted )
        emit readyToSync(this);

    if(mSyncTimer)
        mSyncTimer->stop();
    delete mSyncTimer;
    mSyncTimer = 0;
}

void OwnCloudSync::deleteWatcher()
{
    // Delete the watcher. Should only be called when we are quitting!!!
    delete mFileWatcher;
    mFileWatcher = 0;
}

void OwnCloudSync::localDirectoryChanged(QString name)
{
    // Maybe this was caused by us renaming a file, just wait it out
    while (mFileAccessBusy ) {
        sleep(1);
    }
    // If it was caused by one directory being deleted, then delete it now
    // and don't scan it!
    QDir dir(name);
    if( !dir.exists() ) {
        name = stringRemoveBasePath(name,mLocalDirectory);
        name = mRemoteDirectory + name;
        emit toLog(tr("Local directory was deleted: %1").arg(name));
        deleteFromServer(stringRemoveBasePath(name,mLocalDirectory));
        return;
    }
    // Since we don't want to be scanning the directories every single
    // time a file is changed (since temporary files could be the cause)
    // instead we'll add them to a list and have a separate timer
    // randomly go through them
    QString relativeName(name);
    relativeName = stringRemoveBasePath(relativeName,mLocalDirectory);
    // Replace spaces because it may confuse QSet
    relativeName.replace(" ","_sssspace_");
    if( !mScanDirectoriesSet.contains(relativeName) ) {
        // Add to the list
        mScanDirectoriesSet.insert(relativeName);
        mScanDirectories.enqueue(relativeName);
    }
}

void OwnCloudSync::localFileChanged(QString name)
{
    //qDebug() << "Checking file status: " << name;
    QFileInfo info(name);
    name = stringRemoveBasePath(name,mLocalDirectory);
    if( info.exists() ) { // Ok, file did not get deleted
        updateDBLocalFile(name,info.size(),
                        info.lastModified().toUTC().toMSecsSinceEpoch(),"file");
    } else { // File got deleted (or moved!) I can't do anything about
        // the moves for now. But I can delete! So do that for now :)
        name = mRemoteDirectory + name;
        emit toLog(tr("Local file was deleted: %1").arg(name));
        deleteFromServer(stringRemoveBasePath(name,mLocalDirectory));
    }
}

void OwnCloudSync::scanLocalDirectoryForNewFiles(QString path)
{
    QString remote;
    //path = path=="/"?"":path;
    if(mRemoteDirectory != "/") {
        remote = mRemoteDirectory+"/";
    }
    //qDebug() << "Scanning local directory: " << path;
    QDir dir(mLocalDirectory+path);
    dir.setFilter(QDir::Files|QDir::NoDot|QDir::NoDotDot|QDir::AllEntries
                  |QDir::Hidden);
    QStringList list = dir.entryList();
    for( int i = 0; i < list.size(); i++ ) {
        // Skip current and previous directory and conflict related files
        QString name = list.at(i);
        if( isFileFiltered(name) ) {
            continue;
        }

        QSqlQuery query(QSqlDatabase::database(mAccountName));
        query.exec(QString("SELECT * from local_files where file_name='%1'")
                   .arg(path+remote+list[i]));
        if( !query.next() ) { // Ok, this file does not exist. It might be a
            // directory, however, so let's check again!
            query.exec(QString("SELECT * from local_files where "
                               "file_name='%1/'").arg(path+remote+list[i]));
            if( !query.next() ) { // Definitely does not exist! Good!
                // File really doesn't exist!!!
                //qDebug() << "New file found!" << path +remote + list[i];
                processLocalFile(mLocalDirectory+path+list[i]);
            }
        }
        //QString name = pathi+"/"+list[i];
    }
}

void OwnCloudSync::saveDBToFile()
{
    if( sqlite3_util::sqliteDBMemFile( mDB, mDBFileName, true ) ) {
        qDebug() << "Successfully saved DB to file!";
    } else {
        qDebug() << "Failed to save DB to file!";
    }
#ifdef Q_OS_LINUX
    saveWalletPassword();
#endif
}

void OwnCloudSync::loadDBFromFile()
{
    if( sqlite3_util::sqliteDBMemFile( mDB, mDBFileName, false ) ) {
        qDebug() << "Successfully loaded DB from file!";
    } else {
        qDebug() << "Failed to load DB from file!";
    }
}

void OwnCloudSync::deleteRemovedFiles()
{
    // Any file that has not been found will be deleted!

    if( mIsFirstRun ) {
        //qDebug() << "Looking for server files to delete!";
        // Since we don't always query local files except for the first run
        // only do this if it is the first run
        QSqlQuery local(QSqlDatabase::database(mAccountName));

        // First delete the files
        local.exec("SELECT file_name from local_files where found='' "
                   "AND file_type='file';");
        while(local.next()) {
            // Local files were deleted. Delete from server too.
            //qDebug() << "Deleting file from server: " << local.value(0).toString();
            //emit toLog(tr("File claims to be not found: %1").arg(
            //                            local.value(0).toString()));
            deleteFromServer(local.value(0).toString());
        }

        // Then delete the collections
        local.exec("SELECT file_name from local_files where found='' "
                   "AND file_type='collection';");
        while(local.next()) {
            //emit toLog(tr("Directory claims to be not found: %1").arg(
            //                            local.value(0).toString()));
            // Local files were deleted. Delete from server too.
            qDebug() << "Deleting directory from server: " << local.value(0).toString();
            deleteFromServer(local.value(0).toString());
        }
    }

    //qDebug() << "Looking for local files to delete!";
    QSqlQuery server(QSqlDatabase::database(mAccountName));
    // First delete the files
    server.exec("SELECT file_name from server_files where found=''"
                "AND file_type='file';");
    while(server.next()) {
        // Server files were deleted. Delete from local too.
        qDebug() << "Deleting file from local:  " << server.value(0).toString();
        //emit toLog(tr("Deleting local file: %1").arg(
        //               server.value(0).toString()));
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

void OwnCloudSync::deleteFromLocal(QString name, bool isDir)
{
    // Remove the watcher before deleting.
    QString localName = stringRemoveBasePath(name,mRemoteDirectory);
    mFileWatcher->removePath(mLocalDirectory+localName);
    if(!isDir) {
        if( !QFile::remove(mLocalDirectory+localName ) ) {
            qDebug() << "File deletion failed: " << mLocalDirectory+localName;
            return;
        }
        emit toLog(tr("Deleted local file: %1").arg(name));
    } else {
        QDir dir;
        if( !dir.rmdir(mLocalDirectory+localName) ) {
            qDebug() << "Directory deletion failed: "
                        << mLocalDirectory+localName;
            return;
        }
        emit toLog(tr("Deleted local directory: %1").arg(name));
    }
    dropFromDB("local_files","file_name",name);
    dropFromDB("server_files","file_name",name);
}

void OwnCloudSync::deleteFromServer(QString name)
{
    // Delete from server
    mWebdav->deleteFile(name);
    emit toLog(tr("Deleting from server: %1").arg(name));
    dropFromDB("server_files","file_name",name);
    dropFromDB("local_files","file_name",name);
}

void OwnCloudSync::dropFromDB(QString table, QString column, QString condition)
{
    QSqlQuery drop(QSqlDatabase::database(mAccountName));
    drop.exec("DELETE FROM "+table+" WHERE "+column+"='"+condition+"';");
}

void OwnCloudSync::processFileConflict(QString name, QString wins)
{
    QString localName = stringRemoveBasePath(name,mRemoteDirectory);
    if( wins == "local" ) {
        QFileInfo info(mLocalDirectory+localName);
        QFile::remove(mLocalDirectory+getConflictName(localName));
        mUploadingConflictFiles.enqueue(FileInfo(name,info.size()));
        mUploadingConflictFilesSet.insert(name.replace(" ","_sssspace_"));
    } else {
        // Stop watching the old file, since it will get removed
        mFileWatcher->removePath(mLocalDirectory+localName);
        QFileInfo info(mLocalDirectory+getConflictName(localName));
        qint64 last = info.lastModified().toMSecsSinceEpoch();
        QFile::remove(mLocalDirectory+localName);
        QFile::rename(mLocalDirectory+getConflictName(localName),
                      mLocalDirectory+localName);
        QSqlQuery query(QSqlDatabase::database(mAccountName));
        QString statement = QString("UPDATE local_files SET last_sync='%1'"
                                  "WHERE file_name='%2';").arg(last).arg(name);
        query.exec(statement);

        // Add back to the watcher
        mFileWatcher->addPath(mLocalDirectory+localName);
        clearFileConflict(name);
    }
}

void OwnCloudSync::clearFileConflict(QString name)
{
    QSqlQuery query(QSqlDatabase::database(mAccountName));
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

QString OwnCloudSync::getConflictName(QString name)
{
    QFileInfo info(name);
    return QString(info.absolutePath()+
                   "/_ocs_serverconflict."+info.fileName());
}

void OwnCloudSync::initialize(QString host, QString user, QString pass,
                              QString remote, QString local, qint64 time)
{
    mHost = stringRemoveBasePath(host,"/files/webdav.php");
    mUsername = user;
    mPassword = pass;
    mRemoteDirectory = remote;
    mLocalDirectory = local;
    mUpdateTime = time;
    // Initialize WebDAV
    mWebdav->initialize(mHost+"/files/webdav.php",
                        mUsername,mPassword,"/files/webdav.php");

    // Create a File System Watcher

    delete mFileWatcher;
    mFileWatcher = new QFileSystemWatcher(this);
    connect(mFileWatcher,SIGNAL(fileChanged(QString)),
            this, SLOT(localFileChanged(QString)));
    connect(mFileWatcher,SIGNAL(directoryChanged(QString)),
            this, SLOT(localDirectoryChanged(QString)));

    mFileWatcher->addPath(mLocalDirectory+"/");
    saveConfigToDB();
    saveDBToFile();
    mSettingsCheck = true;
    mWebdav->dirList(remote+"/");
    mSyncPosition = CHECKSETTINGS;
    restartRequestTimer();
}

QStringList OwnCloudSync::getFilterList()
{
    QStringList list;
    QList<QString> filters = mFilters.toList();
    for( int i = 0; i < filters.size(); i++ ) {
        list << filters[i];
        //qDebug() << filters[i];
    }
    return list;
}

bool OwnCloudSync::isFileFiltered(QString name)
{
    // Standard filters applicable to *ALL* files
    if( name == "." || name == ".." ||
         name.contains("_ocs_serverconflict.")) {
        //qDebug() << "File: " +name+" ignored by " + mAccountName;
        return true;
    }
    QList<QString> list = mFilters.toList();

    // Else, look through the filters and see if this file is excluded
    for( int i = 0; i < mFilters.size(); i++ ) {
        QString filter = list[i];
        if(filter.contains("*")) { // Must build general expression
            filter.replace("?","\\\?");
            filter.replace(".","\\\.");
            filter.replace("*",".*");
            QRegExp reg(filter);
            if( name.contains(reg) ) {
                //qDebug() << "File: " +name+" ignored by " + mAccountName + " because of " + filter;
                return true;
            }

        } else if( name.contains(filter) ) {
            //qDebug() << "File: " +name+" ignored by " + mAccountName + " because of " + filter;
            return true;
        }
    }
    return false;
}

void OwnCloudSync::deleteAccount()
{
    // Stop all transfer processes
    mHardStop = true;

    // Stop and delete the timer
    if(mSyncTimer) {
        mSyncTimer->stop();
    }

    // Delete the database
    mDB.close();
    QFile dbFile(mConfigDirectory+"/"+mAccountName+".db");
    dbFile.remove();
}

void OwnCloudSync::requestTimedout()
{
    //emit toLog(tr("The request timed out"));
    mBusy = false;
    start();
    emit toLog(tr("Sync timedout %1: %2").arg(mAccountName)
                            .arg(QDateTime::currentDateTime().toString()));
    emit finishedSync(this);
    mLastSyncAborted = mSyncPosition;
    stopRequestTimer();
}

void OwnCloudSync::restartRequestTimer()
{
    mRequestTimer->start(7000);
}

void OwnCloudSync::stopRequestTimer()
{
    mRequestTimer->stop();
}

bool OwnCloudSync::needsSync()
{
    if(mLastSyncAborted != SYNCFINISHED ) {
        return false;
    }
    return mNeedsSync;
}

#ifdef Q_OS_LINUX

void OwnCloudSync::requestPassword()
{
    QMap<QString,QString> map;
    mWallet->readMap(mAccountName,map);
    if(map.size()) {
        mPassword = map[mAccountName];
        initialize();
    }
}

void OwnCloudSync::walletOpened(bool ok)
{
    if( (ok && (mWallet->hasFolder("owncloud_sync"))
         || mWallet->createFolder("owncloud_sync"))
         && mWallet->setFolder("owncloud_sync")) {
            //emit toLog("Wallet opened!");
            //qDebug() << "Wallet opened!" <<
            //           KWallet::Wallet::FormDataFolder() ;
            if (mReadPassword ) {
                requestPassword();
            }
    } else {
        qDebug() << "Error opening wallet";
    }
}

void OwnCloudSync::saveWalletPassword()
{
    QMap<QString,QString> map;
    map[mAccountName] = mPassword;
    if( mWallet )
        mWallet->writeMap(mAccountName,map);
}

#endif

QString OwnCloudSync::stringRemoveBasePath(QString path, QString base)
{
    if( base != "/" )  {
        path.replace(QRegExp("^"+base),"");
    }
    return path;
}

void OwnCloudSync::serverDirectoryCreated(QString name)
{
    emit toLog(tr("Created directory on server: %1").arg(name));
    processNextStep();
}
