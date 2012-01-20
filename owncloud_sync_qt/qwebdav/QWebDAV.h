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
#ifndef QWEBDAV_H
#define QWEBDAV_H

#include <QNetworkAccessManager>
#include <QSslError>
#include <QDebug>
#include <QNetworkReply>

class QBuffer;
class QUrl;
class QFile;
class QWebDAVTransferRequestReply;


class QWebDAV : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit QWebDAV(QObject *parent = 0);
    void initialize(QString hostname, QString username, QString password,
                    QString pathFilter = "");

    enum DAVType {
        DAVNONE,
        DAVLIST,
        DAVGET,
        DAVPUT,
        DAVMKCOL,
        DAVDELETE,
        DAVMOVE,
        DAVLOCK,
        DAVUNLOCK
    };

    enum ATTRIBUTETYPE {
        ATTDATA = 1,
        ATTFILE,
        ATTPREFIX,
        ATTLOCKTYPE
    };

    struct TransferLockRequest {
        bool put;
        QString fileName;
        QString fileNameTemp;
        QString absoluteFileName;
        QString put_prefix;
        QString token;
        QString tokenTemp;
        QWebDAVTransferRequestReply *reply;
        TransferLockRequest() {}
        TransferLockRequest(bool isPut, QString name, QString temp, QString path,
                            QString prefix,QWebDAVTransferRequestReply *Reply)
        {
            put = isPut;
            fileName = name;
            fileNameTemp = temp;
            absoluteFileName = path;
            put_prefix = prefix;
            reply = Reply;
            token = "";
            tokenTemp = "";
        }
    };

    struct FileInfo {
        QString fileName;
        QString lastModified;
        qlonglong size;
        qlonglong sizeAvailable;
        QString type;
        bool locked;
        FileInfo(QString name, QString last, qlonglong fileSize,
                 qlonglong available, QString fileType, bool lock = false ) {
            fileName = name;
            lastModified = last;
            size = fileSize;
            sizeAvailable = available;
            if( fileType == "" ) {
                type = "file";
            } else {
                type = fileType;
            }
            locked = lock;
        };
        QString toString()
        {
            QString available;
            if( type == "collection") {
                available = QString("\nAvailable Space: %1").arg(sizeAvailable);
            }
            QString output = QString("\nFile Name:       %1\nLast Modified:   %2"
                                     "\nFile Size:       %3\nType:            %4"
                                     )
                                     .arg(fileName)
                                     .arg(lastModified).arg(size).arg(type);
                    output += available;
            output.append("\n\n\n");
            return output;

        }
        void print() {
            qDebug() << toString().toAscii();
        }

        QString formatSize()
        {
            //if ( size > 1099511627776 ) // 1TB
            return QString();
        }
    };

    // DAV Public Functions
    QNetworkReply* deleteFile(QString name);
    void dirList(QString dir = "/");
    QNetworkReply* list(QString dir, int depth = 1);
    QNetworkReply* get(QString fileName );
    QNetworkReply* put(QString fileName , QByteArray data,
                       QString put_prefix="");
    QNetworkReply* put(QString fileName , QString absoluteFileName,
                       QString put_prefix="");
    QNetworkReply* mkdir(QString dirName );
    QNetworkReply* sendWebdavRequest( QUrl url, DAVType type,
                                      QByteArray verb = 0,QIODevice *data = 0,
                                      QString extra = "1", QString extra2 = "");
    QNetworkReply* lock(QString name, QString type = "");
    QNetworkReply* unlock(QString name);
    QNetworkReply* unlock(QString name, QString token);

private:
    QString mHostname;
    QString mUsername;
    QString mPassword;
    QString mPathFilter;
    bool mInitialized;
    bool mFirstAuthentication;
    static qint64 mRequestNumber;
    QHash<qint64,QByteArray*> mRequestQueries;
    QHash<qint64,QBuffer*>    mRequestData;
    QHash<qint64,QFile*> mRequestFile;
    QHash<QString,QString> mLockTokens;
    QHash<QString,TransferLockRequest> mTransferLockRequests;

    void processDirList(QByteArray xml, QString url);
    void processFile(QNetworkReply* reply);
    void processLocalDirectory(QString dirPath);
    void processPutFinished(QNetworkReply *reply);
    void connectReplyFinished(QNetworkReply *reply);
    void processLockRequest(QByteArray xml, QString url, QString type);
    QNetworkReply* put_locked(QString fileName , QByteArray data,
                       QString put_prefix="");
    QNetworkReply* put_locked(QString fileName , QString absoluteFileName,
                       QString put_prefix="");

signals:
    void directoryListingReady(QList<QWebDAV::FileInfo>);
    void fileReady(QNetworkReply *reply, QString fileName);
    void uploadComplete(QString name);
    void directoryCreated(QString name);
    void directoryListingError(QString url);
    void errorFileLocked(QString fileName);

public slots:
    void slotFinished ( QNetworkReply* );
    void slotReplyFinished();
    void slotAuthenticationRequired(QNetworkReply*, QAuthenticator*);
    void slotReadyRead();
    void slotSslErrors(QList<QSslError> errorList);
    void slotError(QNetworkReply::NetworkError error);
};

class QWebDAVTransferRequestReply : public QNetworkReply
{
    Q_OBJECT
public:
    QWebDAVTransferRequestReply() { mReply = 0; }
    ~QWebDAVTransferRequestReply()
    {
        if(mReply)
            mReply->deleteLater();
    }

    void setReply(QNetworkReply *reply,bool get = true)
    {
        mReply = reply;
        if(get) {
            connect(reply,SIGNAL(downloadProgress(qint64,qint64)),
                    this, SLOT(downloadProgress(qint64,qint64)));
        } else {
            connect(reply,SIGNAL(uploadProgress(qint64,qint64)),
                    this, SLOT(uploadProgress(qint64,qint64)));
        }
    };

    qint64 readData(char*,qint64) {}
    void abort() {}
private:
    QNetworkReply *mReply;

public slots:
    void downloadProgress(qint64 current,qint64 total)
    {
        emit downloadProgress(current,total);
    }

    void uploadProgress(qint64 current,qint64 total)
    {
        emit uploadProgress(current,total);
    }
};

#endif // QWEBDAV_H
