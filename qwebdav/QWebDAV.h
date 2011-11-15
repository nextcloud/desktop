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
#ifndef QWEBDAV_H
#define QWEBDAV_H

#include <QNetworkAccessManager>
#include <QSslError>
#include <QDebug>
#include <QNetworkReply>

class QNetworkReply;
class QBuffer;
class QUrl;


class QWebDAV : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit QWebDAV(QObject *parent = 0);
    void initialize(QString hostname, QString username, QString password,
                    QString pathFilter = "");

    enum DAVType {
        DAVLIST,
        DAVGET,
        DAVPUT,
        DAVMKCOL,
        DAVDELETE
    };

    struct FileInfo {
        QString fileName;
        QString lastModified;
        qlonglong size;
        qlonglong sizeAvailable;
        QString type;
        FileInfo(QString name, QString last, qlonglong fileSize,
                 qlonglong available, QString fileType ) {
            fileName = name;
            lastModified = last;
            size = fileSize;
            sizeAvailable = available;
            if( fileType == "" ) {
                type = "file";
            } else {
                type = fileType;
            }
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
    QNetworkReply* put(QString fileName , QByteArray data);
    QNetworkReply* mkdir(QString dirName );
    QNetworkReply* sendWebdavRequest( QUrl url, DAVType type,
                                      QByteArray verb = 0,QIODevice *data = 0,
                                      int depth = 1);

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

    void processDirList(QByteArray xml, QString url);
    void processFile(QNetworkReply* reply);
    void processLocalDirectory(QString dirPath);
    void connectReplyFinished(QNetworkReply *reply);

signals:
    void directoryListingReady(QList<QWebDAV::FileInfo>);
    void fileReady(QByteArray data, QString fileName);
    void uploadComplete(QString name);
    void directoryCreated(QString name);
    void directoryListingError(QString url);

public slots:
    void slotFinished ( QNetworkReply* );
    void slotReplyFinished();
    void slotAuthenticationRequired(QNetworkReply*, QAuthenticator*);
    void slotReadyRead();
    void slotSslErrors(QList<QSslError> errorList);
    void slotError(QNetworkReply::NetworkError error);

};

#endif // QWEBDAV_H
