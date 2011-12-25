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
#ifndef OWNCLOUDSYNC_H
#define OWNCLOUDSYNC_H

#include "SyncGlobal.h"

#include <QMainWindow>
#include "QWebDAV.h"
#include <QSqlDatabase>
#include <QQueue>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QSet>
#include <QSqlQuery>

class QTimer;
class QFileSystemWatcher;
class QNetworkReply;
class OwnPasswordManager;

class OwnCloudSync : public QObject
{
    Q_OBJECT

public:
    explicit OwnCloudSync(QString name, OwnPasswordManager *passwordManager,
                          QSet<QString> *globalFilters,QString configDir);
    ~OwnCloudSync();
    void initialize(QString host, QString user, QString pass, QString remote,
                    QString local, qint64 time);

    struct FileInfo {
        QString name;
        qint64 size;
        FileInfo(QString fileName, qint64 fileSize) {
            name = fileName;
            size = fileSize;
        }
    };

    enum SyncPosition {
        SYNCFINISHED,
        CHECKSETTINGS,
        LISTREMOTEDIR,
        LISTLOCALDIR,
        TRANSFER
    };

    QSqlQuery getConflicts() {
        QSqlQuery query(QSqlDatabase::database(mAccountName));
        query.exec("SELECT * from conflicts;");
        return query;
    };

    QString getName() { return mAccountName; }
    QString getHost() { return mHost; }
    QString getUserName() { return mUsername; }
    QString getPassword() { return mPassword; }
    QString getRemoteDirectory() { return mRemoteDirectory; }
    QString getLocalDirectory() { return mLocalDirectory; }
    qint64 getUpdateTime() { return mUpdateTime; }
    bool isEnabled() { return mIsEnabled; }

    void setEnabled(bool enabled);
    void saveConfigToDB();
    void processFileConflict(QString name, QString wins);
    bool needsSync();
    void deleteWatcher();
    void stop();
    QString getLastSync();
    QStringList getFilterList();
    void addFilter(QString filter);
    void removeFilter(QString filter);
    void hardStop();
    void deleteAccount();
    void setSaveDBTime(qint64 seconds);
    void pause() { mIsPaused = true; }
    void resume() {
        mIsPaused = false;
        if(mSyncPosition == TRANSFER) {
            processNextStep();
        }
    }

private:
    bool mStorePasswordInDB;
    OwnPasswordManager *mPasswordManager;
    QWebDAV *mWebdav;
    bool mIsEnabled;
    bool mAllowedToSync;
    bool mNeedsSync;
    bool mNotifySyncEmitted;
    bool mHardStop;
    QSet<QString> mFilters;
    QSet<QString> *mGlobalFilters;
    QString mLastSync;
    QSqlDatabase mDB;
    QString mDBFileName;
    QQueue<QString> mDirectoryQueue;
    QString mHomeDirectory;
    QString mRemoteDirectory;
    QString mLocalDirectory;
    QString mConfigDirectory;
    QString mHost;
    QString mPassword;
    QString mUsername;
    QTimer *mSyncTimer;
    QTimer *mSaveDBTimer;
    QTimer *mRequestTimer;
    QQueue<QString>  mMakeServerDirs;
    QQueue<FileInfo> mUploadingFiles;
    QQueue<FileInfo> mDownloadingFiles;
    QQueue<FileInfo> mDownloadConflict;
    QQueue<FileInfo> mUploadingConflictFiles;
    qint64 mTotalToDownload;
    qint64 mTotalToUpload;
    qint64 mTotalToTransfer;
    qint64 mTotalTransfered;
    qint64 mTotalDownloaded;
    qint64 mTotalUploaded;
    qint64 mCurrentFileSize;
    QString mCurrentFile;
    QString mTransferState;
    bool mBusy;
    bool mDBOpen;
    qint64 mUpdateTime;
    QFileSystemWatcher *mFileWatcher;
    bool mIsFirstRun;
    bool mDownloadingConflictingFile;
    QSet<QString> mScanDirectoriesSet;
    QQueue<QString> mScanDirectories;
    QSet<QString> mUploadingConflictFilesSet;
    bool mFileAccessBusy;
    bool mConflictsExist;
    bool mSettingsCheck;
    QString mAccountName;
    SyncPosition mLastSyncAborted;
    SyncPosition mSyncPosition;
    bool mReadPassword;
    bool mIsPaused;

    void updateDBLocalFile(QString name,qint64 size,qint64 last,QString type);
    void scanLocalDirectory(QString dirPath);
    QSqlQuery queryDBFileInfo(QString fileName, QString table);
    QSqlQuery queryDBAllFiles(QString table);
    void syncFiles();
    void upload(FileInfo fileName);
    void download(FileInfo fileName);
    void updateDBDownload(QString fileName);
    void copyServerProcessing(QString fileName);
    void copyLocalProcessing(QString fileName);
    void processNextStep();
    void createDataBase();
    void updateDBVersion(int fromVersion);
    void initialize();
    void readConfigFromDB();
    void scanLocalDirectoryForNewFiles(QString name);
    void processLocalFile(QString name);
    void deleteRemovedFiles();
    void deleteFromLocal(QString name, bool isDir);
    void deleteFromServer(QString name);
    void dropFromDB(QString table, QString column, QString condition );
    void setFileConflict(QString name, qint64 size, QString server_last,
                         QString local_last);
    void clearFileConflict(QString name);
    QString getConflictName(QString name);
    void settingsAreFine();
    void start();
    bool isFileFiltered(QString name);
    void restartRequestTimer();
    void stopRequestTimer();

    // String manipulation functions
    QString stringRemoveBasePath(QString path, QString base);

signals:
    void toLog(QString text);
    void toStatus(QString text);
    void toMessage(QString caption, QString body,
                   QSystemTrayIcon::MessageIcon icon);
    void conflictExists(OwnCloudSync*);
    void conflictResolved(OwnCloudSync*);
    void progressFile(qint64 value);
    void progressTotal(qint64 value);
    void readyToSync(OwnCloudSync*);
    void finishedSync(OwnCloudSync*);

public slots:
    void directoryListingError(QString url);
    void processDirectoryListing(QList<QWebDAV::FileInfo> fileInfo);
    void processFileReady(QNetworkReply *reply,QString fileName);
    void updateDBUpload(QString fileName);
    void timeToSync();
    void sync();
    void updateStatus();
    void transferProgress(qint64 current,qint64 total);
    void localFileChanged(QString name);
    void localDirectoryChanged(QString name);
    void saveDBToFile();
    void loadDBFromFile();
    void requestTimedout();
    void serverDirectoryCreated(QString name);
};

#endif // OWNCLOUDSYNC_H
