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
    QQueue<FileInfo> mDownloadConflict;
    QQueue<FileInfo> mUploadingConflictFiles;
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
    QIcon mDefaultConflictIcon;
    QIcon mSyncConflictIcon;
    QFileSystemWatcher *mFileWatcher;
    bool mIsFirstRun;
    bool mDownloadingConflictingFile;
    QSet<QString> mScanDirectoriesSet;
    QQueue<QString> mScanDirectories;
    QSet<QString> mUploadingConflictFilesSet;
    bool mFileAccessBusy;
    bool mConflictsExist;

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
    void setFileConflict(QString name, qint64 size, QString server_last,
                         QString local_last);
    void processFileConflict(QString name, QString wins);
    void clearFileConflict(QString name);
    QString getConflictName(QString name);

public slots:
    void processDirectoryListing(QList<QWebDAV::FileInfo> fileInfo);
    void processFileReady(QByteArray data,QString fileName);
    void updateDBUpload(QString fileName);
    void timeToSync();
    void updateStatus();
    void transferProgress(qint64 current,qint64 total);
    void systemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void localFileChanged(QString name);
    void localDirectoryChanged(QString name);
    void closeEvent(QCloseEvent *event);
    void saveDBToFile();
    void loadDBFromFile();

    // GUI related slots
    void on_buttonSave_clicked();
    void on_buttonSyncDir_clicked();
    void on_linePassword_textEdited(QString text);
    void on_lineHost_textEdited(QString text);
    void on_lineSyncDir_textEdited(QString text);
    void on_lineUser_textEdited(QString text);
    void on_time_valueChanged(int value);
    void on_conflict_clicked();
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
};

#endif // SYNCWINDOW_H
