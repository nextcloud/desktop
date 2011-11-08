#ifndef OWNCLOUDSYNC_H
#define OWNCLOUDSYNC_H

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

class OwnCloudSync : public QObject
{
    Q_OBJECT

public:
    explicit OwnCloudSync(QString name);
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
    bool needsSync() { return mNeedsSync; }
    void deleteWatcher();
    void stop();
    QString getLastSync();

private:
    QWebDAV *mWebdav;
    bool mIsEnabled;
    bool mAllowedToSync;
    bool mNeedsSync;
    bool mNotifySyncEmitted;
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

    void updateDBLocalFile(QString name,qint64 size,qint64 last,QString type);
    void scanLocalDirectory(QString dirPath);
    QSqlQuery queryDBFileInfo(QString fileName, QString table);
    QSqlQuery queryDBAllFiles(QString table);
    void syncFiles();
    void upload(FileInfo fileName);
    void download(FileInfo fileName);
    void updateDBDownload(QString fileName);
    void processNextStep();
    void createDataBase();
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
    void processFileReady(QByteArray data,QString fileName);
    void updateDBUpload(QString fileName);
    void timeToSync();
    void sync();
    void updateStatus();
    void transferProgress(qint64 current,qint64 total);
    void localFileChanged(QString name);
    void localDirectoryChanged(QString name);
    void saveDBToFile();
    void loadDBFromFile();
};

#endif // OWNCLOUDSYNC_H
