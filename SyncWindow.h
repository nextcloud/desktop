#ifndef SYNCWINDOW_H
#define SYNCWINDOW_H

#include <QMainWindow>
#include "QWebDAV.h"
#include <QSqlDatabase>
#include <QQueue>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QSet>

class QTimer;
class QFileSystemWatcher;

namespace Ui {
    class SyncWindow;
}

namespace sqlite3_util {
    bool sqliteDBMemFile( QSqlDatabase memdb, QString filename, bool save );
}

class SyncWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SyncWindow(QWidget *parent = 0);
    ~SyncWindow();
    QWebDAV *mWebdav;

    struct FileInfo {
        QString name;
        qint64 size;
        FileInfo(QString fileName, qint64 fileSize) {
            name = fileName;
            size = fileSize;
        }
    };

private:
    Ui::SyncWindow *ui;
    QSystemTrayIcon *mSystemTray;
    QSqlDatabase mDB;
    QString mDBFileName;
    QQueue<QString> mDirectoryQueue;
    QString mHomeDirectory;
    QString mSyncDirectory;
    QString mConfigDirectory;
    QString mHost;
    QString mPassword;
    QString mUsername;
    QTimer *mSyncTimer;
    QTimer *mSaveDBTimer;
    QQueue<FileInfo> mUploadingFiles;
    QQueue<FileInfo> mDownloadingFiles;
    //QHash<QString,qlonglong> mUploadingFiles;
    //QHash<QString,qlonglong> mDownloadingFiles;
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
    QIcon mDefaultIcon;
    QIcon mSyncIcon;
    QFileSystemWatcher *mFileWatcher;
    bool mIsFirstRun;
    QSet<QString> mScanDirectoriesSet;
    QQueue<QString> mScanDirectories;

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
    void saveConfigToDB();
    void scanLocalDirectoryForNewFiles(QString name);
    void processLocalFile(QString name);
    void deleteRemovedFiles();
    void deleteFromLocal(QString name, bool isDir);
    void deleteFromServer(QString name);
    void dropFromDB(QString table, QString column, QString condition );

public slots:
    void processDirectoryListing(QList<QWebDAV::FileInfo> fileInfo);
    void processFileReady(QByteArray data,QString fileName);
    void updateDBUpload(QString fileName);
    void timeToSync();
    void updateStatus();
    void transferProgress(qint64 current,qint64 total);
    void systemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void saveButtonClicked();
    void localFileChanged(QString name);
    void localDirectoryChanged(QString name);
    void closeEvent(QCloseEvent *event);
    void saveDBToFile();
    void loadDBFromFile();
};

#endif // SYNCWINDOW_H
