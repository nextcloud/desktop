#ifndef SYNCWINDOW_H
#define SYNCWINDOW_H

#include <QMainWindow>
#include "QWebDAV.h"
#include <QSqlDatabase>
#include <QQueue>
#include <QHash>
#include <QSystemTrayIcon>
#include <QIcon>

class QTimer;

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
    QQueue<QString> mDirectoryQueue;
    QString mHomeDirectory;
    QString mSyncDirectory;
    QString mConfigDirectory;
    QString mHost;
    QString mPassword;
    QString mUsername;
    QTimer *mSyncTimer;
    QTimer *mStatusTimer;
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

public slots:
    void processDirectoryListing(QList<QWebDAV::FileInfo> fileInfo);
    void processFileReady(QByteArray data,QString fileName);
    void updateDBUpload(QString fileName);
    void timeToSync();
    void updateStatus();
    void transferProgress(qint64 current,qint64 total);
    void systemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void saveButtonClicked();
};

#endif // SYNCWINDOW_H
